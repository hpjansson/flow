/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-shunt-impl-unix.c - Shunt implementation for Unix-likes.
 *
 * Copyright (C) 2006 Hans Petter Jansson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Hans Petter Jansson <hpj@copyleft.no>
 */

/* This file is not compiled by itself. It is #included in flow-shunt.c */

/* Implementation details
 * ----------------------
 *
 * This is the generic async I/O implementation. It favours portability at
 * the expense of efficiency. In particular, it should work on both Linux,
 * Windows and BSD derivates. It requires thread support.
 *
 * Sockets and pipes are handled together in one thread that blocks on
 * select (). Reads and writes are nonblock, so sockets do not block each
 * other.
 *
 * Files are handled with one thread each. This is because file I/O
 * always blocks on Linux (and probably other Unix OSes). Each file thread
 * is blocking on either read () or write () depending on the operation
 * currently being performed, or a GCond if the thread is idle.
 *
 * Another reason to handle files differently is that they are not
 * interchangeable with sockets on Windows, where select (), recv () and
 * send () only apply to sockets, while read () and write () only apply
 * to files. */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>

#ifndef G_PLATFORM_WIN32
# include <unistd.h>
# include <sys/socket.h>
# include <sys/stat.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/tcp.h>
#else
# include <io.h>
# include <process.h>
# include <winsock2.h>
#endif

#include <time.h>

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
# ifdef SO_NOSIGPIPE
#  define USE_SO_NOSIGPIPE 1
# endif
#endif

#include "flow-anonymous-event.h"
#include "flow-common-impl-unix.h"

#define DEBUG(x)

/* Stack size used for file shunt threads and the socket shunt thread. Not
 * to be used for user-implemented worker threads. */

#define THREAD_STACK_SIZE 16384

#ifdef G_DISABLE_ASSERT
# define assert_non_fatal_errno(errnum, fatal_errnos) \
  G_STMT_START{ (void)0; }G_STMT_END
#else
# define assert_non_fatal_errno(errnum, fatal_errnos) \
  real_assert_non_fatal_errno (errnum, fatal_errnos, __FILE__, __LINE__)
#endif

typedef enum
{
  SHUNT_TYPE_THREAD,
  SHUNT_TYPE_FILE,
  SHUNT_TYPE_PIPE,
  SHUNT_TYPE_TCP,
  SHUNT_TYPE_TCP_LISTENER,
  SHUNT_TYPE_UDP
}
ShuntType;

typedef struct
{
  FlowShunt       shunt;

  gint64          read_bytes_remaining;
  gint64          read_offset;

  GCond          *cond;
  gint            fd;
}
FileShunt;

/* Parameters passed to file thread */
typedef struct
{
  FileShunt *file_shunt;

  gchar     *path;

  guint      proc_access  : 6;

  guint      user_access  : 6;
  guint      group_access : 6;
  guint      other_access : 6;

  guint      create       : 1;
  guint      destructive  : 1;
}
FileShuntParams;

typedef struct
{
  FlowShunt  shunt;

  GCond     *cond;
}
ThreadShunt;

typedef struct
{
  ThreadShunt    *thread_shunt;

  FlowWorkerFunc *worker_func;
  gpointer        worker_data;
}
ThreadShuntParams;

typedef struct
{
  FlowShunt shunt;

  GPid      child_pid;

  gint      read_fd;
  gint      write_fd;
}
PipeShunt;

typedef struct
{
  FlowShunt shunt;
  gint      fd;
}
SocketShunt;

typedef struct
{
  SocketShunt  socket_shunt;

  FlowSockaddr remote_src_sa;
  FlowSockaddr remote_dest_sa;
}
UdpShunt;

typedef struct
{
  SocketShunt socket_shunt;
}
TcpShunt;

typedef struct
{
  SocketShunt socket_shunt;
  FlowQuality quality;  /* QoS for spawned sockets */
}
TcpListenerShunt;

typedef struct
{
  const gchar *domain;
  gint         code;
}
ErrCode;

typedef struct
{
  gint    errnum;
  ErrCode errcodes [3];
}
ErrnoMap;

static gpointer socket_shunt_main (void);

static GStaticMutex    global_mutex       = G_STATIC_MUTEX_INIT;
static FlowWakeupPipe  wakeup_pipe        = FLOW_WAKEUP_PIPE_INVALID;
static GThread        *watch_thread;
static GPtrArray      *active_socket_shunts;
static gpointer        socket_buffer      = NULL;

/* --------------------------------------- *
 * Errno maps for Linux 2.6.16 / glibc 2.4 *
 * --------------------------------------- */

