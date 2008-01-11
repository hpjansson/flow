/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-addr-impl-unix.c - Internet address, Unix-like implementation.
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
# include <ifaddrs.h>
#else
# include <io.h>
# include <winsock2.h>
# include <ws2tcpip.h>
# include <time.h>
#endif

#include "flow-gobject-util.h"
#include "flow-ip-addr.h"

#include "flow-common-impl-unix.h"

static FlowIPAddr *
flow_ip_addr_impl_get_interface_to (FlowIPAddr *ip_addr)
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

#ifndef G_PLATFORM_WIN32  /* Unix-specific code follows */

static GList *
flow_ip_addr_impl_get_interfaces (void)
{
  GList         *interface_list = NULL;
  gint           len, lastlen;
  gchar         *buf, *p0;
  gint           fd;
  struct ifconf  ifc;
  struct ifreq  *ifr;

  flow_init_sockets ();

  /* TODO: IPv6 method */

  fd = socket (AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return NULL;

  len     = 16 * sizeof (struct ifreq);
  lastlen = 0;

  for (;;)
  {
    buf = g_new0 (gchar, len);

    ifc.ifc_len = len;
    ifc.ifc_buf = buf;

    if (ioctl (fd, SIOCGIFCONF, &ifc) < 0)
    {
      /* Might have failed because our buffer was too small */
      if (errno != EINVAL || lastlen != 0)
      {
        g_free (buf);
        return NULL;
      }
    }
    else
    {
      /* Break if we got all the interfaces */
      if (ifc.ifc_len == lastlen)
        break;

      lastlen = ifc.ifc_len;
    }

    /* Did not allocate big enough buffer - try again */
    len += 8 * sizeof (struct ifreq);
    g_free (buf);
  }

  for (p0 = buf; p0 < (buf + ifc.ifc_len); p0 += sizeof (struct ifreq))
  {
    FlowSockaddr  addr;
    int           rv;
    FlowIPAddr   *ip_addr;

    ifr = (struct ifreq*) p0;

    /* Ignore non-AF_INET */
    if (ifr->ifr_addr.sa_family != AF_INET &&
        ifr->ifr_addr.sa_family != AF_INET6)
      continue;

    /* Save the address - the next call will clobber it */
    memcpy (&addr, &ifr->ifr_addr, sizeof(addr));

    /* Get the flags */
    rv = ioctl (fd, SIOCGIFFLAGS, ifr);
    if (rv == -1)
      continue;

    /* Ignore loopback and entries that aren't up */
    if (!(ifr->ifr_flags & IFF_UP) ||
        (ifr->ifr_flags & IFF_LOOPBACK))
      continue;

    /* Create a FlowIPAddr for this one and add it to our list */
    ip_addr = flow_ip_addr_new ();
    flow_ip_addr_set_sockaddr (ip_addr, &addr);
    interface_list = g_list_prepend (interface_list, ip_addr);
  }

  g_free (buf);

  interface_list = g_list_reverse (interface_list);
  return interface_list;
}

#else  /* Win32-specific code follows */

static GList *
flow_ip_addr_impl_get_interfaces (void)
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
