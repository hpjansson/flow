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
#include <stdio.h>  /* fflush */
#include <string.h>  /* strcat */
#include <sys/stat.h>  /* stat */
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

static guint64 current_input_file_size;
static guint64 global_byte_total;

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

    flow_packet_unref (packet);  /* Also releases the object */
  }

  flow_packet_queue_clear (queue);
}

#define SECONDS_IN_MINUTE 60
#define MINUTES_IN_HOUR 60
#define HOURS_IN_DAY 24
#define SECONDS_IN_HOUR ((SECONDS_IN_MINUTE) * (MINUTES_IN_HOUR))
#define SECONDS_IN_DAY ((SECONDS_IN_HOUR) * (HOURS_IN_DAY))

#define BUF_SIZE 1024

static gchar *
format_time_interval (gint64 interval)
{
  gchar s0 [BUF_SIZE] = "";
  gchar s1 [BUF_SIZE];
  gint64 days, hours, minutes, seconds;

  /* Convert to seconds, with rounding */
  interval = (interval + 500000) / 1000000;

  days = interval / SECONDS_IN_DAY;
  hours = (interval - days * SECONDS_IN_DAY) / SECONDS_IN_HOUR;
  minutes = (interval - days * SECONDS_IN_DAY - hours * SECONDS_IN_HOUR) / SECONDS_IN_MINUTE;
  seconds = (interval - days * SECONDS_IN_DAY - hours * SECONDS_IN_HOUR - minutes * SECONDS_IN_MINUTE);

  if (days > 0)
    g_snprintf (s0, BUF_SIZE, "%" G_GUINT64_FORMAT "d", days);

  if (days > 0 || hours > 0)
  {
    g_snprintf (s1, BUF_SIZE, "%" G_GUINT64_FORMAT "h", hours);
    strcat (s0, s1);
  }

  if (days > 0 || hours > 0 || minutes > 0)
  {
    g_snprintf (s1, BUF_SIZE, "%" G_GUINT64_FORMAT "m", minutes);
    strcat (s0, s1);
  }

  g_snprintf (s1, BUF_SIZE, "%" G_GUINT64_FORMAT "s", seconds);
  strcat (s0, s1);

  return g_strdup (s0);
}

static void
print_final_statistics (guint64 byte_total, guint64 time_interval)
{
  guint64 byte_rate = byte_total;
  gchar *time_interval_s = format_time_interval (time_interval);

  if (time_interval > 0)
    byte_rate = (byte_total * 1000000) / time_interval;

  g_printerr ("\r%" G_GUINT64_FORMAT "MiB copied - %s elapsed - %" G_GUINT64_FORMAT "MiB/s          \n",
              byte_total / (G_GUINT64_CONSTANT (1024) * G_GUINT64_CONSTANT (1024)),
              time_interval_s,
              byte_rate / (G_GUINT64_CONSTANT (1024) * G_GUINT64_CONSTANT (1024)));

  g_free (time_interval_s);
}

static gboolean
print_statistics_cb (FlowController *controller)
{
  guint64 byte_rate = flow_controller_get_byte_rate (controller);
  guint64 byte_total = flow_controller_get_byte_total (controller);
  gchar *time_left_s = byte_rate > 0 ? format_time_interval (((current_input_file_size - byte_total) * 1000000) / byte_rate) : g_strdup ("?");

  g_printerr ("\r%" G_GUINT64_FORMAT "MiB copied - %s left - %" G_GUINT64_FORMAT "MiB/s          ",
              byte_total / (G_GUINT64_CONSTANT (1024) * G_GUINT64_CONSTANT (1024)),
              time_left_s,
              byte_rate / (G_GUINT64_CONSTANT (1024) * G_GUINT64_CONSTANT (1024)));
  fflush (stderr);

  g_free (time_left_s);
  return TRUE;
}

static gint64
get_file_size (const gchar *path)
{
  struct stat st;

  if (stat (path, &st) < 0) {
    g_printerr ("Could not stat input file.\n");
    return -1;
  }

  return st.st_size;
}

static void
copy_file_cb (const gchar *input_file, const gchar **output_files, gint n_output_files)
{
  FlowController *controller;
  FlowSplitter *splitter;
  FlowJoiner *joiner;
  FlowUserAdapter *user_adapter;
  FlowFileConnector *connector;
  FlowFileConnectOp *op;
  FlowDetailedEvent *detailed_event;
  FlowSegmentRequest *seg_req;
  FlowPacket *packet;
  gint i;

  g_print ("%s -> [", input_file);

  n_output_files_left = n_output_files;

  /* Set up controller for rate measurement */

  controller = flow_controller_new ();

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

  /* Connect input file's output to repeater's input, with controller in the middle */

  flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (connector))),
                    FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (controller))));
  flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (controller))),
                    FLOW_PAD (flow_splitter_get_input_pad (splitter)));

  /* Create output files and connect their inputs to repeater's outputs */

  for (i = 0; i < n_output_files; i++)
  {
    g_print ("%s%s", output_files [i], i + 1 < n_output_files ? ", " : "");

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

  g_print ("]\n");

  current_input_file_size = get_file_size (input_file);
  if (current_input_file_size < 0)
    return;

  /* Set up statistics timer */

  g_timeout_add_seconds (1, (GSourceFunc) print_statistics_cb, controller);

  /* Run until done */

  g_main_loop_run (main_loop);

  global_byte_total = flow_controller_get_byte_total (controller);
}

gint
main (gint argc, gchar **argv)
{
  gint64 start_time, now;
  gint i;

  /* Init */

  if (argc < 3)
  {
    g_printerr ("Usage: %s infile outfile [outfile [outfile [...]]]\n", argv [0]);
    return 1;
  }

  g_type_init ();

  start_time = g_get_monotonic_time ();

  /* Run */

  main_loop = g_main_loop_new (NULL, FALSE);
  copy_file_cb (argv [1], (const gchar **) &argv [2], argc - 2);

  /* Print statistics and exit */

  now = g_get_monotonic_time ();
  print_final_statistics (global_byte_total, now - start_time);

  return 0;
}