static const ErrnoMap file_open_errno_map [] =
{
  /* The requested access to the file is not allowed, or search
   * permission is denied for one of the directories in the path prefix
   * of pathname, or the file did not exist yet and write access to
   * the parent directory is not allowed. (See also path_resolution(2).) */
  { EACCES,        { { FLOW_FILE_DOMAIN,   FLOW_FILE_PERMISSION_DENIED },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Pathname refers to a directory and the access requested involved
   * writing (that is, O_WRONLY or O_RDWR is set). */
  { EISDIR,        { { FLOW_FILE_DOMAIN,   FLOW_FILE_IS_NOT_A_FILE },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Too many symbolic links were encountered in resolving pathname,
   * or O_NOFOLLOW was specified but pathname was a symbolic link. */
  { ELOOP,         { { FLOW_FILE_DOMAIN,   FLOW_FILE_TOO_MANY_LINKS },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* The process already has the maximum number of files open. */
  { EMFILE,        { { FLOW_FILE_DOMAIN,   FLOW_FILE_OUT_OF_HANDLES },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR } } },

  /* The system limit on the total number of open files has been
   * reached. */
  { ENFILE,        { { FLOW_FILE_DOMAIN,   FLOW_FILE_OUT_OF_HANDLES },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 } } },

  /* Pathname was too long. */
  { ENAMETOOLONG,  { { FLOW_FILE_DOMAIN,   FLOW_FILE_PATH_TOO_LONG },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Pathname was to be created but the device containing pathname
   * has no room for the new file. */
  { ENOSPC,        { { FLOW_FILE_DOMAIN,   FLOW_FILE_NO_SPACE },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 } } },

  /* A component used as a directory in pathname is not, in fact, a
   * directory, or O_DIRECTORY was specified and pathname was not a
   * directory. */
  { ENOTDIR,       { { FLOW_FILE_DOMAIN,   FLOW_FILE_DOES_NOT_EXIST },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* O_NONBLOCK | O_WRONLY is set, the named file is a FIFO and no
   * process has the file open for reading. Or, the file is a device
   * special file and no corresponding device exists. */
  { ENXIO,         { { FLOW_FILE_DOMAIN,   FLOW_FILE_IS_NOT_A_FILE },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Pathname refers to a file on a read-only filesystem and write
   * access was requested. */
  { EROFS,         { { FLOW_FILE_DOMAIN,   FLOW_FILE_IS_READ_ONLY },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Pathname refers to an executable image which is currently being
   * executed and write access was requested. */
  { ETXTBSY,       { { FLOW_FILE_DOMAIN,   FLOW_FILE_IS_LOCKED },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Pathname refers to a device special file and no corresponding
   * device exists.  (This is a Linux kernel bug; in this situation
   * ENXIO must be returned.) */
  { ENODEV,        { { FLOW_FILE_DOMAIN,   FLOW_FILE_IS_NOT_A_FILE },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* O_CREAT is not set and the named file does not exist. Or, a
   * directory component in pathname does not exist or is a dangling
   * symbolic link. */
  { ENOENT,        { { FLOW_FILE_DOMAIN,   FLOW_FILE_DOES_NOT_EXIST },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Insufficient kernel memory was available. */
  { ENOMEM,        { { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 },
                     { NULL,               -1 } } },

  { 0,             { { NULL,               -1 },
                     { NULL,               -1 },
                     { NULL,               -1 } } }
};

static const gint file_open_fatal_errnos [] =
{
  /* Pathname refers to a regular file, too large to be opened; see
   * O_LARGEFILE. We should handle large files, though, so either Flow
   * has a bug or it's been miscompiled. */
  EOVERFLOW,

  /* The O_NOATIME flag was specified, but the effective user ID of
   * the caller did not match the owner of the file and the caller
   * was not privileged (CAP_FOWNER). We don't support O_NOATIME. */
  EPERM,

  /* Pathname already exists and O_CREAT and O_EXCL were used. We don't
   * support O_EXCL, so this never happens. If you want to create a
   * lock file, read the open(2) man page, and write your own code to
   * do that. */
  EEXIST,

  /* The O_NONBLOCK flag was specified, and an incompatible lease was
   * held on the file (see fcntl(2)). We never specify O_NONBLOCK, so this
   * should not happen. */
  EWOULDBLOCK,

  0
};

static const ErrnoMap file_read_errno_map [] =
{
  /* Low-level I/O error */
  { EIO,           { { FLOW_STREAM_DOMAIN, FLOW_STREAM_PHYSICAL_ERROR },
                     { NULL,               -1 },
                     { NULL,               -1 } } },

  { 0,             { { NULL,               -1 },
                     { NULL,               -1 },
                     { NULL,               -1 } } }
};

static const gint file_read_fatal_errnos [] =
{
  EBADF,
  EFAULT,
  EINVAL,
  EISDIR,
  0
};

static const ErrnoMap file_write_errno_map [] =
{
  /* Low-level I/O error */
  { EIO,           { { FLOW_STREAM_DOMAIN, FLOW_STREAM_PHYSICAL_ERROR },
                     { NULL,               -1 },
                     { NULL,               -1 } } },

  /* Out of disk space */
  { ENOSPC,        { { FLOW_FILE_DOMAIN,   FLOW_FILE_NO_SPACE, },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 } } },

  { 0,             { { NULL,               -1 },
                     { NULL,               -1 },
                     { NULL,               -1 } } }
};

static const gint file_write_fatal_errnos [] =
{
  EBADF,
  EFAULT,
  EINVAL,
  0
};

static const ErrnoMap tcp_bind_errno_map [] =
{
  /* The address is protected, and the user is not the superuser. */
  { EACCES,        { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ADDRESS_PROTECTED },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* The given address is already in use. */
  { EADDRINUSE,    { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ADDRESS_IN_USE },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* A non-existent interface was requested or the requested source
   * address was not local. */
  { EADDRNOTAVAIL, { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ADDRESS_DOES_NOT_EXIST },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Network device not available or not capable of sending IP. */
  { ENODEV,        { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ADDRESS_DOES_NOT_EXIST },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Not enough free memory. This often means that the memory allocation
   * is limited by the socket buffer limits, not by the system memory,
   * but this is not 100% consistent. */
  { ENOBUFS,       { { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 },
                     { NULL,               -1 } } },

  /* Not enough free memory. This often means that the memory allocation
   * is limited by the socket buffer limits, not by the system memory,
   * but this is not 100% consistent. */
  { ENOMEM,        { { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 },
                     { NULL,               -1 } } },

  { 0,             { { NULL,               -1 },
                     { NULL,               -1 },
                     { NULL,               -1 } } }
};

static const gint tcp_bind_fatal_errnos [] =
{
  EBADF,
  ENOTSOCK,
  EFAULT,
  EINVAL,
  ESOCKTNOSUPPORT,
  0
};

static const ErrnoMap tcp_listen_errno_map [] =
{
  /* Another socket is already listening on the same port. */
  { EADDRINUSE,    { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ADDRESS_IN_USE },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  { 0,             { { NULL,               -1 },
                     { NULL,               -1 },
                     { NULL,               -1 } } }
};

static const gint tcp_listen_fatal_errnos [] =
{
  EBADF,
  ENOTSOCK,
  EOPNOTSUPP,
  0
};

static const ErrnoMap tcp_accept_errno_map [] =
{
  /* Not enough free memory. This often means that the memory allocation
   * is limited by the socket buffer limits, not by the system memory,
   * but this is not 100% consistent. */
  { ENOBUFS,       { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ACCEPT_ERROR },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 } } },

  /* Not enough free memory. This often means that the memory allocation
   * is limited by the socket buffer limits, not by the system memory,
   * but this is not 100% consistent. */
  { ENOMEM,        { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ACCEPT_ERROR },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 } } },

  /* The process already has the maximum number of file descriptors
   * open. */
  { EMFILE,        { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ACCEPT_ERROR },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR } } },

  /* The system limit on the total number of open files has been
   * reached. */
  { ENFILE,        { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ACCEPT_ERROR },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 } } },

  { 0,             { { NULL,               -1 },
                     { NULL,               -1 },
                     { NULL,               -1 } } }
};

static const gint tcp_accept_fatal_errnos [] =
{
  EBADF,
  EINVAL,
  ENOTSOCK,
  EOPNOTSUPP,
  EFAULT,
  0
};

static const ErrnoMap tcp_connect_errno_map [] =
{
  /* The user tried to connect to a broadcast address without having
   * the socket broadcast flag enabled or the connection request
   * failed because of a local firewall rule. */
  { EACCES,        { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ADDRESS_PROTECTED },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* The user tried to connect to a broadcast address without having
   * the socket broadcast flag enabled or the connection request
   * failed because of a local firewall rule. */
  { EPERM,         { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ADDRESS_PROTECTED },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* No more free local ports or insufficient entries in the routing
   * cache. For PF_INET see the net.ipv4.ip_local_port_range sysctl
   * in ip(7) on how to increase the number of local ports. */
  { EAGAIN,        { { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 },
                     { NULL,               -1 } } },

  /* No one listening on the remote address. */
  { ECONNREFUSED,  { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_CONNECTION_REFUSED },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Network is unreachable. */
  { ENETUNREACH,   { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_NETWORK_UNREACHABLE },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  { 0,             { { NULL,               -1 },
                     { NULL,               -1 },
                     { NULL,               -1 } } }
};

static const gint tcp_connect_fatal_errnos [] =
{
  EADDRINUSE,
  EAFNOSUPPORT,
  EALREADY,
  EBADF,
  EFAULT,
  EISCONN,
  ENOTSOCK,
  0
};

static const gint tcp_shutdown_fatal_errnos [] =
{
  EBADF,
  ENOTSOCK,
  0
};

static const gint tcp_recv_fatal_errnos [] =
{
  EBADF,
  EINVAL,
  ENOTCONN,
  EOPNOTSUPP,
  0
};

static const gint tcp_socket_fatal_errnos [] =
{
  EAFNOSUPPORT,
  EINVAL,
  EPROTONOSUPPORT,
  0
};

static const gint file_seek_fatal_errnos [] =
{
  EBADF,
  ESPIPE,
  0
};

static const gint setsockopt_fatal_errnos [] =
{
  EBADF,
  EFAULT,
  EINVAL,
  ENOPROTOOPT,
  ENOTSOCK,
  0
};

static const ErrnoMap socket_write_errno_map [] =
{
  /* (For Unix domain sockets, which are identified by pathname) Write permission
   * is denied on the destination socket file, or search permission is
   * denied for one of the directories the path prefix. */
  { EACCES,        { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_ADDRESS_PROTECTED },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Connection reset. */
  { ECONNRESET,    { { FLOW_SOCKET_DOMAIN, FLOW_SOCKET_CONNECTION_RESET },
                     { FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR },
                     { NULL,               -1 } } },

  /* Input/output error. */
  { EIO,           { { NULL,               -1 },
                     { NULL,               -1 },
                     { NULL,               -1 } } },

  /* Out of memory. */
  { ENOMEM,        { { FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR },
                     { NULL,               -1 },
                     { NULL,               -1 } } },

  { 0,             { { NULL,               -1 },
                     { NULL,               -1 },
                     { NULL,               -1 } } }
};

static const gint socket_write_fatal_errnos [] =
{
  EBADF,
  EDESTADDRREQ,
  EFAULT,
  EINVAL,
  EISCONN,
  ENOTCONN,
  ENOTSOCK,
  EOPNOTSUPP,
  EPIPE,
  0
};

static const gint socket_read_fatal_errnos [] =
{
  EBADF,
  EFAULT,
  EINVAL,
  0
};

/* -------------- *
 * Error Handling *
 * -------------- */

static void
real_assert_non_fatal_errno (gint errnum, const gint *fatal_errnos, const gchar *file_str, gint line_num)
{
  gint i;

  for (i = 0; fatal_errnos [i]; i++)
  {
    if (fatal_errnos [i] == errnum)
    {
      gchar *message;
      gchar *error_str;

      error_str = flow_strerror (errnum);
      message = g_strdup_printf ("%s:%d ** Caught unexpected I/O error: '%s' (%d). "
                                 "This is a programming error - please report it!",
                                 file_str, line_num, error_str, errnum);
      g_error (message);
      g_free (message);
      g_free (error_str);
    }
  }
}

static FlowDetailedEvent *
generate_errno_event (gint errnum, const ErrnoMap *errno_map)
{
  FlowDetailedEvent *detailed_event;
  gchar             *error_str;
  gint               i;

  error_str = flow_strerror (errnum);
  detailed_event = flow_detailed_event_new_literal (error_str);
  g_free (error_str);

  if (!errno_map)
    return detailed_event;

  /* errno(3): Valid error numbers are all non-zero; errno is never set to zero by any
   * library function. */
  for (i = 0; errno_map [i].errnum != 0; i++)
  {
    if (errnum == errno_map [i].errnum)
    {
      gint j;

      for (j = 0; j < 3; j++)
      {
        if (errno_map [i].errcodes [j].domain != NULL)
          flow_detailed_event_add_code (detailed_event,
                                        errno_map [i].errcodes [j].domain,
                                        errno_map [i].errcodes [j].code);
      }

      break;
    }
  }

  return detailed_event;
}

/* ----------------- *
 * Utility Functions *
 * ----------------- */

static void
generate_simple_event (FlowShunt *shunt, const gchar *domain, gint code)
{
  FlowPacket *packet;

  packet = flow_create_simple_event_packet (domain, code);
  flow_packet_queue_push_packet (shunt->read_queue, packet);
}

static void
report_position (FlowShunt *shunt, gint64 position)
{
  FlowPacket *packet;

  /* Report new position */

  packet = flow_packet_new_take_object (flow_position_new (FLOW_OFFSET_ANCHOR_BEGIN, position), 0);
  flow_packet_queue_push_packet (shunt->read_queue, packet);
  flow_shunt_read_state_changed (shunt);

  shunt->offset_changed = FALSE;
}

static gint
anchor_to_whence (FlowOffsetAnchor anchor)
{
  switch (anchor)
  {
    case FLOW_OFFSET_ANCHOR_CURRENT:
      return SEEK_CUR;
    case FLOW_OFFSET_ANCHOR_BEGIN:
      return SEEK_SET;
    case FLOW_OFFSET_ANCHOR_END:
      return SEEK_END;
  }

  g_assert_not_reached ();
  return -1;
}

static void
close_read_fd (FlowShunt *shunt)
{
  if (!shunt->can_read)
    return;

  switch (shunt->shunt_type)
  {
    case SHUNT_TYPE_TCP_LISTENER:
    case SHUNT_TYPE_TCP:
    case SHUNT_TYPE_UDP:
      {
        SocketShunt *socket_shunt = (SocketShunt *) shunt;

        if (!shunt->can_write)
        {
          flow_close_socket_fd (socket_shunt->fd);
          socket_shunt->fd = -1;
        }
        else if (shutdown (socket_shunt->fd, SHUT_RD) < 0)
        {
          gint saved_errno = errno;

          /* We may get ENOTCONN here for sockets that got connection refused, so
           * don't include it in the fatality list. */
          assert_non_fatal_errno (saved_errno, tcp_shutdown_fatal_errnos);
        }
      }
      break;

    case SHUNT_TYPE_PIPE:
      {
        PipeShunt *pipe_shunt = (PipeShunt *) shunt;

        flow_close_socket_fd (pipe_shunt->read_fd);
        pipe_shunt->read_fd = -1;
      }
      break;

    case SHUNT_TYPE_FILE:
      {
        FileShunt *file_shunt = (FileShunt *) shunt;

        if (!shunt->can_write)
        {
          flow_close_file_fd (file_shunt->fd);
          file_shunt->fd = -1;
        }
      }
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  shunt->can_read = FALSE;

  if (!shunt->was_destroyed)
    flow_shunt_read_state_changed (shunt);
}

static void
close_write_fd (FlowShunt *shunt)
{
  if (!shunt->can_write)
    return;

  switch (shunt->shunt_type)
  {
    case SHUNT_TYPE_TCP_LISTENER:
    case SHUNT_TYPE_TCP:
    case SHUNT_TYPE_UDP:
      {
        SocketShunt *socket_shunt = (SocketShunt *) shunt;

        if (!shunt->can_read)
        {
          flow_close_socket_fd (socket_shunt->fd);
          socket_shunt->fd = -1;
        }
        else if (shutdown (socket_shunt->fd, SHUT_WR) < 0)
        {
          gint saved_errno = errno;

          assert_non_fatal_errno (saved_errno, tcp_shutdown_fatal_errnos);
        }
      }
      break;

    case SHUNT_TYPE_PIPE:
      {
        PipeShunt *pipe_shunt = (PipeShunt *) shunt;

        flow_close_socket_fd (pipe_shunt->write_fd);
        pipe_shunt->write_fd = -1;
      }
      break;

    case SHUNT_TYPE_FILE:
      {
        FileShunt *file_shunt = (FileShunt *) shunt;

        if (!shunt->can_read)
        {
          flow_close_file_fd (file_shunt->fd);
          file_shunt->fd = -1;
        }
      }
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  shunt->can_write = FALSE;

  if (!shunt->was_destroyed)
    flow_shunt_write_state_changed (shunt);
}

static void
ip_socket_set_quality (gint fd, FlowQuality quality)
{
  gint tos;

  switch (quality)
  {
    case FLOW_QUALITY_HIGH_RELIABILITY:
      tos = IPTOS_RELIABILITY;
      break;

    case FLOW_QUALITY_HIGH_THROUGHPUT:
      tos = IPTOS_THROUGHPUT;
      break;

    case FLOW_QUALITY_LOW_LATENCY:
      tos = IPTOS_LOWDELAY;
      break;

    case FLOW_QUALITY_LOW_COST:
      tos = IPTOS_MINCOST;
      break;

    case FLOW_QUALITY_UNSPECIFIED:
      /* Don't touch the default value */
      return;
  }

  if (setsockopt (fd, IPPROTO_IP, IP_TOS, &tos, sizeof (tos)) < 0)
    assert_non_fatal_errno (errno, setsockopt_fatal_errnos);
}

static void
tcp_socket_set_quality (gint fd, FlowQuality quality)
{
  ip_socket_set_quality (fd, quality);

  if (quality == FLOW_QUALITY_LOW_LATENCY)
  {
    gint on = 1;

    if (setsockopt (fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof (on)) < 0)
      assert_non_fatal_errno (errno, setsockopt_fatal_errnos);
  }
}

/* -------------------- *
 * Global init/shutdown *
 * -------------------- */

static void
flow_shunt_impl_init (void)
{
  if (!g_thread_supported ())
    g_thread_init (NULL);

  /* For all types that are used in multiple threads, make sure they're
   * initialized in the main thread first. Otherwise, we get a race condition
   * which may result in two threads trying to initialize it simultaneously. */

  g_type_class_ref (FLOW_TYPE_DETAILED_EVENT);
  g_type_class_ref (FLOW_TYPE_ANONYMOUS_EVENT);
  g_type_class_ref (FLOW_TYPE_IP_SERVICE);
  g_type_class_ref (FLOW_TYPE_POSITION);

  active_socket_shunts = g_ptr_array_new ();
  socket_buffer = g_malloc (IO_BUFFER_DEFAULT_SIZE);

  flow_wakeup_pipe_init (&wakeup_pipe);

  watch_thread = g_thread_create_full ((GThreadFunc) socket_shunt_main, NULL,
                                       THREAD_STACK_SIZE, FALSE, FALSE,
                                       G_THREAD_PRIORITY_NORMAL, NULL);
}

static void
flow_shunt_impl_finalize (void)
{
  flow_wakeup_pipe_destroy (&wakeup_pipe);

  g_ptr_array_free (active_socket_shunts, TRUE);
  active_socket_shunts = NULL;

  g_free (socket_buffer);
  socket_buffer = NULL;

  /* FIXME: Do something about ShuntSources, but in flow-shunt.c */

#if 0
  if (dispatch_source_id)
  {
    g_source_remove (dispatch_source_id);
    dispatch_source_id = 0;
  }
#endif
}

/* ----------- *
 * Global Lock *
 * ----------- */

static inline void
flow_shunt_impl_lock (void)
{
  g_static_mutex_lock (&global_mutex);
}

static inline void
flow_shunt_impl_unlock (void)
{
  g_static_mutex_unlock (&global_mutex);
}

/* ----------- *
 * Destruction *
 * ----------- */

/* Invoked from flow_shunt_destroy () */
static void
flow_shunt_impl_destroy_shunt (FlowShunt *shunt)
{
  if (shunt->shunt_type == SHUNT_TYPE_FILE)
  {
    FileShunt *file_shunt = (FileShunt *) shunt;

    g_cond_signal (file_shunt->cond);
  }
  else
  {
    g_ptr_array_remove_fast (active_socket_shunts, shunt);
  }
}

/* Invoked from generic dispatch_for_shunt () or flow_shunt_destroy () */
/* Assumes that caller is holding the impl lock */
static void
flow_shunt_impl_finalize_shunt (FlowShunt *shunt)
{
  /* File shunts are finalized in their own thread */
  if (shunt->shunt_type == SHUNT_TYPE_FILE)
    return;

  close_read_fd (shunt);
  close_write_fd (shunt);

  flow_shunt_finalize_common (shunt);

  switch (shunt->shunt_type)
  {
    case SHUNT_TYPE_PIPE:
      {
        PipeShunt *pipe_shunt = (PipeShunt *) shunt;
        g_slice_free (PipeShunt, pipe_shunt);
      }
      break;

    case SHUNT_TYPE_TCP:
      {
        TcpShunt *tcp_shunt = (TcpShunt *) shunt;
        g_slice_free (TcpShunt, tcp_shunt);
      }
      break;

    case SHUNT_TYPE_TCP_LISTENER:
      {
        TcpListenerShunt *tcp_listener_shunt = (TcpListenerShunt *) shunt;
        g_slice_free (TcpListenerShunt, tcp_listener_shunt);
      }
      break;

    case SHUNT_TYPE_UDP:
      {
        UdpShunt *udp_shunt = (UdpShunt *) shunt;
        g_slice_free (UdpShunt, udp_shunt);
      }
      break;

    default:
      g_assert_not_reached ();
      break;
  }
}

/* -------------------- *
 * I/O watch management *
 * -------------------- */

/* Invoked from flow_shunt_read_state_changed () */
static void
flow_shunt_impl_need_reads (FlowShunt *shunt)
{
  switch (shunt->shunt_type)
  {
    case SHUNT_TYPE_UDP:
    case SHUNT_TYPE_TCP:
    case SHUNT_TYPE_TCP_LISTENER:
    case SHUNT_TYPE_PIPE:
      /* Only add once to active_socket_shunts array. Therefore, check that
       * we didn't already add it for the inverse operation. */
      if (!shunt->doing_writes)
      {
        g_ptr_array_add (active_socket_shunts, shunt);
      }
      flow_wakeup_pipe_wakeup (&wakeup_pipe);
      shunt->doing_reads = TRUE;
      break;

    case SHUNT_TYPE_FILE:
      {
        FileShunt *file_shunt = (FileShunt *) shunt;

        if (file_shunt->read_bytes_remaining)
        {
          shunt->doing_reads = TRUE;
          g_cond_signal (file_shunt->cond);
        }
        else
        {
          shunt->need_reads = FALSE;
        }
      }
      break;
  }
}

/* Invoked from flow_shunt_write_state_changed () */
static void
flow_shunt_impl_need_writes (FlowShunt *shunt)
{
  switch (shunt->shunt_type)
  {
    case SHUNT_TYPE_UDP:
    case SHUNT_TYPE_TCP:
    case SHUNT_TYPE_TCP_LISTENER:
    case SHUNT_TYPE_PIPE:
      /* Only add once to active_socket_shunts array. Therefore, check that
       * we didn't already add it for the inverse operation. */
      if (!shunt->doing_reads)
      {
        g_ptr_array_add (active_socket_shunts, shunt);
      }
      flow_wakeup_pipe_wakeup (&wakeup_pipe);
      shunt->doing_writes = TRUE;
      break;

    case SHUNT_TYPE_FILE:
      {
        FileShunt *file_shunt = (FileShunt *) shunt;

        if (file_shunt->read_bytes_remaining)
        {
          shunt->need_writes = FALSE;
        }
        else
        { 
          shunt->doing_writes = TRUE;
          g_cond_signal (file_shunt->cond);
        }
      }
      break;
  }
}

/* ----------------------------- *
 * Socket and Pipe Low-level I/O *
 * ----------------------------- */

static void
socket_buffer_check (FlowShunt *shunt)
{
  if (shunt->io_buffer_desired_size > shunt->io_buffer_size)
  {
    shunt->io_buffer_size = shunt->io_buffer_desired_size;
    g_free (socket_buffer);
    socket_buffer = g_malloc (shunt->io_buffer_size);
  }
}

static void
tcp_listener_shunt_read (FlowShunt *shunt)
{
  FlowSockaddr      sa;
  guint             sa_len             = sizeof (FlowSockaddr);
  gint              new_fd;
  SocketShunt      *socket_shunt       = (SocketShunt *) shunt;
  TcpListenerShunt *tcp_listener_shunt = (TcpListenerShunt *) shunt;

  new_fd = accept (socket_shunt->fd, (struct sockaddr *) &sa, &sa_len);

  if G_UNLIKELY (new_fd < 0)
  {
    FlowDetailedEvent *detailed_event;
    gint               saved_errno;

    /* Error: Failed to accept incoming connection - either it was aborted on the
     * remote end, or we have a local resource exhaustion problem. Aborted connection
     * attempts are ignored, but other problems are passed on. */

    saved_errno = errno;

    assert_non_fatal_errno (saved_errno, tcp_accept_fatal_errnos);

    /* We silently handle these errors:
     *
     * ECONNABORTED: Remote end aborted connection attempt.
     * EPERM: Firewall rules forbid connection (Linux only?). */

    if (saved_errno != ECONNABORTED && saved_errno != EPERM)
    {
      detailed_event = generate_errno_event (saved_errno, tcp_accept_errno_map);
      flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
      flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
    }
  }
  else if (!flow_socket_set_nonblock (new_fd, TRUE))
  {
    /* Error: Could not set socket nonblock. If this happens, our implementation will
     * not work as intended, so abort execution. */

    g_assert_not_reached ();
  }
  else
  {
    FlowIPService      *new_ip_service;
    FlowShunt          *new_shunt;
    TcpShunt           *new_tcp_shunt;
    FlowAnonymousEvent *anonymous_event;
    gint                on = 1;

    /* Success - set up new connected shunt */

    new_tcp_shunt = g_slice_new0 (TcpShunt);
    new_shunt = (FlowShunt *) new_tcp_shunt;
    flow_shunt_init_common (new_shunt, shunt->shunt_source);
    new_shunt->shunt_type = SHUNT_TYPE_TCP;

    tcp_socket_set_quality (new_fd, tcp_listener_shunt->quality);

    if (setsockopt (new_fd, SOL_SOCKET, SO_OOBINLINE, &on, sizeof (on)) < 0)
      assert_non_fatal_errno (errno, setsockopt_fatal_errnos);

#ifdef USE_SO_NOSIGPIPE
    /* For MacOS X, BSD */
    setsockopt (new_fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof (on));
#endif

    new_tcp_shunt->socket_shunt.fd = new_fd;

    new_shunt->can_read  = TRUE;
    new_shunt->can_write = TRUE;

    /* Queue remote IP service as first packet on shunt */

    new_ip_service = flow_ip_service_new ();
    flow_ip_service_set_sockaddr (new_ip_service, &sa);
    flow_packet_queue_push_packet (new_shunt->read_queue, flow_packet_new_take_object (new_ip_service, 0));

    flow_shunt_read_state_changed (new_shunt);
    flow_shunt_write_state_changed (new_shunt);

    /* Queue shunt on listener */

    anonymous_event = flow_anonymous_event_new ();
    flow_anonymous_event_set_data (anonymous_event, new_shunt);
    flow_anonymous_event_set_destroy_notify (anonymous_event, (GDestroyNotify) flow_shunt_destroy);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (anonymous_event, 0));
  }

  flow_shunt_read_state_changed (shunt);
}

static void
socket_shunt_read (FlowShunt *shunt)
{
  FlowSockaddr sa;
  guint        sa_len = sizeof (FlowSockaddr);
  gint         result;
  gint         saved_errno;

#if 1

  /* socket_shunt_write() does proper error handling for connection-established,
   * so wait until that has run. */

  if G_UNLIKELY (!shunt->dispatched_begin)
    return;

#else

  if G_UNLIKELY (!shunt->dispatched_begin)
  {
    /* Begin stream */

    shunt->dispatched_begin = TRUE;
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_BEGIN);
  }

#endif

  socket_buffer_check (shunt);
  errno = 0;

  /* TCP listeners are sufficiently different as to warrant a separate function */

  if (shunt->shunt_type == SHUNT_TYPE_TCP_LISTENER)
  {
    tcp_listener_shunt_read (shunt);
    return;
  }

  switch (shunt->shunt_type)
  {
    case SHUNT_TYPE_TCP:
      {
        SocketShunt *socket_shunt = (SocketShunt *) shunt;

        result = recv (socket_shunt->fd, socket_buffer, shunt->io_buffer_size, 0);
      }
      break;

    case SHUNT_TYPE_UDP:
      {
        SocketShunt *socket_shunt = (SocketShunt *) shunt;

        result = recvfrom (socket_shunt->fd, socket_buffer, shunt->io_buffer_size, 0, (struct sockaddr *) &sa, &sa_len);
      }
      break;

    case SHUNT_TYPE_PIPE:
      {
        PipeShunt *pipe_shunt = (PipeShunt *) shunt;

        result = read (pipe_shunt->read_fd, socket_buffer, shunt->io_buffer_size);
      }
      break;

    default:
      result = -1;
      g_assert_not_reached ();
      break;
  }

  saved_errno = errno;

  if G_LIKELY (result > 0)
  {
    FlowPacket *packet;

    /* Data */

    if (shunt->shunt_type == SHUNT_TYPE_UDP)
    {
      UdpShunt *udp_shunt = (UdpShunt *) shunt;

      /* For UDP, dispatch source address first, but only if it changed */

      if (memcmp (&udp_shunt->remote_src_sa, &sa, sa_len))
      {
        FlowIPService *ip_service;

        memcpy (&udp_shunt->remote_src_sa, &sa, sa_len);

        ip_service = flow_ip_service_new ();
        flow_ip_service_set_sockaddr (ip_service, &udp_shunt->remote_src_sa);

        flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (ip_service, 0));
      }
    }

    packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, socket_buffer, result);
    flow_packet_queue_push_packet (shunt->read_queue, packet);
  }
  else if (result == 0 || saved_errno != EINTR)
  {
    /* End stream */

    assert_non_fatal_errno (saved_errno, tcp_recv_fatal_errnos);

    g_assert (shunt->dispatched_end == FALSE);

    shunt->dispatched_end = TRUE;
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_END);
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);

    close_read_fd (shunt);
  }

  flow_shunt_read_state_changed (shunt);
}

static FlowDetailedEvent *
get_connect_error_from_tcp_shunt (FlowShunt *shunt)
{
  SocketShunt       *socket_shunt   = (SocketShunt *) shunt;
  FlowDetailedEvent *detailed_event = NULL;
  gint               err            = 0;
  guint              err_len        = sizeof (err);

  g_assert (shunt->shunt_type == SHUNT_TYPE_TCP);

  /* Was the connection attempt successful? */

#ifdef G_PLATFORM_WIN32
  getsockopt (socket_shunt->fd, SOL_SOCKET, SO_ERROR, (gchar *) &err, &err_len);
#else
  getsockopt (socket_shunt->fd, SOL_SOCKET, SO_ERROR, &err, &err_len);
#endif

  if (err != 0)
  {
    assert_non_fatal_errno (err, tcp_connect_fatal_errnos);
    detailed_event = generate_errno_event (err, tcp_connect_errno_map);
  }

  return detailed_event;
}

static void
socket_shunt_write (FlowShunt *shunt)
{
  if G_UNLIKELY (!shunt->dispatched_begin)
  {
    FlowDetailedEvent *detailed_event = NULL;

    if (shunt->shunt_type == SHUNT_TYPE_TCP)
      detailed_event = get_connect_error_from_tcp_shunt (shunt);

    if (!detailed_event)
    {
      /* Begin stream */

      shunt->dispatched_begin = TRUE;
      generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
      generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_BEGIN);

      flow_shunt_read_state_changed (shunt);
    }
    else
    {
      /* Failed connect */

      shunt->dispatched_begin = TRUE;
      shunt->dispatched_end   = TRUE;

      flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
      flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));

      close_read_fd (shunt);
      close_write_fd (shunt);

      flow_shunt_read_state_changed (shunt);
      flow_shunt_write_state_changed (shunt);

      return;
    }
  }

  while (!shunt->was_destroyed)
  {
    FlowPacket       *packet;
    gint              packet_offset;
    FlowPacketFormat  packet_format;

    if (!flow_packet_queue_peek_packet (shunt->write_queue, &packet, &packet_offset))
    {
      /* Nothing to do */
      break;
    }

    packet_format = flow_packet_get_format (packet);

    if G_LIKELY (packet_format == FLOW_PACKET_FORMAT_BUFFER)
    {
      guint8        *buffer;
      gint           buffer_len;
      gint           result;
      gint           saved_errno;

      buffer = (guint8 *) flow_packet_get_data (packet) + packet_offset;
      buffer_len = flow_packet_get_size (packet) - packet_offset;

      errno = 0;

      if (shunt->shunt_type == SHUNT_TYPE_TCP)
      {
        SocketShunt *socket_shunt = (SocketShunt *) shunt;

        result = send (socket_shunt->fd, buffer, buffer_len, MSG_NOSIGNAL);
      }
      else if (shunt->shunt_type == SHUNT_TYPE_UDP)
      {
        SocketShunt *socket_shunt = (SocketShunt *) shunt;
        UdpShunt    *udp_shunt    = (UdpShunt *) shunt;

        result = sendto (socket_shunt->fd, buffer, buffer_len, MSG_NOSIGNAL,
                         (struct sockaddr *) &udp_shunt->remote_dest_sa,
                         flow_sockaddr_get_len (&udp_shunt->remote_dest_sa));
      }
      else if (shunt->shunt_type == SHUNT_TYPE_PIPE)
      {
        PipeShunt *pipe_shunt = (PipeShunt *) shunt;

        result = write (pipe_shunt->write_fd, buffer, buffer_len);
      }
      else
      {
        /* You can't write data to e.g. a TcpListener */

        result = -1;
        g_assert_not_reached ();
      }

      saved_errno = errno;

      if G_UNLIKELY (result < 0)
      {
        FlowDetailedEvent *detailed_event;

        if (saved_errno == EAGAIN || saved_errno == EINTR || saved_errno == ENOBUFS)
          break;

        if (saved_errno == EMSGSIZE)
        {
          /* UDP packet too big? */

          detailed_event = generate_errno_event (saved_errno, NULL);
          flow_detailed_event_add_code (detailed_event, FLOW_SOCKET_DOMAIN, FLOW_SOCKET_OVERSIZED_PACKET);
          flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
          flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));

          flow_packet_queue_drop_packet (shunt->write_queue);
          continue;
        }

        /* Broken pipe */

        assert_non_fatal_errno (saved_errno, socket_write_fatal_errnos);

        detailed_event = generate_errno_event (saved_errno, socket_write_errno_map);
        flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END_CONVERSE);
        flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));

        close_write_fd (shunt);
        flow_shunt_read_state_changed (shunt);
        break;
      }
      else if (result < buffer_len)
      {
        /* Partial write */
        flow_packet_queue_pop_bytes_exact (shunt->write_queue, NULL, result);
        break;
      }
      else
      {
        /* Complete write */
        flow_packet_queue_drop_packet (shunt->write_queue);
      }
    }
    else if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
    {
      GObject *object = flow_packet_get_data (packet);

      if (FLOW_IS_DETAILED_EVENT (object))
      {
        FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) object;

        if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
            flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
        {
          /* User requested end-of-stream */
          close_write_fd (shunt);
        }

        if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END_CONVERSE) ||
            flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
        {
          /* User wants to stop reading */
          close_read_fd (shunt);

          if (!shunt->dispatched_end)
          {
            generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_END);
            generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
            shunt->dispatched_end = TRUE;

            flow_shunt_read_state_changed (shunt);
          }
        }
      }
      else if (shunt->shunt_type == SHUNT_TYPE_UDP)
      {
        SocketShunt *socket_shunt = (SocketShunt *) shunt;
        UdpShunt    *udp_shunt    = (UdpShunt *) shunt;
        gboolean     reconnect;

        if (FLOW_IS_IP_SERVICE (object))
        {
          FlowIPService *ip_service = (FlowIPService *) object;

          flow_ip_service_get_sockaddr (ip_service, &udp_shunt->remote_dest_sa, 0);
          reconnect = TRUE;
        }
        else if (FLOW_IS_IP_ADDR (object))
        {
          FlowIPAddr *ip_addr = (FlowIPAddr *) object;

          flow_ip_addr_get_sockaddr (ip_addr, &udp_shunt->remote_dest_sa,
                                     flow_sockaddr_get_port (&udp_shunt->remote_dest_sa));
          reconnect = TRUE;
        }
        else
        {
          reconnect = FALSE;
        }

        if (reconnect)
        {
          if (connect (socket_shunt->fd, (struct sockaddr *) &udp_shunt->remote_dest_sa,
                       flow_sockaddr_get_len (&udp_shunt->remote_dest_sa)) != 0)
          {
            FlowDetailedEvent *detailed_event;
            gint               saved_errno = errno;

            /* This is actually a UDP connect, not TCP, but the difference is minimal -
             * presumably, we can use the same error maps. */

            assert_non_fatal_errno (saved_errno, tcp_connect_fatal_errnos);

            detailed_event = generate_errno_event (saved_errno, tcp_connect_errno_map);
            flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
            flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
          }
        }
      }

      flow_packet_queue_drop_packet (shunt->write_queue);
    }
    else
    {
      /* Unknown packet format. Just drop it. */
      flow_packet_queue_drop_packet (shunt->write_queue);
    }
  }

  flow_shunt_write_state_changed (shunt);
}

static void
socket_shunt_exception (FlowShunt *shunt)
{
  if (!shunt->dispatched_begin)
  {
    FlowDetailedEvent *detailed_event = NULL;

    /* Failed connect */

    shunt->dispatched_begin = TRUE;
    shunt->dispatched_end   = TRUE;

    if (shunt->shunt_type == SHUNT_TYPE_TCP)
      detailed_event = get_connect_error_from_tcp_shunt (shunt);

    if (detailed_event)
    {
      flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
      flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
    }
    else
    {
      generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    }
  }
  else
  {
    /* Other error */

    shunt->dispatched_end = TRUE;
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_END_CONVERSE);
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_END);
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
  }

  close_read_fd (shunt);
  close_write_fd (shunt);

  flow_shunt_read_state_changed (shunt);
  flow_shunt_write_state_changed (shunt);
}

static gpointer
socket_shunt_main (void)
{
  flow_shunt_impl_lock ();

  /* Implementation finalized? */
  if (!flow_wakeup_pipe_is_valid (&wakeup_pipe))
    goto out;

  for (;;)
  {
    fd_set  read_fds;
    fd_set  write_fds;
    fd_set  exception_fds;
    gint    fd_max = flow_wakeup_pipe_get_watch_fd (&wakeup_pipe);
    gint    result;
    guint   i;

    /* Clear sets */

    FD_ZERO (&read_fds);
    FD_ZERO (&write_fds);
    FD_ZERO (&exception_fds);

    /* Add wakeup pipe to set */

    FD_SET (flow_wakeup_pipe_get_watch_fd (&wakeup_pipe), &read_fds);

    /* Add other fds to sets, clean out array */

    for (i = 0; i < active_socket_shunts->len; )
    {
      FlowShunt *shunt = g_ptr_array_index (active_socket_shunts, i);
      gint       read_fd;
      gint       write_fd;

      if (shunt->shunt_type == SHUNT_TYPE_PIPE)
      {
        PipeShunt *pipe_shunt = (PipeShunt *) shunt;

        read_fd  = pipe_shunt->read_fd;
        write_fd = pipe_shunt->write_fd;
      }
      else
      {
        SocketShunt *socket_shunt = (SocketShunt *) shunt;

        read_fd  = socket_shunt->fd;
        write_fd = socket_shunt->fd;
      }

      if (shunt->need_reads)
      {
        g_assert (read_fd >= 0);

        FD_SET (read_fd, &read_fds);
        FD_SET (read_fd, &exception_fds);
        fd_max = MAX (fd_max, read_fd);
      }
      else
      {
        shunt->doing_reads = FALSE;
      }

      if (shunt->need_writes)
      {
        g_assert (write_fd >= 0);

        FD_SET (write_fd, &write_fds);
        FD_SET (write_fd, &exception_fds);
        fd_max = MAX (fd_max, write_fd);
      }
      else
      {
        shunt->doing_writes = FALSE;
      }

      if (!shunt->doing_reads && !shunt->doing_writes)
      {
        g_ptr_array_remove_index_fast (active_socket_shunts, i);
        continue;
      }

      i++;
    }

    flow_shunt_impl_unlock ();

    /* --- UNLOCKED CODE BEGINS --- */

    result = select (fd_max + 1, &read_fds, &write_fds, &exception_fds, NULL);

    /* --- UNLOCKED CODE ENDS --- */

    flow_shunt_impl_lock ();

    /* Implementation finalized? */
    if (!flow_wakeup_pipe_is_valid (&wakeup_pipe))
      break;

    if (result < 1)
    {
      /* select () returned, but no FDs are ready. This can happen if we're
       * interrupted by a signal. Just restart the select (). */
      continue;
    }

    /* Clear wakeup events */

    if (FD_ISSET (flow_wakeup_pipe_get_watch_fd (&wakeup_pipe), &read_fds))
      flow_wakeup_pipe_handle_wakeup (&wakeup_pipe);

    /* Process events */

    for (i = 0; i < active_socket_shunts->len; i++)
    {
      FlowShunt *shunt = g_ptr_array_index (active_socket_shunts, i);
      gint       read_fd;
      gint       write_fd;

      if (shunt->shunt_type == SHUNT_TYPE_PIPE)
      {
        PipeShunt *pipe_shunt = (PipeShunt *) shunt;

        read_fd  = pipe_shunt->read_fd;
        write_fd = pipe_shunt->write_fd;
      }
      else
      {
        SocketShunt *socket_shunt = (SocketShunt *) shunt;

        read_fd  = socket_shunt->fd;
        write_fd = socket_shunt->fd;
      }

      if (shunt->doing_reads)
      {
        if (FD_ISSET (read_fd, &read_fds))
          socket_shunt_read (shunt);

        if (FD_ISSET (read_fd, &exception_fds))
          socket_shunt_exception (shunt);
      }

      if (shunt->doing_writes)
      {
        if (FD_ISSET (write_fd, &write_fds))
          socket_shunt_write (shunt);

        if (FD_ISSET (write_fd, &exception_fds))
          socket_shunt_exception (shunt);
      }
    }
  }

out:
  flow_shunt_impl_unlock ();
  return NULL;
}

/* ------------------ *
 * File Low-level I/O *
 * ------------------ */

static void
file_shunt_read (FlowShunt *shunt)
{
  FileShunt    *file_shunt = (FileShunt *) shunt;
  gint64        max_read;
  gint          result;
  gint          saved_errno;
  ShuntType     shunt_type;
  FlowPacket   *packet;
  gpointer      packet_data;
  gint          fd;

  socket_buffer_check (shunt);

  max_read = MIN (file_shunt->read_bytes_remaining, shunt->io_buffer_size);
  if (max_read < 1)
  {
    flow_shunt_read_state_changed (shunt);
    return;
  }

  shunt_type = shunt->shunt_type;
  fd         = file_shunt->fd;

  flow_shunt_impl_unlock ();

  /* --- UNLOCKED CODE BEGINS --- */

  packet = flow_packet_alloc_for_data (max_read, &packet_data);

  result = read (fd, packet_data, max_read);
  saved_errno = errno;

  /* --- UNLOCKED CODE ENDS --- */

  flow_shunt_impl_lock ();

  if G_LIKELY (result > 0)
  {
    /* Data */

    if (result < max_read)
    {
      FlowPacket *new_packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, packet_data, result);
      flow_packet_unref (packet);
      packet = new_packet;
    }

    flow_packet_queue_push_packet (shunt->read_queue, packet);

    file_shunt->read_bytes_remaining -= result;
    g_assert (file_shunt->read_bytes_remaining >= 0);

    if (file_shunt->read_bytes_remaining == 0)
    {
      shunt->need_reads  = FALSE;
      shunt->doing_reads = FALSE;
      generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_END);
      flow_shunt_write_state_changed (shunt);  /* Process subsequent writes, if any */
    }
  }
  else if (result == 0)
  {
    /* EOF: End segment */

    flow_packet_unref (packet);

    file_shunt->read_bytes_remaining = 0;
    shunt->need_reads  = FALSE;
    shunt->doing_reads = FALSE;

    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_END);

    /* Send EOF event */
    generate_simple_event (shunt, FLOW_FILE_DOMAIN, FLOW_FILE_REACHED_END);

    flow_shunt_write_state_changed (shunt);  /* Process subsequent writes, if any */
  }
  else if (saved_errno != EINTR && saved_errno != EAGAIN)
  {
    FlowDetailedEvent *detailed_event;

    /* I/O error */

    assert_non_fatal_errno (saved_errno, file_read_fatal_errnos);

    flow_packet_unref (packet);

    file_shunt->read_bytes_remaining = 0;
    shunt->need_reads  = FALSE;
    shunt->doing_reads = FALSE;

    detailed_event = generate_errno_event (saved_errno, file_read_errno_map);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));

    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_END);
    flow_shunt_write_state_changed (shunt);  /* Process subsequent writes, if any */
  }

  flow_shunt_read_state_changed (shunt);
}

