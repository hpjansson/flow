/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-user-adapter.c - Lets arbitrary code interface with a stream.
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
#include "flow-user-adapter.h"

/* --- FlowUserAdapter properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_user_adapter)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowUserAdapter definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_user_adapter, FlowUserAdapter, FLOW_TYPE_SIMPLEX_ELEMENT, 0)

/* --- FlowUserAdapter implementation --- */

static inline void
notify_user (FlowUserAdapter *user_adapter)
{
  if (user_adapter->user_notify_func)
    user_adapter->user_notify_func (user_adapter->user_notify_data);
}

static void
flow_user_adapter_push_from_user (FlowUserAdapter *user_adapter)
{
  FlowElement *element    = FLOW_ELEMENT (user_adapter);
  FlowPad     *output_pad = g_ptr_array_index (element->output_pads, 0);

  while (!flow_pad_is_blocked (output_pad))
  {
    FlowPacket *packet;

    packet = flow_packet_queue_pop_packet (user_adapter->queue_from_user);
    if (!packet)
      break;

    flow_handle_universal_events (element, packet);
    flow_pad_push (output_pad, packet);
  }
}

static void
flow_user_adapter_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowUserAdapter *user_adapter = FLOW_USER_ADAPTER (element);
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  packet_queue = flow_pad_get_packet_queue (input_pad);

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    flow_handle_universal_events (element, packet);
    flow_packet_queue_push_packet (user_adapter->queue_to_user, packet);
  }

  notify_user (user_adapter);
}

static void
flow_user_adapter_type_init (GType type)
{
}

static void
flow_user_adapter_class_init (FlowUserAdapterClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->output_pad_blocked   = NULL;
  element_klass->output_pad_unblocked = (void (*) (FlowElement *, FlowPad *)) flow_user_adapter_push_from_user;
  element_klass->process_input        = flow_user_adapter_process_input;
}

static void
flow_user_adapter_init (FlowUserAdapter *user_adapter)
{
  user_adapter->queue_to_user   = flow_packet_queue_new ();
  user_adapter->queue_from_user = flow_packet_queue_new ();
}

static void
flow_user_adapter_construct (FlowUserAdapter *user_adapter)
{
}

static void
flow_user_adapter_dispose (FlowUserAdapter *user_adapter)
{
}

static void
flow_user_adapter_finalize (FlowUserAdapter *user_adapter)
{
  flow_gobject_unref_clear (user_adapter->queue_to_user);
  flow_gobject_unref_clear (user_adapter->queue_from_user);
}

/* --- FlowUserAdapter public API --- */

FlowUserAdapter *
flow_user_adapter_new (void)
{
  return g_object_new (FLOW_TYPE_USER_ADAPTER, NULL);
}

void
flow_user_adapter_set_notify_func (FlowUserAdapter *user_adapter, FlowNotifyFunc func, gpointer user_data)
{
  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  user_adapter->user_notify_func = func;
  user_adapter->user_notify_data = user_data;

  if (func && flow_packet_queue_get_length_packets (user_adapter->queue_to_user))
    notify_user (user_adapter);
}

FlowPacketQueue *
flow_user_adapter_get_input_queue (FlowUserAdapter *user_adapter)
{
  g_return_val_if_fail (FLOW_IS_USER_ADAPTER (user_adapter), NULL);

  return user_adapter->queue_to_user;
}

FlowPacketQueue *
flow_user_adapter_get_output_queue (FlowUserAdapter *user_adapter)
{
  g_return_val_if_fail (FLOW_IS_USER_ADAPTER (user_adapter), NULL);

  return user_adapter->queue_from_user;
}

void
flow_user_adapter_push (FlowUserAdapter *user_adapter)
{
  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  flow_user_adapter_push_from_user (user_adapter);
}

void
flow_user_adapter_block (FlowUserAdapter *user_adapter)
{
  FlowElement *element;
  FlowPad     *input_pad;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  element = FLOW_ELEMENT (user_adapter);
  input_pad = g_ptr_array_index (element->input_pads, 0);

  flow_pad_block (input_pad);
}

void
flow_user_adapter_unblock (FlowUserAdapter *user_adapter)
{
  FlowElement *element;
  FlowPad     *input_pad;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  element = FLOW_ELEMENT (user_adapter);
  input_pad = g_ptr_array_index (element->input_pads, 0);

  flow_pad_unblock (input_pad);
}
