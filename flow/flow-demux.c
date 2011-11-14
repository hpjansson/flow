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
#include "flow-demux.h"
#include "flow-util.h"
#include "flow-gobject-util.h"

typedef struct 
{
  FlowInputPad *current_output_pad;
  GHashTable *pads_by_channel_id;
} FlowDemuxPrivate;

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_demux)
FLOW_GOBJECT_PROPERTIES_END   ()

FLOW_GOBJECT_MAKE_IMPL        (flow_demux, FlowDemux, FLOW_TYPE_SPLITTER, 0)

static void flow_demux_process_input (FlowElement *element, FlowPad *input_pad);



static void
flow_demux_type_init (GType type)
{
}

static void
flow_demux_class_init (FlowDemuxClass * klass)
{
  FlowElementClass *element_class = FLOW_ELEMENT_CLASS (klass);
  
  element_class->process_input = flow_demux_process_input;
}

static void
flow_demux_init (FlowDemux *demux)
{
  FlowDemuxPrivate *priv = demux->priv;

  priv->current_output_pad = NULL;
  priv->pads_by_channel_id = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
flow_demux_construct (FlowDemux *demux)
{
}

static void
flow_demux_dispose (FlowDemux *demux)
{
}

static void
flow_demux_finalize (FlowDemux *demux)
{
  FlowDemuxPrivate *priv = demux->priv;
  
  g_hash_table_destroy (priv->pads_by_channel_id);
}

/* Public API */

/**
 * flow_demux_new:
 *
 * Creates a new #FlowDemux element.
 *
 * Return value: A new #FlowDemux element.
 **/
FlowDemux *
flow_demux_new (void)
{
  return g_object_new (FLOW_TYPE_DEMUX, NULL);
}

FlowOutputPad *
flow_demux_add_channel (FlowDemux *demux, guint channel_id)
{
  FlowDemuxPrivate *priv;
  FlowOutputPad *pad;
  
  g_return_val_if_fail (demux != NULL, NULL);

  priv = (FlowDemuxPrivate *) demux->priv;

  pad = flow_splitter_add_output_pad (FLOW_SPLITTER (demux));
  g_hash_table_insert (priv->pads_by_channel_id, GUINT_TO_POINTER (channel_id), pad);
  
  return pad;
}

/* Called when the input pad is ready for reading */
static void
flow_demux_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowDemux *demux = FLOW_DEMUX (element);
  FlowDemuxPrivate *priv = demux->priv;
  FlowPacketQueue *packet_queue = flow_pad_get_packet_queue (input_pad);
  FlowPacket *packet;
  
  while ((packet = flow_packet_queue_pop_packet (packet_queue)) != NULL)
  {
    if (flow_handle_universal_events (element, packet))
      continue;

    if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
    {
      gpointer object = flow_packet_get_data (packet);
      
      if (FLOW_IS_MUX_EVENT (object))
      {
        FlowMuxEvent *event = FLOW_MUX_EVENT (object);
        guint channel_id = flow_mux_event_get_channel_id (event);
        priv->current_output_pad = g_hash_table_lookup (priv->pads_by_channel_id,
                                                        GUINT_TO_POINTER (channel_id));
        flow_packet_unref (packet);
      }
      else
      {
        int i;
        
        /* broadcast packet */
        for (i = 1; i < element->output_pads->len; i++)
        {
          FlowPad *pad = g_ptr_array_index (element->output_pads, i);
          flow_pad_push (pad, flow_packet_ref (packet));
        }
        if (element->output_pads->len > 0)
          flow_pad_push (g_ptr_array_index (element->output_pads, 0), packet);
      }
    }
    else /* Buffer */
    {
      if (priv->current_output_pad == NULL)
        flow_packet_unref (packet);
      else
        flow_pad_push (FLOW_PAD (priv->current_output_pad), packet);
    }
  }
}