static void
file_shunt_handle_stream_end (FlowShunt *shunt)
{
  /* User requested end-of-stream */

  if (!shunt->dispatched_end)
  {
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
    shunt->dispatched_end = TRUE;
  }

  close_read_fd (shunt);
  close_write_fd (shunt);
}

static void
file_shunt_write (FlowShunt *shunt)
{
  FileShunt *file_shunt = (FileShunt *) shunt;

#if 0
  if G_UNLIKELY (!shunt->dispatched_begin)
  {
    /* Begin stream */

    shunt->dispatched_begin = TRUE;
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
    flow_shunt_read_state_changed (shunt);
  }
#endif

  while (!shunt->was_destroyed)
  {
    FlowPacket       *packet;
    gint              packet_offset;
    FlowPacketFormat  packet_format;

    if (!flow_packet_queue_peek_packet (shunt->write_queue, &packet, &packet_offset))
    {
      /* Nothing to do */
      break;
    }

    packet_format = flow_packet_get_format (packet);

    /* If we've generated a serious error, the stream must be restarted by the user. Until
     * we receive a restart event or a request to close the stream, drop all packets. */

    if G_UNLIKELY (shunt->wait_for_restart)
    {
      if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
      {
        FlowDetailedEvent *detailed_event = flow_packet_get_data (packet);

        if (FLOW_IS_DETAILED_EVENT (detailed_event))
        {
          if (flow_detailed_event_matches (detailed_event, FLOW_FILE_DOMAIN, FLOW_FILE_RESTART))
          {
            shunt->wait_for_restart = FALSE;
          }
          else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
                   flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
          {
            file_shunt_handle_stream_end (shunt);
          }
        }
      }

      flow_packet_queue_drop_packet (shunt->write_queue);
      continue;
    }

    /* Handle packet */

    if G_LIKELY (packet_format == FLOW_PACKET_FORMAT_BUFFER)
    {
      guint8        *buffer;
      gint           buffer_len;
      gint           result;
      gint           saved_errno;
      gint           fd;

      buffer = (guint8 *) flow_packet_get_data (packet) + packet_offset;
      buffer_len = flow_packet_get_size (packet) - packet_offset;

      fd = file_shunt->fd;

      flow_shunt_impl_unlock ();

      /* --- UNLOCKED CODE BEGINS --- */

      result = write (fd, buffer, buffer_len);
      saved_errno = errno;

      /* --- UNLOCKED CODE ENDS --- */

      flow_shunt_impl_lock ();

      if G_UNLIKELY (result < 0)
      {
        FlowDetailedEvent *detailed_event;

        /* Programmer error? */

        assert_non_fatal_errno (saved_errno, file_write_fatal_errnos);

        /* Just an interrupt? */

        if (saved_errno == EAGAIN || saved_errno == EINTR)
          break;

        /* Dispatch error and wait for it to be acknowledged */

        shunt->wait_for_restart = TRUE;

        detailed_event = generate_errno_event (saved_errno, file_write_errno_map);
        flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
        flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));

        flow_shunt_read_state_changed (shunt);
        break;
      }
      else if (result < buffer_len)
      {
        /* Partial write */
        flow_packet_queue_pop_bytes_exact (shunt->write_queue, NULL, result);
        shunt->offset_changed = TRUE;
        break;
      }
      else
      {
        /* Complete write */
        flow_packet_queue_drop_packet (shunt->write_queue);
        shunt->offset_changed = TRUE;
      }
    }
    else if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
    {
      gpointer object = flow_packet_get_data (packet);

      if (FLOW_IS_POSITION (object))
      {
        FlowOffsetAnchor anchor;
        gint64           offset;
        off_t            result;
        gint             saved_errno;

        anchor = flow_position_get_anchor (object);
        offset = flow_position_get_offset (object);

        result = lseek (file_shunt->fd, offset, anchor_to_whence (anchor));
        saved_errno = errno;

        if (result >= 0)
        {
          report_position (shunt, result);
        }
        else
        {
          /* Seek failed */

          /* Programmer error? */
          assert_non_fatal_errno (saved_errno, file_seek_fatal_errnos);

          /* The Linux man page doesn't mention any would-be application errors, so
           * it's our fault somehow. */
          g_assert_not_reached ();
        }
      }
      else if (FLOW_IS_SEGMENT_REQUEST (object))
      {
        g_assert (file_shunt->read_bytes_remaining == 0);
        file_shunt->read_bytes_remaining = flow_segment_request_get_length (object);

        if (file_shunt->read_bytes_remaining < 0)
          file_shunt->read_bytes_remaining = G_MAXINT64;

        if (shunt->offset_changed)
        {
          off_t result;
          gint  saved_errno;

          result = lseek (file_shunt->fd, 0, SEEK_CUR);
          saved_errno = errno;

          if (result >= 0)
          {
            report_position (shunt, result);
          }
          else
          {
            /* Seek failed */

            /* Programmer error? */
            assert_non_fatal_errno (saved_errno, file_seek_fatal_errnos);

            /* The Linux man page doesn't mention any would-be application errors, so
             * it's our fault somehow. */
            g_assert_not_reached ();
          }
        }

        shunt->need_writes  = FALSE;
        shunt->doing_writes = FALSE;

        generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_BEGIN);
        flow_shunt_read_state_changed (shunt);

        /* The next operation will be a read, so bail out of write loop */
        flow_packet_queue_drop_packet (shunt->write_queue);
        break;
      }
      else if (FLOW_IS_DETAILED_EVENT (object) &&
               (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
                flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED)))
      {
        file_shunt_handle_stream_end (shunt);
      }

      flow_packet_queue_drop_packet (shunt->write_queue);
    }
    else
    {
      /* Unknown packet format. Just drop it. */
      flow_packet_queue_drop_packet (shunt->write_queue);
    }
  }

  flow_shunt_write_state_changed (shunt);
}

