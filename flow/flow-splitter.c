/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-splitter.c - A one-to-many unidirectional element.
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
#include "flow-splitter.h"

/* --- FlowSplitter private data --- */

struct _FlowSplitterPrivate
{
  guint64 buffer_limit;
  guint64 buffer_low_water_mark;
  FlowPacketQueue *output_queue;
  GHashTable *output_queue_iters;
};

/* --- FlowSplitter properties --- */

static guint64
flow_splitter_get_buffer_size_internal (FlowSplitter *splitter)
{
  FlowSplitterPrivate *priv = splitter->priv;

  return priv->buffer_limit;
}

static void
flow_splitter_set_buffer_size_internal (FlowSplitter *splitter, guint64 buffer_limit)
{
  FlowSplitterPrivate *priv = splitter->priv;

  priv->buffer_limit = buffer_limit;
  priv->buffer_low_water_mark = (buffer_limit * 50) / 256;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_splitter)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT64,
                               "buffer-limit", "Buffer limit", "Buffer limit",
                               G_PARAM_READWRITE,
                               flow_splitter_get_buffer_size_internal,
                               flow_splitter_set_buffer_size_internal,
                               0, G_MAXUINT64, 0)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowSplitter definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_splitter, FlowSplitter, FLOW_TYPE_ELEMENT, 0)

/* --- FlowSplitter implementation --- */

static void
push_to_output_pad (FlowSplitter *splitter, FlowPad *output_pad)
{
  FlowSplitterPrivate *priv = splitter->priv;
  FlowPacketIter iter;

  iter = g_hash_table_lookup (priv->output_queue_iters, output_pad);

  if (!flow_pad_is_blocked (output_pad) && flow_packet_iter_next (priv->output_queue, &iter))
  {
    do
    {
      FlowPacket *packet = flow_packet_iter_peek_packet (priv->output_queue, &iter);
      flow_pad_push (output_pad, flow_packet_ref (packet));
    }
    while (!flow_pad_is_blocked (output_pad) && flow_packet_iter_next (priv->output_queue, &iter));

    g_hash_table_insert (priv->output_queue_iters, output_pad, iter);
  }
}

static void
trim_output_queue (FlowElement *element)
{
  FlowSplitterPrivate *priv = ((FlowSplitter *) element)->priv;
  FlowPacketIter iter = NULL;
  gboolean stop = FALSE;
  gint n_drop = 0;
  gint inc = 1;
  guint i;

  while (!stop && flow_packet_iter_next (priv->output_queue, &iter))
  {
    for (i = 0; i < element->output_pads->len; i++)
    {
      FlowPad *output_pad = g_ptr_array_index (element->output_pads, i);
      FlowPacketIter this_iter = g_hash_table_lookup (priv->output_queue_iters, output_pad);

      if (this_iter == NULL)
      {
        stop = TRUE;
        inc = 0;
        break;
      }

      if (this_iter == iter)
        stop = TRUE;
    }

    n_drop += inc;
  }

  if (n_drop > 0)
  {
    for (i = 0; i < element->output_pads->len; i++)
    {
      FlowPad *output_pad = g_ptr_array_index (element->output_pads, i);
      FlowPacketIter this_iter = g_hash_table_lookup (priv->output_queue_iters, output_pad);

      if (this_iter == iter)
        g_hash_table_insert (priv->output_queue_iters, output_pad, NULL);
    }

    for (i = 0; i < n_drop; i++)
      flow_packet_queue_drop_packet (priv->output_queue);
  }
}

static void
flow_splitter_output_pad_blocked (FlowElement *element, FlowPad *output_pad)
{
}

static void
flow_splitter_output_pad_unblocked (FlowElement *element, FlowPad *output_pad)
{
  FlowSplitterPrivate *priv = ((FlowSplitter *) element)->priv;

  /* Push packets to output_pad until re-blocked or end-of-queue */
  push_to_output_pad ((FlowSplitter *) element, output_pad);
  trim_output_queue (element);

  if (flow_packet_queue_get_length_bytes (priv->output_queue) <= priv->buffer_low_water_mark)
  {
    FlowPad *input_pad = g_ptr_array_index (element->input_pads, 0);
    flow_pad_unblock (input_pad);
  }
}

static void
flow_splitter_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowSplitterPrivate *priv = ((FlowSplitter *) element)->priv;
  FlowPacketQueue     *packet_queue;
  FlowPacket          *packet;
  guint                i;

  packet_queue = flow_pad_get_packet_queue (input_pad);

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    flow_handle_universal_events (element, packet);
    flow_packet_queue_push_packet (priv->output_queue, packet);
  }

  for (i = 0; i < element->output_pads->len; i++)
  {
    FlowPad *output_pad = g_ptr_array_index (element->output_pads, i);
    push_to_output_pad ((FlowSplitter *) element, output_pad);
  }

  trim_output_queue (element);

  if (flow_packet_queue_get_length_bytes (priv->output_queue) > priv->buffer_limit)
  {
    FlowPad *input_pad = g_ptr_array_index (element->input_pads, 0);
    flow_pad_block (input_pad);
  }
}

