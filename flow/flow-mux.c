/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-mux.c - A stream multiplexer
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
#include "flow-mux.h"
#include "flow-util.h"

typedef struct
{
  FlowMuxEvent *event;
  gboolean eof;
} ChannelInfo;

typedef struct 
{
  FlowInputPad *current_input_pad;
  gint open_channels;
} FlowMuxPrivate;

#define FLOW_MUX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), FLOW_TYPE_MUX, FlowMuxPrivate))

static void flow_mux_done (FlowMux *mux);
static void flow_mux_process_input (FlowElement *element, FlowPad *input_pad);



static void flow_mux_init (FlowMux *mux);
static void flow_mux_dispose (GObject *object);
static void flow_mux_finalize (GObject *object);
static GObject *flow_mux_constructor (GType                  type,
                                      guint                  n_construct_properties,
                                      GObjectConstructParam *construct_properties);

static GObjectClass *parent_class = NULL;
static GQuark channel_info_quark;

static void
flow_mux_class_init (FlowMuxClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  FlowElementClass *element_class = FLOW_ELEMENT_CLASS (klass);
  
  channel_info_quark = g_quark_from_static_string ("flow-mux-channel-info");
  
  parent_class = (GObjectClass *) g_type_class_peek_parent (klass);

  g_type_class_add_private (klass, sizeof (FlowMuxPrivate));

  element_class->process_input = flow_mux_process_input;
  
  gobject_class->constructor = flow_mux_constructor;
  gobject_class->dispose = flow_mux_dispose;
  gobject_class->finalize = flow_mux_finalize;
}

GType
flow_mux_get_type (void)
{
  static const GTypeInfo mux_type_info = {
    sizeof(FlowMuxClass),	/* class_size */
    NULL,		/* base_init */
    NULL,		/* base_finalize */
    (GClassInitFunc) flow_mux_class_init,
    NULL,		/* class_finalize */
    NULL,		/* class_data */
    sizeof(FlowMux),
    0,		/* n_preallocs */
    (GInstanceInitFunc) flow_mux_init,
  };
  static GType mux_type = 0;

  if (!mux_type)
    mux_type = g_type_register_static (FLOW_TYPE_JOINER,
                                       "FlowMux",
                                       &mux_type_info, 0);
  
  return mux_type;
}

static void
flow_mux_init (FlowMux *mux)
{
  FlowMuxPrivate *priv = FLOW_MUX_GET_PRIVATE (mux);

  priv->current_input_pad = NULL;
  priv->open_channels = 0;
}

static GObject *
flow_mux_constructor (GType                  type,
                      guint                  n_construct_properties,
                      GObjectConstructParam *construct_properties)
{
  GObject *obj = parent_class->constructor (type,
                                            n_construct_properties,
                                            construct_properties);
  
  return obj;
}

static void
flow_mux_dispose (GObject *object)
{
  FlowMux *mux = FLOW_MUX (object);
  FlowElement *element = FLOW_ELEMENT (object);
  gint i;
  
  if (FLOW_ELEMENT (mux)->was_disposed)
    return;
  
  for (i = 0; i < element->input_pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (element->input_pads, i);
    ChannelInfo *info = g_object_get_qdata (G_OBJECT (pad), channel_info_quark);
    
    g_object_set_qdata (G_OBJECT (pad), channel_info_quark, NULL);
    g_object_unref (info->event);
    g_free (info);
  }
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
flow_mux_finalize (GObject *object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Public API */

FlowMux *
flow_mux_new (void)
{
  return g_object_new (FLOW_TYPE_MUX, NULL);
}

FlowInputPad *
flow_mux_add_channel (FlowMux *mux, FlowMuxEvent *event)
{
  FlowMuxPrivate *priv;
  FlowInputPad *pad;
  ChannelInfo *info;
  
  g_return_val_if_fail (mux != NULL, NULL);
  g_return_val_if_fail (event != NULL, NULL);

  priv = FLOW_MUX_GET_PRIVATE (mux);

  pad = flow_joiner_add_input_pad (FLOW_JOINER (mux));

  info = g_new (ChannelInfo, 1);
  info->event = event;
  g_object_ref (info->event);
  info->eof = FALSE;
  
  g_object_set_qdata (G_OBJECT (pad), channel_info_quark, info);
  
  priv->open_channels++;

  return pad;
}

FlowInputPad *
flow_mux_add_channel_id (FlowMux *mux, gint id)
{
  FlowMuxEvent *event = flow_mux_event_new (id);
  FlowInputPad *pad = flow_mux_add_channel (mux, event);

  g_object_unref (event);
  return pad;
}

static void
flow_mux_channel_shutdown (FlowMux *mux, ChannelInfo *info, gboolean call_done)
{
  FlowMuxPrivate *priv = FLOW_MUX_GET_PRIVATE (mux);
  
  g_object_unref (info->event);
  priv->open_channels--;

  if (call_done && priv->open_channels <= 0)
    flow_mux_done (mux);
}

/* Called when one of the input pads is ready for reading */
static void
flow_mux_process_input (FlowElement *element, FlowPad *input_pad)
{
  ChannelInfo *info = (ChannelInfo *) g_object_get_qdata (G_OBJECT (input_pad), channel_info_quark);
  FlowMux *mux = FLOW_MUX (element);
  FlowMuxPrivate *priv = FLOW_MUX_GET_PRIVATE (mux);
  FlowPacketQueue *packet_queue = flow_pad_get_packet_queue (input_pad);
  FlowPacket *packet;
  
  while ((packet = flow_packet_queue_pop_packet (packet_queue)) != NULL)
  {
    if (flow_handle_universal_events (element, packet))
      continue;

    if (G_UNLIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT))
    {
      gpointer object = flow_packet_get_data (packet);
      
      if (FLOW_IS_DETAILED_EVENT (object))
      {
        if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
          flow_mux_channel_shutdown (mux, info, TRUE);
      }
    }
    else
    {
      FlowPad *output_pad = FLOW_PAD (flow_joiner_get_output_pad (FLOW_JOINER (mux)));
      
      if (FLOW_PAD (priv->current_input_pad) != input_pad)
      {
        /* Change the channel */
        g_object_ref (info->event);
        flow_pad_push (output_pad, flow_packet_new_take_object (info->event, 0));
        priv->current_input_pad = FLOW_INPUT_PAD (input_pad);
      }
      flow_pad_push (output_pad, packet);
    }
  }
}

static void
flow_mux_done (FlowMux *mux)
{
  FlowMuxPrivate *priv = FLOW_MUX_GET_PRIVATE (mux);
  FlowPad *output_pad = FLOW_PAD (flow_joiner_get_output_pad (FLOW_JOINER (mux)));
  FlowPacket *packet;
  
  priv->current_input_pad = NULL;
  packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
  flow_pad_push (output_pad, packet);
}
