/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-mux.c - Tests for the multiplexer (flow-{de}mux).
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

#define TEST_UNIT_NAME "FlowMux"
#define TEST_TIMEOUT_S 20

#include "test-common.c"
#include "test-mux-common.c"

#define N_PADS 5
#define BUFFER_SIZE 4096
#define ITERATIONS 500

static void
test_run (void)
{
  FlowMux *mux;
  FlowUserAdapter *adapter;
  FlowInputPad *pads[N_PADS];
  FlowInputPad *last_pad = NULL;
  FlowPacket *packet;
  GList *expected_packets = NULL;
  guchar *buffer;
  int i, k, len;

  mux = flow_mux_new ();
  for (i = 0; i < N_PADS; i++)
    pads[i] = flow_mux_add_channel_id (mux, i);

  adapter = flow_user_adapter_new ();
  flow_pad_connect (FLOW_PAD (flow_joiner_get_output_pad (FLOW_JOINER (mux))),
                    FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (adapter))));
  
  buffer = g_malloc (BUFFER_SIZE);

  for (i = 0; i < ITERATIONS; i++)
  {
    len = g_random_int_range (1, BUFFER_SIZE);
    memset (buffer, 0xaa, len);

    if (g_random_int () % 2 == 0)
      packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer, len);
    else
      packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
    
    k = g_random_int_range (0, N_PADS);
    if (last_pad != pads[k])
    {
      FlowMuxEvent *event = flow_mux_event_new (k);
      expected_packets = g_list_prepend (expected_packets, flow_packet_new_take_object (event, 0));
      last_pad = pads[k];
    }
    expected_packets = g_list_prepend (expected_packets, flow_packet_ref (packet));
    flow_pad_push (FLOW_PAD (pads[k]), packet);
  }
  for (k = 0; k < N_PADS; k++)
    flow_pad_push (FLOW_PAD (pads[k]),
                   flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END));

  expected_packets = g_list_prepend (expected_packets,
                                     flow_create_simple_event_packet (FLOW_STREAM_DOMAIN,
                                                                      FLOW_STREAM_END));
  
  expected_packets = g_list_reverse (expected_packets);

  check_user_adapter_packets (adapter, expected_packets);
  
  g_free (buffer);
}
