/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-udp-connect-op.h - Operation: Associate with remote UDP port.
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

#ifndef _FLOW_UDP_CONNECT_OP_H
#define _FLOW_UDP_CONNECT_OP_H

#include <flow/flow-event.h>
#include <flow/flow-ip-service.h>

#define FLOW_TYPE_UDP_CONNECT_OP            (flow_udp_connect_op_get_type ())
#define FLOW_UDP_CONNECT_OP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_UDP_CONNECT_OP, FlowUdpConnectOp))
#define FLOW_UDP_CONNECT_OP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_UDP_CONNECT_OP, FlowUdpConnectOpClass))
#define FLOW_IS_UDP_CONNECT_OP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_UDP_CONNECT_OP))
#define FLOW_IS_UDP_CONNECT_OP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_UDP_CONNECT_OP))
#define FLOW_UDP_CONNECT_OP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_UDP_CONNECT_OP, FlowUdpConnectOpClass))
GType   flow_udp_connect_op_get_type        (void) G_GNUC_CONST;

typedef struct _FlowUdpConnectOp      FlowUdpConnectOp;
typedef struct _FlowUdpConnectOpClass FlowUdpConnectOpClass;

struct _FlowUdpConnectOp
{
  FlowEvent   parent;

  /* --- Private --- */

  gpointer    priv;
};

struct _FlowUdpConnectOpClass
{
  FlowEventClass parent_class;

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowUdpConnectOp *flow_udp_connect_op_new                (FlowIPService *remote_ip_service, gint local_port);

FlowIPService    *flow_udp_connect_op_get_remote_service (FlowUdpConnectOp *udp_connect_op);
gint              flow_udp_connect_op_get_local_port     (FlowUdpConnectOp *udp_connect_op);

#endif /* _FLOW_UDP_CONNECT_OP_H */
