/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-packet.c - FlowPacket test.
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

#define TEST_UNIT_NAME "FlowPacket"
#define TEST_TIMEOUT_S 20

#include "test-common.c"

#define BUFFER_SIZE (1 << 19)  /* 1 Mi */
#define ITERATIONS  500

static void
test_run (void)
{
  FlowPacket *packet;
  guchar     *buffer;
  guint       len;
  gint        i;

  buffer = g_malloc (BUFFER_SIZE);

  for (i = 0; i < ITERATIONS; i++)
  {
    /* Test buffer */

    len = g_random_int_range (1, BUFFER_SIZE);
    memset (buffer, 0xaa, len);

    packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, len);

    if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
      test_end (TEST_RESULT_FAILED, "wrong format for buffer");
    if (flow_packet_get_size (packet) != len)
      test_end (TEST_RESULT_FAILED, "wrong size for buffer");
    if (memcmp (flow_packet_get_data (packet), buffer, len))
      test_end (TEST_RESULT_FAILED, "bad data in buffer");

    flow_packet_free (packet);

    /* TODO: Test object */
  }

  g_free (buffer);
}
