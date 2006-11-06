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
#include "flow-gobject-util.h"
#include "flow-tcp-io-listener.h"

#define USER_ADAPTER_NAME  "user-adapter"
#define TCP_CONNECTOR_NAME "tcp-connector"

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
  FlowElement *old_tcp_connector;
  FlowElement *user_adapter;
  FlowTcpIO   *tcp_io;
  FlowBin     *bin;

  g_return_val_if_fail (FLOW_IS_TCP_IO_LISTENER (tcp_io_listener), NULL);

  tcp_connector = (FlowElement *) flow_tcp_listener_pop_connection (FLOW_TCP_LISTENER (tcp_io_listener));
  if (!tcp_connector)
    return NULL;

  tcp_io = flow_tcp_io_new ();
  bin    = FLOW_BIN (tcp_io);

  /* Replace the FlowTcpConnector in tcp_io's bin */

  old_tcp_connector = flow_bin_get_element (bin, TCP_CONNECTOR_NAME);

  if (old_tcp_connector)
  {
    flow_pad_disconnect (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (old_tcp_connector))));
    flow_pad_disconnect (FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (old_tcp_connector))));

    flow_bin_remove_element (bin, old_tcp_connector);
  }

  flow_bin_add_element (bin, tcp_connector, TCP_CONNECTOR_NAME);

  /* Connect it to the FlowUserAdapter */

  user_adapter = flow_bin_get_element (bin, USER_ADAPTER_NAME);

  if (user_adapter)
  {
    flow_pad_connect (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (tcp_connector))),
                      FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (user_adapter))));
    flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (tcp_connector))),
                      FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (user_adapter))));
  }

  return tcp_io;
}
