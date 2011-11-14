/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-mux-deserializer.c - Serializes a multiplexed stream
 *
 * Copyright (C) 2007 Andreas Rottmann
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
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors: Andreas Rottmann <a.rottmann@gmx.at>
 */

#include "config.h"
#include "flow-mux-deserializer.h"
#include "flow-mux-serializer.h"
#include "flow-mux-event.h"
#include "flow-gobject-util.h"
#include "flow-util.h"

typedef struct 
{
  guint32 size_left;
  FlowMuxHeaderOps ops;
  gpointer ops_user_data;
} FlowMuxDeserializerPrivate;

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_mux_deserializer)
FLOW_GOBJECT_PROPERTIES_END   ()

FLOW_GOBJECT_MAKE_IMPL        (flow_mux_deserializer, FlowMuxDeserializer, FLOW_TYPE_SIMPLEX_ELEMENT, 0)

static void flow_mux_deserializer_process_input (FlowElement *element, FlowPad *input_pad);



static void
flow_mux_deserializer_type_init (GType type)
{
}

static void
flow_mux_deserializer_class_init (FlowMuxDeserializerClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->process_input = flow_mux_deserializer_process_input;
}

static void
flow_mux_deserializer_init (FlowMuxDeserializer *mux_deserializer)
{
  FlowMuxDeserializerPrivate *priv = mux_deserializer->priv;

  priv->size_left = 0;
  priv->ops = flow_mux_serializer_default_ops;
  priv->ops_user_data = NULL;
}

static void
flow_mux_deserializer_construct (FlowMuxDeserializer *deserializer)
{
}

static void
flow_mux_deserializer_dispose (FlowMuxDeserializer *deserializer)
{
}

static void
flow_mux_deserializer_finalize (FlowMuxDeserializer *deserializer)
{
}

static void
flow_mux_deserializer_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowMuxDeserializerPrivate *priv = FLOW_MUX_DESERIALIZER (element)->priv;
  FlowPacketQueue *packet_queue = flow_pad_get_packet_queue (input_pad);
  FlowPacket *packet;
  guint hdr_size = priv->ops.get_size (priv->ops_user_data);
  guchar *hdr_buffer = g_alloca (hdr_size);
  FlowPad *output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (
                                          FLOW_SIMPLEX_ELEMENT (element)));
  
  while (flow_packet_queue_peek_packet (packet_queue, &packet, NULL))
  {
    gboolean forward_packet = TRUE;
    
    if (flow_handle_universal_events (element, packet))
    {
      forward_packet = FALSE;
    }
    else if (packet->format == FLOW_PACKET_FORMAT_OBJECT)
    {
      /* Pass on */
    }
    else if (priv->size_left == 0)
    {
      guint channel_id;
      guint32 size;
      
      if (!flow_packet_queue_pop_bytes_exact (packet_queue, hdr_buffer, hdr_size))
        break;

      priv->ops.parse (hdr_buffer, &channel_id, &size, priv->ops_user_data);
      priv->size_left = size;

      flow_pad_push (output_pad,
                     flow_packet_new_take_object (flow_mux_event_new (channel_id), 0));
      continue; /* no packet to forward or free */
    }
    else
    {
      if (priv->size_left < packet->size)
      {
        guchar *buffer = g_malloc (priv->size_left);
        gboolean success = flow_packet_queue_pop_bytes_exact (packet_queue, buffer, priv->size_left);
        g_assert (success);
        packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, priv->size_left);
        priv->size_left = 0;
      }
      else
        priv->size_left -= packet->size;
    }
    
    packet = flow_packet_queue_pop_packet (packet_queue);
    if (forward_packet)
      flow_pad_push (output_pad, packet);
    else
      flow_packet_unref (packet);
  }
}

/* Public API */

FlowMuxDeserializer *
flow_mux_deserializer_new (void)
{
  return g_object_new (FLOW_TYPE_MUX_DESERIALIZER, NULL);
}

gint
flow_mux_deserializer_get_header_size (FlowMuxDeserializer *deserializer)
{
  FlowMuxDeserializerPrivate *priv = deserializer->priv;

  return priv->ops.get_size (priv->ops_user_data);
}

void
flow_mux_deserializer_unparse_header (FlowMuxDeserializer *deserializer,
                                      guint8 *hdr,
                                      guint channel_id,
                                      guint32 size)
{
  FlowMuxDeserializerPrivate *priv = deserializer->priv;

  priv->ops.unparse (hdr, channel_id, size, priv->ops_user_data);
}
