/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-demux.c - Tests for the demultiplexer.
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

#define TEST_UNIT_NAME "FlowDemux"
#define TEST_TIMEOUT_S 20

#include "test-common.c"
#include "test-mux-common.c"

#define N_PADS 5
#define BUFFER_SIZE 4096
#define ITERATIONS 500

static void
test_run (void)
{
  FlowDemux *demux;
  FlowOutputPad *o_pads[N_PADS];
  FlowInputPad *input_pad;
  FlowUserAdapter *adapters[N_PADS];
  GList *expected_packets[N_PADS];
  gint last_channel_id = -1;
  int i, k, len;
  guchar *buffer;
  FlowPacket *packet;
  
  buffer = g_malloc (BUFFER_SIZE);

  demux = flow_demux_new ();
  for (i = 0; i < N_PADS; i++)
  {
    expected_packets[i] = NULL;
    o_pads[i] = flow_demux_add_channel (demux, i);
    adapters[i] = flow_user_adapter_new ();
    flow_pad_connect (FLOW_PAD (o_pads[i]),
                      FLOW_PAD (flow_simplex_element_get_input_pad (
                                        FLOW_SIMPLEX_ELEMENT (adapters[i]))));
  }
  input_pad = flow_splitter_get_input_pad (FLOW_SPLITTER (demux));
  for (i = 0; i < ITERATIONS; i++)
  {
    len = g_random_int_range (1, BUFFER_SIZE);
    memset (buffer, 0xaa, len);

    k = g_random_int_range (0, N_PADS);
    
    if (g_random_int () % 2 == 0)
      packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, len);
    else
    {
      int j;
      packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
      for (j = 0; j < N_PADS; j++)
      {
        if (j != k)
          expected_packets[j] = g_list_prepend (expected_packets[j], flow_packet_copy (packet));
      }
    }
    
    if (last_channel_id != k)
    {
      flow_pad_push (FLOW_PAD (input_pad),
                     flow_packet_new_take_object (flow_mux_event_new (k), 0));
      last_channel_id = k;
    }
    flow_pad_push (FLOW_PAD (input_pad), packet);
    expected_packets[k] = g_list_prepend (expected_packets[k], flow_packet_copy (packet));
  }
  flow_pad_push (FLOW_PAD (input_pad),
                 flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END));
  for (i = 0; i < N_PADS; i++)
    expected_packets[i] = g_list_prepend (expected_packets[i],
                                          flow_create_simple_event_packet (FLOW_STREAM_DOMAIN,
                                                                           FLOW_STREAM_END));
  for (i = 0; i < N_PADS; i++)
  {
    expected_packets[i] = g_list_reverse (expected_packets[i]);
    check_user_adapter_packets (adapters[i], expected_packets[i]);
  }
}
