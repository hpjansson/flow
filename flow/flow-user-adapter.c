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

/* --- FlowUserAdapter private data --- */

struct _FlowUserAdapterPrivate
{
  FlowPacketQueue    *input_queue;
  FlowPacketQueue    *output_queue;

  GMainLoop          *input_loop;
  GMainLoop          *output_loop;

  FlowNotifyFunc      input_notify_func;
  gpointer            input_notify_data;

  FlowNotifyFunc      output_notify_func;
  gpointer            output_notify_data;

  guint               io_callback_id;

  /* waiting_for_... means the user is in a blocking call. Blocking calls can
   * be recursive if the GMainContext is shared with other facilities, so we
   * maintain a depth count.
   *
   * This is used to prevent async notifications from firing while we're in
   * a blocking call (for apps that mix sync and async access). */

  guint               input_is_blocked   :  1;
  guint               output_is_blocked  :  1;
  guint               waiting_for_input  : 14;
  guint               waiting_for_output : 14;
};

/* --- FlowUserAdapter properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_user_adapter)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowUserAdapter definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_user_adapter, FlowUserAdapter, FLOW_TYPE_SIMPLEX_ELEMENT, 0)

/* --- FlowUserAdapter implementation --- */

static inline void
notify_user_of_input (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv = user_adapter->priv;

  if (!priv->input_is_blocked && priv->input_notify_func)
    priv->input_notify_func (priv->input_notify_data);
}

static inline void
notify_user_of_output (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv = user_adapter->priv;

  if (!priv->output_is_blocked && priv->output_notify_func)
    priv->output_notify_func (priv->output_notify_data);
}

