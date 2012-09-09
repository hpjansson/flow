/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-controller.c - Flow rate measurement and control element.
 *
 * Copyright (C) 2012 Hans Petter Jansson
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
#include "flow-controller.h"

/* --- FlowController private data --- */

struct _FlowControllerPrivate
{
  guint timeout_id;
  gint64 last_timeout_time;  /* in microseconds */
  guint64 byte_total;
  guint64 last_byte_total;
  guint64 byte_rate;
};

/* --- FlowController properties --- */

static guint64
flow_controller_get_byte_total_internal (FlowController *controller)
{
  FlowControllerPrivate *priv = controller->priv;

  return priv->byte_total;
}

static guint64
flow_controller_get_byte_rate_internal (FlowController *controller)
{
  FlowControllerPrivate *priv = controller->priv;

  return priv->byte_rate;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_controller)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT64,
                               "byte-total", "Total bytes counted",
                               "Count of bytes that have passed through the controller",
                               G_PARAM_READABLE,
                               flow_controller_get_byte_total_internal,
                               NULL,
                               0, G_MAXUINT64, 0)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT64,
                               "byte-rate", "Byte rate",
                               "Bytes passing through the controller per second",
                               G_PARAM_READABLE,
                               flow_controller_get_byte_rate_internal,
                               NULL,
                               0, G_MAXUINT64, 0)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowController definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_controller, FlowController, FLOW_TYPE_SIMPLEX_ELEMENT, 0)

/* --- FlowController implementation --- */

static gboolean
timeout_cb (FlowController *controller)
{
  FlowControllerPrivate *priv = controller->priv;
  gint64 now;
  gint64 time_diff;
  guint64 byte_diff;

  if (priv->last_byte_total > priv->byte_total)
    priv->last_byte_total = priv->byte_total;

  now = g_get_monotonic_time ();
  time_diff = now - priv->last_timeout_time;

  if (time_diff > 100000)
  {
    byte_diff = ((priv->byte_total - priv->last_byte_total) * 1000000) / time_diff;
    priv->byte_rate = (priv->byte_rate * 16 + byte_diff * 2) / 18;
  }

  priv->last_timeout_time = now;
  priv->last_byte_total = priv->byte_total;

  return TRUE;
}

static void
flow_controller_process_input (FlowController *controller, FlowPad *input_pad)
{
  FlowControllerPrivate *priv    = controller->priv;
  FlowElement           *element = (FlowElement *) controller;
  FlowPacketQueue       *packet_queue;
  FlowPacket            *packet;
  FlowPad               *output_pad;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  output_pad = g_ptr_array_index (element->output_pads, 0);

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    flow_handle_universal_events (element, packet);
    priv->byte_total += flow_packet_get_size (packet);
    flow_pad_push (output_pad, packet);
  }
}

static void
flow_controller_output_pad_unblocked (FlowController *controller, FlowPad *output_pad)
{
  FlowControllerPrivate *priv      = controller->priv;
  FlowElement           *element   = (FlowElement *) controller;
  FlowPad               *input_pad = g_ptr_array_index (element->input_pads, 0);

  flow_controller_process_input (controller, input_pad);
  flow_pad_unblock (input_pad);
}

static void
flow_controller_type_init (GType type)
{
}

static void
flow_controller_class_init (FlowControllerClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->output_pad_unblocked = (void (*) (FlowElement *, FlowPad *)) flow_controller_output_pad_unblocked;
  element_klass->process_input        = (void (*) (FlowElement *, FlowPad *)) flow_controller_process_input;
}

static void
flow_controller_init (FlowController *controller)
{
  FlowControllerPrivate *priv = controller->priv;

  priv->timeout_id = flow_timeout_add_seconds_to_current_thread (1, (GSourceFunc) timeout_cb, controller);
  priv->last_timeout_time = g_get_monotonic_time ();
}

static void
flow_controller_construct (FlowController *controller)
{
}

static void
flow_controller_dispose (FlowController *controller)
{
}

static void
flow_controller_finalize (FlowController *controller)
{
  FlowControllerPrivate *priv = controller->priv;

  flow_source_remove_from_current_thread (priv->timeout_id);
}

/* --- FlowController public API --- */

FlowController *
flow_controller_new (void)
{
  return g_object_new (FLOW_TYPE_CONTROLLER, NULL);
}

guint64
flow_controller_get_byte_total (FlowController *controller)
{
  g_return_val_if_fail (FLOW_IS_CONTROLLER (controller), 0);

  return flow_controller_get_byte_total_internal (controller);
}

guint64
flow_controller_get_byte_rate (FlowController *controller)
{
  g_return_val_if_fail (FLOW_IS_CONTROLLER (controller), 0);

  return flow_controller_get_byte_rate_internal (controller);
}
