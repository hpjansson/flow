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
#include "flow-gobject-util.h"

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

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_mux)
FLOW_GOBJECT_PROPERTIES_END   ()

FLOW_GOBJECT_MAKE_IMPL        (flow_mux, FlowMux, FLOW_TYPE_JOINER, 0)

static void flow_mux_process_input (FlowElement *element, FlowPad *input_pad);



static GQuark channel_info_quark;

static void
flow_mux_type_init (GType type)
{
}

static void
flow_mux_class_init (FlowMuxClass * klass)
{
  FlowElementClass *element_class = FLOW_ELEMENT_CLASS (klass);
  
  channel_info_quark = g_quark_from_static_string ("flow-mux-channel-info");
  
  element_class->process_input = flow_mux_process_input;
}

static void
flow_mux_init (FlowMux *mux)
{
  FlowMuxPrivate *priv = mux->priv;

  priv->current_input_pad = NULL;
  priv->open_channels = 0;
}

static void
flow_mux_construct (FlowMux *mux)
{
}

static void
flow_mux_dispose (FlowMux *mux)
{
  FlowElement *element = FLOW_ELEMENT (mux);
  gint i;
  
  if (FLOW_ELEMENT (mux)->was_disposed)
    return;
  
  for (i = 0; i < element->input_pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (element->input_pads, i);
    ChannelInfo *info = g_object_get_qdata (G_OBJECT (pad), channel_info_quark);
    
    g_object_set_qdata (G_OBJECT (pad), channel_info_quark, NULL);
    if (info->event)
      g_object_unref (info->event);
    g_free (info);
  }
}

static void
flow_mux_finalize (FlowMux *mux)
{
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

  priv = (FlowMuxPrivate *) mux->priv;
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
flow_mux_add_channel_id (FlowMux *mux, guint id)
{
  FlowMuxEvent *event = flow_mux_event_new (id);
  FlowInputPad *pad = flow_mux_add_channel (mux, event);

  g_object_unref (event);
  return pad;
}

static void
flow_mux_channel_shutdown (FlowMux *mux, ChannelInfo *info)
{
  FlowMuxPrivate *priv = mux->priv;
  
  g_object_unref (info->event);
  info->event = NULL;
  priv->open_channels--;

  if (priv->open_channels <= 0)
  {
    FlowPad *output_pad = FLOW_PAD (flow_joiner_get_output_pad (FLOW_JOINER (mux)));
    FlowPacket *packet;
    
    priv->current_input_pad = NULL;
    packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
    flow_pad_push (output_pad, packet);
  }
}

/* Called when one of the input pads is ready for reading */
static void
flow_mux_process_input (FlowElement *element, FlowPad *input_pad)
{
  ChannelInfo *info = (ChannelInfo *) g_object_get_qdata (G_OBJECT (input_pad), channel_info_quark);
  FlowMux *mux = FLOW_MUX (element);
  FlowMuxPrivate *priv = mux->priv;
  FlowPacketQueue *packet_queue = flow_pad_get_packet_queue (input_pad);
  FlowPacket *packet;
  
  while ((packet = flow_packet_queue_pop_packet (packet_queue)) != NULL)
  {
    gboolean forward_packet = TRUE;
    
    if (flow_handle_universal_events (element, packet))
      continue;

    if (G_UNLIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT))
    {
      gpointer object = flow_packet_get_data (packet);
      
      if (FLOW_IS_DETAILED_EVENT (object))
      {
        if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
        {
          flow_mux_channel_shutdown (mux, info);
          flow_packet_free (packet);
          forward_packet = FALSE;
        }
      }
    }
    if (forward_packet)
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
