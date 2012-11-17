/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ssh-connect-op.h - Operation: Connect to remote SSH server.
 *
 * Copyright (C) 2012 Hans Petter Jansson
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

#ifndef _FLOW_SSH_CONNECT_OP_H
#define _FLOW_SSH_CONNECT_OP_H

#include <flow/flow-event.h>
#include <flow/flow-ip-service.h>

#define FLOW_TYPE_SSH_CONNECT_OP            (flow_ssh_connect_op_get_type ())
#define FLOW_SSH_CONNECT_OP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_SSH_CONNECT_OP, FlowSshConnectOp))
#define FLOW_SSH_CONNECT_OP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_SSH_CONNECT_OP, FlowSshConnectOpClass))
#define FLOW_IS_SSH_CONNECT_OP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_SSH_CONNECT_OP))
#define FLOW_IS_SSH_CONNECT_OP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_SSH_CONNECT_OP))
#define FLOW_SSH_CONNECT_OP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_SSH_CONNECT_OP, FlowSshConnectOpClass))
GType   flow_ssh_connect_op_get_type        (void) G_GNUC_CONST;

typedef struct _FlowSshConnectOp        FlowSshConnectOp;
typedef struct _FlowSshConnectOpPrivate FlowSshConnectOpPrivate;
typedef struct _FlowSshConnectOpClass   FlowSshConnectOpClass;

struct _FlowSshConnectOp
{
  FlowEvent   parent;

  /*< private >*/

  FlowSshConnectOpPrivate *priv;
};

struct _FlowSshConnectOpClass
{
  FlowEventClass parent_class;

  /*< private >*/

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowSshConnectOp *flow_ssh_connect_op_new                (FlowIPService *remote_ip_service, const gchar *remote_user);

FlowIPService    *flow_ssh_connect_op_get_remote_service (FlowSshConnectOp *ssh_connect_op);
const gchar      *flow_ssh_connect_op_get_remote_user    (FlowSshConnectOp *ssh_connect_op);

#endif /* _FLOW_SSH_CONNECT_OP_H */
