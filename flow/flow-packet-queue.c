/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-packet-queue.c - A FlowPacket queue that can do partial dequeues.
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
#include "flow-packet-queue.h"
#include <string.h>  /* memcpy */

/* --- FlowPacketQueue properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_packet_queue)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowPacketQueue definition --- */

FLOW_GOBJECT_MAKE_IMPL_NO_PRIVATE (flow_packet_queue, FlowPacketQueue, G_TYPE_OBJECT, 0)

/* --- FlowPacketQueue implementation --- */

/* Because we special-case the first packet pushed to an
 * otherwise empty queue, we need the following three helpers
 * to simplify the push/pop logic.
 *
 * The special casing saves us having to allocate a GList node
 * in the common case where a single packet is pushed and then
 * immediately popped. This happens because elements don't
 * necessarily process on a per-packet basis, so we always queue
 * to input pads.
 *
 * For more details, see flow-input-pad.c.
 *
 * FIXME: The whole first_packet deal complicates the code too much. We
 * should switch to using a circular pointer array instead of GQueue,
 * obviating the need for a faster special case.
 **/

static inline FlowPacket *
peek_packet (FlowPacketQueue *packet_queue)
{
  FlowPacket *packet;

  packet = packet_queue->first_packet;
  if (packet)
    return packet;

  return g_queue_peek_head (packet_queue->queue);
}

static inline FlowPacket *
pop_packet (FlowPacketQueue *packet_queue)
{
  FlowPacket *packet;

  packet = packet_queue->first_packet;

  if (packet)
  {
    packet_queue->first_packet = NULL;
    return packet;
  }

  return g_queue_pop_head (packet_queue->queue);
}

static inline void
push_packet (FlowPacketQueue *packet_queue, FlowPacket *packet)
{
  if (!packet_queue->queue->head && !packet_queue->first_packet)
  {
    packet_queue->first_packet = packet;
    return;
  }

  g_queue_push_tail (packet_queue->queue, packet);
}

static inline void
push_packet_to_head (FlowPacketQueue *packet_queue, FlowPacket *packet)
{
  if (!packet_queue->queue->head && !packet_queue->first_packet)
  {
    packet_queue->first_packet = packet;
    return;
  }

  g_queue_push_head (packet_queue->queue, packet);
}

static gint
pop_bytes (FlowPacketQueue *packet_queue, gpointer dest, gint max)
{
  FlowPacket *packet;
  gint        dequeued_bytes; 
  gint        remain;
  guint8     *p;

  /* Dequeue */

  for (p = dest, remain = max; remain; )
  {
    guint8     *data;
    gint        packet_len;
    gint        increment;

    packet = peek_packet (packet_queue);
    if (!packet || flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
      break;

    data = flow_packet_get_data (packet);

    packet_len = flow_packet_get_size (packet);
    increment  = packet_len - packet_queue->packet_position;
    increment  = MIN (increment, remain);

    if (dest)
      memcpy (p, data + packet_queue->packet_position, increment);

    remain                        -= increment;
    p                             += increment;
    packet_queue->packet_position += increment;

    if (packet_queue->packet_position == packet_len)
    {
      packet = pop_packet (packet_queue);
      flow_packet_unref (packet);

      packet_queue->packet_position = 0;
    }
  }

  dequeued_bytes = max - remain;

  packet_queue->bytes_in_queue      -= dequeued_bytes;
  packet_queue->data_bytes_in_queue -= dequeued_bytes;

  return dequeued_bytes;
}

static void
consolidate_partial_packet (FlowPacketQueue *packet_queue)
{
  FlowPacket *packet;
  FlowPacket *new_packet;
  guint8     *data;

  if (packet_queue->packet_position == 0)
    return;

  packet = peek_packet (packet_queue);
  g_assert (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER);

  data = flow_packet_get_data (packet) + packet_queue->packet_position;

  new_packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, data,
                                flow_packet_get_size (packet) - packet_queue->packet_position);
  flow_packet_unref (packet);

  if (packet_queue->first_packet == packet)
    packet_queue->first_packet = new_packet;
  else
    packet_queue->queue->head->data = new_packet;

  packet_queue->packet_position = 0;
}

