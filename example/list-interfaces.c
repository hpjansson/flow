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

gint
main (gint argc, gchar *argv [])
{
  GList      *interface_list;
  FlowIPAddr *internet_interface;
  GList      *l;

  g_type_init ();

  internet_interface = flow_ip_addr_get_internet_interface ();
  interface_list = flow_ip_addr_get_interfaces ();

  if (!interface_list)
    g_print ("No interfaces found.\n");

  for (l = interface_list; l; l = g_list_next (l))
  {
    FlowIPAddr       *this_interface = l->data;
    gchar            *name;

    name = flow_ip_addr_get_string (this_interface);

    g_print ("%s\n", name);

    g_free (name);
    g_object_unref (this_interface);
  }

  if (internet_interface)
  {
    gchar *name = flow_ip_addr_get_string (internet_interface);
    g_print ("---\n%s is the Internet interface.\n", name);
    g_free (name);
  }
  else
    g_print ("---\nNo Internet interface.\n");

  g_list_free (interface_list);
  if (internet_interface)
    g_object_unref (internet_interface);

  return 0;
}
