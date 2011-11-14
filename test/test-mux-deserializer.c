/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-mux-deserializer.c - Tests for the multiplexer deserializer.
 *
 * Copyright (C) 2007 Andreas Rottmann
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
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors: Andreas Rottmann <a.rottmann@gmx.at>
 */

#define TEST_UNIT_NAME "FlowMuxDeserializer"
#define TEST_TIMEOUT_S 20

#include "test-common.c"

#define N_CHANNELS 5
#define BUFFER_SIZE 4096
#define MAX_CHUNK_SIZE (1 << 16)
#define ITERATIONS 500
#define BUFFER_PROPABILITY 5

typedef struct
{
    guint channel_id;
    guint32 size;
} Chunk;

static void
add_chunk (GQueue *expected_chunks, guint channel_id, guint32 size)
{
  Chunk *chunk = g_new (Chunk, 1);
  chunk->channel_id = channel_id;
  chunk->size = size;
  g_queue_push_tail (expected_chunks, chunk);
}

static void
test_run (void)
{
  FlowMuxDeserializer *deserializer;
  FlowUserAdapter *adapter;
  FlowPad *input_pad;
  FlowPacket *packet;
  GQueue *expected_chunks;
  guint32 chunk_size;
  guchar *buffer;
  int i, len;
  Chunk *chunk;
  FlowPacketQueue *input_packet_queue;
  guint current_channel_id;
  
  deserializer = flow_mux_deserializer_new ();

  adapter = flow_user_adapter_new ();
  flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (
                                      FLOW_SIMPLEX_ELEMENT (deserializer))),
                    FLOW_PAD (flow_simplex_element_get_input_pad (
                                      FLOW_SIMPLEX_ELEMENT (adapter))));
  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT(deserializer)));
  
  buffer = g_malloc (BUFFER_SIZE);
  memset (buffer, 0xaa, BUFFER_SIZE);

  chunk_size = 0;
  expected_chunks = g_queue_new ();
  for (i = 0; i < ITERATIONS; i++)
  {
    FlowPacket *header;
    guint hdr_size;
    
    current_channel_id = g_random_int_range (0, N_CHANNELS);
    len = g_random_int_range (1, MAX_CHUNK_SIZE);
    hdr_size = flow_mux_deserializer_get_header_size (deserializer);
    flow_mux_deserializer_unparse_header (deserializer, buffer, current_channel_id, len);
    header = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, hdr_size);
    flow_pad_push (input_pad, header);
    add_chunk (expected_chunks, current_channel_id, len);

    while (len > 0)
    {
      guint n = g_random_int_range (1, BUFFER_SIZE);
      if (n > len)
        n = len;
      flow_pad_push (input_pad, flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, n));
      len -= n;
    }
  }
  
  flow_pad_push (input_pad, flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END));

  input_packet_queue = flow_user_adapter_get_input_queue (adapter);

  while ((chunk = g_queue_pop_head (expected_chunks)) != NULL)
  {
    guint32 size;
    gpointer data;

    while ((packet = flow_packet_queue_pop_packet (input_packet_queue)) == NULL)
      flow_user_adapter_wait_for_input (adapter);

    data = flow_packet_get_data (packet);
    if (packet->format != FLOW_PACKET_FORMAT_OBJECT
        || !FLOW_IS_MUX_EVENT (data))
      test_end (TEST_RESULT_FAILED, "expected mux event packet");

    if (flow_mux_event_get_channel_id (FLOW_MUX_EVENT (data)) != chunk->channel_id)
      test_end (TEST_RESULT_FAILED, "unexpected channel id in mux event packet");

    flow_packet_unref (packet);
    
    size = 0;
    while (TRUE)
    {
      while (!flow_packet_queue_peek_packet (input_packet_queue, &packet, NULL))
        flow_user_adapter_wait_for_input (adapter);
      if (packet->format != FLOW_PACKET_FORMAT_BUFFER)
        break;
      size += packet->size;
      flow_packet_queue_drop_packet (input_packet_queue);
    } 

    if (size != chunk->size)
      test_end (TEST_RESULT_FAILED, "chunk differs in size");
    
    g_free (chunk);
  }
  g_queue_free (expected_chunks);
  g_free (buffer);
}