static void
clear_queue (FlowPacketQueue *packet_queue)
{
  FlowPacket *packet;

  packet = packet_queue->first_packet;
  if (packet)
  {
    flow_packet_unref (packet);
    packet_queue->first_packet = NULL;
  }

  while ((packet = g_queue_pop_head (packet_queue->queue)))
  {
    flow_packet_unref (packet);
  }
}

static void
flow_packet_queue_type_init (GType type)
{
}

static void
flow_packet_queue_class_init (FlowPacketQueueClass *klass)
{
}

static void
flow_packet_queue_init (FlowPacketQueue *packet_queue)
{
  packet_queue->queue = g_queue_new ();
}

static void
flow_packet_queue_construct (FlowPacketQueue *packet_queue)
{
}

static void
flow_packet_queue_dispose (FlowPacketQueue *packet_queue)
{
}

static void
flow_packet_queue_finalize (FlowPacketQueue *packet_queue)
{
  clear_queue (packet_queue);
  g_queue_free (packet_queue->queue);
}

/* --- FlowPacketQueue public API --- */

/**
 * flow_packet_queue_new:
 * 
 * Creates a new packet queue.
 * 
 * Return value: A new #FlowPacketQueue.
 **/
FlowPacketQueue *
flow_packet_queue_new (void)
{
  return g_object_new (FLOW_TYPE_PACKET_QUEUE, NULL);
}

gint
flow_packet_queue_get_length_packets (FlowPacketQueue *packet_queue)
{
  gint length;

  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), 0);

  length = packet_queue->queue->length;
  if (packet_queue->first_packet)
    length++;

  return length;
}

gint
flow_packet_queue_get_length_bytes (FlowPacketQueue *packet_queue)
{
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), 0);

  return packet_queue->bytes_in_queue;
}

gint
flow_packet_queue_get_length_data_bytes (FlowPacketQueue *packet_queue)
{
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), 0);

  return packet_queue->data_bytes_in_queue;
}

/**
 * flow_packet_queue_push_packet:
 * @packet_queue: A packet queue.
 * @packet:       A packet.
 * 
 * Pushes @packet onto @packet_queue. Packets are popped in the order they were
 * pushed (first in, first out).
 **/
void
flow_packet_queue_push_packet (FlowPacketQueue *packet_queue, FlowPacket *packet)
{
  gint packet_size;

  g_return_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue));
  g_return_if_fail (packet != NULL);

  if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER &&
      flow_packet_get_size (packet) == 0)
  {
    flow_packet_unref (packet);
    return;
  }

  push_packet (packet_queue, packet);

  packet_size = flow_packet_get_size (packet);
  packet_queue->bytes_in_queue += packet_size;

  if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
    packet_queue->data_bytes_in_queue += packet_size;
}

/**
 * flow_packet_queue_push_bytes:
 * @packet_queue: A packet queue.
 * @src:          A pointer to the data to push.
 * @n:            Number of bytes to push from @src.
 * 
 * This convenience function creates a new packet containing a copy of
 * @n bytes starting at @src, and pushes it onto @packet_queue. Packets are
 * popped in the order they were pushed (first in, first out).
 **/
void
flow_packet_queue_push_bytes (FlowPacketQueue *packet_queue, gconstpointer src, gint n)
{
  FlowPacket *packet;

  g_return_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue));
  g_return_if_fail (src != NULL);
  g_return_if_fail (n >= 0);

  if (n == 0)
    return;

  packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, (gpointer) src, n);
  push_packet (packet_queue, packet);

  packet_queue->bytes_in_queue      += n;
  packet_queue->data_bytes_in_queue += n;
}

/**
 * flow_packet_queue_push_packet_to_head:
 * @packet_queue: A packet queue.
 * @packet:       A packet.
 * 
 * Pushes @packet onto head of @packet_queue. This packet effectively skips
 * the queue, and will be the next packet popped.
 **/
