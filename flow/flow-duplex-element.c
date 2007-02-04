/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-duplex-element.c - A unidirectional processing element.
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
#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-duplex-element.h"

#define UPSTREAM_INDEX   0
#define DOWNSTREAM_INDEX 1

/* --- FlowDuplexElement private data --- */

typedef struct
{
}
FlowDuplexElementPrivate;

/* --- FlowDuplexElement properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_duplex_element)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowDuplexElement definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_duplex_element, FlowDuplexElement, FLOW_TYPE_ELEMENT, 0)

/* --- FlowDuplexElement implementation --- */

static void
flow_duplex_element_output_pad_blocked (FlowElement *element, FlowPad *output_pad)
{
  gint input_index;

  if (output_pad == g_ptr_array_index (element->output_pads, UPSTREAM_INDEX))
    input_index = DOWNSTREAM_INDEX;
  else
    input_index = UPSTREAM_INDEX;

  flow_pad_block (g_ptr_array_index (element->input_pads, input_index));
}

static void
flow_duplex_element_output_pad_unblocked (FlowElement *element, FlowPad *output_pad)
{
  gint input_index;

  if (output_pad == g_ptr_array_index (element->output_pads, UPSTREAM_INDEX))
    input_index = DOWNSTREAM_INDEX;
  else
    input_index = UPSTREAM_INDEX;

  flow_pad_unblock (g_ptr_array_index (element->input_pads, input_index));
}

static void
flow_duplex_element_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;
  gint             output_index;

  packet_queue = flow_pad_get_packet_queue (input_pad);

  if (input_pad == g_ptr_array_index (element->input_pads, UPSTREAM_INDEX))
    output_index = DOWNSTREAM_INDEX;
  else
    output_index = UPSTREAM_INDEX;

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    flow_handle_universal_events (element, packet);
    flow_pad_push (g_ptr_array_index (element->output_pads, output_index), packet);
  }
}

static void
flow_duplex_element_type_init (GType type)
{
}

static void
flow_duplex_element_class_init (FlowDuplexElementClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->output_pad_blocked   = flow_duplex_element_output_pad_blocked;
  element_klass->output_pad_unblocked = flow_duplex_element_output_pad_unblocked;
  element_klass->process_input        = flow_duplex_element_process_input;
}

static void
flow_duplex_element_init (FlowDuplexElement *duplex_element)
{
  FlowElement *element = (FlowElement *) duplex_element;

  g_ptr_array_add (element->input_pads,  g_object_new (FLOW_TYPE_INPUT_PAD,  "owner-element", duplex_element, NULL));
  g_ptr_array_add (element->input_pads,  g_object_new (FLOW_TYPE_INPUT_PAD,  "owner-element", duplex_element, NULL));
  g_ptr_array_add (element->output_pads, g_object_new (FLOW_TYPE_OUTPUT_PAD, "owner-element", duplex_element, NULL));
  g_ptr_array_add (element->output_pads, g_object_new (FLOW_TYPE_OUTPUT_PAD, "owner-element", duplex_element, NULL));
}

static void
flow_duplex_element_construct (FlowDuplexElement *duplex_element)
{
}

static void
flow_duplex_element_dispose (FlowDuplexElement *duplex_element)
{
}

static void
flow_duplex_element_finalize (FlowDuplexElement *duplex_element)
{
}

/* --- FlowDuplexElement public API --- */

FlowInputPad *
flow_duplex_element_get_upstream_input_pad (FlowDuplexElement *duplex_element)
{
  FlowElement *element = (FlowElement *) duplex_element;

  g_return_val_if_fail (FLOW_IS_DUPLEX_ELEMENT (duplex_element), NULL);

  return g_ptr_array_index (element->input_pads, UPSTREAM_INDEX);
}

FlowOutputPad *
flow_duplex_element_get_upstream_output_pad (FlowDuplexElement *duplex_element)
{
  FlowElement *element = (FlowElement *) duplex_element;

  g_return_val_if_fail (FLOW_IS_DUPLEX_ELEMENT (duplex_element), NULL);

  return g_ptr_array_index (element->output_pads, UPSTREAM_INDEX);
}

FlowInputPad *
flow_duplex_element_get_downstream_input_pad (FlowDuplexElement *duplex_element)
{
  FlowElement *element = (FlowElement *) duplex_element;

  g_return_val_if_fail (FLOW_IS_DUPLEX_ELEMENT (duplex_element), NULL);

  return g_ptr_array_index (element->input_pads, DOWNSTREAM_INDEX);
}

FlowOutputPad *
flow_duplex_element_get_downstream_output_pad (FlowDuplexElement *duplex_element)
{
  FlowElement *element = (FlowElement *) duplex_element;

  g_return_val_if_fail (FLOW_IS_DUPLEX_ELEMENT (duplex_element), NULL);

  return g_ptr_array_index (element->output_pads, DOWNSTREAM_INDEX);
}
