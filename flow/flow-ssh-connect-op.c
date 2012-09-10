/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ssh-connect-op.c - Operation: Connect to remote SSH server.
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-event.h"
#include "flow-ssh-connect-op.h"

/* --- FlowSshConnectOp private data --- */

struct _FlowSshConnectOpPrivate
{
  FlowIPService *remote_ip_service;
};

/* --- FlowSshConnectOp properties --- */

static FlowIPService *
flow_ssh_connect_op_get_remote_ip_service_internal (FlowSshConnectOp *ssh_connect_op)
{
  FlowSshConnectOpPrivate *priv = ssh_connect_op->priv;

  return priv->remote_ip_service;
}

static void
flow_ssh_connect_op_set_remote_ip_service_internal (FlowSshConnectOp *ssh_connect_op, FlowIPService *remote_ip_service)
{
  FlowSshConnectOpPrivate *priv = ssh_connect_op->priv;

  if (remote_ip_service)
    g_object_ref (remote_ip_service);

  if (priv->remote_ip_service)
    g_object_unref (priv->remote_ip_service);

  priv->remote_ip_service = remote_ip_service;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ssh_connect_op)
FLOW_GOBJECT_PROPERTY         (G_TYPE_OBJECT,
                               "remote-ip-service", "Remote IP Service", "Remote IP service to connect to",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_ssh_connect_op_get_remote_ip_service_internal,
                               flow_ssh_connect_op_set_remote_ip_service_internal,
                               flow_ip_service_get_type)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowSshConnectOp definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_ssh_connect_op, FlowSshConnectOp, FLOW_TYPE_EVENT, 0)

/* --- FlowSshConnectOp implementation --- */

static gchar *
append_ip_str (gchar *orig, FlowIPAddr *addr)
{
  gchar *addr_str;
  gchar *new;

  addr_str = flow_ip_addr_get_string (addr);

  if (!orig)
    return addr_str;

  new = g_strjoin (" or ", orig, addr_str, NULL);

  g_free (orig);
  g_free (addr_str);
  return new;
}

static void
flow_ssh_connect_op_update_description (FlowSshConnectOp *ssh_connect_op)
{
  FlowSshConnectOpPrivate *priv                     = ssh_connect_op->priv;
  FlowEvent               *event                    = FLOW_EVENT (ssh_connect_op);
  gchar                   *ip_list_str              = NULL;
  GList                   *address_list;
  GList                   *l;

  if (event->description)
    return;

  /* Target address list */

  address_list = flow_ip_service_list_addresses (priv->remote_ip_service);

  for (l = address_list; l; l = g_list_next (l))
  {
    FlowIPAddr *addr = l->data;
    ip_list_str = append_ip_str (ip_list_str, addr);
  }

  flow_unref_and_free_object_list (address_list);

  /* Bring it all together */

  event->description = g_strdup_printf ("SSH Connect to IP %s port %d",
                                        ip_list_str,
                                        flow_ip_service_get_port (priv->remote_ip_service));

  g_free (ip_list_str);
}

static void
flow_ssh_connect_op_type_init (GType type)
{
}

static void
flow_ssh_connect_op_class_init (FlowSshConnectOpClass *klass)
{
  FlowEventClass *event_klass = FLOW_EVENT_CLASS (klass);

  event_klass->update_description = (void (*) (FlowEvent *)) flow_ssh_connect_op_update_description;
}

static void
flow_ssh_connect_op_init (FlowSshConnectOp *ssh_connect_op)
{
}

static void
flow_ssh_connect_op_construct (FlowSshConnectOp *ssh_connect_op)
{
}

static void
flow_ssh_connect_op_dispose (FlowSshConnectOp *ssh_connect_op)
{
}

static void
flow_ssh_connect_op_finalize (FlowSshConnectOp *ssh_connect_op)
{
  FlowSshConnectOpPrivate *priv = ssh_connect_op->priv;

  flow_gobject_unref_clear (priv->remote_ip_service);
}

/* --- FlowSshConnectOp public API --- */

FlowSshConnectOp *
flow_ssh_connect_op_new (FlowIPService *remote_ip_service)
{
  g_return_val_if_fail (FLOW_IS_IP_SERVICE (remote_ip_service), NULL);

  return g_object_new (FLOW_TYPE_SSH_CONNECT_OP,
                       "remote-ip-service", remote_ip_service,
                       NULL);
}

FlowIPService *
flow_ssh_connect_op_get_remote_service (FlowSshConnectOp *ssh_connect_op)
{
  FlowSshConnectOpPrivate *priv;

  g_return_val_if_fail (FLOW_IS_SSH_CONNECT_OP (ssh_connect_op), NULL);

  priv = ssh_connect_op->priv;
  return priv->remote_ip_service;
}
