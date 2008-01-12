/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-network-util.h - Networking utilities.
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

#ifndef _FLOW_NETWORK_UTIL_H
#define _FLOW_NETWORK_UTIL_H

#include <flow/flow-ip-addr.h>

G_BEGIN_DECLS

GList            *flow_get_network_interfaces   (void);
FlowIPAddr       *flow_get_network_interface_to (FlowIPAddr *ip_addr);
FlowIPAddr       *flow_get_internet_interface   (void);

G_END_DECLS

#endif  /* _FLOW_UTIL_H */
