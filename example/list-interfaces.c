/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* list-interfaces.c - List active IP interfaces on system.
 *
 * Copyright (C) 2008 Hans Petter Jansson
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
#include <flow/flow.h>

static void
print_internet_interface (FlowIPAddrFamily family)
{
  FlowIPAddr *internet_interface;

  flow_set_preferred_ip_addr_family (family);

  internet_interface = flow_get_internet_interface ();

  if (internet_interface)
  {
    gchar *name = flow_ip_addr_get_string (internet_interface);
    g_print ("%s is the Internet interface ", name);
    g_free (name);
  }
  else
  {
    g_print ("No Internet interface ");
  }

  if (family == FLOW_IP_ADDR_IPV4)
    g_print ("(IPv4 preference).\n");
  else
    g_print ("(IPv6 preference).\n");

  if (internet_interface)
    g_object_unref (internet_interface);
}

gint
main (gint argc, gchar *argv [])
{
  GList *interface_list;
  GList *l;

  /* List active interfaces */

  interface_list = flow_get_network_interfaces ();

  if (!interface_list)
    g_print ("No interfaces found.\n");

  for (l = interface_list; l; l = g_list_next (l))
  {
    FlowIPAddr *this_interface = l->data;
    gchar      *name;

    name = flow_ip_addr_get_string (this_interface);

    g_print ("%s\n", name);

    g_free (name);
    g_object_unref (this_interface);
  }

  g_list_free (interface_list);

  /* List Internet interfaces */

  g_print ("---\n");

  print_internet_interface (FLOW_IP_ADDR_IPV4);
  print_internet_interface (FLOW_IP_ADDR_IPV6);

  return 0;
}
