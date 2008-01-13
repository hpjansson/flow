/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-common-impl-unix.h - Utilities for Unix-like implementation.
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

/* This header file is internal to the library. It will not be installed. */

#ifndef _FLOW_SOCKET_UTIL_H
#define _FLOW_SOCKET_UTIL_H

#ifndef G_PLATFORM_WIN32
# include <arpa/inet.h>
#else
# include <winsock2.h>

/* Win32 compatibility; MSVC stuff */

# undef mode_t
# undef R_OK
# undef W_OK
# undef X_OK
# undef F_OK
# undef S_IRUSR
# undef S_IWUSR
# undef S_IXUSR
# undef S_IRWXU
# undef S_ISDIR

# define mode_t short
# define R_OK 4
# define W_OK 2
# define X_OK 0
# define F_OK 0
# define S_IRUSR _S_IREAD
# define S_IWUSR _S_IWRITE
# define S_IXUSR _S_IEXEC
# define S_IRWXU (S_IRUSR | S_IWUSR | S_IXUSR)

# define strcasecmp  stricmp
# define strncasecmp strnicmp

# define S_ISDIR(mode) ((mode) & _S_IFDIR ? TRUE : FALSE)
# define S_ISREG(mode) ((mode) & _S_IFREG ? TRUE : FALSE)

# define open  _open
# define close _close
# define read  _read
# define write _write
# define fstat _fstat
# define lseek _lseek
# define rmdir _rmdir
# define stat  _stat

#endif

#include <glib.h>
#include <flow-ip-service.h>

#define FLOW_WAKEUP_PIPE_INVALID { { -1, -1 } }

typedef enum
{
  FLOW_ADDR_FAMILY_INVALID,
  FLOW_ADDR_FAMILY_IPV4,
  FLOW_ADDR_FAMILY_IPV6,
  FLOW_ADDR_FAMILY_UNIX
}
FlowAddrFamily;

typedef struct
{
  gchar storage [128];
}
FlowSockaddr;

typedef struct
{
  gint fd [2];
}
FlowWakeupPipe;

gboolean          flow_init_sockets              (void) G_GNUC_INTERNAL;

gint              flow_addr_family_to_af         (FlowAddrFamily family) G_GNUC_INTERNAL;
FlowAddrFamily    flow_addr_family_from_af       (gint af) G_GNUC_INTERNAL;

gboolean          flow_sockaddr_init             (FlowSockaddr *dest_sa, FlowAddrFamily type,
                                                  const gchar *addr_str, gint port) G_GNUC_INTERNAL;

gchar            *flow_sockaddr_to_string        (FlowSockaddr *sa) G_GNUC_INTERNAL;

FlowAddrFamily    flow_sockaddr_get_family       (FlowSockaddr *sa) G_GNUC_INTERNAL;
gint              flow_sockaddr_get_len          (FlowSockaddr *sa) G_GNUC_INTERNAL;
gpointer          flow_sockaddr_get_addr         (FlowSockaddr *sa) G_GNUC_INTERNAL;
gint              flow_sockaddr_get_addr_len     (FlowSockaddr *sa) G_GNUC_INTERNAL;
gint              flow_sockaddr_get_port         (FlowSockaddr *sa) G_GNUC_INTERNAL;

gboolean          flow_socket_set_nonblock       (gint fd, gboolean nonblock) G_GNUC_INTERNAL;
gboolean          flow_pipe_set_nonblock         (gint fd, gboolean nonblock) G_GNUC_INTERNAL;

gboolean          flow_wakeup_pipe_init          (FlowWakeupPipe *wakeup_pipe) G_GNUC_INTERNAL;
void              flow_wakeup_pipe_destroy       (FlowWakeupPipe *wakeup_pipe) G_GNUC_INTERNAL;
gint              flow_wakeup_pipe_get_watch_fd  (FlowWakeupPipe *wakeup_pipe) G_GNUC_INTERNAL;
void              flow_wakeup_pipe_wakeup        (FlowWakeupPipe *wakeup_pipe) G_GNUC_INTERNAL;
void              flow_wakeup_pipe_handle_wakeup (FlowWakeupPipe *wakeup_pipe) G_GNUC_INTERNAL;
gboolean          flow_wakeup_pipe_is_valid      (FlowWakeupPipe *wakeup_pipe) G_GNUC_INTERNAL;

gboolean          flow_ip_addr_get_sockaddr      (FlowIPAddr *ip_addr, FlowSockaddr *dest_sa, gint port) G_GNUC_INTERNAL;
gboolean          flow_ip_addr_set_sockaddr      (FlowIPAddr *ip_addr, FlowSockaddr *sa) G_GNUC_INTERNAL;

gboolean          flow_ip_service_get_sockaddr   (FlowIPService *ip_service, FlowSockaddr *dest_sa, gint addr_index) G_GNUC_INTERNAL;
gboolean          flow_ip_service_set_sockaddr   (FlowIPService *ip_service, FlowSockaddr *sa) G_GNUC_INTERNAL;

void              flow_close_socket_fd           (gint fd) G_GNUC_INTERNAL;
void              flow_close_file_fd             (gint fd) G_GNUC_INTERNAL;
void              flow_close_pipe_fd             (gint fd) G_GNUC_INTERNAL;

#endif  /* _FLOW_SOCKET_UTIL_H */
