/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-mux-serializer.c - Tests for the multiplexer serializer.
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

#define TEST_UNIT_NAME "FlowMuxSerializer"
#define TEST_TIMEOUT_S 20

#include "test-common.c"

#define N_CHANNELS 5
#define BUFFER_SIZE 4096
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
  FlowMuxSerializer *serializer;
  FlowUserAdapter *adapter;
  FlowPad *input_pad;
  FlowPacket *packet;
  GQueue *expected_chunks;
  guint32 chunk_size;
  guchar *buffer;
  int i, len;
  Chunk *chunk;
  FlowPacketQueue *input_packet_queue;
  guchar *hdr_buffer;
  guint current_channel_id;
  guint hdr_size;
  
  serializer = flow_mux_serializer_new ();

  adapter = flow_user_adapter_new ();
  flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (
                                      FLOW_SIMPLEX_ELEMENT (serializer))),
                    FLOW_PAD (flow_simplex_element_get_input_pad (
                                      FLOW_SIMPLEX_ELEMENT (adapter))));
  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT(serializer)));
  
  buffer = g_malloc (BUFFER_SIZE);

  current_channel_id = g_random_int_range (0, N_CHANNELS);
  flow_pad_push (input_pad, flow_packet_new_take_object (flow_mux_event_new (current_channel_id), 0));
  chunk_size = 0;
  expected_chunks = g_queue_new ();
  for (i = 0; i < ITERATIONS; i++)
  {
    len = g_random_int_range (1, BUFFER_SIZE);
    memset (buffer, 0xaa, len);

    if (g_random_int () % BUFFER_PROPABILITY != 0)
    {
      packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, len);
      chunk_size += len;
    }
    else
    {
      guint channel_id = g_random_int_range (0, N_CHANNELS);

      add_chunk (expected_chunks, current_channel_id, chunk_size);
      
      packet = flow_packet_new_take_object (flow_mux_event_new (channel_id), 0);
      current_channel_id = channel_id;
      chunk_size = 0;
    }
    flow_pad_push (input_pad, packet);
  }
  if (chunk_size > 0)
    add_chunk (expected_chunks, current_channel_id, chunk_size);
  
  flow_pad_push (input_pad, flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END));

  hdr_size = flow_mux_serializer_get_header_size (serializer);
  hdr_buffer = g_alloca (hdr_size);
  input_packet_queue = flow_user_adapter_get_input_queue (adapter);

  while ((chunk = g_queue_pop_head (expected_chunks)) != NULL)
  {
    guint channel_id;
    guint32 size;

    while (!flow_packet_queue_peek_packet (input_packet_queue, NULL, NULL))
      flow_user_adapter_wait_for_input (adapter);
    
    if (!flow_packet_queue_pop_bytes_exact (input_packet_queue, hdr_buffer, hdr_size))
      test_end (TEST_RESULT_FAILED, "header corrupted");
    
    flow_mux_serializer_parse_header (serializer, hdr_buffer, &channel_id, &size);
    if (chunk->channel_id != channel_id || chunk->size != size)
      test_end (TEST_RESULT_FAILED, "chunk corrupt");

    while (size > 0)
    {
      guint to_read = size;
      gint read;
      
      if (to_read > BUFFER_SIZE)
        to_read = BUFFER_SIZE;
      read = flow_packet_queue_pop_bytes (input_packet_queue, buffer, to_read);
      g_assert (read > 0);
      size -= read;
    }
    g_free (chunk);
  }
  g_queue_free (expected_chunks);
  g_free (buffer);
}
