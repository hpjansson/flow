/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* benchmark-propagation.c - Benchmark packet propagation through elements.
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

#define BENCHMARK_UNIT_NAME "propagation"

#include "benchmark-common.c"

#define PACKET_SIZE_DOUBLINGS 20
#define SUBTEST_SECONDS       2
#define MAX_PIPELINE_LENGTH   64

static FlowElement *elements [MAX_PIPELINE_LENGTH];
static guint        packet_size;
static guint        pipeline_length;

static void
benchmark_packet_size (void)
{
  FlowPad  *input_pad  = FLOW_PAD (flow_simplex_element_get_input_pad  (FLOW_SIMPLEX_ELEMENT (elements [0])));
  FlowPad  *output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (elements [pipeline_length - 1])));
  gint      n_packets  = 0;
  gpointer  buf;

  benchmark_start_cpu_timer (SUBTEST_SECONDS);

  while (benchmark_is_running)
  {
    FlowPacketQueue *packet_queue;
    FlowPacket      *packet;

    buf = g_malloc0 (packet_size);
    packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buf, packet_size);
    g_free (buf);

    flow_pad_push (input_pad, packet);

    packet_queue = flow_pad_get_packet_queue (output_pad);
    packet = flow_packet_queue_pop_packet (packet_queue);

    g_assert (packet != NULL);

    flow_packet_unref (packet);
    n_packets++;
  }

#if defined (BENCHMARK_PACKET_SIZE)
  benchmark_add_data_point (packet_size, (gdouble) n_packets * (gdouble) packet_size / (gdouble) SUBTEST_SECONDS);
#else
  benchmark_add_data_point (pipeline_length, (gdouble) n_packets * (gdouble) packet_size / (gdouble) SUBTEST_SECONDS);
#endif
}

static void
benchmark_pipeline_length (void)
{
  guint i;

  /* Set up pipeline */

  for (i = 0; i < pipeline_length; i++)
    elements [i] = g_object_new (FLOW_TYPE_SIMPLEX_ELEMENT, NULL);

  for (i = 1; i < pipeline_length; i++)
  {
    FlowOutputPad *output_pad;
    FlowInputPad  *input_pad;

    output_pad = flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (elements [i - 1]));
    input_pad  = flow_simplex_element_get_input_pad  (FLOW_SIMPLEX_ELEMENT (elements [i]));

    flow_pad_connect (FLOW_PAD (output_pad), FLOW_PAD (input_pad));
  }

#if defined (BENCHMARK_PACKET_SIZE)
  for (i = 0, packet_size = 1; i < PACKET_SIZE_DOUBLINGS; i++, packet_size <<= 1)
  {
    benchmark_packet_size ();
  }
#else
  packet_size = 1;
  benchmark_packet_size ();
#endif

  for (i = 0; i < pipeline_length; i++)
    g_object_unref (elements [i]);
}

static void
benchmark_run (void)
{
  gint i;

  benchmark_begin_data_plot ("Packet propagation", "Packet size (bytes)", "Data propagated (bytes/s)");
  benchmark_begin_data_set ();

  for (i = 2; i < MAX_PIPELINE_LENGTH; i++)
  {
    pipeline_length = i;
    benchmark_pipeline_length ();
  }
}
