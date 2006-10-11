/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tcp-connector.c - Connection-oriented origin/endpoint for TCP transport.
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

#include "flow-gobject-util.h"
#include "flow-tcp-connector.h"
#include "flow-detailed-event.h"

#define MAX_BUFFER_PACKETS 16
#define MAX_BUFFER_BYTES   4096

/* --- FlowTcpConnector properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_tcp_connector)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowTcpConnector definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_tcp_connector, FlowTcpConnector, FLOW_TYPE_CONNECTOR, 0)

/* --- FlowTcpConnector implementation --- */

static void
shunt_read (FlowShunt *shunt, FlowPacket *packet, FlowTcpConnector *tcp_connector)
{
}

static FlowPacket *
shunt_write (FlowShunt *shunt, FlowTcpConnector *tcp_connector)
{
  FlowPad    *input_pad;
  FlowPacket *packet;

  
}

static void
connect_to_remote_service (FlowTcpConnector *tcp_connector)
{
  g_assert (tcp_connector->shunt == NULL);

  /* FIXME: Need a way to specify local (originating) port */

  if (tcp_connector->next_remote_service)
  {
    if (tcp_connector->remote_service)
      g_object_unref (tcp_connector->remote_service);

    tcp_connector->remote_service = tcp_connector->next_remote_service;
    tcp_connector->next_remote_service = NULL;
  }

  if (!tcp_connector->remote_service)
  {
    g_warning ("FlowTcpConnector got STREAM_BEGIN before IP address.");
    return;
  }

  tcp_connector->shunt = flow_connect_to_tcp (tcp_connector->remote_service, -1);
  flow_shunt_set_read_func (tcp_connector->shunt, (FlowShuntReadFunc *) shunt_read, tcp_connector);
  flow_shunt_set_write_func (tcp_connector->shunt, (FlowShuntWriteFunc *) shunt_write, tcp_connector);

  flow_connector_set_state_internal (FLOW_CONNECTOR (tcp_connector), FLOW_CONNECTIVITY_CONNECTING);
}

static void
set_remote_service (FlowTcpConnector *tcp_connector, FlowIPService *ip_service)
{
  g_object_ref (ip_service);

  if (tcp_connector->next_remote_service)
    g_object_unref (tcp_connector->next_remote_service);

  tcp_connector->next_remote_service = ip_service;
}

static FlowPacket *
handle_outbound_packet (FlowTcpConnector *tcp_connector, FlowPacket *packet)
{
  FlowPacketFormat packet_format = flow_packet_get_format (packet);
  gpointer         packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_IP_SERVICE (packet_data))
    {
      set_remote_service (tcp_connector, packet_data);
      flow_packet_free (packet);
      packet = NULL;
    }
    else if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        connect_to_remote_service (tcp_connector);
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (tcp_connector), FLOW_CONNECTIVITY_DISCONNECTING);
      }
    }
  }

  return packet;
}

static void
flow_tcp_connector_process_input (FlowTcpConnector *tcp_connector, FlowPad *input_pad)
{
  FlowPacketQueue *packet_queue;

  packet_queue = flow_pad_get_packet_queue (input_pad);

  while (!tcp_connector->shunt)
  {
    FlowPacket *packet;
 
    /* Not connected or connecting to anything; process input packets immediately. These
     * packets may change the desired peer address or request beginning-of-stream, or they
     * may be bogus data to be discarded. */

    packet = flow_packet_queue_pop_packet (packet_queue);
    if (!packet)
      break;

    packet = handle_outbound_packet (tcp_connector, packet);
    if (packet)
      flow_packet_free (packet);
  }

  if (flow_packet_queue_get_length_bytes (packet_queue) >= MAX_BUFFER_BYTES ||
      flow_packet_queue_get_length_packets (packet_queue) >= MAX_BUFFER_PACKETS)
  {
    flow_pad_block (input_pad);
  }
}

static void
flow_tcp_connector_type_init (GType type)
{
}

static void
flow_tcp_connector_class_init (FlowTcpConnectorClass *klass)
{
  FlowElementClass *element_klass = (FlowElementClass *) klass;

  element_klass->process_input = (void (*) (FlowElement *, FlowPad *)) flow_tcp_connector_process_input;
}

static void
flow_tcp_connector_init (FlowTcpConnector *tcp_connector)
{
  tcp_connector->queue_to_shunt = flow_packet_queue_new ();
}

static void
flow_tcp_connector_construct (FlowTcpConnector *tcp_connector)
{
}

static void
flow_tcp_connector_dispose (FlowTcpConnector *tcp_connector)
{
  flow_gobject_unref_clear (tcp_connector->remote_service);
  flow_gobject_unref_clear (tcp_connector->next_remote_service);
  flow_gobject_unref_clear (tcp_connector->queue_to_shunt);

  if (tcp_connector->shunt)
  {
    flow_shunt_destroy (tcp_connector->shunt);
    tcp_connector->shunt = NULL;
  }
}

static void
flow_tcp_connector_finalize (FlowTcpConnector *tcp_connector)
{
}

/* --- FlowTcpConnector public API --- */

FlowTcpConnector *
flow_tcp_connector_new (void)
{
  return g_object_new (FLOW_TYPE_TCP_CONNECTOR, NULL);
}
