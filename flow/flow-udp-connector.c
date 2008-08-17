/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-udp-connector.c - Origin/endpoint for UDP transport.
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

#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-detailed-event.h"
#include "flow-udp-connect-op.h"
#include "flow-udp-connector.h"

#define MAX_BUFFER_PACKETS 16
#define MAX_BUFFER_BYTES   4096

static void        shunt_read  (FlowShunt *shunt, FlowPacket *packet, FlowUdpConnector *udp_connector);
static FlowPacket *shunt_write (FlowShunt *shunt, FlowUdpConnector *udp_connector);

/* --- FlowUdpConnector private data --- */

typedef struct
{
  FlowUdpConnectOp *op;
  FlowUdpConnectOp *next_op;

  FlowShunt        *shunt;
}
FlowUdpConnectorPrivate;

/* --- FlowUdpConnector properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_udp_connector)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowUdpConnector definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_udp_connector, FlowUdpConnector, FLOW_TYPE_CONNECTOR, 0)

/* --- FlowUdpConnector implementation --- */

static void
setup_shunt (FlowUdpConnector *udp_connector)
{
  FlowUdpConnectorPrivate *priv = udp_connector->priv;
  FlowPad                 *output_pad;

  flow_shunt_set_read_func (priv->shunt, (FlowShuntReadFunc *) shunt_read, udp_connector);
  flow_shunt_set_write_func (priv->shunt, (FlowShuntWriteFunc *) shunt_write, udp_connector);

  output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (udp_connector)));

  if (flow_pad_is_blocked (output_pad))
  {
    flow_shunt_block_reads (priv->shunt);
  }
}

static void
connect_to_remote_service (FlowUdpConnector *udp_connector)
{
  FlowUdpConnectorPrivate *priv = udp_connector->priv;

  if (priv->shunt)
  {
    /* We already have an active shunt. This can happen when a shunt has
     * been installed using _flow_udp_connector_install_connected_shunt (). */

    return;
  }

  if (priv->next_op)
  {
    if (priv->op)
      g_object_unref (priv->op);

    priv->op = priv->next_op;
    priv->next_op = NULL;
  }

  if (!priv->op)
  {
    g_warning ("FlowUdpConnector got FLOW_STREAM_BEGIN before connect op.");
    return;
  }

  priv->shunt = flow_open_udp_port (flow_udp_connect_op_get_local_service (priv->op));

  setup_shunt (udp_connector);
  flow_connector_set_state_internal (FLOW_CONNECTOR (udp_connector), FLOW_CONNECTIVITY_CONNECTING);
}

static void
set_op (FlowUdpConnector *udp_connector, FlowUdpConnectOp *op)
{
  FlowUdpConnectorPrivate *priv = udp_connector->priv;

  g_object_ref (op);

  if (priv->next_op)
    g_object_unref (priv->next_op);

  priv->next_op = op;
}

static FlowPacket *
handle_outbound_packet (FlowUdpConnector *udp_connector, FlowPacket *packet)
{
  FlowPacketFormat packet_format = flow_packet_get_format (packet);
  gpointer         packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_UDP_CONNECT_OP (packet_data))
    {
      set_op (udp_connector, packet_data);
      flow_packet_free (packet);
      packet = NULL;
    }
    else if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        connect_to_remote_service (udp_connector);
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (udp_connector), FLOW_CONNECTIVITY_DISCONNECTING);
      }
    }
    else
    {
      flow_handle_universal_events (FLOW_ELEMENT (udp_connector), packet);
    }
  }

  return packet;
}

static FlowPacket *
handle_inbound_packet (FlowUdpConnector *udp_connector, FlowPacket *packet)
{
  FlowUdpConnectorPrivate *priv          = udp_connector->priv;
  FlowPacketFormat         packet_format = flow_packet_get_format (packet);
  gpointer                 packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (udp_connector), FLOW_CONNECTIVITY_CONNECTED);
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
               flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
      {
        flow_shunt_destroy (priv->shunt);
        priv->shunt = NULL;
        flow_connector_set_state_internal (FLOW_CONNECTOR (udp_connector), FLOW_CONNECTIVITY_DISCONNECTED);
      }
    }
    else
    {
      flow_handle_universal_events (FLOW_ELEMENT (udp_connector), packet);
    }
  }

  return packet;
}

static void
shunt_read (FlowShunt *shunt, FlowPacket *packet, FlowUdpConnector *udp_connector)
{
  packet = handle_inbound_packet (udp_connector, packet);

  if (packet)
  {
    FlowPad *output_pad;

    output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (udp_connector)));
    flow_pad_push (output_pad, packet);
  }
}

