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

static GMainLoop *main_loop;
static const gchar *input_file;
static const gchar **output_files;
static gint n_output_files;

static gboolean
copy_file_cb (gpointer data)
{
  FlowSplitter *splitter;
  FlowFileConnector *connector, *originator;
  FlowFileConnectOp *op;
  FlowDetailedEvent *detailed_event;
  FlowSegmentRequest *seg_req;
  FlowPacket *packet;
  gint i;

  g_print ("%s\n", input_file);

  splitter = flow_splitter_new ();

  connector = flow_file_connector_new ();

  op = flow_file_connect_op_new (input_file, FLOW_READ_ACCESS, FALSE, FALSE,
                                 FLOW_NO_ACCESS, FLOW_NO_ACCESS, FLOW_NO_ACCESS);
  packet = flow_packet_new_take_object (op, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))), packet);

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
  packet = flow_packet_new_take_object (detailed_event, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))), packet);

  seg_req = flow_segment_request_new (-1);
  packet = flow_packet_new_take_object (seg_req, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))), packet);

  flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (connector))),
                    FLOW_PAD (flow_splitter_get_input_pad (splitter)));

  for (i = 0; i < n_output_files; i++)
  {
    g_print (" -> %s\n", output_files [i]);

    connector = flow_file_connector_new ();
    op = flow_file_connect_op_new (output_files [i], FLOW_WRITE_ACCESS, TRUE, TRUE,
                                   FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                                   FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                                   FLOW_NO_ACCESS);
    packet = flow_packet_new_take_object (op, 0);
    flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))), packet);

    flow_pad_connect (FLOW_PAD (flow_splitter_add_output_pad (splitter)),
                      FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (connector))));
  }

  return FALSE;
}

gint
main (gint argc, gchar *argv [])
{
  gint i;

  /* Collect arguments */

  if (argc <= 1)
    return 1;

  n_output_files = argc - 2;
  if (n_output_files <= 0)
    return 1;

  g_type_init ();

  input_file = argv [1];
  output_files = g_new (const gchar **, n_output_files);

  for (i = 2; i < argc; i++)
  {
    output_files [i - 2] = argv [i];
  }

  /* Run */

  main_loop = g_main_loop_new (NULL, FALSE);
  g_idle_add (copy_file_cb, NULL);
  g_main_loop_run (main_loop);

  return 0;
}
