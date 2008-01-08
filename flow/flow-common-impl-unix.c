/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-common-impl-unix.c - Utilities for Unix-like implementation.
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

#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifndef G_PLATFORM_WIN32
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <fcntl.h>
# include <unistd.h>
#else
# include <io.h>
#endif

#include "flow-common-impl-unix.h"

#define DEBUG(x)

#define WAKEUP_READ  0
#define WAKEUP_WRITE 1

#ifdef HAVE_IPV6

/* IPv4 and IPv6 capable */

#define FLOW_SOCKADDR_GET_FAMILY(sa)                             \
  (((struct sockaddr *) (sa))->sa_family)

#define FLOW_SOCKADDR_GET_LEN(sa)                                \
  ((((struct sockaddr *) (sa))->sa_family == AF_INET) ?          \
   sizeof (struct sockaddr_in) : sizeof (struct sockaddr_in6))

#define FLOW_SOCKADDR_GET_ADDR(sa)                               \
  ((((struct sockaddr *) (sa))->sa_family == AF_INET) ?          \
   (gpointer) &((struct sockaddr_in *) (sa))->sin_addr :         \
   (gpointer) &((struct sockaddr_in6 *) (sa))->sin6_addr)

#define FLOW_SOCKADDR_GET_ADDR_LEN(sa)                           \
  ((((struct sockaddr *) (sa))->sa_family == AF_INET) ?          \
   sizeof (struct in_addr) : sizeof (struct in6_addr))

#define FLOW_SOCKADDR_GET_PORT(sa)                               \
  ((((struct sockaddr *) (sa))->sa_family == AF_INET) ?          \
   ((struct sockaddr_in *) (sa))->sin_port :                     \
   ((struct sockaddr_in6 *) (sa))->sin6_port)

#else

/* IPv4 only */

#define FLOW_SOCKADDR_GET_FAMILY(sa)                             \
  (((struct sockaddr *) (sa))->sa_family)

#define FLOW_SOCKADDR_GET_LEN(sa)                                \
  ((((struct sockaddr *) (sa))->sa_family == AF_INET) ?          \
   sizeof (struct sockaddr_in) : 0)

#define FLOW_SOCKADDR_GET_ADDR(sa)                               \
  ((((struct sockaddr *) (sa))->sa_family == AF_INET) ?          \
   (gpointer) &((struct sockaddr_in *) (sa))->sin_addr :         \
   (gpointer) NULL)

#define FLOW_SOCKADDR_GET_ADDR_LEN(sa)                           \
  ((((struct sockaddr *) (sa))->sa_family == AF_INET) ?          \
   sizeof (struct in_addr) : 0)

#define FLOW_SOCKADDR_GET_PORT(sa)                               \
  ((((struct sockaddr *) (sa))->sa_family == AF_INET) ?          \
   ((struct sockaddr_in *) (sa))->sin_port : 0)

#endif

static gchar *
preferred_ntop (gint family, struct in_addr *sa_in_addr, gchar *buf, gint buf_len)
{
#if defined (HAVE_INET_NTOP)

  return (gchar *) inet_ntop (family, sa_in_addr, buf, buf_len);

#else

  static GStaticMutex  mutex = G_STATIC_MUTEX_INIT;
  const gchar         *addr_str = NULL;
  gchar               *result;

  if (family != AF_INET)
    return NULL;

  g_static_mutex_lock (&mutex);

  addr_str = inet_ntoa (*sa_in_addr);

  if (addr_str)
  {
    strncpy (buf, addr_str, buf_len);
    buf [buf_len - 1] = '\0';
    result = buf;
  }
  else
  {
    result = NULL;
  }

  g_static_mutex_unlock (&mutex);

  return result;

#endif
}

