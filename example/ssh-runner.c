/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* ssh-runner.c - Run commands on an SSH server.
 *
 * Copyright (C) 2012 Hans Petter Jansson
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

static void
incoming_message_cb (gpointer user_data)
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
      g_main_loop_quit (main_loop);
    }

    flow_packet_unref (packet);  /* Also releases the object */
  }

  flow_packet_queue_clear (queue);
}

static void
run (void)
{
  FlowIPService *ip_service;
  FlowUserAdapter *user_adapter;
  FlowSshRunner *runner;
  FlowSshConnectOp *op;
  FlowDetailedEvent *detailed_event;
  FlowPacket *packet;

  ip_service = flow_ip_service_new ();
  flow_ip_service_set_name (ip_service, "mayak.nanosleep.org");

  /* FIXME: Is this necessary? */
  flow_ip_service_sync_resolve (ip_service, NULL);

  /* Set up input file */

  runner = flow_ssh_runner_new ();

  op = flow_ssh_connect_op_new (ip_service);
  g_object_unref (ip_service);

  packet = flow_packet_new_take_object (op, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (runner))), packet);

  /* Receive input */

  user_adapter = flow_user_adapter_new ();
  flow_user_adapter_set_input_notify (user_adapter, incoming_message_cb, user_adapter);

  flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (runner))),
                    FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (user_adapter))));

  /* Tell input file to stream itself from start to finish */

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
  packet = flow_packet_new_take_object (detailed_event, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (runner))), packet);

#if 0
  seg_req = flow_segment_request_new (-1);
  packet = flow_packet_new_take_object (seg_req, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (runner))), packet);
#endif

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
  packet = flow_packet_new_take_object (detailed_event, 0);
  flow_pad_push (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (runner))), packet);

  /* Run until done */

  g_main_loop_run (main_loop);
}

gint
main (gint argc, gchar **argv)
{
  /* Init */

  if (argc < 3)
  {
    g_printerr ("Usage: %s\n", argv [0]);
    return 1;
  }

  g_type_init ();

  /* Run */

  main_loop = g_main_loop_new (NULL, FALSE);
  run ();

  return 0;
}
