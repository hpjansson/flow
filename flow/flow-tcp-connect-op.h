/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tcp-connect-op.h - Operation: Connect to remote TCP port.
 *
 * Copyright (C) 2007 Hans Petter Jansson
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

#ifndef _FLOW_TCP_CONNECT_OP_H
#define _FLOW_TCP_CONNECT_OP_H

#include <flow/flow-event.h>
#include <flow/flow-ip-service.h>

#define FLOW_TYPE_TCP_CONNECT_OP            (flow_tcp_connect_op_get_type ())
#define FLOW_TCP_CONNECT_OP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_TCP_CONNECT_OP, FlowTcpConnectOp))
#define FLOW_TCP_CONNECT_OP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_TCP_CONNECT_OP, FlowTcpConnectOpClass))
#define FLOW_IS_TCP_CONNECT_OP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_TCP_CONNECT_OP))
#define FLOW_IS_TCP_CONNECT_OP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_TCP_CONNECT_OP))
#define FLOW_TCP_CONNECT_OP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_TCP_CONNECT_OP, FlowTcpConnectOpClass))
GType   flow_tcp_connect_op_get_type        (void) G_GNUC_CONST;

typedef struct _FlowTcpConnectOp        FlowTcpConnectOp;
typedef struct _FlowTcpConnectOpPrivate FlowTcpConnectOpPrivate;
typedef struct _FlowTcpConnectOpClass   FlowTcpConnectOpClass;

struct _FlowTcpConnectOp
{
  FlowEvent   parent;

  /*< private >*/

  FlowTcpConnectOpPrivate *priv;
};

struct _FlowTcpConnectOpClass
{
  FlowEventClass parent_class;

  /*< private >*/

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowTcpConnectOp *flow_tcp_connect_op_new                (FlowIPService *remote_ip_service, gint local_port);

FlowIPService    *flow_tcp_connect_op_get_remote_service (FlowTcpConnectOp *tcp_connect_op);
gint              flow_tcp_connect_op_get_local_port     (FlowTcpConnectOp *tcp_connect_op);

#endif /* _FLOW_TCP_CONNECT_OP_H */
