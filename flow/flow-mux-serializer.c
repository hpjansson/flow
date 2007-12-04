/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-mux-serializer.c - Serializes a multiplexed stream
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
#include "flow-mux-serializer.h"
#include "flow-mux-event.h"
#include "flow-gobject-util.h"
#include "flow-util.h"

typedef struct 
{
    gboolean have_channel_id;
    guint channel_id;
    GQueue *packets;
    guint32 packets_size;
    FlowMuxHeaderOps ops;
    gpointer ops_user_data;
} FlowMuxSerializerPrivate;

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_mux_serializer)
FLOW_GOBJECT_PROPERTIES_END   ()

FLOW_GOBJECT_MAKE_IMPL        (flow_mux_serializer, FlowMuxSerializer, FLOW_TYPE_SIMPLEX_ELEMENT, 0)

#define FLOW_MUX_HEADER_SIZE 6

static void flow_mux_serializer_process_input (FlowElement *element, FlowPad *input_pad);
static guint flow_mux_hdr_get_size (gpointer user_data);
static void flow_mux_hdr_parse (const guint8 *buffer, guint *channel_id, guint32 *size,
                                gpointer user_data);
static void flow_mux_hdr_unparse (guint8 *buffer, guint channel_id, guint32 size,
                                  gpointer user_data);



static void
flow_mux_serializer_type_init (GType type)
{
}

static void
flow_mux_serializer_class_init (FlowMuxSerializerClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->process_input = flow_mux_serializer_process_input;
}

FlowMuxHeaderOps flow_mux_serializer_default_ops = {
  .get_size = flow_mux_hdr_get_size,
  .parse = flow_mux_hdr_parse,
  .unparse = flow_mux_hdr_unparse
};

static void
flow_mux_serializer_init (FlowMuxSerializer *mux_serializer)
{
  FlowMuxSerializerPrivate *priv = mux_serializer->priv;

  priv->have_channel_id = FALSE;
  priv->channel_id = 0;
  priv->packets = g_queue_new ();
  priv->ops = flow_mux_serializer_default_ops;
  priv->ops_user_data = NULL;
}

static void
flow_mux_serializer_construct (FlowMuxSerializer *serializer)
{
}

static void
flow_mux_serializer_dispose (FlowMuxSerializer *serializer)
{
}

static void
flow_mux_serializer_finalize (FlowMuxSerializer *serializer)
{
  FlowMuxSerializerPrivate *priv = serializer->priv;
  g_queue_free (priv->packets);
}

static void
flow_mux_serializer_flush (FlowMuxSerializer *serializer)
{
  FlowMuxSerializerPrivate *priv = serializer->priv;
  FlowPad *output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (
                                          FLOW_SIMPLEX_ELEMENT (serializer)));
  FlowPacket *packet;
  
  if (priv->have_channel_id)
  {
    guint size = priv->ops.get_size (priv->ops_user_data);
    guint8 *buffer = g_alloca (size);
    FlowPacket *header;

    priv->ops.unparse (buffer, priv->channel_id, priv->packets_size, priv->ops_user_data);
    header = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, size);
    flow_pad_push (output_pad, header);
  }
  while ((packet = g_queue_pop_head (priv->packets)) != NULL)
  {
    if (!priv->have_channel_id)
    {
      flow_packet_free (packet);
      continue;
    }
    flow_pad_push (output_pad, packet);
  }
  priv->packets_size = 0;
}

static void
flow_mux_serializer_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowMuxSerializerPrivate *priv = FLOW_MUX_SERIALIZER (element)->priv;
  FlowPacketQueue *packet_queue = flow_pad_get_packet_queue (input_pad);
  FlowPacket *packet;
  
  while ((packet = flow_packet_queue_pop_packet (packet_queue)) != NULL)
  {
    gboolean queue_packet = TRUE;
    
    if (flow_handle_universal_events (element, packet))
      continue;

    if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
    { 
      gpointer object = flow_packet_get_data (packet);

      if (FLOW_IS_MUX_EVENT (object))
      {
        FlowMuxEvent *event = FLOW_MUX_EVENT (object);
        guint channel_id = flow_mux_event_get_channel_id (event);

        if (priv->have_channel_id)
          flow_mux_serializer_flush (FLOW_MUX_SERIALIZER (element));
        else
          priv->have_channel_id = TRUE;
        
        priv->channel_id = channel_id;
        queue_packet = FALSE;
      }
      else if (FLOW_IS_DETAILED_EVENT (object))
      {
        if (flow_detailed_event_matches (FLOW_DETAILED_EVENT (object),
                                         FLOW_STREAM_DOMAIN, FLOW_STREAM_END)
            || flow_detailed_event_matches (FLOW_DETAILED_EVENT (object),
                                            FLOW_STREAM_DOMAIN, FLOW_STREAM_FLUSH))
        {
          flow_mux_serializer_flush (FLOW_MUX_SERIALIZER (element));
        }
      }
    }
    if (queue_packet)
    {
      g_queue_push_tail (priv->packets, packet);
      priv->packets_size += packet->size;
    }
    else
      flow_packet_free (packet);
  }
}

static guint
flow_mux_hdr_get_size (gpointer user_data)
{
  return FLOW_MUX_HEADER_SIZE;
}

static void
flow_mux_hdr_parse (const guint8 *buffer, guint *channel_id, guint32 *size,
                    gpointer user_data)
{
  const guint8 *data = buffer;
  
  *channel_id = g_ntohs(*(guint16 *)data); data += 2;
  *size = g_ntohl(*(guint32 *)data); data += 4;
}

static void
flow_mux_hdr_unparse (guint8 *buffer, guint channel_id, guint32 size,
                      gpointer user_data)
{
  guchar *data = buffer;

  g_return_if_fail (channel_id < (1 << 16));
  
  *((guint16 *)data) = g_htons(channel_id); data += 2;
  *((guint32 *)data) = g_htonl(size); data += 4;
}

/* Public API */

FlowMuxSerializer *
flow_mux_serializer_new (void)
{
  return g_object_new (FLOW_TYPE_MUX_SERIALIZER, NULL);
}

guint
flow_mux_serializer_get_header_size (FlowMuxSerializer *serializer)
{
  FlowMuxSerializerPrivate *priv = serializer->priv;

  return priv->ops.get_size (priv->ops_user_data);
}

void
flow_mux_serializer_parse_header (FlowMuxSerializer *serializer,
                                  const guint8 *hdr,
                                  guint *channel_id,
                                  guint32 *size)
{
  FlowMuxSerializerPrivate *priv = serializer->priv;

  priv->ops.parse (hdr, channel_id, size, priv->ops_user_data);
}