static gint
preferred_pton (gint family, const gchar *src, gpointer dest)
{
#if defined (G_PLATFORM_WIN32)

  guint32 addr;

  addr = inet_addr (src);
  if (addr == INADDR_NONE && strcmp (src, "255.255.255.255"))
    return 0;

  *((guint32 *) dest) = addr;
  return 1;

#elif defined (HAVE_INET_PTON)

  gint result = inet_pton (family, src, dest);

  DEBUG (g_print ("inet_pton returned %d.\n", result));
  return result;

#else

  if (inet_aton (src, dest))
  {
    DEBUG (g_print ("inet_aton successful.\n"));
    return 1;
  }

  DEBUG (g_print ("inet_aton failed.\n"));
  return 0;

#endif
}

static gint
addr_family_to_af (FlowAddrFamily family)
{
  switch (family)
  {
    case FLOW_ADDR_FAMILY_IPV4:
      return AF_INET;

#ifdef HAVE_IPV6

    case FLOW_ADDR_FAMILY_IPV6:
      return AF_INET6;

#endif

    case FLOW_ADDR_FAMILY_UNIX:
      return AF_UNIX;

    default:
      return AF_UNSPEC;
  }
}

static FlowAddrFamily
addr_family_from_af (gint af)
{
  switch (af)
  {
    case AF_INET:
      return FLOW_ADDR_FAMILY_IPV4;

#ifdef HAVE_IPV6

    case AF_INET6:
      return FLOW_ADDR_FAMILY_IPV6;

#endif

    case AF_UNIX:
      return FLOW_ADDR_FAMILY_UNIX;

    default:
      return FLOW_ADDR_FAMILY_INVALID;
  }
}

gint
flow_addr_family_to_af (FlowAddrFamily family)
{
  return addr_family_to_af (family);
}

FlowAddrFamily
flow_addr_family_from_af (gint af)
{
  return addr_family_from_af (af);
}

gboolean
flow_init_sockets (void)
{
#ifdef G_PLATFORM_WIN32
  WORD    req_version;
  WSADATA wsa_data;
  gint    err;
  static  GStaticMutex mutex       = G_STATIC_MUTEX_INIT;
  static  gboolean     initialized = FALSE;

  g_static_mutex_lock (&mutex);

  if (initialized)
  {
    g_static_mutex_unlock (&mutex);
    return TRUE;
  }

  req_version = MAKEWORD (2, 0);
  err = WSAStartup (req_version, &wsa_data);
  if (err != 0)
  {
    g_static_mutex_unlock (&mutex);
    return FALSE;
  }

  if (LOBYTE (wsa_data.wVersion) != 2 ||
      HIBYTE (wsa_data.wVersion) != 0)
  {
    WSACleanup ();
    g_static_mutex_unlock (&mutex);
    return FALSE;
  }

  initialized = TRUE;

  g_static_mutex_unlock (&mutex);
#endif

  return TRUE;
}

FlowAddrFamily
flow_sockaddr_get_family (FlowSockaddr *sa)
{
  gint af = FLOW_SOCKADDR_GET_FAMILY (sa);

  return addr_family_from_af (af);
}

gint
flow_sockaddr_get_len (FlowSockaddr *sa)
{
  return FLOW_SOCKADDR_GET_LEN (sa);
}

gpointer
flow_sockaddr_get_addr (FlowSockaddr *sa)
{
  return FLOW_SOCKADDR_GET_ADDR (sa);
}

gint
flow_sockaddr_get_addr_len (FlowSockaddr *sa)
{
  return FLOW_SOCKADDR_GET_ADDR_LEN (sa);
}

gint
flow_sockaddr_get_port (FlowSockaddr *sa)
{
  return FLOW_SOCKADDR_GET_PORT (sa);
}

