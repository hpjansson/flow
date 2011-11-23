/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-collector.c - A packet endpoint.
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
#include "flow-collector.h"

/* --- FlowCollector private data --- */

struct _FlowCollectorPrivate
{
};

/* --- FlowCollector properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_collector)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowCollector definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_collector, FlowCollector, FLOW_TYPE_ELEMENT, G_TYPE_FLAG_ABSTRACT)

/* --- FlowCollector implementation --- */

static void
flow_collector_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  packet_queue = flow_pad_get_packet_queue (input_pad);

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    flow_handle_universal_events (element, packet);
    flow_packet_unref (packet);
  }
}

static void
flow_collector_type_init (GType type)
{
}

static void
flow_collector_class_init (FlowCollectorClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->process_input = flow_collector_process_input;
}

static void
flow_collector_init (FlowCollector *collector)
{
  FlowElement *element = (FlowElement *) collector;

  g_ptr_array_add (element->input_pads, g_object_new (FLOW_TYPE_INPUT_PAD, "owner-element", collector, NULL));
}

static void
flow_collector_construct (FlowCollector *collector)
{
}

static void
flow_collector_dispose (FlowCollector *collector)
{
}

static void
flow_collector_finalize (FlowCollector *collector)
{
}

/* --- FlowCollector public API --- */

/**
 * flow_collector_get_input_pad:
 * @collector: A #FlowCollector.
 * 
 * Gets @collector's input pad.
 * 
 * Return value: A #FlowInputPad.
 **/
FlowInputPad *
flow_collector_get_input_pad (FlowCollector *collector)
{
  FlowElement *element = (FlowElement *) collector;

  g_return_val_if_fail (FLOW_IS_COLLECTOR (collector), NULL);

  return g_ptr_array_index (element->input_pads, 0);
}
