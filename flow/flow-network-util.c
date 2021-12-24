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

static FlowIPAddrFamily preferred_ip_addr_family       = FLOW_IP_ADDR_IPV4;
static GMutex           preferred_ip_addr_family_mutex;

FlowIPAddrFamily
flow_get_preferred_ip_addr_family (void)
{
  FlowIPAddrFamily family;

  g_mutex_lock (&preferred_ip_addr_family_mutex);
  family = preferred_ip_addr_family;
  g_mutex_unlock (&preferred_ip_addr_family_mutex);

  return family;
}

void
flow_set_preferred_ip_addr_family (FlowIPAddrFamily family)
{
  g_mutex_lock (&preferred_ip_addr_family_mutex);
  preferred_ip_addr_family = family;
  g_mutex_unlock (&preferred_ip_addr_family_mutex);
}

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

static FlowIPAddr *
get_internet_interface (FlowIPAddr *scratch_addr, FlowIPAddrFamily family)
{
  if (family == FLOW_IP_ADDR_IPV4)
  {
    /* Any IPv4 Internet address will do. We use fix.no's address. */
    flow_ip_addr_set_string (scratch_addr, "212.71.72.21");
  }
  else
  {
    /* For IPv6, use an example address */
    flow_ip_addr_set_string (scratch_addr, "2001::");
  }

  return flow_get_network_interface_to (scratch_addr);
}

FlowIPAddr *
flow_get_internet_interface (void)
{
  FlowIPAddr       *ip_addr;
  FlowIPAddr       *if_addr;
  FlowIPAddrFamily  family;

  family = flow_get_preferred_ip_addr_family ();
  ip_addr = flow_ip_addr_new ();

  if_addr = get_internet_interface (ip_addr, family);
  if (!if_addr)
  {
    if_addr = get_internet_interface (ip_addr, family == FLOW_IP_ADDR_IPV4 ?
                                      FLOW_IP_ADDR_IPV6 : FLOW_IP_ADDR_IPV4);
  }

  g_object_unref (ip_addr);
  return if_addr;
}