gboolean
flow_sockaddr_init (FlowSockaddr *dest_sa, FlowAddrFamily family, const gchar *addr_str, gint port)
{
  g_return_val_if_fail (dest_sa != NULL, FALSE);

  if (family == FLOW_ADDR_FAMILY_IPV4)
  {
    struct sockaddr_in in_sa;

    if (!addr_str || !strcmp (addr_str, "0.0.0.0"))
      in_sa.sin_addr.s_addr = INADDR_ANY;
    else if (preferred_pton (AF_INET, addr_str, &in_sa.sin_addr) < 1)
      return FALSE;

    in_sa.sin_family = AF_INET;
    in_sa.sin_port   = g_htons (port);
    memcpy (dest_sa, &in_sa, sizeof (in_sa));
  }

#ifdef HAVE_IPV6

  else if (family == FLOW_ADDR_FAMILY_IPV6)
  {
    struct sockaddr_in6 in6_sa;

    if (!addr_str)
      memset (&in6_sa.sin6_addr, 0, sizeof (in6_sa.sin6_addr));
    else if (preferred_pton (AF_INET6, addr_str, &in6_sa.sin6_addr) < 1)
      return FALSE;

    in6_sa.sin6_family = AF_INET6;
    in6_sa.sin6_port   = g_htons (port);
    memcpy (dest_sa, &in6_sa, sizeof (in6_sa));
  }

#endif

  else
  {
    g_assert_not_reached ();
  }

  return TRUE;
}

gchar *
flow_sockaddr_to_string (FlowSockaddr *sa)
{
  gchar buf [256];

  if (!preferred_ntop (FLOW_SOCKADDR_GET_FAMILY (sa), flow_sockaddr_get_addr (sa), buf, 256))
    return NULL;

  return g_strdup (buf);
}

#ifndef G_PLATFORM_WIN32

static gboolean
unix_set_nonblock (gint fd, gboolean nonblock)
{
  gint flags;

  flags = fcntl (fd, F_GETFL);
  if (flags < 0)
    return FALSE;

  if (fcntl (fd, F_SETFL, nonblock ? flags | O_NONBLOCK : flags & ~O_NONBLOCK) < 0)
    return FALSE;

  return TRUE;
}

#endif

gboolean
flow_socket_set_nonblock (gint fd, gboolean nonblock)
{
#ifdef G_PLATFORM_WIN32  /* Windows */

  u_long arg = nonblock ? 1 : 0;

  if (ioctlsocket (fd, FIONBIO, &arg))
    return FALSE;

  return TRUE;

#else  /* Unix */

  return unix_set_nonblock (fd, nonblock);

#endif
}

gboolean
flow_pipe_set_nonblock (gint fd, gboolean nonblock)
{
#ifdef G_PLATFORM_WIN32
  return FALSE;
#else
  return unix_set_nonblock (fd, nonblock);
#endif
}

gboolean
flow_wakeup_pipe_init (FlowWakeupPipe *wakeup_pipe)
{
  g_return_val_if_fail (wakeup_pipe != NULL, FALSE);
  g_return_val_if_fail (!flow_wakeup_pipe_is_valid (wakeup_pipe), FALSE);

#ifdef G_PLATFORM_WIN32  /* Windows */

  {
    struct sockaddr_in sa_in_1, sa_in_2;
    gint               my_fd [2] = { -1, -1 };
    gint               len;

    my_fd [WAKEUP_WRITE] = socket (AF_INET, SOCK_DGRAM, 0);
    if (my_fd [WAKEUP_WRITE] < 0)
      goto error;

    sa_in_1.sin_family = AF_INET;
    sa_in_1.sin_port = 0;
    sa_in_1.sin_addr.s_addr = htonl (INADDR_ANY);

    if (bind (my_fd [WAKEUP_WRITE], (struct sockaddr *) &sa_in_1, sizeof (sa_in_1)) != 0)
      goto error;

    sa_in_2.sin_family = AF_INET;
    sa_in_2.sin_port = 0;
    sa_in_2.sin_addr.s_addr = htonl (INADDR_ANY);

    my_fd [WAKEUP_READ] = socket (AF_INET, SOCK_DGRAM, 0);
    if (my_fd [WAKEUP_READ] < 0)
      goto error;

    if (bind (my_fd [WAKEUP_READ], (struct sockaddr *) &sa_in_2, sizeof (sa_in_2)) != 0)
      goto error;

    len = sizeof (sa_in_2);
    if (getsockname (my_fd [WAKEUP_READ], (struct sockaddr *) &sa_in_2, &len) != 0)
      goto error;

    sa_in_2.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

    if (connect (my_fd [WAKEUP_WRITE], (struct sockaddr *) &sa_in_2, sizeof (sa_in_2)) < 0)
      goto error;

    flow_socket_set_nonblock (my_fd [WAKEUP_READ], TRUE);
    flow_socket_set_nonblock (my_fd [WAKEUP_WRITE], TRUE);

    wakeup_pipe->fd [WAKEUP_READ]  = my_fd [WAKEUP_READ];
    wakeup_pipe->fd [WAKEUP_WRITE] = my_fd [WAKEUP_WRITE];

    return TRUE;

error:
    if (my_fd [WAKEUP_READ] >= 0)
      flow_close_socket_fd (my_fd [WAKEUP_READ]);
    if (my_fd [WAKEUP_WRITE] >= 0)
      flow_close_socket_fd (my_fd [WAKEUP_WRITE]);

    wakeup_pipe->fd [WAKEUP_READ]  = -1;
    wakeup_pipe->fd [WAKEUP_WRITE] = -1;

    return FALSE;
  }

#else  /* Unix */

  pipe (wakeup_pipe->fd);

  flow_socket_set_nonblock (wakeup_pipe->fd [WAKEUP_READ], TRUE);
  flow_socket_set_nonblock (wakeup_pipe->fd [WAKEUP_WRITE], TRUE);

  return TRUE;

#endif
}