static void
flow_user_adapter_push_output (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv       = user_adapter->priv;
  FlowElement            *element    = FLOW_ELEMENT (user_adapter);
  FlowPad                *output_pad = g_ptr_array_index (element->output_pads, 0);

  while (!flow_pad_is_blocked (output_pad))
  {
    FlowPacket *packet;

    packet = flow_packet_queue_pop_packet (priv->output_queue);
    if (!packet)
    {
      if (priv->waiting_for_output)
      {
        /* g_main_loop_is_running () is cheap compared to g_main_loop_quit () */
        if (g_main_loop_is_running (priv->output_loop))
          g_main_loop_quit (priv->output_loop);
      }
      else
      {
        notify_user_of_output (user_adapter);
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
  FlowUserAdapter        *user_adapter = FLOW_USER_ADAPTER (element);
  FlowUserAdapterPrivate *priv         = user_adapter->priv;
  FlowPacketQueue        *packet_queue;
  FlowPacket             *packet;
  gboolean                received_something = FALSE;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    flow_handle_universal_events (element, packet);
    flow_packet_queue_push_packet (priv->input_queue, packet);
    received_something = TRUE;
  }

  if (priv->waiting_for_input)
  {
    /* g_main_loop_is_running () is cheap compared to g_main_loop_quit () */
    if (g_main_loop_is_running (priv->input_loop))
      g_main_loop_quit (priv->input_loop);
  }
  else
  {
    if (received_something)
      notify_user_of_input (user_adapter);
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
  FlowUserAdapterPrivate *priv = user_adapter->priv;

  priv->input_queue  = flow_packet_queue_new ();
  priv->output_queue = flow_packet_queue_new ();
}

static void
flow_user_adapter_construct (FlowUserAdapter *user_adapter)
{
}

static void
flow_user_adapter_dispose (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv = user_adapter->priv;

  if (priv->io_callback_id)
  {
    flow_source_remove_from_current_thread (priv->io_callback_id);
    priv->io_callback_id = 0;
  }
}

static void
flow_user_adapter_finalize (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv = user_adapter->priv;

  if (priv->input_loop)
  {
    g_main_loop_unref (priv->input_loop);
    priv->input_loop = NULL;
  }

  if (priv->output_loop)
  {
    g_main_loop_unref (priv->output_loop);
    priv->output_loop = NULL;
  }

  flow_gobject_unref_clear (priv->input_queue);
  flow_gobject_unref_clear (priv->output_queue);
}

static gboolean
do_scheduled_io (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv    = user_adapter->priv;
  FlowElement            *element = FLOW_ELEMENT (user_adapter);
  FlowPacketQueue        *input_queue;
  FlowPacketQueue        *output_queue;
  FlowPad                *input_pad;
  FlowPad                *output_pad;

  priv->io_callback_id = 0;

  input_queue  = priv->input_queue;
  output_queue = priv->output_queue;
  input_pad    = g_ptr_array_index (element->input_pads, 0);
  output_pad   = g_ptr_array_index (element->output_pads, 0);

  if (!priv->waiting_for_input)
    flow_user_adapter_process_input (element, input_pad);

  flow_user_adapter_push_output (user_adapter);

  return FALSE;
}

static void
schedule_io (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv = user_adapter->priv;

  if (priv->io_callback_id)
    return;

  priv->io_callback_id =
    flow_idle_add_to_current_thread ((GSourceFunc) do_scheduled_io, user_adapter);
}

/* --- FlowUserAdapter public API --- */

FlowUserAdapter *
flow_user_adapter_new (void)
{
  return g_object_new (FLOW_TYPE_USER_ADAPTER, NULL);
}

void
flow_user_adapter_set_input_notify (FlowUserAdapter *user_adapter, FlowNotifyFunc func, gpointer user_data)
{
  FlowUserAdapterPrivate *priv;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  priv->input_notify_func = func;
  priv->input_notify_data = user_data;

  schedule_io (user_adapter);
}

void
flow_user_adapter_set_output_notify (FlowUserAdapter *user_adapter, FlowNotifyFunc func, gpointer user_data)
{
  FlowUserAdapterPrivate *priv;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  priv->output_notify_func = func;
  priv->output_notify_data = user_data;

  schedule_io (user_adapter);
}

FlowPacketQueue *
flow_user_adapter_get_input_queue (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;

  g_return_val_if_fail (FLOW_IS_USER_ADAPTER (user_adapter), NULL);

  priv = user_adapter->priv;

  return priv->input_queue;
}

FlowPacketQueue *
flow_user_adapter_get_output_queue (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;

  g_return_val_if_fail (FLOW_IS_USER_ADAPTER (user_adapter), NULL);

  priv = user_adapter->priv;

  return priv->output_queue;
}

void
flow_user_adapter_push (FlowUserAdapter *user_adapter)
{
  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  schedule_io (user_adapter);
}

void
flow_user_adapter_block_input (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;
  FlowElement            *element;
  FlowPad                *input_pad;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  priv->input_is_blocked = TRUE;

  /* Don't block the input pad if we're waiting for input somewhere in the stack */

  if (!priv->waiting_for_input)
  {
    element = FLOW_ELEMENT (user_adapter);
    input_pad = g_ptr_array_index (element->input_pads, 0);

    flow_pad_block (input_pad);
  }
}

void
flow_user_adapter_unblock_input (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;
  FlowElement            *element;
  FlowPad                *input_pad;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  priv->input_is_blocked = FALSE;

  element = FLOW_ELEMENT (user_adapter);
  input_pad = g_ptr_array_index (element->input_pads, 0);

  flow_pad_unblock (input_pad);

  schedule_io (user_adapter);
}

void
flow_user_adapter_block_output (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  priv->output_is_blocked = TRUE;
}

void
flow_user_adapter_unblock_output (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  priv->output_is_blocked = FALSE;

  schedule_io (user_adapter);
}

void
flow_user_adapter_wait_for_input (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;
  FlowElement            *element;
  FlowPad                *input_pad;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  priv->waiting_for_input++;

  if G_UNLIKELY (!priv->input_loop)
  {
    GMainContext *main_context;

    main_context = flow_get_main_context_for_current_thread ();
    priv->input_loop = g_main_loop_new (main_context, FALSE);
  }

  /* Make sure the input pad is not blocked when we go to sleep */

  element = FLOW_ELEMENT (user_adapter);
  input_pad = g_ptr_array_index (element->input_pads, 0);

  if (flow_pad_is_blocked (input_pad))
  {
    gint n_packets = flow_packet_queue_get_length_packets (priv->input_queue);

    flow_pad_unblock (input_pad);

    /* If we got packets as a result of unblocking the pad, return immediately */

    if (flow_packet_queue_get_length_packets (priv->input_queue) > n_packets)
    {
      priv->waiting_for_input--;
      return;
    }
  }

  g_main_loop_run (priv->input_loop);

  priv->waiting_for_input--;

  /* If the user doesn't want input notifications, make sure the input pad is blocked */

  if (!priv->waiting_for_input && priv->input_is_blocked)
  {
    input_pad = g_ptr_array_index (FLOW_ELEMENT (user_adapter)->input_pads, 0);
    flow_pad_block (input_pad);
  }
}

void
flow_user_adapter_wait_for_output (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;
  guint                   n_packets;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  priv->waiting_for_output++;

  if G_UNLIKELY (!priv->output_loop)
  {
    GMainContext *main_context;

    main_context = flow_get_main_context_for_current_thread ();
    priv->output_loop = g_main_loop_new (main_context, FALSE);
  }

  /* Make sure we have packets and that the output pad is blocked when we go to sleep */

  flow_user_adapter_push_output (user_adapter);

  n_packets = flow_packet_queue_get_length_packets (priv->output_queue);
  if (!n_packets)
  {
    /* Nothing to send, so return immediately */

    priv->waiting_for_output--;
    return;
  }

  g_main_loop_run (priv->output_loop);

  priv->waiting_for_output--;
}

void
flow_user_adapter_interrupt_input (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  if (!priv->input_loop)
    return;

  /* g_main_loop_is_running () is cheap compared to g_main_loop_quit () */
  if (g_main_loop_is_running (priv->input_loop))
    g_main_loop_quit (priv->input_loop);
}

void
flow_user_adapter_interrupt_output (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;

  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  priv = user_adapter->priv;

  if (!priv->output_loop)
    return;

  /* g_main_loop_is_running () is cheap compared to g_main_loop_quit () */
  if (g_main_loop_is_running (priv->output_loop))
    g_main_loop_quit (priv->output_loop);
}

gboolean
flow_user_adapter_is_output_choked (FlowUserAdapter *user_adapter)
{
  FlowUserAdapterPrivate *priv;
  FlowElement            *element;
  FlowPad                *output_pad;

  g_return_val_if_fail (FLOW_IS_USER_ADAPTER (user_adapter), FALSE);

  element = (FlowElement *) user_adapter;
  output_pad = g_ptr_array_index (element->output_pads, 0);

  return flow_pad_is_blocked (output_pad);
}

