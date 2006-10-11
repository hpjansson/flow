/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-output-pad.c - An element output connector.
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
#include "flow-output-pad.h"

/* --- FlowOutputPad properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_output_pad)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowOutputPad definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_output_pad, FlowOutputPad, FLOW_TYPE_PAD, 0)

/* --- FlowOutputPad implementation --- */

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
try_push_to_connected (FlowPad *pad)
{
  if (!pad->packet_queue)
    return;

  pad_dispatch_enter (pad);

  while (!pad->is_blocked && pad->connected_pad)
  {
    FlowPacket *packet = flow_packet_queue_pop_packet (pad->packet_queue);

    if (!packet)
    {
      /* Output pads are unlikely to accumulate packets, since as long as they're
       * connected and unblocked, they will pass them on without queuing. Therefore,
       * we can destroy the queue itself when it's not in use, and save some memory. */

      g_object_unref (pad->packet_queue);
      pad->packet_queue = NULL;
      break;
    }

    flow_pad_push (pad->connected_pad, packet);
  }

  pad_dispatch_leave (pad);

  /* At this point, pad may be gone (if dynamic) */
}

static void
flow_output_pad_block (FlowPad *pad)
{
  FlowElement      *element = pad->owner_element;
  FlowElementClass *element_klass;

  element_dispatch_enter (element);

  element_klass = FLOW_ELEMENT_GET_CLASS (element);
  if (element_klass->output_pad_blocked)
    element_klass->output_pad_blocked (element, pad);

  /* At this point, pad may be gone (if dynamic) */

  element_dispatch_leave (element);

  /* At this point, pad and element may be gone */
}

static void
flow_output_pad_unblock (FlowPad *pad)
{
  FlowElement      *element = pad->owner_element;
  FlowElementClass *element_klass;

  element_dispatch_enter (element);
  pad_dispatch_enter (pad);

  element_klass = FLOW_ELEMENT_GET_CLASS (element);
  if (element_klass->output_pad_blocked)
    element_klass->output_pad_unblocked (element, pad);

  try_push_to_connected (pad);

  pad_dispatch_leave (pad);

  /* At this point, pad may be gone (if dynamic) */

  element_dispatch_leave (element);

  /* At this point, pad and element may be gone */
}

static void
flow_output_pad_push (FlowPad *pad, FlowPacket *packet)
{
  FlowElement *element = pad->owner_element;

  element_dispatch_enter (element);

  if (!packet)
  {
    try_push_to_connected (pad);
  }
  else if G_LIKELY (!pad->is_blocked && pad->connected_pad)
  {
    flow_pad_push (pad->connected_pad, packet);
  }
  else
  {
    if (!pad->packet_queue)
      pad->packet_queue = flow_packet_queue_new ();

    flow_packet_queue_push_packet (pad->packet_queue, packet);
  }

  /* At this point, pad may be gone (if dynamic) */

  element_dispatch_leave (element);

  /* At this point, pad and element may be gone */
}

static void
flow_output_pad_type_init (GType type)
{
}

static void
flow_output_pad_class_init (FlowOutputPadClass *klass)
{
  FlowPadClass *pad_klass = FLOW_PAD_CLASS (klass);

  pad_klass->block   = flow_output_pad_block;
  pad_klass->unblock = flow_output_pad_unblock;
  pad_klass->push    = flow_output_pad_push;
}

static void
flow_output_pad_init (FlowOutputPad *output_pad)
{
}

static void
flow_output_pad_construct (FlowOutputPad *output_pad)
{
}

static void
flow_output_pad_dispose (FlowOutputPad *output_pad)
{
}

static void
flow_output_pad_finalize (FlowOutputPad *output_pad)
{
}

/* --- FlowOutputPad public API --- */
