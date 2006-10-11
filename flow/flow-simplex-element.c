/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-simplex-element.c - A unidirectional processing element.
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
#include "flow-simplex-element.h"

/* --- FlowSimplexElement properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_simplex_element)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowSimplexElement definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_simplex_element, FlowSimplexElement, FLOW_TYPE_ELEMENT, 0)

/* --- FlowSimplexElement implementation --- */

static void
flow_simplex_element_output_pad_blocked (FlowElement *element, FlowPad *output_pad)
{
  flow_pad_block (g_ptr_array_index (element->input_pads, 0));
}

static void
flow_simplex_element_output_pad_unblocked (FlowElement *element, FlowPad *output_pad)
{
  flow_pad_unblock (g_ptr_array_index (element->input_pads, 0));
}

static void
flow_simplex_element_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  packet_queue = flow_pad_get_packet_queue (input_pad);

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    flow_pad_push (g_ptr_array_index (element->output_pads, 0), packet);
  }
}

static void
flow_simplex_element_type_init (GType type)
{
}

static void
flow_simplex_element_class_init (FlowSimplexElementClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->output_pad_blocked   = flow_simplex_element_output_pad_blocked;
  element_klass->output_pad_unblocked = flow_simplex_element_output_pad_unblocked;
  element_klass->process_input        = flow_simplex_element_process_input;
}

static void
flow_simplex_element_init (FlowSimplexElement *simplex_element)
{
  FlowElement *element = (FlowElement *) simplex_element;

  g_ptr_array_add (element->input_pads,  g_object_new (FLOW_TYPE_INPUT_PAD,  "owner-element", simplex_element, NULL));
  g_ptr_array_add (element->output_pads, g_object_new (FLOW_TYPE_OUTPUT_PAD, "owner-element", simplex_element, NULL));
}

static void
flow_simplex_element_construct (FlowSimplexElement *simplex_element)
{
}

static void
flow_simplex_element_dispose (FlowSimplexElement *simplex_element)
{
}

static void
flow_simplex_element_finalize (FlowSimplexElement *simplex_element)
{
}

/* --- FlowSimplexElement public API --- */

FlowInputPad *
flow_simplex_element_get_input_pad  (FlowSimplexElement *simplex_element)
{
  FlowElement *element = (FlowElement *) simplex_element;

  g_return_val_if_fail (FLOW_IS_SIMPLEX_ELEMENT (simplex_element), NULL);

  return g_ptr_array_index (element->input_pads, 0);
}

FlowOutputPad *
flow_simplex_element_get_output_pad (FlowSimplexElement *simplex_element)
{
  FlowElement *element = (FlowElement *) simplex_element;

  g_return_val_if_fail (FLOW_IS_SIMPLEX_ELEMENT (simplex_element), NULL);

  return g_ptr_array_index (element->output_pads, 0);
}
