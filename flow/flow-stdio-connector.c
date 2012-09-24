/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-stdio-connector.c - Origin/endpoint for stdio.
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

#include <string.h>

#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-detailed-event.h"
#include "flow-stdio-connector.h"

#define MAX_BUFFER_PACKETS 32

static void        shunt_read  (FlowShunt *shunt, FlowPacket *packet, FlowStdioConnector *stdio_connector);
static FlowPacket *shunt_write (FlowShunt *shunt, FlowStdioConnector *stdio_connector);

/* --- FlowStdioConnector private data --- */

struct _FlowStdioConnectorPrivate
{
  FlowShunt         *shunt;

  guint              reading_from_shunt : 1;
  guint              writing_to_shunt   : 1;
};

/* --- FlowStdioConnector properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_stdio_connector)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowStdioConnector definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_stdio_connector, FlowStdioConnector, FLOW_TYPE_CONNECTOR, 0)

/* --- FlowStdioConnector implementation --- */

static void
set_reading_from_shunt (FlowStdioConnector *stdio_connector, gboolean is_reading)
{
  FlowStdioConnectorPrivate *priv = stdio_connector->priv;

  if (is_reading == priv->reading_from_shunt)
    return;

  priv->reading_from_shunt = is_reading;

  if (!priv->shunt)
    return;

  if (is_reading)
    flow_shunt_unblock_reads (priv->shunt);
  else
    flow_shunt_block_reads (priv->shunt);
}

static void
set_writing_to_shunt (FlowStdioConnector *stdio_connector, gboolean is_writing)
{
  FlowStdioConnectorPrivate *priv = stdio_connector->priv;

  if (is_writing == priv->writing_to_shunt)
    return;

  priv->writing_to_shunt = is_writing;

  if (!priv->shunt)
    return;

  if (is_writing)
    flow_shunt_unblock_writes (priv->shunt);
  else
    flow_shunt_block_writes (priv->shunt);
}

static void
maybe_unblock_input_pad (FlowStdioConnector *stdio_connector)
{
  FlowPad         *input_pad;
  FlowPacketQueue *packet_queue;

  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (stdio_connector)));
  packet_queue = flow_pad_get_packet_queue (input_pad);

  if (!packet_queue ||
      (flow_packet_queue_get_length_packets (packet_queue) < MAX_BUFFER_PACKETS &&
       flow_packet_queue_get_length_bytes (packet_queue) < flow_connector_get_write_queue_limit (FLOW_CONNECTOR (stdio_connector)) / 2 + 1))
  {
    flow_pad_unblock (input_pad);
  }
}

static void
setup_shunt (FlowStdioConnector *stdio_connector)
{
  FlowStdioConnectorPrivate *priv = stdio_connector->priv;
  FlowConnector            *connector = FLOW_CONNECTOR (stdio_connector);
  FlowPad                  *output_pad;
  FlowPacketQueue          *packet_queue;

  flow_shunt_set_read_func (priv->shunt, (FlowShuntReadFunc *) shunt_read, stdio_connector);
  flow_shunt_set_write_func (priv->shunt, (FlowShuntWriteFunc *) shunt_write, stdio_connector);

  flow_shunt_set_io_buffer_size (priv->shunt, flow_connector_get_io_buffer_size (connector));
  flow_shunt_set_queue_limit (priv->shunt, flow_connector_get_read_queue_limit (connector));

  priv->reading_from_shunt = FALSE;
  priv->writing_to_shunt = FALSE;

  output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (stdio_connector)));
  packet_queue = flow_pad_get_packet_queue (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (stdio_connector))));

  set_reading_from_shunt (stdio_connector, flow_pad_is_blocked (output_pad) ? FALSE : TRUE);
  set_writing_to_shunt (stdio_connector, packet_queue && flow_packet_queue_get_length_packets (packet_queue) > 0 ? TRUE : FALSE);

  maybe_unblock_input_pad (stdio_connector);
}

static FlowPacket *
handle_outbound_packet (FlowStdioConnector *stdio_connector, FlowPacket *packet)
{
  FlowPacketFormat packet_format = flow_packet_get_format (packet);
  gpointer         packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (stdio_connector), FLOW_CONNECTIVITY_CONNECTING);
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (stdio_connector), FLOW_CONNECTIVITY_DISCONNECTING);
      }
    }
    else
    {
      flow_handle_universal_events (FLOW_ELEMENT (stdio_connector), packet);
    }
  }

  return packet;
}

static FlowPacket *
handle_inbound_packet (FlowStdioConnector *stdio_connector, FlowPacket *packet)
{
  FlowStdioConnectorPrivate *priv          = stdio_connector->priv;
  FlowPacketFormat          packet_format = flow_packet_get_format (packet);
  gpointer                  packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (stdio_connector), FLOW_CONNECTIVITY_CONNECTED);
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
               flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
      {
        if (priv->shunt)
        {
          flow_shunt_destroy (priv->shunt);
          priv->shunt = NULL;
        }

        flow_connector_set_state_internal (FLOW_CONNECTOR (stdio_connector), FLOW_CONNECTIVITY_DISCONNECTED);
      }
    }
    else
    {
      flow_handle_universal_events (FLOW_ELEMENT (stdio_connector), packet);
    }
  }

  return packet;
}

