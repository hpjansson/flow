/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-packet-queue.c - FlowPacketQueue test.
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

#define TEST_UNIT_NAME "FlowPacketQueue"
#define TEST_TIMEOUT_S 20

#include "test-common.c"

#define BUFFER_SIZE 256
#define PACKETS_NUM 100000

static FlowPacket *
get_random_buffer_packet (void)
{
  guchar buffer [BUFFER_SIZE];
  guint  len;

  len = g_random_int_range (1, BUFFER_SIZE);
  memset (buffer, 0xaa, len);
  return flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, len);
}

static void
test_run (void)
{
  FlowPacketQueue  *packet_queue;
  FlowPacket      **packets;
  gint              i;

  packets = g_new (FlowPacket *, PACKETS_NUM);

  packet_queue = flow_packet_queue_new ();

  /* Push a bunch of buffer packets */

  for (i = 0; i < PACKETS_NUM; i++)
  {
    packets [i] = get_random_buffer_packet ();
    flow_packet_queue_push_packet (packet_queue, packets [i]);
  }

  /* Pop them and make sure they're in the right order */

  for (i = 0; i < PACKETS_NUM; i++)
  {
    FlowPacket *packet = flow_packet_queue_pop_packet (packet_queue);

    if (packet != packets [i])
      test_end (TEST_RESULT_FAILED, "bad packet pop order");

    flow_packet_unref (packet);
  }

  g_free (packets);
}