void
flow_packet_queue_push_packet_to_head (FlowPacketQueue *packet_queue, FlowPacket *packet)
{
  gint packet_size;

  g_return_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue));
  g_return_if_fail (packet != NULL);

  if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER &&
      flow_packet_get_size (packet) == 0)
  {
    flow_packet_unref (packet);
    return;
  }

  consolidate_partial_packet (packet_queue);
  push_packet_to_head (packet_queue, packet);

  packet_size = flow_packet_get_size (packet);
  packet_queue->bytes_in_queue += packet_size;

  if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
    packet_queue->data_bytes_in_queue += packet_size;
}

/**
 * flow_packet_queue_pop_packet:
 * @packet_queue: A packet queue.
 * 
 * Pops the next packet from @packet_queue. If flow_packet_queue_pop_bytes ()
 * was called previously, resulting in a partially popped packet, a new packet
 * is created and returned, containing the remainder of that packet's data.
 * 
 * Return value: A packet, or %NULL if the queue is empty.
 **/
FlowPacket *
flow_packet_queue_pop_packet (FlowPacketQueue *packet_queue)
{
  FlowPacket       *packet;
  FlowPacket       *new_packet;
  FlowPacketFormat  packet_format;
  gint              packet_len;
  gint              new_packet_len;

  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), NULL);

  packet = pop_packet (packet_queue);
  if (!packet)
    return NULL;

  packet_len    = flow_packet_get_size (packet);
  packet_format = flow_packet_get_format (packet);

  if (packet_queue->packet_position == 0)
  {
    packet_queue->bytes_in_queue -= packet_len;

    if (packet_format == FLOW_PACKET_FORMAT_BUFFER)
      packet_queue->data_bytes_in_queue -= packet_len;

    return packet;
  }

  /* We're at an odd intra-packet position. Need to fake a packet. This will only happen
   * if the user mixes calls to pop_packet() and pop_bytes(). */

  g_assert (packet_format == FLOW_PACKET_FORMAT_BUFFER);

  new_packet_len = packet_len - packet_queue->packet_position;
  new_packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER,
                                (guint8 *) flow_packet_get_data (packet) + packet_queue->packet_position,
                                new_packet_len);

  flow_packet_unref (packet);
  packet_queue->packet_position = 0;
  packet_queue->bytes_in_queue      -= new_packet_len;
  packet_queue->data_bytes_in_queue -= new_packet_len;

  return new_packet;
}

gint
flow_packet_queue_pop_bytes (FlowPacketQueue *packet_queue, gpointer dest, gint n_max)
{
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), 0);
  g_return_val_if_fail (n_max >= 0, 0);

  return pop_bytes (packet_queue, dest, n_max);
}

/**
 * flow_packet_queue_pop_bytes_exact:
 * @packet_queue: A packet queue.
 * @dest:         A pointer at which to store the data, or %NULL to discard.
 * @n:            Number of bytes to pop from @packet_queue.
 * 
 * This convenience function pops an arbitrary amount of data from @packet_queue,
 * spanning or partially processing packets if necessary. It will only process
 * contiguous #FLOW_PACKET_FORMAT_BUFFER packets, and refuses to span other
 * packet types. If @packet_queue does not have @n contiguous bytes immediately
 * available, the function does nothing and returns %FALSE.
 * 
 * Return value: %TRUE if the request could be satisfied, %FALSE otherwise.
 **/
gboolean
flow_packet_queue_pop_bytes_exact (FlowPacketQueue *packet_queue, gpointer dest, gint n)
{
  FlowPacket *packet;
  gint        n_contiguous_bytes; 
  gint        dequeued_bytes;
  GList      *l;

  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), FALSE);
  g_return_val_if_fail (n >= 0, FALSE);

  if (n == 0)
    return TRUE;

  /* Quick check, make sure we have enough data overall */

  if (n > packet_queue->data_bytes_in_queue)
    return FALSE;

  /* Make sure we have enough contiguous data */

  packet = peek_packet (packet_queue);
  if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
    return FALSE;

  n_contiguous_bytes = flow_packet_get_size (packet) - packet_queue->packet_position;

  l = packet_queue->queue->head;
  if (!packet_queue->first_packet && l)
    l = g_list_next (l);

  for ( ; n_contiguous_bytes < n; l = g_list_next (l))
  {
    if (!l)
      return FALSE;

    packet = l->data;
    if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
      return FALSE;

    n_contiguous_bytes += flow_packet_get_size (packet);
  }

  /* Dequeue */

  dequeued_bytes = pop_bytes (packet_queue, dest, n);
  g_assert (dequeued_bytes == n);

  return TRUE;
}

