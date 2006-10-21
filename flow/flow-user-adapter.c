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
#include "flow-context-mgmt.h"
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
flow_user_adapter_push_output (FlowUserAdapter *user_adapter)
{
  FlowElement *element    = FLOW_ELEMENT (user_adapter);
  FlowPad     *output_pad = g_ptr_array_index (element->output_pads, 0);

  while (!flow_pad_is_blocked (output_pad))
  {
    FlowPacket *packet;

    packet = flow_packet_queue_pop_packet (user_adapter->output_queue);
    if (!packet)
    {
      if (user_adapter->waiting_for_output)
      {
        /* g_main_loop_is_running () is cheap compared to g_main_loop_quit () */
        if (g_main_loop_is_running (user_adapter->output_loop))
          g_main_loop_quit (user_adapter->output_loop);
      }
      else
      {
        notify_user (user_adapter);
      }

      break;
    }

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
    flow_packet_queue_push_packet (user_adapter->input_queue, packet);
  }

  if (user_adapter->waiting_for_input)
  {
    /* g_main_loop_is_running () is cheap compared to g_main_loop_quit () */
    if (g_main_loop_is_running (user_adapter->input_loop))
      g_main_loop_quit (user_adapter->input_loop);
  }
  else
  {
    notify_user (user_adapter);
  }
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
  element_klass->output_pad_unblocked = (void (*) (FlowElement *, FlowPad *)) flow_user_adapter_push_output;
  element_klass->process_input        = flow_user_adapter_process_input;
}

static void
flow_user_adapter_init (FlowUserAdapter *user_adapter)
{
  user_adapter->input_queue   = flow_packet_queue_new ();
  user_adapter->output_queue = flow_packet_queue_new ();
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
  if (user_adapter->input_loop)
  {
    g_main_loop_unref (user_adapter->input_loop);
    user_adapter->input_loop = NULL;
  }

  if (user_adapter->output_loop)
  {
    g_main_loop_unref (user_adapter->output_loop);
    user_adapter->output_loop = NULL;
  }

  flow_gobject_unref_clear (user_adapter->input_queue);
  flow_gobject_unref_clear (user_adapter->output_queue);
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

  if (func && flow_packet_queue_get_length_packets (user_adapter->input_queue))
    notify_user (user_adapter);
}

FlowPacketQueue *
flow_user_adapter_get_input_queue (FlowUserAdapter *user_adapter)
{
  g_return_val_if_fail (FLOW_IS_USER_ADAPTER (user_adapter), NULL);

  return user_adapter->input_queue;
}

FlowPacketQueue *
flow_user_adapter_get_output_queue (FlowUserAdapter *user_adapter)
{
  g_return_val_if_fail (FLOW_IS_USER_ADAPTER (user_adapter), NULL);

  return user_adapter->output_queue;
}

void
flow_user_adapter_push (FlowUserAdapter *user_adapter)
{
  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  flow_user_adapter_push_output (user_adapter);
}

void
flow_user_adapter_block (FlowUserAdapter *user_adapter)
{
  FlowElement *element;
  FlowPad     *input_pad;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  /* Don't block the input pad if we're waiting for input somewhere in the stack */

  if (!user_adapter->waiting_for_input)
  {
    element = FLOW_ELEMENT (user_adapter);
    input_pad = g_ptr_array_index (element->input_pads, 0);

    flow_pad_block (input_pad);
  }
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

void
flow_user_adapter_wait_for_input (FlowUserAdapter *user_adapter)
{
  FlowElement *element;
  FlowPad     *input_pad;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  user_adapter->waiting_for_input++;

  if G_UNLIKELY (!user_adapter->input_loop)
  {
    GMainContext *main_context;

    main_context = flow_get_main_context_for_current_thread ();
    user_adapter->input_loop = g_main_loop_new (main_context, FALSE);
  }

  /* Make sure the input pad is not blocked when we go to sleep */

  element = FLOW_ELEMENT (user_adapter);
  input_pad = g_ptr_array_index (element->input_pads, 0);

  if (flow_pad_is_blocked (input_pad))
  {
    guint n_packets = flow_packet_queue_get_length_packets (user_adapter->input_queue);

    flow_pad_unblock (input_pad);

    /* If we got packets as a result of unblocking the pad, return immediately */

    if (flow_packet_queue_get_length_packets (user_adapter->input_queue) > n_packets)
    {
      user_adapter->waiting_for_input--;
      return;
    }
  }

  g_main_loop_run (user_adapter->input_loop);

  user_adapter->waiting_for_input--;
}

void
flow_user_adapter_wait_for_output (FlowUserAdapter *user_adapter)
{
  FlowElement *element;
  FlowPad     *output_pad;
  guint        n_packets;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  user_adapter->waiting_for_output++;

  if G_UNLIKELY (!user_adapter->output_loop)
  {
    GMainContext *main_context;

    main_context = flow_get_main_context_for_current_thread ();
    user_adapter->output_loop = g_main_loop_new (main_context, FALSE);
  }

  /* Make sure we have packets and that the output pad is blocked when we go to sleep */

  element = FLOW_ELEMENT (user_adapter);
  output_pad = g_ptr_array_index (element->output_pads, 0);

  if (!flow_pad_is_blocked (output_pad))
    flow_user_adapter_push_output (user_adapter);

  n_packets = flow_packet_queue_get_length_packets (user_adapter->output_queue);
  if (!n_packets)
  {
    /* Nothing to send, so return immediately */
    user_adapter->waiting_for_output--;
    return;
  }

  g_main_loop_run (user_adapter->output_loop);

  user_adapter->waiting_for_output--;
}