static void
shunt_read (FlowShunt *shunt, FlowPacket *packet, FlowStdioConnector *stdio_connector)
{
  packet = handle_inbound_packet (stdio_connector, packet);

  if (packet)
  {
    FlowPad *output_pad;

    output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (stdio_connector)));
    flow_pad_push (output_pad, packet);
  }
}

static FlowPacket *
shunt_write (FlowShunt *shunt, FlowStdioConnector *stdio_connector)
{
  FlowPad         *input_pad;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (stdio_connector)));
  maybe_unblock_input_pad (stdio_connector);
  packet_queue = flow_pad_get_packet_queue (input_pad);

  if (!packet_queue || flow_packet_queue_get_length_packets (packet_queue) == 0)
  {
    set_writing_to_shunt (stdio_connector, FALSE);
    return NULL;
  }

  do
  {
    packet = flow_packet_queue_pop_packet (packet_queue);
    if (!packet)
      break;

    packet = handle_outbound_packet (stdio_connector, packet);
  }
  while (!packet);

  maybe_unblock_input_pad (stdio_connector);
  return packet;
}

static void
flow_stdio_connector_process_input (FlowStdioConnector *stdio_connector, FlowPad *input_pad)
{
  FlowPacketQueue *packet_queue;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  if (flow_packet_queue_get_length_bytes (packet_queue) >= flow_connector_get_write_queue_limit (FLOW_CONNECTOR (stdio_connector)) ||
      flow_packet_queue_get_length_packets (packet_queue) >= MAX_BUFFER_PACKETS)
  {
    flow_pad_block (input_pad);
  }

  if (flow_packet_queue_get_length_packets (packet_queue) > 0)
    set_writing_to_shunt (stdio_connector, TRUE);
}

static void
flow_stdio_connector_output_pad_blocked (FlowStdioConnector *stdio_connector, FlowPad *output_pad)
{
  set_reading_from_shunt (stdio_connector, FALSE);
}

static void
flow_stdio_connector_output_pad_unblocked (FlowStdioConnector *stdio_connector, FlowPad *output_pad)
{
  set_reading_from_shunt (stdio_connector, TRUE);
}

static void
flow_stdio_connector_type_init (GType type)
{
}

static void
property_changed (FlowStdioConnector *stdio_connector, GParamSpec *param_spec)
{
  FlowStdioConnectorPrivate *priv = stdio_connector->priv;
  FlowConnector *connector = FLOW_CONNECTOR (stdio_connector);

  if (!param_spec || !param_spec->name)
    return;

  if (!strcmp (param_spec->name, "io-buffer-size"))
  {
    if (priv->shunt)
      flow_shunt_set_io_buffer_size (priv->shunt, flow_connector_get_io_buffer_size (connector));
  }
  else if (!strcmp (param_spec->name, "read-queue-limit"))
  {
    if (priv->shunt)
      flow_shunt_set_queue_limit (priv->shunt, flow_connector_get_read_queue_limit (connector));
  }
  else if (!strcmp (param_spec->name, "write-queue-limit"))
  {
    maybe_unblock_input_pad (stdio_connector);
  }
}

static void
flow_stdio_connector_class_init (FlowStdioConnectorClass *klass)
{
  FlowElementClass *element_klass = (FlowElementClass *) klass;

  element_klass->process_input        = (void (*) (FlowElement *, FlowPad *)) flow_stdio_connector_process_input;
  element_klass->output_pad_blocked   = (void (*) (FlowElement *, FlowPad *)) flow_stdio_connector_output_pad_blocked;
  element_klass->output_pad_unblocked = (void (*) (FlowElement *, FlowPad *)) flow_stdio_connector_output_pad_unblocked;
}

static void
flow_stdio_connector_init (FlowStdioConnector *stdio_connector)
{
  FlowStdioConnectorPrivate *priv = stdio_connector->priv;

  priv->shunt = flow_open_stdio ();
  g_signal_connect_swapped (stdio_connector, "notify", (GCallback) property_changed, stdio_connector);

  setup_shunt (stdio_connector);
}

static void
flow_stdio_connector_construct (FlowStdioConnector *stdio_connector)
{
}

static void
flow_stdio_connector_dispose (FlowStdioConnector *stdio_connector)
{
  FlowStdioConnectorPrivate *priv = stdio_connector->priv;

  if (priv->shunt)
  {
    flow_shunt_destroy (priv->shunt);
    priv->shunt = NULL;
  }
}

static void
flow_stdio_connector_finalize (FlowStdioConnector *stdio_connector)
{
}

/* --- FlowStdioConnector public API --- */

/**
 * flow_stdio_connector_new:
 *
 * Creates a new #FlowStdioConnector.
 *
 * Return value: A new #FlowStdioConnector.
 **/
FlowStdioConnector *
flow_stdio_connector_new (void)
{
  return g_object_new (FLOW_TYPE_STDIO_CONNECTOR, NULL);
}
