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
test_pack_unpack (FlowPacketQueue *packet_queue)
{
  FlowPacketByteIter iter;
  guchar buf [256];
  guchar *p;
  guint64 t64;
  guint32 t32;

  p = flow_pack_uint64 (0xffffffffffffffff, buf);
  if (p - buf != 9)
    test_end (TEST_RESULT_FAILED, "high uint64 packing error");

  flow_packet_queue_push_bytes (packet_queue, buf, p - buf);

  p = flow_pack_uint64 (0, buf);
  if (p - buf != 1)
    test_end (TEST_RESULT_FAILED, "low uint64 packing error");

  flow_packet_queue_push_bytes (packet_queue, buf, p - buf);

  p = flow_pack_uint32 (0xffffffff, buf);
  if (p - buf != 5)
    test_end (TEST_RESULT_FAILED, "high uint32 packing error");

  flow_packet_queue_push_bytes (packet_queue, buf, p - buf);

  p = flow_pack_uint32 (0, buf);
  if (p - buf != 1)
    test_end (TEST_RESULT_FAILED, "low uint32 packing error");

  flow_packet_queue_push_bytes (packet_queue, buf, p - buf);

  flow_packet_byte_iter_init (packet_queue, &iter);
  if (flow_packet_byte_iter_get_remaining_bytes (&iter) != 16)
    test_end (TEST_RESULT_FAILED, "byte iter bad remaining length (should be 16)");

  t64 = 42;
  if (!flow_unpack_uint64_from_iter (&iter, &t64))
    test_end (TEST_RESULT_FAILED, "high uint64 unpacking error");

  if (t64 != 0xffffffffffffffff)
    test_end (TEST_RESULT_FAILED, "high unpacked uint64 mismatch");

  t64 = 42;
  if (!flow_unpack_uint64_from_iter (&iter, &t64))
    test_end (TEST_RESULT_FAILED, "low uint64 unpacking error");
  
  if (t64 != 0)
    test_end (TEST_RESULT_FAILED, "low unpacked uint64 mismatch");

  t32 = 42;
  if (!flow_unpack_uint32_from_iter (&iter, &t32))
    test_end (TEST_RESULT_FAILED, "high uint32 unpacking error");
  
  if (t32 != 0xffffffff)
    test_end (TEST_RESULT_FAILED, "high unpacked uint32 mismatch");

  t32 = 42;
  if (!flow_unpack_uint32_from_iter (&iter, &t32))
    test_end (TEST_RESULT_FAILED, "low uint32 unpacking error");
  
  if (t32 != 0)
    test_end (TEST_RESULT_FAILED, "low unpacked uint32 mismatch");

  flow_packet_byte_iter_drop_preceding_data (&iter);
  if (flow_packet_byte_iter_get_remaining_bytes (&iter) != 0)
    test_end (TEST_RESULT_FAILED, "byte iter bad remaining length (should be 0)");
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

  test_pack_unpack (packet_queue);

  g_object_unref (packet_queue);
  g_free (packets);
}
