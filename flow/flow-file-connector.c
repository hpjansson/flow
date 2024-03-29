/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-file-connector.c - Connection-oriented origin/endpoint for local transport.
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

#include <string.h>

#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-detailed-event.h"
#include "flow-file-connect-op.h"
#include "flow-file-connector.h"

#define MAX_BUFFER_PACKETS 32

static void        shunt_read  (FlowShunt *shunt, FlowPacket *packet, FlowFileConnector *file_connector);
static FlowPacket *shunt_write (FlowShunt *shunt, FlowFileConnector *file_connector);

/* --- FlowFileConnector private data --- */

struct _FlowFileConnectorPrivate
{
  FlowFileConnectOp *op;
  FlowFileConnectOp *next_op;

  FlowShunt         *shunt;

  guint              reading_from_shunt : 1;
  guint              writing_to_shunt   : 1;
};

/* --- FlowFileConnector properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_file_connector)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowFileConnector definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_file_connector, FlowFileConnector, FLOW_TYPE_CONNECTOR, 0)

/* --- FlowFileConnector implementation --- */

static void
set_reading_from_shunt (FlowFileConnector *file_connector, gboolean is_reading)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;

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
set_writing_to_shunt (FlowFileConnector *file_connector, gboolean is_writing)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;

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
maybe_unblock_input_pad (FlowFileConnector *file_connector)
{
  FlowPad         *input_pad;
  FlowPacketQueue *packet_queue;

  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (file_connector)));
  packet_queue = flow_pad_get_packet_queue (input_pad);

  if (!packet_queue ||
      (flow_packet_queue_get_length_packets (packet_queue) < MAX_BUFFER_PACKETS &&
       flow_packet_queue_get_length_bytes (packet_queue) < flow_connector_get_write_queue_limit (FLOW_CONNECTOR (file_connector)) / 2 + 1))
  {
    flow_pad_unblock (input_pad);
  }
}

static void
setup_shunt (FlowFileConnector *file_connector)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;
  FlowConnector            *connector = FLOW_CONNECTOR (file_connector);
  FlowPad                  *output_pad;
  FlowPacketQueue          *packet_queue;

  flow_shunt_set_read_func (priv->shunt, (FlowShuntReadFunc *) shunt_read, file_connector);
  flow_shunt_set_write_func (priv->shunt, (FlowShuntWriteFunc *) shunt_write, file_connector);

  flow_shunt_set_io_buffer_size (priv->shunt, flow_connector_get_io_buffer_size (connector));
  flow_shunt_set_queue_limit (priv->shunt, flow_connector_get_read_queue_limit (connector));

  priv->reading_from_shunt = TRUE;
  priv->writing_to_shunt = TRUE;

  output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (file_connector)));
  packet_queue = flow_pad_get_packet_queue (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (file_connector))));

  set_reading_from_shunt (file_connector, !flow_pad_is_blocked (output_pad) ? TRUE : FALSE);
  set_writing_to_shunt (file_connector, packet_queue && flow_packet_queue_get_length_packets (packet_queue) > 0 ? TRUE : FALSE);

  maybe_unblock_input_pad (file_connector);
}

static void
connect_to_path (FlowFileConnector *file_connector)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;

  if (priv->shunt)
  {
    g_warning ("FlowFileConnector got FLOW_STREAM_BEGIN, but stream already open.");
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
    g_warning ("FlowFileConnector got FLOW_STREAM_BEGIN before connect op.");
    return;
  }

  if (flow_file_connect_op_get_create (priv->op))
  {
    FlowAccessMode create_mode_user;
    FlowAccessMode create_mode_group;
    FlowAccessMode create_mode_others;

    flow_file_connect_op_get_create_modes (priv->op,
                                           &create_mode_user,
                                           &create_mode_group,
                                           &create_mode_others);

    priv->shunt = flow_create_file (flow_file_connect_op_get_path (priv->op),
                                    flow_file_connect_op_get_access_mode (priv->op),
                                    flow_file_connect_op_get_replace (priv->op),
                                    create_mode_user,
                                    create_mode_group,
                                    create_mode_others);
  }
  else
  {
    priv->shunt = flow_open_file (flow_file_connect_op_get_path (priv->op),
                                  flow_file_connect_op_get_access_mode (priv->op));
  }

  setup_shunt (file_connector);
  flow_connector_set_state_internal (FLOW_CONNECTOR (file_connector), FLOW_CONNECTIVITY_CONNECTING);
}

static void
set_op (FlowFileConnector *file_connector, FlowFileConnectOp *op)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;

  g_object_ref (op);

  if (priv->next_op)
    g_object_unref (priv->next_op);

  priv->next_op = op;
}

static FlowPacket *
handle_outbound_packet (FlowFileConnector *file_connector, FlowPacket *packet)
{
  FlowPacketFormat packet_format = flow_packet_get_format (packet);
  gpointer         packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_FILE_CONNECT_OP (packet_data))
    {
      set_op (file_connector, packet_data);
      flow_packet_unref (packet);
      packet = NULL;
    }
    else if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        connect_to_path (file_connector);
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (file_connector), FLOW_CONNECTIVITY_DISCONNECTING);
      }
    }
    else
    {
      flow_handle_universal_events (FLOW_ELEMENT (file_connector), packet);
    }
  }

  return packet;
}

