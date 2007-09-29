/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tcp-connect-op.c - Operation: Connect to remote TCP port.
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-event.h"
#include "flow-tcp-connect-op.h"

/* --- FlowTcpConnectOp private data --- */

typedef struct
{
  FlowIPService *remote_ip_service;
  gint           local_port;
}
FlowTcpConnectOpPrivate;

/* --- FlowTcpConnectOp properties --- */

static FlowIPService *
flow_tcp_connect_op_get_remote_ip_service_internal (FlowTcpConnectOp *tcp_connect_op)
{
  FlowTcpConnectOpPrivate *priv = tcp_connect_op->priv;

  return priv->remote_ip_service;
}

static void
flow_tcp_connect_op_set_remote_ip_service_internal (FlowTcpConnectOp *tcp_connect_op, FlowIPService *remote_ip_service)
{
  FlowTcpConnectOpPrivate *priv = tcp_connect_op->priv;

  if (remote_ip_service)
    g_object_ref (remote_ip_service);

  if (priv->remote_ip_service)
    g_object_unref (priv->remote_ip_service);

  priv->remote_ip_service = remote_ip_service;
}

static gint
flow_tcp_connect_op_get_local_port_internal (FlowTcpConnectOp *tcp_connect_op)
{
  FlowTcpConnectOpPrivate *priv = tcp_connect_op->priv;

  return priv->local_port;
}

static void
flow_tcp_connect_op_set_local_port_internal (FlowTcpConnectOp *tcp_connect_op, gint local_port)
{
  FlowTcpConnectOpPrivate *priv = tcp_connect_op->priv;

  priv->local_port = local_port;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_tcp_connect_op)
FLOW_GOBJECT_PROPERTY         (G_TYPE_OBJECT,
                               "remote-ip-service", "Remote IP Service", "Remote IP service to connect to",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_tcp_connect_op_get_remote_ip_service_internal,
                               flow_tcp_connect_op_set_remote_ip_service_internal,
                               NULL)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_INT, "local-port", "Local Port", "Local port to bind to",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_tcp_connect_op_get_local_port_internal,
                               flow_tcp_connect_op_set_local_port_internal,
                               -1, G_MAXINT, -1)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowTcpConnectOp definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_tcp_connect_op, FlowTcpConnectOp, FLOW_TYPE_EVENT, 0)

/* --- FlowTcpConnectOp implementation --- */

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
flow_tcp_connect_op_update_description (FlowTcpConnectOp *tcp_connect_op)
{
  FlowTcpConnectOpPrivate *priv                     = tcp_connect_op->priv;
  FlowEvent               *event                    = FLOW_EVENT (tcp_connect_op);
  gboolean                 must_free_local_port_str = FALSE;
  gchar                   *ip_list_str              = NULL;
  gchar                   *local_port_str;
  GList                   *address_list;
  GList                   *l;

  if (event->description)
    return;

  /* Source port */

  if (priv->local_port >= 0)
  {
    local_port_str = g_strdup_printf ("from local port %d", priv->local_port);
    must_free_local_port_str = TRUE;
  }
  else
  {
    local_port_str = "from any local port";
  }

  /* Target address list */

  address_list = flow_ip_service_list_addresses (priv->remote_ip_service);

  for (l = address_list; l; l = g_list_next (l))
  {
    FlowIPAddr *addr = l->data;
    ip_list_str = append_ip_str (ip_list_str, addr);
  }

  flow_unref_and_free_object_list (address_list);

  /* Bring it all together */

  event->description = g_strdup_printf ("Connect to IP %s port %d",
                                        ip_list_str,
                                        flow_ip_service_get_port (priv->remote_ip_service));

  g_free (ip_list_str);
  if (must_free_local_port_str)
    g_free (local_port_str);
}

static void
flow_tcp_connect_op_type_init (GType type)
{
}

static void
flow_tcp_connect_op_class_init (FlowTcpConnectOpClass *klass)
{
  FlowEventClass *event_klass = FLOW_EVENT_CLASS (klass);

  event_klass->update_description = (void (*) (FlowEvent *)) flow_tcp_connect_op_update_description;
}

static void
flow_tcp_connect_op_init (FlowTcpConnectOp *tcp_connect_op)
{
}

static void
flow_tcp_connect_op_construct (FlowTcpConnectOp *tcp_connect_op)
{
}

static void
flow_tcp_connect_op_dispose (FlowTcpConnectOp *tcp_connect_op)
{
}

static void
flow_tcp_connect_op_finalize (FlowTcpConnectOp *tcp_connect_op)
{
  FlowTcpConnectOpPrivate *priv = tcp_connect_op->priv;

  if (priv->remote_ip_service)
  {
    g_object_unref (priv->remote_ip_service);
    priv->remote_ip_service = NULL;
  }
}

/* --- FlowTcpConnectOp public API --- */

FlowTcpConnectOp *
flow_tcp_connect_op_new (FlowIPService *remote_ip_service, gint local_port)
{
  g_return_val_if_fail (FLOW_IS_IP_SERVICE (remote_ip_service), NULL);

  return g_object_new (FLOW_TYPE_TCP_CONNECT_OP,
                       "remove-ip-service", remote_ip_service,
                       "local-port", local_port,
                       NULL);
}