gboolean
flow_packet_queue_peek_packet (FlowPacketQueue *packet_queue, FlowPacket **packet_out, gint *offset_out)
{
  FlowPacket *packet;

  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), FALSE);

  if (!offset_out)
    consolidate_partial_packet (packet_queue);

  packet = peek_packet (packet_queue);
  if (!packet)
    return FALSE;

  if (packet_out)
    *packet_out = packet;
  if (offset_out)
    *offset_out = packet_queue->packet_position;

  return TRUE;
}

FlowPacket *
flow_packet_queue_peek_nth_packet (FlowPacketQueue *packet_queue, guint n)
{
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), FALSE);

  consolidate_partial_packet (packet_queue);

  if (packet_queue->first_packet)
  {
    if (n == 0)
      return packet_queue->first_packet;

    n--;
  }

  return g_queue_peek_nth (packet_queue->queue, n);
}

void
flow_packet_queue_peek_packets (FlowPacketQueue *packet_queue, FlowPacket **packets_out, gint *n_packets_out)
{
  FlowPacket *packet;
  GList      *l;
  gint        n;
  gint        i;

  g_return_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue));
  g_return_if_fail (packets_out != NULL);
  g_return_if_fail (n_packets_out != NULL);

  if (*n_packets_out == 0)
    return;

  consolidate_partial_packet (packet_queue);
  n = *n_packets_out;

  packet = peek_packet (packet_queue);
  if (!packet)
  {
    *n_packets_out = 0;
    return;
  }

  packets_out [0] = packet;

  l = packet_queue->queue->head;
  if (!packet_queue->first_packet && l)
    l = l->next;

  for (i = 1; i < n && l; i++, l = l->next)
    packets_out [i] = l->data;

  *n_packets_out = i;
}

gboolean
flow_packet_queue_drop_packet (FlowPacketQueue *packet_queue)
{
  FlowPacket *packet;

  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), FALSE);

  packet = pop_packet (packet_queue);
  if (!packet)
    return FALSE;

  if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
  {
    gint dropped_bytes = flow_packet_get_size (packet) - packet_queue->packet_position;

    g_assert (dropped_bytes >= 0);

    packet_queue->packet_position = 0;
    packet_queue->bytes_in_queue      -= dropped_bytes;
    packet_queue->data_bytes_in_queue -= dropped_bytes;
  }

  flow_packet_unref (packet);
  return TRUE;
}

void
flow_packet_queue_steal (FlowPacketQueue *packet_queue, gint n_packets, gint n_bytes, gint n_data_bytes)
{
  g_return_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue));
  g_return_if_fail (n_packets >= 0);
  g_return_if_fail (n_bytes >= 0);
  g_return_if_fail (n_data_bytes >= 0);

  if (n_bytes > packet_queue->bytes_in_queue)
  {
    g_warning ("Tried to steal more bytes than available!");
    packet_queue->bytes_in_queue = 0;
  }
  else
  {
    packet_queue->bytes_in_queue -= n_bytes;
  }

  if (n_data_bytes > packet_queue->data_bytes_in_queue)
  {
    g_warning ("Tried to steal more data bytes than available!");
    packet_queue->data_bytes_in_queue = 0;
  }
  else
  {
    packet_queue->data_bytes_in_queue -= n_data_bytes;
  }

  packet_queue->packet_position = 0;

  while (n_packets--)
  {
    FlowPacket *packet = pop_packet (packet_queue);

    if (!packet)
    {
      g_warning ("Tried to steal more packets than available!");
      return;
    }
  }
}

