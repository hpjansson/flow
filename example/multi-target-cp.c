/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* multi-target-cp.c - Copy file to multiple locations in parallel.
 *
 * Copyright (C) 2011 Hans Petter Jansson
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
#include <flow/flow.h>

/* Size of the low-level I/O buffer employed by each file. Internally, this
 * is the maximum length of a single call to read() or write(), as well
 * as the maximum size of individual data packets generated. */
#define FILE_IO_BUFFER_SIZE 65536

/* Maximum size of data contained in a packet queue. Each file has two
 * queues; one for reading and one for writing. */
#define FILE_QUEUE_SIZE (65536 * 16)

/* Maximum size of data contained in splitter's packet queue. If one or
 * more of the output data streams stalls, packets queued here are still
 * available to the remaining streams. */
#define SPLITTER_QUEUE_SIZE (1024 * 1024 * 16)

static GMainLoop *main_loop;
static gint n_output_files_left;

static void
output_file_message_cb (gpointer user_data)
{
  FlowUserAdapter *user_adapter = user_data;
  FlowPacketQueue *queue;
  FlowPacket *packet;

  queue = flow_user_adapter_get_input_queue (user_adapter);

  while ((packet = flow_packet_queue_pop_first_object (queue)))
  {
    gpointer obj = flow_packet_get_data (packet);
    if (!obj)
      continue;

    if (FLOW_IS_DETAILED_EVENT (obj) &&
        flow_detailed_event_matches (FLOW_DETAILED_EVENT (obj), FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
    {
      n_output_files_left--;
      if (n_output_files_left == 0)
        g_main_loop_quit (main_loop);
    }

    flow_packet_free (packet);  /* Also releases the object */
  }

  flow_packet_queue_clear (queue);
}

static void
copy_file_cb (const gchar *input_file, const gchar **output_files, gint n_output_files)
{
  FlowSplitter *splitter;
  FlowJoiner *joiner;
  FlowUserAdapter *user_adapter;
  FlowFileConnector *connector;
  FlowFileConnectOp *op;
  FlowDetailedEvent *detailed_event;
  FlowSegmentRequest *seg_req;
  FlowPacket *packet;
  gint i;

  g_print ("%s\n", input_file);

  n_output_files_left = n_output_files;

  /* Set up packet repeater */

  splitter = flow_splitter_new ();
  flow_splitter_set_buffer_limit (splitter, SPLITTER_QUEUE_SIZE);

  /* Set up collector for status messages from output files, so we can exit when they're all done */

  joiner = flow_joiner_new ();

  user_adapter = flow_user_adapter_new ();
  flow_user_adapter_set_input_notify (user_adapter, output_file_message_cb, user_adapter);

  flow_pad_connect (FLOW_PAD (flow_joiner_get_output_pad (joiner)),
                    FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (user_adapter))));

  /* Set up input file */

  connector = flow_file_connector_new ();
  flow_connector_set_io_buffer_size (FLOW_CONNECTOR (connector), FILE_IO_BUFFER_SIZE);
  flow_connector_set_read_queue_limit (FLOW_CONNECTOR (connector), FILE_QUEUE_SIZE);
  flow_connector_set_write_queue_limit (FLOW_CONNECTOR (connector), FILE_QUEUE_SIZE);

  op = flow_file_connect_op_new (input_file, FLOW_READ_ACCESS, FALSE, FALSE,
                                 FLOW_NO_ACCESS, FLOW_NO_ACCESS, FLOW_NO_ACCESS);
  packet = flow_packet_new_take_object (op, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))), packet);

  /* Tell input file to stream itself from start to finish */

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
  packet = flow_packet_new_take_object (detailed_event, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))), packet);

  seg_req = flow_segment_request_new (-1);
  packet = flow_packet_new_take_object (seg_req, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))), packet);

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
  packet = flow_packet_new_take_object (detailed_event, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))), packet);

  /* Connect input file's output to repeater's input */

  flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (connector))),
                    FLOW_PAD (flow_splitter_get_input_pad (splitter)));

  /* Create output files and connect their inputs to repeater's outputs */

  for (i = 0; i < n_output_files; i++)
  {
    g_print (" -> %s\n", output_files [i]);

    connector = flow_file_connector_new ();
    flow_connector_set_io_buffer_size (FLOW_CONNECTOR (connector), FILE_IO_BUFFER_SIZE);
    flow_connector_set_read_queue_limit (FLOW_CONNECTOR (connector), FILE_QUEUE_SIZE);
    flow_connector_set_write_queue_limit (FLOW_CONNECTOR (connector), FILE_QUEUE_SIZE);

    op = flow_file_connect_op_new (output_files [i], FLOW_WRITE_ACCESS, TRUE, TRUE,
                                   FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                                   FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                                   FLOW_NO_ACCESS);
    packet = flow_packet_new_take_object (op, 0);
    flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))), packet);

    flow_pad_connect (FLOW_PAD (flow_splitter_add_output_pad (splitter)),
                      FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))));

    flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (connector))),
                      FLOW_PAD (flow_joiner_add_input_pad (joiner)));
  }

  /* Run until done */

  g_main_loop_run (main_loop);
}

gint
main (gint argc, gchar **argv)
{
  gint i;

  /* Init */

  if (argc < 3)
  {
    g_printerr ("Usage: %s infile outfile [outfile [outfile [...]]]\n", argv [0]);
    return 1;
  }

  g_type_init ();

  /* Run */

  main_loop = g_main_loop_new (NULL, FALSE);
  copy_file_cb (argv [1], (const gchar **) &argv [2], argc - 2);
  return 0;
}
