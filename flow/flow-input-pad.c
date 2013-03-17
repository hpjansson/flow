/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-input-pad.c - An element input connector.
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
#include "flow-input-pad.h"

/* --- FlowInputPad private data --- */

struct _FlowInputPadPrivate
{
};

/* --- FlowInputPad properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_input_pad)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowInputPad definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_input_pad, FlowInputPad, FLOW_TYPE_PAD, 0)

/* --- FlowInputPad implementation --- */

static inline void
pad_dispatch_enter (FlowPad *pad)
{
  pad->dispatch_depth++;
}

static inline void
pad_dispatch_leave (FlowPad *pad)
{
  pad->dispatch_depth--;
  if (pad->was_disposed && pad->dispatch_depth == 0)
    g_object_unref (pad);
}

static inline void
element_dispatch_enter (FlowElement *element)
{
  element->dispatch_depth++;
}

static inline void
element_dispatch_leave (FlowElement *element)
{
  element->dispatch_depth--;
  if (element->was_disposed && element->dispatch_depth == 0)
    g_object_unref (element);
}

static inline void
push_to_element (FlowPad *pad, FlowElement *owner_element)
{
  if G_LIKELY (!pad->is_blocked)
  {
    owner_element->current_input = pad;
    FLOW_ELEMENT_GET_CLASS (owner_element)->process_input (owner_element, pad);

    /* At this point, pad may be gone (if dynamic) */
  }
}

static inline void
process_output (FlowPad *pad)
{
  FlowElement *owner_element = pad->owner_element;

  g_assert (owner_element != NULL);

  if (!pad->packet_queue)
    return;

  if G_UNLIKELY (owner_element->current_input)
  {
    /* Already in output processor; don't recurse. This should be extremely
     * rare, so performance is not our primary concern. However, we must
     * handle it to avoid hard-to-debug problems in client code. */

    if (owner_element->current_input == pad)
      return;

    if (!g_slist_find (owner_element->pending_inputs, pad))
      owner_element->pending_inputs = g_slist_prepend (owner_element->pending_inputs, pad);

    return;
  }

  /* Process event from this call */

  push_to_element (pad, owner_element);

  /* At this point, pad may be gone (if dynamic) */

  /* Process events queued by recursion */

  while (owner_element->pending_inputs)
  {
    FlowPad *pending_pad = owner_element->pending_inputs->data;

    owner_element->pending_inputs = g_slist_delete_link (owner_element->pending_inputs, owner_element->pending_inputs);

    if (pending_pad->packet_queue)
      push_to_element (pending_pad, owner_element);
  }

  owner_element->current_input = NULL;
}

static void
flow_input_pad_block (FlowPad *pad)
{
  FlowElement *element = pad->owner_element;

  element_dispatch_enter (element);

  if G_LIKELY (pad->connected_pad)
    flow_pad_block (pad->connected_pad);

  /* At this point, pad may be gone (if dynamic) */

  element_dispatch_leave (element);

  /* At this point, pad and element may be gone */
}

static void
flow_input_pad_unblock (FlowPad *pad)
{
  FlowElement *element = pad->owner_element;

  element_dispatch_enter (element);
  pad_dispatch_enter (pad);

  if G_LIKELY (pad->connected_pad)
    flow_pad_unblock (pad->connected_pad);

  process_output (pad);

  pad_dispatch_leave (pad);

  /* At this point, pad may be gone (if dynamic) */

  element_dispatch_leave (element);

  /* At this point, pad and element may be gone */
}

static void
flow_input_pad_push (FlowPad *pad, FlowPacket *packet)
{
  FlowElement *element = pad->owner_element;

  element_dispatch_enter (element);

  if (packet)
  {
    if (!pad->packet_queue)
      pad->packet_queue = flow_packet_queue_new ();

    flow_packet_queue_push_packet (pad->packet_queue, packet);
  }

  process_output (pad);

  /* At this point, pad may be gone (if dynamic) */

  element_dispatch_leave (element);

  /* At this point, pad and element may be gone */
}

static void
flow_input_pad_type_init (GType type)
{
}

static void
flow_input_pad_class_init (FlowInputPadClass *klass)
{
  FlowPadClass *pad_klass = FLOW_PAD_CLASS (klass);

  pad_klass->block   = flow_input_pad_block;
  pad_klass->unblock = flow_input_pad_unblock;
  pad_klass->push    = flow_input_pad_push;
}

static void
flow_input_pad_init (FlowInputPad *input_pad)
{
}

static void
flow_input_pad_construct (FlowInputPad *input_pad)
{
}

static void
flow_input_pad_dispose (FlowInputPad *input_pad)
{
}

static void
flow_input_pad_finalize (FlowInputPad *input_pad)
{
}

/* --- FlowInputPad public API --- */