/**
 * flow_packet_queue_peek_first_object:
 * @packet_queue: A packet queue.
 * 
 * This convenience function finds the first packet in @packet_queue
 * not of format #FLOW_PACKET_FORMAT_BUFFER and returns it to you,
 * without removing it from the queue.
 * 
 * When using flow_packet_queue_pop_bytes_exact (), you may encounter the
 * situation where you have a few bytes of buffer data, followed by
 * an object indicating a status change, followed by more buffer data.
 * 
 * Processing the object out of order lets you determine if it
 * represents a serious condition (say, EOF) forcing you to discard the
 * preceding (incomplete) data sequence and reset your state machine
 * before processing the subsequent data.
 * 
 * Return value: The first packet containing an object, or %NULL
 *               if none could be found. The packet still belongs to
 *               the queue and may not be freed.
 **/
FlowPacket *
flow_packet_queue_peek_first_object (FlowPacketQueue *packet_queue)
{
  FlowPacket *packet;
  GList      *l;

  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), NULL);

  /* Special-case first packet (see top of file) */

  packet = peek_packet (packet_queue);
  if (!packet)
    return NULL;

  if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
    return packet;

  /* Do rest of queue */

  l = packet_queue->queue->head;
  if (!packet_queue->first_packet && l)
    l = g_list_next (l);

  for ( ; l; l = g_list_next (l))
  {
    packet = l->data;

    if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
      break;
  }

  if (!l)
    return NULL;

  return packet;
}

/**
 * flow_packet_queue_pop_first_object:
 * @packet_queue: A packet queue.
 * 
 * This convenience function finds the first packet in @packet_queue
 * not of format #FLOW_PACKET_FORMAT_BUFFER and returns it to you,
 * removing it from the queue.
 * 
 * Useful for removing an obstruction to flow_packet_queue_pop_bytes_exact ().
 * 
 * See the documentation for flow_packet_queue_peek_first_object ()
 * for a detailed discussion.
 * 
 * Return value: The first packet containing an object, or %NULL
 *               if none could be found.
 **/
FlowPacket *
flow_packet_queue_pop_first_object (FlowPacketQueue *packet_queue)
{
  FlowPacket *packet;
  GList      *l;

  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), NULL);

  /* Special-case first packet (see top of file) */

  packet = peek_packet (packet_queue);
  if (!packet)
    return NULL;

  if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
  {
    pop_packet (packet_queue);
    packet_queue->bytes_in_queue -= flow_packet_get_size (packet);
    return packet;
  }

  /* Do rest of queue */

  l = packet_queue->queue->head;
  if (!packet_queue->first_packet && l)
    l = g_list_next (l);

  for ( ; l; l = g_list_next (l))
  {
    packet = l->data;

    if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
      break;
  }

  if (!l)
    return NULL;

  packet_queue->bytes_in_queue -= flow_packet_get_size (packet);
  g_queue_delete_link (packet_queue->queue, l);

  return packet;
}

/**
 * flow_packet_queue_skip_past_first_object:
 * @packet_queue: A packet queue.
 * 
 * This convenience function frees and removes pending packets in
 * @packet_queue, up to and including the first packet not of format
 * #FLOW_PACKET_FORMAT_BUFFER. If no such packet is found, the whole
 * queue is emptied.
 * 
 * Useful for re-synchronizing a stream for flow_packet_queue_pop_bytes_exact ().
 * 
 * See the documentation for flow_packet_queue_peek_first_object ()
 * for a detailed discussion.
 * 
 * Return value: %TRUE if an object was encountered, %FALSE otherwise.
 **/
gboolean
flow_packet_queue_skip_past_first_object (FlowPacketQueue *packet_queue)
{
  FlowPacket *packet;
 
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), FALSE);

  while ((packet = pop_packet (packet_queue)))
  {
    FlowPacketFormat format = flow_packet_get_format (packet);

    packet_queue->bytes_in_queue -= flow_packet_get_size (packet);
    if (format == FLOW_PACKET_FORMAT_BUFFER)
      packet_queue->data_bytes_in_queue -= flow_packet_get_size (packet);

    flow_packet_unref (packet);

    if (format != FLOW_PACKET_FORMAT_BUFFER)
      break;
  }

  return packet ? TRUE : FALSE;
}

/**
 * flow_packet_queue_clear:
 * @packet_queue: A packet queue.
 * 
 * Frees and removes all pending packets from @packet_queue.
 **/
