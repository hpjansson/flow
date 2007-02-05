/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tcp-io-listener.c - Connection listener that spawns FlowTcpIOs.
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
#include "flow-tcp-io-listener.h"

/* --- FlowTcpIOListener private data --- */

typedef struct
{
}
FlowTcpIOListenerPrivate;

/* --- FlowTcpIOListener properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_tcp_io_listener)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowTcpIOListener definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_tcp_io_listener, FlowTcpIOListener, FLOW_TYPE_TCP_LISTENER, 0)

/* --- FlowTcpIOListener implementation --- */

static void
flow_tcp_io_listener_type_init (GType type)
{
}

static void
flow_tcp_io_listener_class_init (FlowTcpIOListenerClass *klass)
{
}

static void
flow_tcp_io_listener_init (FlowTcpIOListener *tcp_io_listener)
{
}

static void
flow_tcp_io_listener_construct (FlowTcpIOListener *tcp_io_listener)
{
}

static void
flow_tcp_io_listener_dispose (FlowTcpIOListener *tcp_io_listener)
{
}

static void
flow_tcp_io_listener_finalize (FlowTcpIOListener *tcp_io_listener)
{
}

static FlowTcpIO *
setup_tcp_io (FlowElement *tcp_connector)
{
  FlowTcpIO   *tcp_io;
  FlowBin     *bin;
  FlowElement *old_tcp_connector;
  FlowElement *user_adapter;

  tcp_io = flow_tcp_io_new ();
  bin    = FLOW_BIN (tcp_io);

  /* Replace the FlowTcpConnector in tcp_io's bin */

  old_tcp_connector = (FlowElement *) flow_tcp_io_get_tcp_connector (tcp_io);
  if (old_tcp_connector)
  {
    flow_disconnect_element (old_tcp_connector);
    flow_bin_remove_element (bin, old_tcp_connector);
  }

  flow_tcp_io_set_tcp_connector (tcp_io, FLOW_TCP_CONNECTOR (tcp_connector));
  g_object_unref (tcp_connector);

  /* Connect it to the FlowUserAdapter */

  user_adapter = (FlowElement *) flow_io_get_user_adapter (FLOW_IO (tcp_io));
  if (user_adapter)
  {
    flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (tcp_connector),
                                   FLOW_SIMPLEX_ELEMENT (user_adapter));
    flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (user_adapter),
                                   FLOW_SIMPLEX_ELEMENT (tcp_connector));
  }

  return tcp_io;
}

/* --- FlowTcpIOListener public API --- */

FlowTcpIOListener *
flow_tcp_io_listener_new (void)
{
  return g_object_new (FLOW_TYPE_TCP_IO_LISTENER, NULL);
}

FlowTcpIO *
flow_tcp_io_listener_pop_connection (FlowTcpIOListener *tcp_io_listener)
{
  FlowElement *tcp_connector;

  g_return_val_if_fail (FLOW_IS_TCP_IO_LISTENER (tcp_io_listener), NULL);

  tcp_connector = (FlowElement *) flow_tcp_listener_pop_connection (FLOW_TCP_LISTENER (tcp_io_listener));
  if (!tcp_connector)
    return NULL;

  return setup_tcp_io (tcp_connector);
}

FlowTcpIO *
flow_tcp_io_listener_sync_pop_connection (FlowTcpIOListener *tcp_io_listener)
{
  FlowElement *tcp_connector;

  g_return_val_if_fail (FLOW_IS_TCP_IO_LISTENER (tcp_io_listener), NULL);

  tcp_connector = (FlowElement *) flow_tcp_listener_sync_pop_connection (FLOW_TCP_LISTENER (tcp_io_listener));
  if (!tcp_connector)
    return NULL;

  return setup_tcp_io (tcp_connector);
}