static void
file_shunt_open (FileShuntParams *params)
{
  FileShunt      *file_shunt = params->file_shunt;
  FlowShunt      *shunt      = (FlowShunt *) file_shunt;
  gint            flags      = 0;
  gint            saved_errno;
  gint            fd;

  if (params->create)
    flags |= O_CREAT;
  if (params->destructive)
    flags |= O_TRUNC;

  if (params->proc_access & FLOW_READ_ACCESS)
  {
    if (params->proc_access & FLOW_WRITE_ACCESS || params->destructive || params->create)
      flags |= O_RDWR;
    else
      flags |= O_RDONLY;
  }
  else if (params->proc_access & FLOW_WRITE_ACCESS)
  {
    flags |= O_WRONLY;
  }

  if (flags & O_CREAT)
  {
    gint mode = 0;

    if (params->user_access & FLOW_READ_ACCESS)
      mode |= S_IRUSR;
    if (params->user_access & FLOW_WRITE_ACCESS)
      mode |= S_IWUSR;
    if (params->user_access & FLOW_EXECUTE_ACCESS)
      mode |= S_IXUSR;

    if (params->group_access & FLOW_READ_ACCESS)
      mode |= S_IRGRP;
    if (params->group_access & FLOW_WRITE_ACCESS)
      mode |= S_IWGRP;
    if (params->group_access & FLOW_EXECUTE_ACCESS)
      mode |= S_IXGRP;

    if (params->other_access & FLOW_READ_ACCESS)
      mode |= S_IROTH;
    if (params->other_access & FLOW_WRITE_ACCESS)
      mode |= S_IWOTH;
    if (params->other_access & FLOW_EXECUTE_ACCESS)
      mode |= S_IXOTH;

    flow_shunt_impl_unlock ();
    fd = open (params->path, flags, mode);
    flow_shunt_impl_lock ();
  }
  else
  {
    flow_shunt_impl_unlock ();
    fd = open (params->path, flags);
    flow_shunt_impl_lock ();
  }

  saved_errno = errno;

  file_shunt->fd = fd;

  if (fd < 0)
  {
    FlowDetailedEvent *detailed_event;

    detailed_event = generate_errno_event (saved_errno, file_open_errno_map);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }
  else
  {
    if (params->proc_access && FLOW_READ_ACCESS)
      shunt->can_read  = TRUE;
    if (params->proc_access && FLOW_WRITE_ACCESS)
      shunt->can_write = TRUE;

    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
  }

  shunt->dispatched_begin = TRUE;

  flow_shunt_read_state_changed (shunt);
  flow_shunt_write_state_changed (shunt);

  g_free (params->path);
  g_slice_free (FileShuntParams, params);
}