static FlowPacket *
shunt_write (FlowShunt *shunt, FlowUdpConnector *udp_connector)
{
  FlowPad         *input_pad;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (udp_connector)));
  packet_queue = flow_pad_get_packet_queue (input_pad);

  if (!packet_queue ||
      (flow_packet_queue_get_length_packets (packet_queue) < MAX_BUFFER_PACKETS &&
       flow_packet_queue_get_length_bytes (packet_queue) < MAX_BUFFER_BYTES))
  {
    flow_pad_unblock (input_pad);
    packet_queue = flow_pad_get_packet_queue (input_pad);
  }

  if (!packet_queue || flow_packet_queue_get_length_packets (packet_queue) == 0)
  {
    flow_shunt_block_writes (shunt);
    return NULL;
  }

  do
  {
    packet = flow_packet_queue_pop_packet (packet_queue);
    if (!packet)
      break;

    packet = handle_outbound_packet (udp_connector, packet);
  }
  while (!packet);

  return packet;
}

static void
flow_udp_connector_process_input (FlowUdpConnector *udp_connector, FlowPad *input_pad)
{
  FlowUdpConnectorPrivate *priv = udp_connector->priv;
  FlowPacketQueue         *packet_queue;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  while (!priv->shunt)
  {
    FlowPacket *packet;
 
    /* Not connected or connecting to anything; process input packets immediately. These
     * packets may change the desired peer address or request beginning-of-stream, or they
     * may be bogus data to be discarded. */

    packet = flow_packet_queue_pop_packet (packet_queue);
    if (!packet)
      break;

    packet = handle_outbound_packet (udp_connector, packet);
    if (packet)
      flow_packet_free (packet);
  }

  if (flow_packet_queue_get_length_bytes (packet_queue) >= MAX_BUFFER_BYTES ||
      flow_packet_queue_get_length_packets (packet_queue) >= MAX_BUFFER_PACKETS)
  {
    flow_pad_block (input_pad);
  }

  if (priv->shunt)
  {
    /* FIXME: The shunt's locking might be a performance liability. We could cache the state. */
    flow_shunt_unblock_writes (priv->shunt);
  }
}

static void
flow_udp_connector_output_pad_blocked (FlowUdpConnector *udp_connector, FlowPad *output_pad)
{
  FlowUdpConnectorPrivate *priv = udp_connector->priv;

  if (priv->shunt)
    flow_shunt_block_reads (priv->shunt);
}

static void
flow_udp_connector_output_pad_unblocked (FlowUdpConnector *udp_connector, FlowPad *output_pad)
{
  FlowUdpConnectorPrivate *priv = udp_connector->priv;

  if (priv->shunt)
    flow_shunt_unblock_reads (priv->shunt);
}

static void
flow_udp_connector_type_init (GType type)
{
}

static void
flow_udp_connector_class_init (FlowUdpConnectorClass *klass)
{
  FlowElementClass *element_klass = (FlowElementClass *) klass;

  element_klass->process_input        = (void (*) (FlowElement *, FlowPad *)) flow_udp_connector_process_input;
  element_klass->output_pad_blocked   = (void (*) (FlowElement *, FlowPad *)) flow_udp_connector_output_pad_blocked;
  element_klass->output_pad_unblocked = (void (*) (FlowElement *, FlowPad *)) flow_udp_connector_output_pad_unblocked;
}

static void
flow_udp_connector_init (FlowUdpConnector *udp_connector)
{
}

static void
flow_udp_connector_construct (FlowUdpConnector *udp_connector)
{
}

static void
flow_udp_connector_dispose (FlowUdpConnector *udp_connector)
{
  FlowUdpConnectorPrivate *priv = udp_connector->priv;

  flow_gobject_unref_clear (priv->op);
  flow_gobject_unref_clear (priv->next_op);

  if (priv->shunt)
  {
    flow_shunt_destroy (priv->shunt);
    priv->shunt = NULL;
  }
}

static void
flow_udp_connector_finalize (FlowUdpConnector *udp_connector)
{
}

/* --- FlowUdpConnector public API --- */

FlowUdpConnector *
flow_udp_connector_new (void)
{
  return g_object_new (FLOW_TYPE_UDP_CONNECTOR, NULL);
}

FlowIPService *
flow_udp_connector_get_local_service (FlowUdpConnector *udp_connector)
{
  FlowUdpConnectorPrivate *priv;
  FlowIPService           *local_service = NULL;

  g_return_val_if_fail (FLOW_IS_UDP_CONNECTOR (udp_connector), NULL);

  priv = udp_connector->priv;

  if (priv->op)
    local_service = flow_udp_connect_op_get_local_service (priv->op);

  return local_service;
}

FlowIPService *
flow_udp_connector_get_remote_service (FlowUdpConnector *udp_connector)
{
  FlowUdpConnectorPrivate *priv;
  FlowIPService           *remote_service = NULL;

  g_return_val_if_fail (FLOW_IS_UDP_CONNECTOR (udp_connector), NULL);

  priv = udp_connector->priv;

  if (priv->op)
    remote_service = flow_udp_connect_op_get_remote_service (priv->op);

  return remote_service;
}