void
flow_wakeup_pipe_destroy (FlowWakeupPipe *wakeup_pipe)
{
  g_return_if_fail (wakeup_pipe != NULL);
  g_return_if_fail (flow_wakeup_pipe_is_valid (wakeup_pipe));

  flow_close_socket_fd (wakeup_pipe->fd [WAKEUP_READ]);
  flow_close_socket_fd (wakeup_pipe->fd [WAKEUP_WRITE]);

  wakeup_pipe->fd [WAKEUP_READ]  = -1;
  wakeup_pipe->fd [WAKEUP_WRITE] = -1;
}

gint
flow_wakeup_pipe_get_watch_fd (FlowWakeupPipe *wakeup_pipe)
{
  g_return_val_if_fail (wakeup_pipe != NULL, -1);
  g_return_val_if_fail (flow_wakeup_pipe_is_valid (wakeup_pipe), -1);

  return wakeup_pipe->fd [WAKEUP_READ];
}

void
flow_wakeup_pipe_wakeup (FlowWakeupPipe *wakeup_pipe)
{
  const gchar buf [1] = { '\0' };

  g_return_if_fail (wakeup_pipe != NULL);
  g_return_if_fail (flow_wakeup_pipe_is_valid (wakeup_pipe));

#ifdef G_PLATFORM_WIN32  /* Windows */

  send (wakeup_pipe->fd [WAKEUP_WRITE], buf, sizeof (buf), 0);

#else  /* Unix */

  write (wakeup_pipe->fd [WAKEUP_WRITE], buf, sizeof (buf));

#endif
}

void
flow_wakeup_pipe_handle_wakeup (FlowWakeupPipe *wakeup_pipe)
{
  gchar buf [256];

  g_return_if_fail (wakeup_pipe != NULL);
  g_return_if_fail (flow_wakeup_pipe_is_valid (wakeup_pipe));

#ifdef G_PLATFORM_WIN32  /* Windows */

  recv (wakeup_pipe->fd [WAKEUP_READ], buf, sizeof (buf), 0);

#else  /* Unix */

  while (read (wakeup_pipe->fd [WAKEUP_READ], buf, sizeof (buf)) > 0)
    ;

#endif
}

gboolean
flow_wakeup_pipe_is_valid (FlowWakeupPipe *wakeup_pipe)
{
  g_return_val_if_fail (wakeup_pipe != NULL, FALSE);

  if (wakeup_pipe->fd [WAKEUP_READ] >= 0 &&
      wakeup_pipe->fd [WAKEUP_WRITE] >= 0)
    return TRUE;

  return FALSE;
}