static void
file_shunt_finalize (FileShunt *file_shunt)
{
  flow_shunt_finalize_common ((FlowShunt *) file_shunt);

  g_cond_free (file_shunt->cond);
  file_shunt->cond = NULL;

  g_slice_free (FileShunt, file_shunt);
}

static gpointer
file_shunt_main (FileShuntParams *params)
{
  FileShunt *file_shunt = params->file_shunt;
  FlowShunt *shunt      = (FlowShunt *) file_shunt;

  flow_shunt_impl_lock ();
  shunt->in_worker = TRUE;

  /* Tell main thread we're good to go */
  g_cond_signal (file_shunt->cond);

  file_shunt_open (params);

  while (!shunt->was_destroyed)
  {
    if (!shunt->need_reads)
      shunt->doing_reads = FALSE;
    if (!shunt->need_writes)
      shunt->doing_writes = FALSE;

    if (!shunt->doing_reads && !shunt->doing_writes)
    {
      g_cond_wait (file_shunt->cond, g_static_mutex_get_mutex (&global_mutex));
      continue;
    }

    if (shunt->need_reads)
    {
      file_shunt_read (shunt);
    }

    if (shunt->need_writes)
    {
      file_shunt_write (shunt);
    }
  }

  /* Finalize the shunt */
  shunt->in_worker = FALSE;
  file_shunt_finalize (file_shunt);

  flow_shunt_impl_unlock ();

  return NULL;
}

