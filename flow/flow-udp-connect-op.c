/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-udp-connect-op.c - Operation: Associate with remote UDP port.
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

#include <stdlib.h>
#include <string.h>
#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-event.h"
#include "flow-udp-connect-op.h"

/* --- FlowUdpConnectOp private data --- */

struct _FlowUdpConnectOpPrivate
{
  FlowIPService *local_ip_service;
  FlowIPService *remote_ip_service;
};

/* --- FlowUdpConnectOp properties --- */

static FlowIPService *
flow_udp_connect_op_get_local_ip_service_internal (FlowUdpConnectOp *udp_connect_op)
{
  FlowUdpConnectOpPrivate *priv = udp_connect_op->priv;

  return priv->local_ip_service;
}

static void
flow_udp_connect_op_set_local_ip_service_internal (FlowUdpConnectOp *udp_connect_op, FlowIPService *local_ip_service)
{
  FlowUdpConnectOpPrivate *priv = udp_connect_op->priv;

  if (local_ip_service)
    g_object_ref (local_ip_service);

  if (priv->local_ip_service)
    g_object_unref (priv->local_ip_service);

  priv->local_ip_service = local_ip_service;
}

static FlowIPService *
flow_udp_connect_op_get_remote_ip_service_internal (FlowUdpConnectOp *udp_connect_op)
{
  FlowUdpConnectOpPrivate *priv = udp_connect_op->priv;

  return priv->remote_ip_service;
}

static void
flow_udp_connect_op_set_remote_ip_service_internal (FlowUdpConnectOp *udp_connect_op, FlowIPService *remote_ip_service)
{
  FlowUdpConnectOpPrivate *priv = udp_connect_op->priv;

  if (remote_ip_service)
    g_object_ref (remote_ip_service);

  if (priv->remote_ip_service)
    g_object_unref (priv->remote_ip_service);

  priv->remote_ip_service = remote_ip_service;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_udp_connect_op)
FLOW_GOBJECT_PROPERTY         (G_TYPE_OBJECT,
                               "local-ip-service", "Local IP Service", "Local IP service to send from",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_udp_connect_op_get_local_ip_service_internal,
                               flow_udp_connect_op_set_local_ip_service_internal,
                               flow_ip_service_get_type)
FLOW_GOBJECT_PROPERTY         (G_TYPE_OBJECT,
                               "remote-ip-service", "Remote IP Service", "Remote IP service to send to",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_udp_connect_op_get_remote_ip_service_internal,
                               flow_udp_connect_op_set_remote_ip_service_internal,
                               flow_ip_service_get_type)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowUdpConnectOp definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_udp_connect_op, FlowUdpConnectOp, FLOW_TYPE_EVENT, 0)

/* --- FlowUdpConnectOp implementation --- */

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

static gchar *
print_ip_service_description (FlowIPService *ip_service)
{
  gchar                   *ip_list_str = NULL;
  gchar                   *desc;
  GList                   *address_list;
  GList                   *l;

  if (!ip_service)
    return g_strdup ("any address");

  address_list = flow_ip_service_list_addresses (ip_service);

  for (l = address_list; l; l = g_list_next (l))
  {
    FlowIPAddr *addr = l->data;
    ip_list_str = append_ip_str (ip_list_str, addr);
  }

  flow_unref_and_free_object_list (address_list);

  if (ip_list_str)
    desc = g_strdup_printf ("IP %s port %d", ip_list_str, flow_ip_service_get_port (ip_service));
  else
    desc = g_strdup_printf ("any IP port %d", flow_ip_service_get_port (ip_service));

  return desc;
}

static void
flow_udp_connect_op_update_description (FlowUdpConnectOp *udp_connect_op)
{
  FlowUdpConnectOpPrivate *priv                     = udp_connect_op->priv;
  FlowEvent               *event                    = FLOW_EVENT (udp_connect_op);
  gchar                   *local_desc;
  gchar                   *remote_desc;

  if (event->description)
    return;

  local_desc  = print_ip_service_description (priv->local_ip_service);
  remote_desc = print_ip_service_description (priv->remote_ip_service);

  /* Bring it all together */

  event->description = g_strdup_printf ("Send to %s from %s",
                                        remote_desc,
                                        local_desc);

  g_free (local_desc);
  g_free (remote_desc);
}

static void
flow_udp_connect_op_type_init (GType type)
{
}

static void
flow_udp_connect_op_class_init (FlowUdpConnectOpClass *klass)
{
  FlowEventClass *event_klass = FLOW_EVENT_CLASS (klass);

  event_klass->update_description = (void (*) (FlowEvent *)) flow_udp_connect_op_update_description;
}

static void
flow_udp_connect_op_init (FlowUdpConnectOp *udp_connect_op)
{
}

static void
flow_udp_connect_op_construct (FlowUdpConnectOp *udp_connect_op)
{
}

static void
flow_udp_connect_op_dispose (FlowUdpConnectOp *udp_connect_op)
{
}

static void
flow_udp_connect_op_finalize (FlowUdpConnectOp *udp_connect_op)
{
  FlowUdpConnectOpPrivate *priv = udp_connect_op->priv;

  flow_gobject_unref_clear (priv->local_ip_service);
  flow_gobject_unref_clear (priv->remote_ip_service);
}

/* --- FlowUdpConnectOp public API --- */

FlowUdpConnectOp *
flow_udp_connect_op_new (FlowIPService *local_ip_service, FlowIPService *remote_ip_service)
{
  return g_object_new (FLOW_TYPE_UDP_CONNECT_OP,
                       "local-ip-service", local_ip_service,
                       "remote-ip-service", remote_ip_service,
                       NULL);
}

FlowIPService *
flow_udp_connect_op_get_local_service (FlowUdpConnectOp *udp_connect_op)
{
  FlowUdpConnectOpPrivate *priv;

  g_return_val_if_fail (FLOW_IS_UDP_CONNECT_OP (udp_connect_op), NULL);

  priv = udp_connect_op->priv;
  return priv->local_ip_service;
}

FlowIPService *
flow_udp_connect_op_get_remote_service (FlowUdpConnectOp *udp_connect_op)
{
  FlowUdpConnectOpPrivate *priv;

  g_return_val_if_fail (FLOW_IS_UDP_CONNECT_OP (udp_connect_op), NULL);

  priv = udp_connect_op->priv;
  return priv->remote_ip_service;
}