static FlowPacket *
handle_inbound_packet (FlowFileConnector *file_connector, FlowPacket *packet)
{
  FlowFileConnectorPrivate *priv          = file_connector->priv;
  FlowPacketFormat          packet_format = flow_packet_get_format (packet);
  gpointer                  packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (file_connector), FLOW_CONNECTIVITY_CONNECTED);
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
               flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
      {
        if (priv->shunt)
        {
          flow_shunt_destroy (priv->shunt);
          priv->shunt = NULL;
        }

        flow_connector_set_state_internal (FLOW_CONNECTOR (file_connector), FLOW_CONNECTIVITY_DISCONNECTED);
      }
    }
    else
    {
      flow_handle_universal_events (FLOW_ELEMENT (file_connector), packet);
    }
  }

  return packet;
}

static void
shunt_read (FlowShunt *shunt, FlowPacket *packet, FlowFileConnector *file_connector)
{
  packet = handle_inbound_packet (file_connector, packet);

  if (packet)
  {
    FlowPad *output_pad;

    output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (file_connector)));
    flow_pad_push (output_pad, packet);
  }
}

static FlowPacket *
shunt_write (FlowShunt *shunt, FlowFileConnector *file_connector)
{
  FlowPad         *input_pad;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (file_connector)));
  maybe_unblock_input_pad (file_connector);
  packet_queue = flow_pad_get_packet_queue (input_pad);

  if (!packet_queue || flow_packet_queue_get_length_packets (packet_queue) == 0)
  {
    set_writing_to_shunt (file_connector, FALSE);
    return NULL;
  }

  do
  {
    packet = flow_packet_queue_pop_packet (packet_queue);
    if (!packet)
      break;

    packet = handle_outbound_packet (file_connector, packet);
  }
  while (!packet);

  maybe_unblock_input_pad (file_connector);
  return packet;
}

static void
flow_file_connector_process_input (FlowFileConnector *file_connector, FlowPad *input_pad)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;
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

    packet = handle_outbound_packet (file_connector, packet);
    if (packet)
      flow_packet_unref (packet);
  }

  if (flow_packet_queue_get_length_bytes (packet_queue) >= flow_connector_get_write_queue_limit (FLOW_CONNECTOR (file_connector)) ||
      flow_packet_queue_get_length_packets (packet_queue) >= MAX_BUFFER_PACKETS)
  {
    flow_pad_block (input_pad);
  }

  if (flow_packet_queue_get_length_packets (packet_queue) > 0)
    set_writing_to_shunt (file_connector, TRUE);
}

static void
flow_file_connector_output_pad_blocked (FlowFileConnector *file_connector, FlowPad *output_pad)
{
  set_reading_from_shunt (file_connector, FALSE);
}

static void
flow_file_connector_output_pad_unblocked (FlowFileConnector *file_connector, FlowPad *output_pad)
{
  set_reading_from_shunt (file_connector, TRUE);
}

static void
flow_file_connector_type_init (GType type)
{
}

static void
property_changed (FlowFileConnector *file_connector, GParamSpec *param_spec)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;
  FlowConnector *connector = FLOW_CONNECTOR (file_connector);

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
    maybe_unblock_input_pad (file_connector);
  }
}

static void
flow_file_connector_class_init (FlowFileConnectorClass *klass)
{
  FlowElementClass *element_klass = (FlowElementClass *) klass;

  element_klass->process_input        = (void (*) (FlowElement *, FlowPad *)) flow_file_connector_process_input;
  element_klass->output_pad_blocked   = (void (*) (FlowElement *, FlowPad *)) flow_file_connector_output_pad_blocked;
  element_klass->output_pad_unblocked = (void (*) (FlowElement *, FlowPad *)) flow_file_connector_output_pad_unblocked;
}

static void
flow_file_connector_init (FlowFileConnector *file_connector)
{
  g_signal_connect_swapped (file_connector, "notify", (GCallback) property_changed, file_connector);
}

static void
flow_file_connector_construct (FlowFileConnector *file_connector)
{
}

static void
flow_file_connector_dispose (FlowFileConnector *file_connector)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;

  flow_gobject_unref_clear (priv->op);
  flow_gobject_unref_clear (priv->next_op);

  if (priv->shunt)
  {
    flow_shunt_destroy (priv->shunt);
    priv->shunt = NULL;
  }
}

static void
flow_file_connector_finalize (FlowFileConnector *file_connector)
{
}

/* --- FlowFileConnector public API --- */

/**
 * flow_file_connector_new:
 *
 * Creates a new #FlowFileConnector.
 *
 * Return value: A new #FlowFileConnector.
 **/
FlowFileConnector *
flow_file_connector_new (void)
{
  return g_object_new (FLOW_TYPE_FILE_CONNECTOR, NULL);
}

/**
 * flow_file_connector_get_path:
 * @file_connector: A #FlowFileConnector.
 *
 * Returns the current local file system path that @file_connector is
 * connected to, or %NULL if not connected to anything. If non-null, the
 * path was copied, and must be freed by the caller.
 *
 * Return value: A newly allocated path, or %NULL if none was set.
 **/
gchar *
flow_file_connector_get_path (FlowFileConnector *file_connector)
{
  FlowFileConnectorPrivate *priv;
  const gchar              *path = NULL;

  g_return_val_if_fail (FLOW_IS_FILE_CONNECTOR (file_connector), NULL);

  priv = file_connector->priv;

  if (priv->op)
    path = flow_file_connect_op_get_path (priv->op);

  if (path)
    return g_strdup (path);

  return NULL;
}