/* ------------ *
 * Construction *
 * ------------ */

static void
child_setup (void)
{
}

static FileShuntParams *
create_file_shunt_params (const gchar *path, FlowAccessMode access_mode)
{
  FileShunt       *file_shunt;
  FlowShunt       *shunt;
  FileShuntParams *params;

  file_shunt = g_slice_new0 (FileShunt);
  shunt = (FlowShunt *) file_shunt;

  flow_shunt_impl_lock ();
  flow_shunt_init_common (shunt, NULL);
  flow_shunt_impl_unlock ();

  shunt->shunt_type = SHUNT_TYPE_FILE;

  file_shunt->fd      = -1;
  file_shunt->cond    = g_cond_new ();

  params = g_slice_new0 (FileShuntParams);

  params->file_shunt  = file_shunt;
  params->path        = g_strdup (path);
  params->proc_access = access_mode;

  return params;
}

static FileShunt *
create_file_shunt_thread (FileShuntParams *params)
{
  FileShunt         *file_shunt = params->file_shunt;
  FlowShunt         *shunt = (FlowShunt *) file_shunt;
  GThread           *thread;
  FlowDetailedEvent *detailed_event;
  GError            *error = NULL;

  flow_shunt_impl_lock ();

  thread = g_thread_create_full ((GThreadFunc) file_shunt_main, params,
                                 THREAD_STACK_SIZE, FALSE, FALSE,
                                 G_THREAD_PRIORITY_NORMAL, &error);

  if (thread)
  {
    flow_shunt_read_state_changed (shunt);
    flow_shunt_write_state_changed (shunt);

    g_cond_wait (file_shunt->cond, g_static_mutex_get_mutex (&global_mutex));
  }
  else
  {
    detailed_event = flow_detailed_event_new_literal (error->message);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);

    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));

    g_clear_error (&error);
    flow_shunt_read_state_changed (shunt);
  }

  flow_shunt_impl_unlock ();

  return file_shunt;
}

static FlowShunt *
flow_shunt_impl_open_stdio (void)
{
  FlowShunt *shunt;
  PipeShunt *pipe_shunt;

  pipe_shunt = g_slice_new0 (PipeShunt);
  shunt = (FlowShunt *) pipe_shunt;

  flow_shunt_impl_lock ();

  flow_shunt_init_common (shunt, NULL);

  shunt->shunt_type = SHUNT_TYPE_PIPE;

  pipe_shunt->read_fd = STDIN_FILENO;
  flow_pipe_set_nonblock (STDIN_FILENO, TRUE);

  pipe_shunt->write_fd = STDOUT_FILENO;
  flow_pipe_set_nonblock (STDOUT_FILENO, TRUE);

  shunt->can_read  = TRUE;
  shunt->can_write = TRUE;

  flow_shunt_read_state_changed (shunt);
  flow_shunt_write_state_changed (shunt);

  flow_shunt_impl_unlock ();

  return shunt;
}

static FlowShunt *
flow_shunt_impl_open_file (const gchar *path, FlowAccessMode access_mode)
{
  FileShuntParams *params;

  params = create_file_shunt_params (path, access_mode);
  return (FlowShunt *) create_file_shunt_thread (params);
}

static FlowShunt *
flow_shunt_impl_create_file (const gchar *path, FlowAccessMode access_mode,
                             gboolean destructive,
                             FlowAccessMode creation_permissions_user,
                             FlowAccessMode creation_permissions_group,
                             FlowAccessMode creation_permissions_other)
{
  FileShuntParams *params;

  params = create_file_shunt_params (path, access_mode);

  params->create       = TRUE;
  params->destructive  = destructive;
  params->user_access  = creation_permissions_user;
  params->group_access = creation_permissions_group;
  params->other_access = creation_permissions_other;

  return (FlowShunt *) create_file_shunt_thread (params);
}

static gpointer
thread_shunt_main (ThreadShuntParams *params)
{
  ThreadShunt    *thread_shunt = params->thread_shunt;
  FlowShunt      *shunt        = (FlowShunt *) thread_shunt;
  FlowWorkerFunc *worker_func  = params->worker_func;
  gpointer        worker_data  = params->worker_data;

  g_slice_free (ThreadShuntParams, params);

  flow_shunt_impl_lock ();
  g_cond_signal (thread_shunt->cond);
  flow_shunt_impl_unlock ();

  worker_func ((FlowSyncShunt *) thread_shunt, worker_data);

  /* We finalize the shunt here, in the worker. But first, we have to
   * wait for the main thread to set shunt->was_destroyed to TRUE. */

  flow_shunt_impl_lock ();

  while (!shunt->was_destroyed)
    g_cond_wait (thread_shunt->cond, g_static_mutex_get_mutex (&global_mutex));

  flow_shunt_impl_unlock ();

  g_cond_free (thread_shunt->cond);
  g_slice_free (ThreadShunt, thread_shunt);
  return NULL;
}

static FlowShunt *
create_thread_shunt (FlowWorkerFunc func, gpointer user_data, gboolean filter_objects)
{
  FlowShunt         *shunt;
  ThreadShunt       *thread_shunt;
  ThreadShuntParams *params;
  GThread           *thread;
  GError            *error = NULL;

  flow_shunt_impl_lock ();

  thread_shunt = g_slice_new0 (ThreadShunt);
  shunt = (FlowShunt *) thread_shunt;
  flow_shunt_init_common (shunt, NULL);
  shunt->shunt_type = SHUNT_TYPE_THREAD;

  thread_shunt->cond = g_cond_new ();

  params = g_slice_new (ThreadShuntParams);

  params->thread_shunt = thread_shunt;
  params->worker_func  = func;
  params->worker_data  = user_data;

  thread = g_thread_create ((GThreadFunc) thread_shunt_main, params,
                            FALSE /* joinable */, &error);

  if (thread)
  {
    shunt->can_read  = TRUE;
    shunt->can_write = TRUE;

    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
    g_cond_wait (thread_shunt->cond, g_static_mutex_get_mutex (&global_mutex));
  }
  else
  {
    FlowDetailedEvent *detailed_event;

    detailed_event = flow_detailed_event_new_literal (error->message);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);

    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));

    g_clear_error (&error);
  }

  shunt->dispatched_begin = TRUE;

  flow_shunt_read_state_changed (shunt);
  flow_shunt_write_state_changed (shunt);

  flow_shunt_impl_unlock ();

  return shunt;
}

static FlowShunt *
flow_shunt_impl_spawn_worker (FlowWorkerFunc func, gpointer user_data)
{
  return create_thread_shunt (func, user_data, FALSE);
}

