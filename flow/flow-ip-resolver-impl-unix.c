/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-resolver-impl-unix.c - DNS lookup implementation for Unix-likes.
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

/* This file is not compiled by itself. It is #included in flow-ip-resolver.c */

#ifndef G_PLATFORM_WIN32
# include <unistd.h>
# include <sys/socket.h>
# include <netdb.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#else
# include <io.h>
# include <winsock2.h>
#endif

#include "flow-event-codes.h"
#include "flow-common-impl-unix.h"

#ifdef HAVE_GETADDRINFO

static GError *
eai_to_gerror (gint eai)
{
  gint event_code = FLOW_LOOKUP_NO_RECORDS;

  switch (eai)
  {
    case EAI_AGAIN:
      event_code = FLOW_LOOKUP_TEMPORARY_SERVER_FAILURE;
      break;

    case EAI_FAIL:
      event_code = FLOW_LOOKUP_PERMANENT_SERVER_FAILURE;
      break;

    case EAI_BADFLAGS:
    case EAI_FAMILY:
    case EAI_SOCKTYPE:
      g_assert_not_reached ();
      break;

    default:
      break;
  }

  return g_error_new_literal (FLOW_LOOKUP_DOMAIN_QUARK, event_code, gai_strerror (eai));
}

static GList *
flow_ip_resolver_impl_lookup_by_name (const gchar *name, GError **error)
{
  struct addrinfo  hints;
  struct addrinfo *res       = NULL;
  GList           *addr_list = NULL;
  gint             attempts  = 0;
  gint             rv;

  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family   = AF_UNSPEC;
  hints.ai_flags    = AI_ADDRCONFIG;

  do
  {
    rv = getaddrinfo (name, NULL, &hints, &res);
    attempts++;
  }
  while (rv == EAI_AGAIN && attempts < MAX_LOOKUP_ATTEMPTS);

  if (rv == 0)
  {
    struct addrinfo *i;

    for (i = res; i != NULL; i = i->ai_next)
    {
      FlowIPAddr     *ip_addr;
      FlowAddrFamily  family;

      family = flow_addr_family_from_af (i->ai_family);

      if (family != FLOW_ADDR_FAMILY_IPV4 &&
          family != FLOW_ADDR_FAMILY_IPV6)
      {
        DEBUG (g_print (G_STRLOC ": [%s] Got non-IP addr.\n", name));
        continue;
      }

      ip_addr = flow_ip_addr_new ();

      if (!flow_ip_addr_set_sockaddr (ip_addr, (FlowSockaddr *) i->ai_addr))
      {
        DEBUG (g_print (G_STRLOC ": [%s] Couldn't set sockaddr.\n", name));
        g_object_unref (ip_addr);
        continue;
      }

      addr_list = g_list_prepend (addr_list, ip_addr);
    }
  }
  else
  {
    if (error)
      *error = eai_to_gerror (rv);

    DEBUG (g_print (G_STRLOC ": [%s] Lookup returned error %d - %s.\n", name, rv, gai_strerror (rv)));
  }

  if (res)
    freeaddrinfo (res);

  return addr_list;
}

static GList *
flow_ip_resolver_impl_lookup_by_addr (FlowIPAddr *ip_addr, GError **error)
{
  FlowSockaddr     sa;
  gchar            name [NI_MAXHOST];
  gint             attempts = 0;
  gint             rv;

  if (!flow_ip_addr_get_sockaddr (ip_addr, &sa, 0))
    return NULL;

  do
  {
    rv = getnameinfo ((struct sockaddr *) &sa, flow_sockaddr_get_len (&sa),
                      name, sizeof (name),
                      NULL, 0, NI_NAMEREQD);
    attempts++;
  }
  while (rv == EAI_AGAIN && attempts < MAX_LOOKUP_ATTEMPTS);

  if (rv != 0)
  {
    if (error)
      *error = eai_to_gerror (rv);

    DEBUG (g_print (G_STRLOC ": [%s] Lookup returned error %d - %s.\n",
                    flow_ip_addr_get_string (ip_addr), rv, gai_strerror (rv)));
  }

  if (rv != 0)
    return NULL;

  return g_list_prepend (NULL, g_strdup (name));
}

