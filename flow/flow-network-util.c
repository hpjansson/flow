/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-network-util.c - Networking utilities.
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
#include "flow-network-util.h"

/* --- Implementation prototypes --- */

static FlowIPAddr *flow_impl_get_network_interface_to (FlowIPAddr *ip_addr);
static GList      *flow_impl_get_network_interfaces   (void);

/* Select an implementation */
#include "flow-network-util-impl-unix.c"

GList *
flow_get_network_interfaces (void)
{
  return flow_impl_get_network_interfaces ();
}

FlowIPAddr *
flow_get_network_interface_to (FlowIPAddr *ip_addr)
{
  g_return_val_if_fail (FLOW_IS_IP_ADDR (ip_addr), NULL);

  return flow_impl_get_network_interface_to (ip_addr);
}

FlowIPAddr *
flow_get_internet_interface (void)
{
  FlowIPAddr *ip_addr;
  FlowIPAddr *if_addr;

  /* Any Internet address will do. We use fix.no's address. */

  ip_addr = flow_ip_addr_new ();
  flow_ip_addr_set_string (ip_addr, "212.71.72.21");

  if_addr = flow_get_network_interface_to (ip_addr);

  g_object_unref (ip_addr);
  return if_addr;
}