static void
flow_splitter_type_init (GType type)
{
}

static void
flow_splitter_class_init (FlowSplitterClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->output_pad_blocked   = flow_splitter_output_pad_blocked;
  element_klass->output_pad_unblocked = flow_splitter_output_pad_unblocked;
  element_klass->process_input        = flow_splitter_process_input;
}

static void
flow_splitter_init (FlowSplitter *splitter)
{
  FlowSplitterPrivate *priv = splitter->priv;
  FlowElement *element = (FlowElement *) splitter;

  g_ptr_array_add (element->input_pads, g_object_new (FLOW_TYPE_INPUT_PAD, "owner-element", splitter, NULL));
  priv->output_queue = flow_packet_queue_new ();
  priv->output_queue_iters = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
flow_splitter_construct (FlowSplitter *splitter)
{
}

static void
flow_splitter_dispose (FlowSplitter *splitter)
{
  FlowSplitterPrivate *priv = splitter->priv;

  if (priv->output_queue)
  {
    g_object_unref (priv->output_queue);
    priv->output_queue = NULL;
  }
}

static void
flow_splitter_finalize (FlowSplitter *splitter)
{
  FlowSplitterPrivate *priv = splitter->priv;

  g_hash_table_destroy (priv->output_queue_iters);
}

/* --- FlowSplitter public API --- */

FlowSplitter *
flow_splitter_new (void)
{
  return g_object_new (FLOW_TYPE_SPLITTER, NULL);
}

FlowInputPad *
flow_splitter_get_input_pad (FlowSplitter *splitter)
{
  FlowElement *element = (FlowElement *) splitter;

  g_return_val_if_fail (FLOW_IS_SPLITTER (splitter), NULL);

  return g_ptr_array_index (element->input_pads, 0);
}

FlowOutputPad *
flow_splitter_add_output_pad (FlowSplitter *splitter)
{
  FlowSplitterPrivate *priv = splitter->priv;
  FlowElement   *element = (FlowElement *) splitter;
  FlowOutputPad *output_pad;

  g_return_val_if_fail (FLOW_IS_SPLITTER (splitter), NULL);

  output_pad = g_object_new (FLOW_TYPE_OUTPUT_PAD, "owner-element", splitter, NULL);
  flow_g_ptr_array_add_sparse (element->output_pads, output_pad);

  push_to_output_pad ((FlowSplitter *) element, (FlowPad *) output_pad);
  trim_output_queue (element);

  if (flow_packet_queue_get_length_bytes (priv->output_queue) <= priv->buffer_low_water_mark)
  {
    FlowPad *input_pad = g_ptr_array_index (element->input_pads, 0);
    flow_pad_unblock (input_pad);
  }

  return output_pad;
}

void
flow_splitter_remove_output_pad (FlowSplitter *splitter, FlowOutputPad *output_pad)
{
  FlowSplitterPrivate *priv;
  FlowElement *element = (FlowElement *) splitter;
  gboolean     was_removed;

  g_return_if_fail (FLOW_IS_SPLITTER (splitter));
  g_return_if_fail (FLOW_IS_OUTPUT_PAD (output_pad));

  priv = splitter->priv;

  if (element->dispatch_depth)
  {
    was_removed = flow_g_ptr_array_remove_sparse (element->output_pads, output_pad);
    element->output_pad_removed = TRUE;  /* Can't use was_removed because it may already be TRUE */
  }
  else
  {
    was_removed = g_ptr_array_remove_fast (element->output_pads, output_pad);
  }

  if (!was_removed)
  {
    g_warning ("Tried to remove unknown output pad from splitter!");
    return;
  }

  g_hash_table_remove (priv->output_queue_iters, output_pad);
  g_object_unref (output_pad);
}

guint64
flow_splitter_get_buffer_limit (FlowSplitter *splitter)
{
  FlowSplitterPrivate *priv;

  g_return_val_if_fail (FLOW_IS_SPLITTER (splitter), 0);

  priv = splitter->priv;
  return priv->buffer_limit;
}

void
flow_splitter_set_buffer_limit (FlowSplitter *splitter, guint64 limit)
{
  g_return_if_fail (FLOW_IS_SPLITTER (splitter));

  g_object_set (splitter, "buffer-limit", limit, NULL);
}
