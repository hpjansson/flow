/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-joiner.c - A many-to-one unidirectional element.
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
#include "flow-joiner.h"

/* --- FlowJoiner private data --- */

typedef struct
{
}
FlowJoinerPrivate;

/* --- FlowJoiner properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_joiner)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowJoiner definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_joiner, FlowJoiner, FLOW_TYPE_ELEMENT, 0)

/* --- FlowJoiner implementation --- */

static void
flow_joiner_output_pad_blocked (FlowElement *element, FlowPad *output_pad)
{
  guint i;

  /* Our one and only output pad was blocked - respond by blocking
   * all inputs. */

  for (i = 0; i < element->input_pads->len; i++)
  {
    FlowPad *input_pad = g_ptr_array_index (element->input_pads, i);

    if (!input_pad)
      continue;

    flow_pad_block (input_pad);
  }
}

static void
flow_joiner_output_pad_unblocked (FlowElement *element, FlowPad *output_pad)
{
  guint i;

  /* Our one and only output pad was unblocked - respond by unblocking
   * all inputs. */

  for (i = 0; i < element->input_pads->len; i++)
  {
    FlowPad *input_pad = g_ptr_array_index (element->input_pads, i);

    if (!input_pad)
      continue;

    flow_pad_unblock (input_pad);
  }
}

static void
flow_joiner_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  packet_queue = flow_pad_get_packet_queue (input_pad);

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    flow_handle_universal_events (element, packet);
    flow_pad_push (g_ptr_array_index (element->output_pads, 0), packet);
  }
}

static void
flow_joiner_type_init (GType type)
{
}

static void
flow_joiner_class_init (FlowJoinerClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->output_pad_blocked   = flow_joiner_output_pad_blocked;
  element_klass->output_pad_unblocked = flow_joiner_output_pad_unblocked;
  element_klass->process_input        = flow_joiner_process_input;
}

static void
flow_joiner_init (FlowJoiner *joiner)
{
  FlowElement *element = (FlowElement *) joiner;

  g_ptr_array_add (element->output_pads, g_object_new (FLOW_TYPE_OUTPUT_PAD, "owner-element", joiner, NULL));
}

static void
flow_joiner_construct (FlowJoiner *joiner)
{
}

static void
flow_joiner_dispose (FlowJoiner *joiner)
{
}

static void
flow_joiner_finalize (FlowJoiner *joiner)
{
}

/* --- FlowJoiner public API --- */

FlowOutputPad *
flow_joiner_get_output_pad (FlowJoiner *joiner)
{
  FlowElement *element = (FlowElement *) joiner;

  g_return_val_if_fail (FLOW_IS_JOINER (joiner), NULL);

  return g_ptr_array_index (element->output_pads, 0);
}

FlowInputPad *
flow_joiner_add_input_pad (FlowJoiner *joiner)
{
  FlowElement  *element = (FlowElement *) joiner;
  FlowInputPad *input_pad;

  g_return_val_if_fail (FLOW_IS_JOINER (joiner), NULL);

  input_pad = g_object_new (FLOW_TYPE_INPUT_PAD, "owner-element", joiner, NULL);
  flow_g_ptr_array_add_sparse (element->input_pads, input_pad);

  return input_pad;
}

void
flow_joiner_remove_input_pad (FlowJoiner *joiner, FlowInputPad *input_pad)
{
  FlowElement *element = (FlowElement *) joiner;
  gboolean     was_removed;

  g_return_if_fail (FLOW_IS_JOINER (joiner));
  g_return_if_fail (FLOW_IS_INPUT_PAD (input_pad));

  if (element->dispatch_depth)
  {
    was_removed = flow_g_ptr_array_remove_sparse (element->input_pads, input_pad);
    element->input_pad_removed = TRUE;  /* Can't use was_removed because it may already be TRUE */
  }
  else
  {
    was_removed = g_ptr_array_remove_fast (element->input_pads, input_pad);
  }

  if (!was_removed)
  {
    g_warning ("Tried to remove unknown input pad from joiner!");
    return;
  }

  element->pending_inputs = g_slist_remove (element->pending_inputs, input_pad);
  g_object_unref (input_pad);
}