gboolean
flow_ip_addr_get_sockaddr (FlowIPAddr *ip_addr, FlowSockaddr *dest_sa, gint port)
{
  gchar    *addr_str;
  gboolean  result;

  addr_str = flow_ip_addr_get_string (ip_addr);
  result = flow_sockaddr_init (dest_sa, ip_addr ? ip_addr->family : FLOW_IP_ADDR_IPV4, addr_str, port);
  g_free (addr_str);

  return result;
}

gboolean
flow_ip_addr_set_sockaddr (FlowIPAddr *ip_addr, FlowSockaddr *sa)
{
  gchar    *addr_str;
  gboolean  result;

  g_return_val_if_fail (sa != NULL, FALSE);

  addr_str = flow_sockaddr_to_string (sa);
  if (!addr_str)
    return FALSE;

  result = flow_ip_addr_set_string (ip_addr, addr_str);
  g_free (addr_str);

  return result;
}

static void
free_object_list (GList *list)
{
  GList *l;

  for (l = list; l; l = g_list_next (l))
    g_object_unref (l->data);

  g_list_free (list);
}

gboolean
flow_ip_service_get_sockaddr (FlowIPService *ip_service, FlowSockaddr *dest_sa, gint addr_index)
{
  GList      *ip_addr_list;
  FlowIPAddr *ip_addr;
  gboolean    result = FALSE;

  ip_addr_list = flow_ip_service_list_addresses (ip_service);
  ip_addr = g_list_nth_data (ip_addr_list, addr_index);

  if (ip_addr)
    result = flow_ip_addr_get_sockaddr (ip_addr, dest_sa, flow_ip_service_get_port (ip_service));
  else
    result = flow_sockaddr_init (dest_sa, FLOW_IP_ADDR_IPV4, NULL, flow_ip_service_get_port (ip_service));

  free_object_list (ip_addr_list);
  return result;
}

gboolean
flow_ip_service_set_sockaddr (FlowIPService *ip_service, FlowSockaddr *sa)
{
  FlowIPAddr *ip_addr;
  GList      *ip_addr_list;
  GList      *l;

  ip_addr = flow_ip_addr_new ();

  if (!flow_ip_addr_set_sockaddr (ip_addr, sa))
  {
    g_object_unref (ip_addr);
    return FALSE;
  }

  /* Remove previous addresses */

  ip_addr_list = flow_ip_service_list_addresses (ip_service);

  for (l = ip_addr_list; l; l = g_list_next (l))
  {
    flow_ip_service_remove_address (ip_service, l->data);
  }

  free_object_list (ip_addr_list);

  /* Add assigned address and port */

  flow_ip_service_add_address (ip_service, ip_addr);
  flow_ip_service_set_port (ip_service, flow_sockaddr_get_port (sa));

  g_object_unref (ip_addr);
  return TRUE;
}

void
flow_close_file_fd (gint fd)
{
  gint result;

  while (close (fd) != 0)
  {
    result = errno;

    g_assert (result != EBADF);

    if (result == EINTR)
      continue;
  }
}

void
flow_close_pipe_fd (gint fd)
{
  gint result;

  while (close (fd) != 0)
  {
    result = errno;

    g_assert (result != EBADF);

    if (result == EINTR)
      continue;
  }
}

void
flow_close_socket_fd (gint fd)
{
  gint result;

#ifdef G_PLATFORM_WIN32

  while (closesocket (fd) != 0)
  {
    result = WSAGetLastError ();

    g_assert (result != WSANOTINITIALISED);
    g_assert (result != WSAENOTSOCK);

    if (result == WSAEINTR)
      continue;

    if (result != WSAEINPROGRESS &&
        result != WSAEWOULDBLOCK)
      break;

    /* Since SO_DONTLINGER is default on Windows, we shouldn't get here
     * ever. It should return success and close the socket in the background. */
  }

#else

  while (close (fd) != 0)
  {
    result = errno;

    g_assert (result != EBADF);

    if (result == EINTR)
      continue;

    /* Unix should close the socket in the background too, so we
     * shouldn't get here. */
  }

#endif
}