#else

static GError *
h_errno_to_gerror (gint h_errcode)
{
  gint event_code = FLOW_LOOKUP_NO_RECORDS;

  switch (h_errcode)
  {
    case TRY_AGAIN:
      event_code = FLOW_LOOKUP_TEMPORARY_SERVER_FAILURE;
      break;

    case NO_RECOVERY:
      event_code = FLOW_LOOKUP_PERMANENT_SERVER_FAILURE;
      break;

    default:
      break;
  }

  return g_error_new_literal (FLOW_LOOKUP_DOMAIN_QUARK, event_code, hstrerror (h_errcode));
}

static GList *
flow_ip_resolver_impl_lookup_by_name (const gchar *name)
{
  static GStaticMutex  mutex = G_STATIC_MUTEX_INIT;
  GList               *addr_list = NULL;
  struct hostent      *he;
  gint                 i;

  g_static_mutex_lock (&mutex);

  DEBUG (g_print ("Looking up '%s'\n", name));

  he = gethostbyname (name);

  DEBUG (g_print ("Looked up '%s'\n", name));

  if (!he || !flow_addr_family_is_ipv4 (he->h_addrtype))
  {
    g_static_mutex_unlock (&mutex);
    DEBUG (g_print ("No IPv4 hostent!\n"));
    return NULL;
  }

  for (i = 0; he->h_addr_list [i]; i++)
  {
    FlowIPAddr         *ip_addr;
    struct sockaddr_in  sa;

    DEBUG (g_print ("Checking hostent [%d].\n", i));

    ((struct sockaddr *) &sa)->sa_family = he->h_addrtype;
    memcpy (flow_sockaddr_get_addr ((struct sockaddr *) &sa), he->h_addr_list [i], he->h_length);

    ip_addr = flow_ip_addr_new ();
    if (!flow_ip_addr_set_sockaddr (ip_addr, (struct sockaddr *) &sa))
    {
      DEBUG (g_print ("Could not set sockaddr!\n"));
      g_object_unref (ip_addr);
      continue;
    }

    DEBUG (g_print ("Got address: '%s'\n", flow_ip_addr_get_string (ip_addr)));

    addr_list = g_list_prepend (addr_list, ip_addr);
  }

  g_static_mutex_unlock (&mutex);

  return addr_list;
}

static GList *
flow_ip_resolver_impl_lookup_by_addr (FlowIPAddr *ip_addr)
{
  static GStaticMutex  mutex     = G_STATIC_MUTEX_INIT;
  GList               *name_list = NULL;
  struct hostent      *he;
  struct sockaddr_in  *sa;

  if (flow_ip_addr_get_family (ip_addr) != FLOW_IP_ADDR_IPV4)
  {
    DEBUG (g_print ("Supporting IPv4 only.\n"));
    return NULL;
  }

  sa = (struct sockaddr_in *) flow_ip_addr_get_sockaddr (ip_addr, 0);
  if (!sa)
  {
    DEBUG (g_print ("No SA.\n"));
    return NULL;
  }

  g_static_mutex_lock (&mutex);

  he = gethostbyaddr (flow_sockaddr_get_addr ((struct sockaddr *) sa),
                      flow_sockaddr_get_addr_len ((struct sockaddr *) sa),
                      flow_sockaddr_get_family ((struct sockaddr *) sa));

  g_free (sa);

  if (he && he->h_name)
  {
    name_list = g_list_prepend (name_list, g_strdup (he->h_name));
  }
  else
  {
    DEBUG (g_print ("No he.\n"));

    
  }

  g_static_mutex_unlock (&mutex);

  return name_list;
}

#endif
