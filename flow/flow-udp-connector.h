/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-udp-connector.h - Origin/endpoint for UDP transport.
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

#ifndef _FLOW_UDP_CONNECTOR_H
#define _FLOW_UDP_CONNECTOR_H

#include <glib-object.h>
#include <flow-connector.h>
#include <flow-shunt.h>

G_BEGIN_DECLS

#define FLOW_TYPE_UDP_CONNECTOR            (flow_udp_connector_get_type ())
#define FLOW_UDP_CONNECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_UDP_CONNECTOR, FlowUdpConnector))
#define FLOW_UDP_CONNECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_UDP_CONNECTOR, FlowUdpConnectorClass))
#define FLOW_IS_UDP_CONNECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_UDP_CONNECTOR))
#define FLOW_IS_UDP_CONNECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_UDP_CONNECTOR))
#define FLOW_UDP_CONNECTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_UDP_CONNECTOR, FlowUdpConnectorClass))
GType   flow_udp_connector_get_type        (void) G_GNUC_CONST;

typedef struct _FlowUdpConnector      FlowUdpConnector;
typedef struct _FlowUdpConnectorClass FlowUdpConnectorClass;

struct _FlowUdpConnector
{
  FlowConnector    parent;

  /* --- Private --- */

  gpointer         priv;
};

struct _FlowUdpConnectorClass
{
  FlowConnectorClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowUdpConnector *flow_udp_connector_new                (void);

FlowIPService    *flow_udp_connector_get_local_service  (FlowUdpConnector *udp_connector);
FlowIPService    *flow_udp_connector_get_remote_service (FlowUdpConnector *udp_connector);

G_END_DECLS

#endif  /* _FLOW_UDP_CONNECTOR_H */