void
flow_packet_queue_clear (FlowPacketQueue *packet_queue)
{
  g_return_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue));

  clear_queue (packet_queue);
}

FlowPacket *
flow_packet_iter_peek_packet (FlowPacketQueue *packet_queue, FlowPacketIter *packet_iter)
{
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), NULL);
  g_return_val_if_fail (packet_iter != NULL, NULL);

  if (!*packet_iter)
    return NULL;

  if (*packet_iter == (FlowPacketIter) &packet_queue->first_packet)
    return packet_queue->first_packet;

  return ((GList *) (*packet_iter))->data;
}

gboolean
flow_packet_iter_next (FlowPacketQueue *packet_queue, FlowPacketIter *packet_iter)
{
  GList *next;

  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (packet_queue), FALSE);
  g_return_val_if_fail (packet_iter != NULL, FALSE);

  if (!*packet_iter)
  {
    if (packet_queue->first_packet)
    {
      *packet_iter = (FlowPacketIter) &packet_queue->first_packet;
      return TRUE;
    }

    *packet_iter = packet_queue->queue->head;
    if (!*packet_iter)
      return FALSE;

    return TRUE;
  }
  else if (*packet_iter == (FlowPacketIter) &packet_queue->first_packet)
  {
    next = packet_queue->queue->head;
    if (!next)
      return FALSE;

    *packet_iter = next;
    return TRUE;
  }

  next = ((GList *) (*packet_iter))->next;
  if (!next)
    return FALSE;

  *((GList **) packet_iter) = next;
  return TRUE;
}

static gint
byte_iter_peek_bytes (FlowPacketQueue *packet_queue, GList **packet_l_inout, gint *packet_position_inout,
                      gpointer buf_out, gint buf_size, gboolean advance)
{
  GList *l = *packet_l_inout;
  gint packet_position = *packet_position_inout;
  gint n_peeked = 0;

  while (n_peeked < buf_size)
  {
    FlowPacket *packet;
    guint packet_size;
    gint n_to_peek;
    guchar *data;
    GList *m;

    if (l)
    {
      packet = l->data;
    }
    else
    {
      packet = packet_queue->first_packet;
      if (!packet)
      {
        l = packet_queue->queue->head;
        if (l)
          packet = l->data;
      }
    }

    if (!packet)
      break;

    if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
      goto next_packet;

    packet_size = flow_packet_get_size (packet);
    data = flow_packet_get_data (packet);

    n_to_peek = MIN ((buf_size - n_peeked), (packet_size - packet_position));

    if (buf_out)
      memcpy ((guchar *) buf_out + n_peeked, data + packet_position, n_to_peek);

    n_peeked += n_to_peek;
    packet_position += n_to_peek;

    if (packet_position == packet_size)
    {
    next_packet:

      if (l)
        m = g_list_next (l);
      else
        m = packet_queue->queue->head;

      if (!m)
        break;

      packet_position = 0;
      l = m;
    }
  }

  if (advance)
  {
    *packet_l_inout = l;
    *packet_position_inout = packet_position;
  }

  return n_peeked;
}

