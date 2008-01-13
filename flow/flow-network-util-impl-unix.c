/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-network-util-impl-unix.c - Networking utils, Unix-like implementation.
 *
 * Copyright (C) 2006-2008 Hans Petter Jansson
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

/* This file is not compiled on its own. It is #included in flow-network-util.c. */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <glib.h>

#ifndef G_PLATFORM_WIN32
# include <unistd.h>
# include <sys/time.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <ifaddrs.h>  /* Include after net/if.h */
#else
# include <io.h>
# include <winsock2.h>
# include <ws2tcpip.h>
# include <time.h>
#endif

#include "flow-common-impl-unix.h"

#ifndef G_PLATFORM_WIN32  /* Unix-specific code follows */

static GList *
flow_impl_get_network_interfaces (void)
{
  GList          *interface_list = NULL;
  struct ifaddrs *if_addrs;
  struct ifaddrs *if_addr_cur;

  flow_init_sockets ();

  if (getifaddrs (&if_addrs) < 0)
    return NULL;

  for (if_addr_cur = if_addrs; if_addr_cur; if_addr_cur = if_addr_cur->ifa_next)
  {
    FlowIPAddr *ip_addr;

    /* Ignore non-IP */
    if (if_addr_cur->ifa_addr->sa_family != AF_INET &&
        if_addr_cur->ifa_addr->sa_family != AF_INET6)
      continue;

    /* Ignore loopback and entries that aren't up */
    if (!(if_addr_cur->ifa_flags & IFF_UP) ||
        (if_addr_cur->ifa_flags & IFF_LOOPBACK))
      continue;

    /* Create a FlowIPAddr for this one and add it to our list */
    ip_addr = flow_ip_addr_new ();
    flow_ip_addr_set_sockaddr (ip_addr, (FlowSockaddr *) if_addr_cur->ifa_addr);
    interface_list = g_list_prepend (interface_list, ip_addr);
  }

  freeifaddrs (if_addrs);

  interface_list = g_list_reverse (interface_list);
  return interface_list;
}

#else  /* Win32-specific code follows */

static GList *
flow_impl_get_network_interfaces (void)
{
  GList                  *interface_list = NULL;
  SOCKET                  s;
  DWORD                   bytesReturned;
  SOCKADDR_IN            *pAddrInet;
  u_long                  SetFlags;
  INTERFACE_INFO          localAddr [10];  /* No more than 10 IP interfaces */
  struct sockaddr_in      addr;
  gint                    numLocalAddr; 
  gint                    wsError;
  gint                    i;

  flow_init_sockets ();

  if ((s = WSASocket (AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0)) == INVALID_SOCKET)
    return NULL;

  /* Enumerate all IP interfaces */

  wsError = WSAIoctl (s, SIO_GET_INTERFACE_LIST, NULL, 0, &localAddr,
                      sizeof (localAddr), &bytesReturned, NULL, NULL);
  if (wsError == SOCKET_ERROR)
  {
    flow_close_socket_fd (s);
    return NULL;
  }

  flow_close_socket_fd (s);

  /* Display interface information */

  numLocalAddr = (bytesReturned / sizeof (INTERFACE_INFO));
  for (i = 0; i < numLocalAddr; i++)
  {
    pAddrInet = (SOCKADDR_IN *) &localAddr [i].iiAddress;
    memcpy (&addr, pAddrInet, sizeof (addr));

    SetFlags = localAddr [i].iiFlags;
    if ((SetFlags & IFF_UP) || (SetFlags & IFF_LOOPBACK))
    {
      FlowIPAddr *ip_addr;

      /* Create a FlowIPAddr for this one and add it to our list */
      ip_addr = flow_ip_addr_new ();
      flow_ip_addr_set_sockaddr (ip_addr, (FlowSockaddr *) &addr);
      interface_list = g_list_prepend (interface_list, ip_addr);
    }
  }

  return interface_list;
}

#endif

static FlowIPAddr *
flow_impl_get_network_interface_to (FlowIPAddr *ip_addr)
{
  gint          fd = -1;
  FlowSockaddr  remote_sa;
  FlowSockaddr  interface_sa;
  guint         interface_sa_len = sizeof (interface_sa);  /* socklen_t */
  FlowIPAddr   *interface_addr;

  flow_init_sockets ();

#ifdef HAVE_IPV6
  fd = socket (ip_addr->family == FLOW_IP_ADDR_IPV4 ? AF_INET : AF_INET6,
               SOCK_DGRAM, 0);
#else
  fd = socket (AF_INET, SOCK_DGRAM, 0);
#endif

  if (fd < 0)
    goto error;

  flow_ip_addr_get_sockaddr (ip_addr, &remote_sa, 0);

  if (connect (fd, (struct sockaddr *) &remote_sa, flow_sockaddr_get_len (&remote_sa)) < 0)
    goto error;

  if (getsockname (fd, (struct sockaddr *) &interface_sa, &interface_sa_len) != 0)
    goto error;

  interface_addr = flow_ip_addr_new ();
  flow_ip_addr_set_sockaddr (interface_addr, (FlowSockaddr *) &interface_sa);

  flow_close_socket_fd (fd);
  return interface_addr;

error:
  if (fd >= 0)
    flow_close_socket_fd (fd);

  return NULL;
}