static FlowShunt *
flow_shunt_impl_spawn_process (FlowWorkerFunc func, gpointer user_data)
{
#ifdef G_PLATFORM_WIN32

  /* On Windows, you can't fork() without also implicitly calling
   * exec(). So we'll just have to use a thread there. See the Windows
   * documentation on CreateProcess() for more. */

  return create_thread_shunt (func, user_data, TRUE);

#else

  FlowShunt *shunt;
  PipeShunt *pipe_shunt;
  gint       saved_errno  = 0;
  gint       up_fds [2]   = { -1, -1 };
  gint       down_fds [2] = { -1, -1 };
  pid_t      pid          = -1;

  pipe_shunt = g_slice_new0 (PipeShunt);
  shunt = (FlowShunt *) pipe_shunt;

  flow_shunt_impl_lock ();
  flow_shunt_init_common (shunt, NULL);
  flow_shunt_impl_unlock ();

  shunt->shunt_type = SHUNT_TYPE_PIPE;

  if (pipe (up_fds) < 0)
  {
    /* Error: First pipe() failed */

    saved_errno = errno;
  }
  else if (pipe (down_fds) < 0)
  {
    /* Error: Second pipe() failed */

    saved_errno = errno;
  }
  else
  {
    pid = fork ();

    if (pid == 0)
    {
      /* Child process */

      flow_close_pipe_fd (up_fds [0]);
      flow_close_pipe_fd (down_fds [1]);

      flow_pipe_set_nonblock (down_fds [0], FALSE);
      flow_pipe_set_nonblock (up_fds [1], FALSE);

      pipe_shunt->read_fd  = down_fds [0];
      pipe_shunt->write_fd = up_fds [1];

      child_setup ();

      func ((FlowSyncShunt *) pipe_shunt, user_data);
      exit (0);
    }
    else if (pid > 0)
    {
      /* Parent process */

      flow_close_pipe_fd (up_fds [1]);
      flow_close_pipe_fd (down_fds [0]);

      flow_pipe_set_nonblock (up_fds [0], TRUE);
      flow_pipe_set_nonblock (down_fds [1], TRUE);

      up_fds [1]   = -1;
      down_fds [0] = -1;

      shunt->can_read  = TRUE;
      shunt->can_write = TRUE;
    }
    else
    {
      /* Could not fork() */

      saved_errno = errno;
    }
  }

  if (shunt->can_read)
  {
    /* Success */
  }
  else
  {
    FlowDetailedEvent *detailed_event;

    /* Error: pipe() or fork() failed. Must be some sort of resource problem. */

    if (up_fds [0] >= 0)
      flow_close_pipe_fd (up_fds [0]);
    if (up_fds [1] >= 0)
      flow_close_pipe_fd (up_fds [1]);
    if (down_fds [0] >= 0)
      flow_close_pipe_fd (down_fds [0]);
    if (down_fds [1] >= 0)
      flow_close_pipe_fd (down_fds [1]);

    up_fds [0] = up_fds [1] = down_fds [0] = down_fds [1] = -1;

    detailed_event = generate_errno_event (saved_errno, NULL);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }

  pipe_shunt->child_pid = pid;
  pipe_shunt->read_fd   = up_fds [0];
  pipe_shunt->write_fd  = down_fds [1];

  flow_shunt_impl_lock ();

  flow_shunt_read_state_changed (shunt);
  flow_shunt_write_state_changed (shunt);

  flow_shunt_impl_unlock ();

  return shunt;

#endif
}

static FlowShunt *
flow_shunt_impl_spawn_command_line (const gchar *command_line)
{
  FlowShunt  *shunt;
  PipeShunt  *pipe_shunt;
  GError     *error = NULL;
  gchar     **argv  = NULL;
  GPid        pid   = -1;
  gint        stdin_fd;
  gint        stdout_fd;

  pipe_shunt = g_slice_new0 (PipeShunt);
  shunt = (FlowShunt *) pipe_shunt;

  flow_shunt_impl_lock ();
  flow_shunt_init_common (shunt, NULL);
  flow_shunt_impl_unlock ();

  shunt->shunt_type = SHUNT_TYPE_PIPE;

  if (!g_shell_parse_argv (command_line,
                           NULL, &argv,
                           &error))
  {
    FlowDetailedEvent *detailed_event;

    /* Error: Could not parse command line */

    detailed_event = flow_detailed_event_new_literal (error->message);
    flow_detailed_event_add_code (detailed_event, FLOW_EXEC_DOMAIN, FLOW_EXEC_PARSE_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));

    g_clear_error (&error);
  }
  else if (!g_spawn_async_with_pipes (NULL,                 /* cwd */
                                      argv,
                                      NULL,                 /* envp */
                                      G_SPAWN_SEARCH_PATH,  /* flags */
                                      (GSpawnChildSetupFunc) child_setup,
                                      NULL,                 /* user_data */
                                      &pid,
                                      &stdin_fd,
                                      &stdout_fd,
#if 0
                                      &stderr_fd,
#else
                                      NULL,
#endif
                                      &error))
  {
    FlowDetailedEvent *detailed_event;

    /* Error: Could not spawn child process */

    detailed_event = flow_detailed_event_new_literal (error->message);
    flow_detailed_event_add_code (detailed_event, FLOW_EXEC_DOMAIN, FLOW_EXEC_RUN_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));

    g_clear_error (&error);
  }
  else
  {
    /* Success */

    shunt->can_read  = TRUE;
    shunt->can_write = TRUE;

    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
    generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_BEGIN);

    flow_pipe_set_nonblock (stdout_fd, TRUE);
    flow_pipe_set_nonblock (stdin_fd,  TRUE);
#if 0
    flow_pipe_set_nonblock (stderr_fd, TRUE);
#endif

    pipe_shunt->child_pid = pid;
    pipe_shunt->read_fd   = stdout_fd;
    pipe_shunt->write_fd  = stdin_fd;
    /* TODO: The caller might want to use stderr_fd for something */
  }

  if (pid < 0)
  {
    pipe_shunt->child_pid = -1;
    pipe_shunt->read_fd   = -1;
    pipe_shunt->write_fd  = -1;
  }

  if (argv)
    g_strfreev (argv);

  flow_shunt_impl_lock ();

  shunt->dispatched_begin = TRUE;

  flow_shunt_read_state_changed (shunt);
  flow_shunt_write_state_changed (shunt);

  flow_shunt_impl_unlock ();

  return shunt;
}

static FlowShunt *
flow_shunt_impl_open_tcp_listener (FlowIPService *local_service)
{
  FlowShunt          *shunt;
  TcpListenerShunt   *tcp_listener_shunt;
  FlowIPAddr         *ip_addr = NULL;
  FlowIPAddrFamily    family;
  FlowSockaddr        sa;
  gint                fd;

  tcp_listener_shunt = g_slice_new0 (TcpListenerShunt);
  shunt = (FlowShunt *) tcp_listener_shunt;

  flow_shunt_impl_lock ();
  flow_shunt_init_common (shunt, NULL);
  flow_shunt_impl_unlock ();

  shunt->shunt_type = SHUNT_TYPE_TCP_LISTENER;

  if (local_service)
  {
    ip_addr = flow_ip_service_find_address (local_service, FLOW_IP_ADDR_ANY_FAMILY);

    /* Don't mess with the passed-in service */

    if (ip_addr)
    {
      /* Passed-in service provided an IP to bind to */

      g_object_ref (local_service);
    }
    else
    {
      FlowIPService *new_local_service;

      /* No IP; use passed-in port and quality, and bind to all interfaces */

      new_local_service = flow_ip_service_new ();
      flow_ip_service_set_port (new_local_service, flow_ip_service_get_port (local_service));
      flow_ip_service_set_quality (new_local_service, flow_ip_service_get_quality (local_service));

      local_service = new_local_service;
    }
  }
  else
  {
    /* No information provided; use a random, OS-assigned port, and bind to
     * all interfaces. */

    local_service = flow_ip_service_new ();
  }

  if (!ip_addr)
  {
    /* No IP; Bind to all interfaces, use provided port */

    ip_addr = flow_ip_addr_new ();
    flow_ip_addr_set_string (ip_addr, "0.0.0.0");
    flow_ip_service_add_address (local_service, ip_addr);
  }

  tcp_listener_shunt->quality = flow_ip_service_get_quality (local_service);

  family = flow_ip_addr_get_family (ip_addr);
  tcp_listener_shunt->socket_shunt.fd = fd = socket (family == FLOW_IP_ADDR_IPV6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);

  flow_gobject_unref_clear (ip_addr);

  if (fd < 0)
  {
    FlowDetailedEvent *detailed_event;
    gint               saved_errno;

    /* Error: Could not allocate socket */

    saved_errno = errno;

    assert_non_fatal_errno (saved_errno, tcp_socket_fatal_errnos);

    detailed_event = generate_errno_event (saved_errno, NULL);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }
  else if (!flow_socket_set_nonblock (fd, TRUE))
  {
    /* Error: Could not set socket nonblock. If this happens, our implementation will
     * not work as intended, so abort execution. */

    g_assert_not_reached ();
  }
  else if (!flow_ip_service_get_sockaddr (local_service, &sa, 0))
  {
    FlowDetailedEvent *detailed_event;

    detailed_event = flow_detailed_event_new_literal ("Invalid address");
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }
  else
  {
    gint on = 1;

#ifndef G_PLATFORM_WIN32
    if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof (on)) < 0)
#else
    if (setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, (gchar *) &on, sizeof (on)) < 0)
#endif
      assert_non_fatal_errno (errno, setsockopt_fatal_errnos);

#ifdef USE_SO_NOSIGPIPE
    /* For MacOS X, BSD */
    setsockopt (fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof (on));
#endif

    if (bind (fd, (struct sockaddr *) &sa, flow_sockaddr_get_len (&sa)) != 0)
    {
      FlowDetailedEvent *detailed_event;
      gint               saved_errno;

      /* Error: Could not bind to local address */

      saved_errno = errno;

      assert_non_fatal_errno (saved_errno, tcp_bind_fatal_errnos);

      detailed_event = generate_errno_event (saved_errno, tcp_bind_errno_map);
      flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
      flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
      flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
    }
    else if (listen (fd, 16) != 0)
    {
      FlowDetailedEvent *detailed_event;
      gint               saved_errno;

      /* Error: Could not listen */

      saved_errno = errno;

      assert_non_fatal_errno (saved_errno, tcp_listen_fatal_errnos);

      detailed_event = generate_errno_event (saved_errno, tcp_listen_errno_map);
      flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
      flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
      flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
    }
    else
    {
      /* Success */

      shunt->can_read = TRUE;

      generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
      generate_simple_event (shunt, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_BEGIN);
    }
  }

  shunt->dispatched_begin = TRUE;

  if (!shunt->can_read)
  {
    if (fd >= 0)
      flow_close_socket_fd (fd);

    tcp_listener_shunt->socket_shunt.fd = -1;
  }

  flow_shunt_impl_lock ();
  flow_shunt_read_state_changed (shunt);
  flow_shunt_impl_unlock ();

  g_object_unref (local_service);
  return shunt;
}

