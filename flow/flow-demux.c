/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-demux.c - A one-to-many unidirectional element.
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
#include "flow-util.h"
#include "flow-demux.h"

/* --- FlowDemux private data --- */

typedef struct
{
}
FlowDemuxPrivate;

/* --- FlowDemux properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_demux)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowDemux definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_demux, FlowDemux, FLOW_TYPE_ELEMENT, 0)

/* --- FlowDemux implementation --- */

static void
flow_demux_output_pad_blocked (FlowElement *element, FlowPad *output_pad)
{
  FlowPad *input_pad;

  input_pad = g_ptr_array_index (element->input_pads, 0);

  /* Block input pad if any output pad is blocked */

  if (!flow_pad_is_blocked (input_pad))
    flow_pad_block (input_pad);
}

static void
flow_demux_output_pad_unblocked (FlowElement *element, FlowPad *outpud_pad)
{
  guint i;

  /* Block input pad if any output pad is blocked */

  /* NOTE: This may be slow if we have lots of outputs. It could be solved
   * by storing a counter indicating how many blocked outputs we have. I
   * very much doubt it'll be a problem in practice, though. */

  for (i = 0; i < element->output_pads->len; i++)
  {
    FlowPad *output_pad = g_ptr_array_index (element->output_pads, i);

    if (flow_pad_is_blocked (output_pad))
      break;
  }

  if (i >= element->output_pads->len)
  {
    FlowPad *input_pad;

    /* We reached the end - there are no blocked outputs */

    input_pad = g_ptr_array_index (element->input_pads, 0);

    if (!flow_pad_is_blocked (input_pad))
      flow_pad_block (input_pad);
  }
}

static void
flow_demux_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  packet_queue = flow_pad_get_packet_queue (input_pad);

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    guint i;

    flow_handle_universal_events (element, packet);

    if (element->output_pads->len > 0)
    {
      for (i = 1; i < element->output_pads->len; i++)
      {
        flow_pad_push (g_ptr_array_index (element->output_pads, i), flow_packet_copy (packet));
      }

      /* Push the original packet last, as it may get destroyed */
      flow_pad_push (g_ptr_array_index (element->output_pads, 0), packet);
    }
  }
}

static void
flow_demux_type_init (GType type)
{
}

static void
flow_demux_class_init (FlowDemuxClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->output_pad_blocked   = flow_demux_output_pad_blocked;
  element_klass->output_pad_unblocked = flow_demux_output_pad_unblocked;
  element_klass->process_input        = flow_demux_process_input;
}

static void
flow_demux_init (FlowDemux *demux)
{
  FlowElement *element = (FlowElement *) demux;

  g_ptr_array_add (element->input_pads, g_object_new (FLOW_TYPE_INPUT_PAD, "owner-element", demux, NULL));
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
}

/* --- FlowDemux public API --- */

FlowInputPad *
flow_demux_get_input_pad (FlowDemux *demux)
{
  FlowElement *element = (FlowElement *) demux;

  g_return_val_if_fail (FLOW_IS_DEMUX (demux), NULL);

  return g_ptr_array_index (element->input_pads, 0);
}

FlowOutputPad *
flow_demux_add_output_pad (FlowDemux *demux)
{
  FlowElement   *element = (FlowElement *) demux;
  FlowOutputPad *output_pad;

  g_return_val_if_fail (FLOW_IS_DEMUX (demux), NULL);

  output_pad = g_object_new (FLOW_TYPE_OUTPUT_PAD, "owner-element", demux, NULL);
  flow_g_ptr_array_add_sparse (element->output_pads, output_pad);

  return output_pad;
}

void
flow_demux_remove_output_pad (FlowDemux *demux, FlowOutputPad *output_pad)
{
  FlowElement *element = (FlowElement *) demux;
  gboolean     was_removed;

  g_return_if_fail (FLOW_IS_DEMUX (demux));
  g_return_if_fail (FLOW_IS_OUTPUT_PAD (output_pad));

  if (element->dispatch_depth)
  {
    was_removed = flow_g_ptr_array_remove_sparse (element->output_pads, output_pad);
    element->output_pad_removed = TRUE;  /* Can't use was_removed because it may already be TRUE */
  }
  else
  {
    was_removed = g_ptr_array_remove_fast (element->output_pads, output_pad);
  }

  if (!was_removed)
  {
    g_warning ("Tried to remove unknown output pad from demux!");
    return;
  }

  g_object_unref (output_pad);
}
