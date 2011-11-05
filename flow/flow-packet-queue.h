/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-packet-queue.h - A FlowPacket queue that can do partial dequeues.
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

#ifndef _FLOW_PACKET_QUEUE_H
#define _FLOW_PACKET_QUEUE_H

#include <glib-object.h>
#include <flow/flow-packet.h>

G_BEGIN_DECLS

#define FLOW_TYPE_PACKET_QUEUE            (flow_packet_queue_get_type ())
#define FLOW_PACKET_QUEUE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_PACKET_QUEUE, FlowPacketQueue))
#define FLOW_PACKET_QUEUE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_PACKET_QUEUE, FlowPacketQueueClass))
#define FLOW_IS_PACKET_QUEUE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_PACKET_QUEUE))
#define FLOW_IS_PACKET_QUEUE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_PACKET_QUEUE))
#define FLOW_PACKET_QUEUE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_PACKET_QUEUE, FlowPacketQueueClass))
GType   flow_packet_queue_get_type        (void) G_GNUC_CONST;

typedef struct _FlowPacketQueue      FlowPacketQueue;
typedef struct _FlowPacketQueueClass FlowPacketQueueClass;

struct _FlowPacketQueue
{
  GObject     object;

  /* --- Private --- */

  FlowPacket *first_packet;
  GQueue     *queue;
  gint        packet_position;
  gint        bytes_in_queue;
  gint        data_bytes_in_queue;
};

struct _FlowPacketQueueClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowPacketQueue  *flow_packet_queue_new                    (void);

gint              flow_packet_queue_get_length_packets     (FlowPacketQueue *packet_queue);
gint              flow_packet_queue_get_length_bytes       (FlowPacketQueue *packet_queue);
gint              flow_packet_queue_get_length_data_bytes  (FlowPacketQueue *packet_queue);

void              flow_packet_queue_clear                  (FlowPacketQueue *packet_queue);

void              flow_packet_queue_push_packet            (FlowPacketQueue *packet_queue, FlowPacket *packet);
void              flow_packet_queue_push_bytes             (FlowPacketQueue *packet_queue, gconstpointer src, gint n);
void              flow_packet_queue_push_packet_to_head    (FlowPacketQueue *packet_queue, FlowPacket *packet);

FlowPacket       *flow_packet_queue_pop_packet             (FlowPacketQueue *packet_queue);
gint              flow_packet_queue_pop_bytes              (FlowPacketQueue *packet_queue, gpointer dest, gint n_max);
gboolean          flow_packet_queue_pop_bytes_exact        (FlowPacketQueue *packet_queue, gpointer dest, gint n);

gboolean          flow_packet_queue_peek_packet            (FlowPacketQueue *packet_queue,
                                                            FlowPacket **packet_out, gint *offset_out);
FlowPacket       *flow_packet_queue_peek_nth_packet        (FlowPacketQueue *packet_queue, guint n);
void              flow_packet_queue_peek_packets           (FlowPacketQueue *packet_queue,
                                                            FlowPacket **packet_out, gint *n_packets);
gboolean          flow_packet_queue_drop_packet            (FlowPacketQueue *packet_queue);
void              flow_packet_queue_steal                  (FlowPacketQueue *packet_queue,
                                                            gint n_packets, gint n_bytes, gint n_data_bytes);

FlowPacket       *flow_packet_queue_peek_first_object      (FlowPacketQueue *packet_queue);
FlowPacket       *flow_packet_queue_pop_first_object       (FlowPacketQueue *packet_queue);
gboolean          flow_packet_queue_skip_past_first_object (FlowPacketQueue *packet_queue);

G_END_DECLS

#endif  /* _FLOW_PACKET_QUEUE_H */