static FlowShunt *
flow_shunt_impl_connect_to_tcp (FlowIPService *remote_service, gint local_port)
{
  FlowShunt        *shunt;
  TcpShunt         *tcp_shunt;
  FlowIPAddr       *ip_addr;
  FlowIPAddrFamily  family= FLOW_IP_ADDR_IPV4;
  FlowSockaddr      sa;
  gint              fd;

  tcp_shunt = g_slice_new0 (TcpShunt);
  shunt = (FlowShunt *) tcp_shunt;

  flow_shunt_impl_lock ();
  flow_shunt_init_common (shunt, NULL);
  flow_shunt_impl_unlock ();

  shunt->shunt_type = SHUNT_TYPE_TCP;

  memset (&sa, 0, sizeof (sa));

  ip_addr = flow_ip_service_find_address (remote_service, FLOW_IP_ADDR_ANY_FAMILY);
  if (ip_addr)
    family = flow_ip_addr_get_family (ip_addr);

  flow_gobject_unref_clear (ip_addr);

  tcp_shunt->socket_shunt.fd = fd = socket (family == FLOW_IP_ADDR_IPV6 ? AF_INET6 : AF_INET, SOCK_STREAM, 0);

  if (fd < 0)
  {
    FlowDetailedEvent *detailed_event;
    gint               saved_errno;

    /* Error: Could not create socket */

    saved_errno = errno;
    detailed_event = generate_errno_event (saved_errno, NULL);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }
  else if (!flow_socket_set_nonblock (fd, TRUE))
  {
    /* Error: Could not set socket nonblock. If this happens, our implementation will
     * not work as intended, so abort execution. */

    g_assert_not_reached ();
  }
  else if (!flow_ip_service_get_sockaddr (remote_service, &sa, 0))
  {
    FlowDetailedEvent *detailed_event;

    detailed_event = flow_detailed_event_new_literal ("Invalid address");
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }
  else if (connect (fd, (struct sockaddr *) &sa, flow_sockaddr_get_len (&sa)) != 0 &&
#ifndef G_PLATFORM_WIN32
      errno != EINPROGRESS
#else
      WSAGetLastError () != WSAEWOULDBLOCK
#endif
     )
  {
    FlowDetailedEvent *detailed_event;
    gint               saved_errno;

    /* Error: Failed to start connection attempt */

    saved_errno = errno;

    assert_non_fatal_errno (saved_errno, tcp_connect_fatal_errnos);

    detailed_event = generate_errno_event (saved_errno, tcp_connect_errno_map);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }
  else
  {
    gint on = 1;

    /* Success */

    tcp_socket_set_quality (fd, flow_ip_service_get_quality (remote_service));

    if (setsockopt (fd, SOL_SOCKET, SO_OOBINLINE, &on, sizeof (on)) < 0)
      assert_non_fatal_errno (errno, setsockopt_fatal_errnos);

#ifdef USE_SO_NOSIGPIPE
    /* For MacOS X, BSD */
    setsockopt (fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof (on));
#endif

    shunt->can_read  = TRUE;
    shunt->can_write = TRUE;
  }

  if (!shunt->can_read)
  {
    /* Failure: Close down resources */

    if (fd >= 0)
    {
      flow_close_socket_fd (fd);
      tcp_shunt->socket_shunt.fd = -1;
    }
  }

  flow_shunt_impl_lock ();

  flow_shunt_read_state_changed (shunt);
  flow_shunt_write_state_changed (shunt);

  flow_shunt_impl_unlock ();

  return shunt;
}

static FlowShunt *
flow_shunt_impl_open_udp_port (FlowIPService *local_service)
{
  FlowShunt        *shunt;
  UdpShunt         *udp_shunt;
  FlowIPAddr       *ip_addr;
  FlowIPAddrFamily  family = FLOW_IP_ADDR_IPV4;
  FlowSockaddr      sa;
  gint              fd;

  udp_shunt = g_slice_new0 (UdpShunt);
  shunt = (FlowShunt *) udp_shunt;

  flow_shunt_impl_lock ();
  flow_shunt_init_common (shunt, NULL);
  flow_shunt_impl_unlock ();

  shunt->shunt_type = SHUNT_TYPE_UDP;

  ip_addr = flow_ip_service_find_address (local_service, FLOW_IP_ADDR_ANY_FAMILY);
  if (ip_addr)
    family = flow_ip_addr_get_family (ip_addr);

  flow_gobject_unref_clear (ip_addr);

  udp_shunt->socket_shunt.fd = fd = socket (family == FLOW_IP_ADDR_IPV6 ? AF_INET6 : AF_INET, SOCK_DGRAM, 0);

  if (fd < 0)
  {
    FlowDetailedEvent *detailed_event;
    gint               saved_errno;

    /* Error: Could not create socket */

    saved_errno = errno;
    detailed_event = generate_errno_event (saved_errno, NULL);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_RESOURCE_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }
  else if (!flow_socket_set_nonblock (fd, TRUE))
  {
    /* Error: Could not set socket nonblock. If this happens, our implementation will
     * not work as intended, so abort execution. */

    g_assert_not_reached ();
  }
  else if (!flow_ip_service_get_sockaddr (local_service, &sa, 0))
  {
    FlowDetailedEvent *detailed_event;

    detailed_event = flow_detailed_event_new_literal ("Invalid address");
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_APP_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }
  else if (bind (fd, (struct sockaddr *) &sa, flow_sockaddr_get_len (&sa)) != 0)
  {
    FlowDetailedEvent *detailed_event;
    gint               saved_errno;

    /* Error: Could not bind to local address */

    saved_errno = errno;

    assert_non_fatal_errno (saved_errno, tcp_bind_fatal_errnos);

    detailed_event = generate_errno_event (saved_errno, tcp_bind_errno_map);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_packet_queue_push_packet (shunt->read_queue, flow_packet_new_take_object (detailed_event, 0));
  }
  else
  {
    /* Success */

#ifdef USE_SO_NOSIGPIPE
    {
      gint on = 1;

      /* For MacOS X, BSD */
      setsockopt (fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof (on));
    }
#endif

    ip_socket_set_quality (fd, flow_ip_service_get_quality (local_service));

    shunt->can_read  = TRUE;
    shunt->can_write = TRUE;
  }

  if (!shunt->can_read)
  {
    /* Failure: Close down resources */

    if (fd >= 0)
    {
      flow_close_socket_fd (fd);
      udp_shunt->socket_shunt.fd = -1;
    }
  }

  flow_shunt_impl_lock ();

  flow_shunt_read_state_changed (shunt);
  flow_shunt_write_state_changed (shunt);

  flow_shunt_impl_unlock ();

  return shunt;
}

/* ---------------------- *
 * Synchronous Operations *
 * ---------------------- */

/* These operations are for process and worker children which get passed a
 * FlowSyncShunt reference. */

static gboolean
flow_sync_shunt_impl_try_read (FlowSyncShunt *sync_shunt, FlowPacket **packet_dest)
{
  FlowShunt  *shunt = (FlowShunt *) sync_shunt;
  FlowPacket *packet;
  gboolean    still_open = TRUE;

  switch (shunt->shunt_type)
  {
    case SHUNT_TYPE_PIPE:
      {
        PipeShunt *pipe_shunt = (PipeShunt *) sync_shunt;
        gpointer   buffer;
        gint       result;
        gint       saved_errno;

        shunt->io_buffer_size = shunt->io_buffer_desired_size;
        buffer = alloca (shunt->io_buffer_desired_size);

        flow_pipe_set_nonblock (pipe_shunt->read_fd, TRUE);
        result = read (pipe_shunt->read_fd, buffer, shunt->io_buffer_size);
        saved_errno = errno;

        if (result > 0)
        {
          packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, result);
        }
        else if (result == 0 || saved_errno == EAGAIN || saved_errno == EINTR)
        {
          packet = NULL;
        }
        else
        {
          /* Error */

          assert_non_fatal_errno (saved_errno, socket_read_fatal_errnos);
          packet = NULL;
          still_open = FALSE;
        }
      }
      break;

    case SHUNT_TYPE_THREAD:
      flow_shunt_impl_lock ();

      shunt->io_buffer_size = shunt->io_buffer_desired_size;

      packet = flow_packet_queue_pop_packet (shunt->write_queue);
      if (packet)
        flow_shunt_write_state_changed (shunt);

      flow_shunt_impl_unlock ();
      break;

    default:
      packet = NULL;
      g_assert_not_reached ();
      break;
  }

  *packet_dest = packet;
  return still_open;
}

static gboolean
flow_sync_shunt_impl_read (FlowSyncShunt *sync_shunt, FlowPacket **packet_dest)
{
  FlowShunt  *shunt = (FlowShunt *) sync_shunt;
  FlowPacket *packet;
  gboolean    still_open = TRUE;

  switch (shunt->shunt_type)
  {
    case SHUNT_TYPE_PIPE:
      {
        PipeShunt *pipe_shunt = (PipeShunt *) sync_shunt;
        gpointer   buffer;
        gint       result;

        shunt->io_buffer_size = shunt->io_buffer_desired_size;
        buffer = alloca (shunt->io_buffer_size);

        flow_pipe_set_nonblock (pipe_shunt->read_fd, TRUE);

        for (;;)
        {
          fd_set read_fds;

          FD_ZERO (&read_fds);
          FD_SET (pipe_shunt->read_fd, &read_fds);

          result = select (pipe_shunt->read_fd + 1, &read_fds, NULL, NULL, NULL);
          if (result == 0 || (result < 0 && errno == EINTR))
            continue;

          result = read (pipe_shunt->read_fd, buffer, shunt->io_buffer_size);
          if (result < 0 && errno == EINTR)
            continue;

          break;
        }

        if (result > 0)
        {
          packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, result);
        }
        else
        {
          /* Error */

          assert_non_fatal_errno (errno, socket_read_fatal_errnos);
          packet = NULL;
          still_open = FALSE;
        }
      }
      break;

    case SHUNT_TYPE_THREAD:
      {
        ThreadShunt *thread_shunt = (ThreadShunt *) sync_shunt;

        flow_shunt_impl_lock ();

        shunt->io_buffer_size = shunt->io_buffer_desired_size;

        while (!(packet = flow_packet_queue_pop_packet (shunt->write_queue)))
        {
          g_cond_wait (thread_shunt->cond, g_static_mutex_get_mutex (&global_mutex));
        }

        flow_shunt_write_state_changed (shunt);
        flow_shunt_impl_unlock ();
      }
      break;

    default:
      packet = NULL;
      g_assert_not_reached ();
      break;
  }

  *packet_dest = packet;
  return still_open;
}

static void
flow_sync_shunt_impl_write (FlowSyncShunt *sync_shunt, FlowPacket *packet)
{
  FlowShunt *shunt = (FlowShunt *) sync_shunt;

  g_return_if_fail (sync_shunt != NULL);
  g_return_if_fail (packet != NULL);

  switch (shunt->shunt_type)
  {
    case SHUNT_TYPE_PIPE:
      {
        PipeShunt *pipe_shunt = (PipeShunt *) sync_shunt;
        guchar    *p0, *p1;

        flow_pipe_set_nonblock (pipe_shunt->write_fd, FALSE);

        p0 = flow_packet_get_data (packet);
        p1 = p0 + flow_packet_get_size (packet);

        while (p0 < p1)
        {
          gint result;

          result = write (pipe_shunt->write_fd, p0, p1 - p0);

          if (result < 1)
          {
            if (errno != EINTR && errno != EAGAIN)
              break;
          }
          else
          {
            p0 += result;
          }
        }

        flow_packet_unref (packet);
      }
      break;

    case SHUNT_TYPE_THREAD:
      flow_shunt_impl_lock ();
      flow_packet_queue_push_packet (shunt->read_queue, packet);
      flow_shunt_read_state_changed (shunt);
      flow_shunt_impl_unlock ();
      break;

    default:
      g_assert_not_reached ();
      break;
  }
}
