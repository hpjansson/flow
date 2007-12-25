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

#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-detailed-event.h"
#include "flow-file-connect-op.h"
#include "flow-file-connector.h"

#define MAX_BUFFER_PACKETS 16
#define MAX_BUFFER_BYTES   4096

static void        shunt_read  (FlowShunt *shunt, FlowPacket *packet, FlowFileConnector *file_connector);
static FlowPacket *shunt_write (FlowShunt *shunt, FlowFileConnector *file_connector);

/* --- FlowFileConnector private data --- */

typedef struct
{
  FlowFileConnectOp *op;
  FlowFileConnectOp *next_op;

  FlowShunt         *shunt;
}
FlowFileConnectorPrivate;

/* --- FlowFileConnector properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_file_connector)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowFileConnector definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_file_connector, FlowFileConnector, FLOW_TYPE_CONNECTOR, 0)

/* --- FlowFileConnector implementation --- */

static void
setup_shunt (FlowFileConnector *file_connector)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;
  FlowPad                  *output_pad;

  flow_shunt_set_read_func (priv->shunt, (FlowShuntReadFunc *) shunt_read, file_connector);
  flow_shunt_set_write_func (priv->shunt, (FlowShuntWriteFunc *) shunt_write, file_connector);

  output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (file_connector)));

  if (flow_pad_is_blocked (output_pad))
  {
    flow_shunt_block_reads (priv->shunt);
  }
}

static void
connect_to_path (FlowFileConnector *file_connector)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;

  if (priv->shunt)
  {
    /* We already have an active shunt. This can happen when a shunt has
     * been installed using _flow_file_connector_install_connected_shunt (). */

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

#if 0
  priv->shunt = flow_open_file (flow_file_connect_op_get_path (priv->op),
                                flow_file_connect_op_get_access_mode (priv->op));
#else
  priv->shunt = flow_create_file (flow_file_connect_op_get_path (priv->op),
                                  flow_file_connect_op_get_access_mode (priv->op),
                                  TRUE,
                                  FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                                  FLOW_NO_ACCESS,
                                  FLOW_NO_ACCESS);
#endif
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
      flow_packet_free (packet);
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
        g_print ("disconnected\n");

        flow_shunt_destroy (priv->shunt);
        priv->shunt = NULL;
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

    packet = handle_outbound_packet (file_connector, packet);
  }
  while (!packet);

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
flow_file_connector_output_pad_blocked (FlowFileConnector *file_connector, FlowPad *output_pad)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;

  if (priv->shunt)
    flow_shunt_block_reads (priv->shunt);
}

static void
flow_file_connector_output_pad_unblocked (FlowFileConnector *file_connector, FlowPad *output_pad)
{
  FlowFileConnectorPrivate *priv = file_connector->priv;

  if (priv->shunt)
    flow_shunt_unblock_reads (priv->shunt);
}

static void
flow_file_connector_type_init (GType type)
{
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

FlowFileConnector *
flow_file_connector_new (void)
{
  return g_object_new (FLOW_TYPE_FILE_CONNECTOR, NULL);
}

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
