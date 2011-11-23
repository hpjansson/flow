/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tls-tcp-io-listener.c - Connection listener that spawns FlowTlsTcpIOs.
 *
 * Copyright (C) 2006 Hans Petter Jansson
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
#include "flow-util.h"
#include "flow-element-util.h"
#include "flow-gobject-util.h"
#include "flow-tls-tcp-io-listener.h"

/* --- FlowTlsTcpIOListener private data --- */

struct _FlowTlsTcpIOListenerPrivate
{
};

/* --- FlowTlsTcpIOListener properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_tls_tcp_io_listener)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowTlsTcpIOListener definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_tls_tcp_io_listener, FlowTlsTcpIOListener, FLOW_TYPE_TCP_IO_LISTENER, 0)

/* --- FlowTlsTcpIOListener implementation --- */

static void
flow_tls_tcp_io_listener_type_init (GType type)
{
}

static void
flow_tls_tcp_io_listener_class_init (FlowTlsTcpIOListenerClass *klass)
{
}

static void
flow_tls_tcp_io_listener_init (FlowTlsTcpIOListener *tls_tcp_io_listener)
{
}

static void
flow_tls_tcp_io_listener_construct (FlowTlsTcpIOListener *tls_tcp_io_listener)
{
}

static void
flow_tls_tcp_io_listener_dispose (FlowTlsTcpIOListener *tls_tcp_io_listener)
{
}

static void
flow_tls_tcp_io_listener_finalize (FlowTlsTcpIOListener *tls_tcp_io_listener)
{
}

static FlowTlsTcpIO *
setup_tls_tcp_io (FlowElement *tcp_connector)
{
  FlowTlsTcpIO *tls_tcp_io;
  FlowBin      *bin;
  FlowElement  *tls_protocol;

  tls_tcp_io = flow_tls_tcp_io_new ();
  bin        = FLOW_BIN (tls_tcp_io);

  /* Replace the FlowTcpConnector in tls_tcp_io's bin */

  flow_tcp_io_set_tcp_connector (FLOW_TCP_IO (tls_tcp_io), FLOW_TCP_CONNECTOR (tcp_connector));
  g_object_unref (tcp_connector);

  /* Replace the FlowTlsProtocol in tls_tcp_io's bin */

  tls_protocol = (FlowElement *) flow_tls_protocol_new (FLOW_AGENT_ROLE_SERVER);
  flow_tls_tcp_io_set_tls_protocol (tls_tcp_io, FLOW_TLS_PROTOCOL (tls_protocol));
  g_object_unref (tls_protocol);

  return tls_tcp_io;
}

/* --- FlowTlsTcpIOListener public API --- */

FlowTlsTcpIOListener *
flow_tls_tcp_io_listener_new (void)
{
  return g_object_new (FLOW_TYPE_TLS_TCP_IO_LISTENER, NULL);
}

FlowTlsTcpIO *
flow_tls_tcp_io_listener_pop_connection (FlowTlsTcpIOListener *tls_tcp_io_listener)
{
  FlowElement *tcp_connector;

  g_return_val_if_fail (FLOW_IS_TLS_TCP_IO_LISTENER (tls_tcp_io_listener), NULL);

  tcp_connector = (FlowElement *) flow_tcp_listener_pop_connection (FLOW_TCP_LISTENER (tls_tcp_io_listener));
  if (!tcp_connector)
    return NULL;

  return setup_tls_tcp_io (tcp_connector);
}

FlowTlsTcpIO *
flow_tls_tcp_io_listener_sync_pop_connection (FlowTlsTcpIOListener *tls_tcp_io_listener)
{
  FlowElement *tcp_connector;

  g_return_val_if_fail (FLOW_IS_TLS_TCP_IO_LISTENER (tls_tcp_io_listener), NULL);

  tcp_connector = (FlowElement *) flow_tcp_listener_sync_pop_connection (FLOW_TCP_LISTENER (tls_tcp_io_listener));
  if (!tcp_connector)
    return NULL;

  return setup_tls_tcp_io (tcp_connector);
}