static void
byte_iter_drop_preceding (FlowPacketQueue *packet_queue, GList **packet_l_inout, gint *packet_position_inout)
{
  GList *iter_l = *packet_l_inout;
  gint iter_packet_position = *packet_position_inout;
  FlowPacket *packet;

  packet = packet_queue->first_packet;

  if (iter_l)
  {
    GList *l;

    if (packet_queue->first_packet)
    {
      packet_queue->bytes_in_queue -= flow_packet_get_size (packet_queue->first_packet);
      packet_queue->data_bytes_in_queue -= flow_packet_get_size (packet_queue->first_packet);
      flow_packet_unref (packet_queue->first_packet);
      packet_queue->first_packet = NULL;
    }

    for (l = packet_queue->queue->head; l != iter_l; )
    {
      GList *m;

      if (flow_packet_get_format (l->data) != FLOW_PACKET_FORMAT_BUFFER)
        continue;

      packet_queue->bytes_in_queue -= flow_packet_get_size (l->data);
      packet_queue->data_bytes_in_queue -= flow_packet_get_size (l->data);
      flow_packet_unref (l->data);

      m = g_list_next (l);
      g_queue_delete_link (packet_queue->queue, l);
      l = m;
    }

    g_assert (l == iter_l);

    packet = l->data;
  }

  if (packet && flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
  {
    if (iter_packet_position == flow_packet_get_size (packet))
    {
      if (iter_l)
      {
        GList *l = g_list_next (iter_l);
        g_queue_delete_link (packet_queue->queue, iter_l);
        iter_l = l;
      }

      packet_queue->bytes_in_queue -= flow_packet_get_size (packet);
      packet_queue->data_bytes_in_queue -= flow_packet_get_size (packet);

      flow_packet_unref (packet);
      packet_queue->first_packet = NULL;
      iter_packet_position = 0;
    }
  }
  else
  {
    iter_packet_position = 0;
  }

  packet_queue->bytes_in_queue += packet_queue->packet_position;
  packet_queue->data_bytes_in_queue += packet_queue->packet_position;
  packet_queue->bytes_in_queue -= iter_packet_position;
  packet_queue->data_bytes_in_queue -= iter_packet_position;

  packet_queue->packet_position = iter_packet_position;

  *packet_l_inout = iter_l;
  *packet_position_inout = iter_packet_position;
}

void
flow_packet_byte_iter_init (FlowPacketQueue *packet_queue, FlowPacketByteIter *byte_iter)
{
  byte_iter->packet_queue = packet_queue;
  byte_iter->packet_l = NULL;
  byte_iter->packet_position = packet_queue->packet_position;
  byte_iter->queue_position = 0;
}

gint
flow_packet_byte_iter_peek (FlowPacketByteIter *byte_iter, gpointer dest, gint n_max)
{
  g_return_val_if_fail (byte_iter != NULL, 0);
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (byte_iter->packet_queue), 0);
  g_return_val_if_fail (dest != NULL, 0);

  return byte_iter_peek_bytes (byte_iter->packet_queue, &byte_iter->packet_l, &byte_iter->packet_position, dest, n_max, FALSE);
}

gint
flow_packet_byte_iter_pop (FlowPacketByteIter *byte_iter, gpointer dest, gint n_max)
{
  gint n;

  g_return_val_if_fail (byte_iter != NULL, 0);
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (byte_iter->packet_queue), 0);

  n = byte_iter_peek_bytes (byte_iter->packet_queue, &byte_iter->packet_l, &byte_iter->packet_position, dest, n_max, TRUE);
  byte_iter->queue_position += n;

  g_assert (byte_iter->queue_position <= byte_iter->packet_queue->data_bytes_in_queue);

  return n;
}

gint
flow_packet_byte_iter_advance (FlowPacketByteIter *byte_iter, gint n_max)
{
  gint n;

  g_return_val_if_fail (byte_iter != NULL, 0);
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (byte_iter->packet_queue), 0);

  n = byte_iter_peek_bytes (byte_iter->packet_queue, &byte_iter->packet_l, &byte_iter->packet_position, NULL, n_max, TRUE);
  byte_iter->queue_position += n;

  g_assert (byte_iter->queue_position <= byte_iter->packet_queue->data_bytes_in_queue);

  return n;
}

void
flow_packet_byte_iter_drop_preceding_data (FlowPacketByteIter *byte_iter)
{
  g_return_if_fail (byte_iter != NULL);
  g_return_if_fail (FLOW_IS_PACKET_QUEUE (byte_iter->packet_queue));

  byte_iter_drop_preceding (byte_iter->packet_queue, &byte_iter->packet_l, &byte_iter->packet_position);

  byte_iter->queue_position = 0;
}

gint
flow_packet_byte_iter_get_remaining_bytes (FlowPacketByteIter *byte_iter)
{
  g_return_val_if_fail (byte_iter != NULL, 0);
  g_return_val_if_fail (FLOW_IS_PACKET_QUEUE (byte_iter->packet_queue), 0);

  return flow_packet_queue_get_length_data_bytes (byte_iter->packet_queue) - byte_iter->queue_position;
}
