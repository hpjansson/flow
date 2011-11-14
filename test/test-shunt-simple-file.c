/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-shunt-simple-file.c - FlowShunt simple file test.
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

#define TEST_UNIT_NAME "FlowShunt (simple file)"
#define TEST_TIMEOUT_S 60

/* Test variables; adjustable */

#define BUFFER_SIZE            50000000  /* Amount of data to transfer */
#define PACKET_MAX_SIZE        8192      /* Max transfer unit */
#define PACKET_MIN_SIZE        1         /* Min transfer unit */

#define TOTAL_PAUSE_TIME_MS    3000      /* Total time to spend *not* reading or writing */
#define PAUSE_MIN_LENGTH_MS    10        /* Min pause unit */
#define PAUSE_MAX_LENGTH_MS    200       /* Max pause unit */

/* Calculations to determine the probability of pausing for
 * each packet processed. No user serviceable parts inside. */

#define PROBABILITY_MULTIPLIER 1000000  /* For fixed-point fractions */
#define PACKET_AVG_SIZE        (PACKET_MIN_SIZE + ((PACKET_MAX_SIZE - PACKET_MIN_SIZE) / 2))
#define PAUSE_AVG_LENGTH_MS    (PAUSE_MIN_LENGTH_MS + ((PAUSE_MAX_LENGTH_MS - PAUSE_MIN_LENGTH_MS) / 2))
#define NUM_PAUSES             (TOTAL_PAUSE_TIME_MS / PAUSE_AVG_LENGTH_MS)
#define TOTAL_EXPECTED_PACKETS (BUFFER_SIZE / PACKET_AVG_SIZE)
#define TOTAL_EXPECTED_EVENTS  (TOTAL_EXPECTED_PACKETS * 2)  /* Account for both reads and writes */
#define PAUSE_PROBABILITY      ((NUM_PAUSES * PROBABILITY_MULTIPLIER) / TOTAL_EXPECTED_EVENTS)

#include "test-common.c"

static guchar    *buffer;
static guint      src_index;
static guint      dest_index;

static gboolean   finished_writing;
static gboolean   writes_are_blocked;

static gboolean   started_reading;
static gboolean   finished_reading;
static gboolean   in_segment;
static gboolean   reads_are_blocked;

static FlowShunt *read_shunt;
static FlowShunt *write_shunt;

static guint
get_pause_interval_ms (void)
{
  guint n;

  n = g_random_int_range (0, PROBABILITY_MULTIPLIER + 1);
  if (n < PAUSE_PROBABILITY)
  {
    n = g_random_int_range (PAUSE_MIN_LENGTH_MS, PAUSE_MAX_LENGTH_MS + 1);
    return n;
  }

  return 0;
}

static gboolean
read_pause_ended (FlowShunt *shunt)
{
  test_print ("Resuming reads\n");
  reads_are_blocked = FALSE;
  flow_shunt_unblock_reads (shunt);
  return FALSE;
}

static void
read_from_shunt (FlowShunt *shunt, FlowPacket *packet, gpointer data)
{
  guint    packet_size;
  gpointer packet_data;
  guint    pause_ms;

  if (reads_are_blocked)
    test_end (TEST_RESULT_FAILED, "got read while blocked");

  if (data != shunt)
    test_end (TEST_RESULT_FAILED, "read callback user_data does not match");

  if (finished_reading)
    test_end (TEST_RESULT_FAILED, "got read callback after end-of-stream");

  if (!packet)
    test_end (TEST_RESULT_FAILED, "got read with NULL packet");

  packet_data = flow_packet_get_data (packet);
  if (!packet_data)
    test_end (TEST_RESULT_FAILED, "got NULL packet data");

  if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
  {
    FlowDetailedEvent *detailed_event = packet_data;

    if (FLOW_IS_DETAILED_EVENT (detailed_event))
    {
      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        test_print ("Read: Beginning of stream marker\n");

        if (started_reading)
          test_end (TEST_RESULT_FAILED, "got multiple beginning-of-stream markers");

        started_reading = TRUE;
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_BEGIN))
      {
        test_print ("Read: Beginning of segment marker\n");

        if (!started_reading)
          test_end (TEST_RESULT_FAILED, "segment started before stream");

        if (finished_reading)
          test_end (TEST_RESULT_FAILED, "segment started after stream end");

        if (in_segment)
          test_end (TEST_RESULT_FAILED, "got nested beginning-of-segment markers");

        in_segment = TRUE;
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
      {
        test_print ("Read: End of stream marker\n");

        if (!started_reading)
          test_end (TEST_RESULT_FAILED, "stream ended without starting");

        if (finished_reading)
          test_end (TEST_RESULT_FAILED, "got multiple end-of-stream markers");

        if (in_segment)
          test_end (TEST_RESULT_FAILED, "stream ended inside an open segment");

        if (dest_index != BUFFER_SIZE)
          test_end (TEST_RESULT_FAILED, "did not pass through all the data");

        finished_reading = TRUE;
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_END))
      {
        test_print ("Read: End of segment marker\n");

        if (!started_reading)
          test_end (TEST_RESULT_FAILED, "segment end before stream start");

        if (finished_reading)
          test_end (TEST_RESULT_FAILED, "segment end after stream end");

        if (!in_segment)
          test_end (TEST_RESULT_FAILED, "end of segment, but no segment open");

        in_segment = FALSE;

        /* Wait a bit before quitting, so shunts have a chance to generate invalid events */
        g_timeout_add (1000, (GSourceFunc) test_quit_main_loop, NULL);
      }
    }
    else if (!FLOW_IS_EVENT (detailed_event))
    {
      test_end (TEST_RESULT_FAILED, "got a weird object from read shunt");
    }
  }
  else if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
  {
    packet_size = flow_packet_get_size (packet);
    if (packet_size == 0)
      test_end (TEST_RESULT_FAILED, "got zero-size buffer packet");

    test_print ("Read: %d byte packet at offset %d\n", packet_size, dest_index);

    if (!started_reading)
      test_end (TEST_RESULT_FAILED, "got data before start of stream");

    if (finished_reading)
      test_end (TEST_RESULT_FAILED, "got data after end of stream");

    if (!in_segment)
      test_end (TEST_RESULT_FAILED, "got data outside segment");

    if (dest_index + packet_size > BUFFER_SIZE)
      test_end (TEST_RESULT_FAILED, "read too much data");

    if (memcmp (packet_data, buffer + dest_index, packet_size))
      test_end (TEST_RESULT_FAILED, "output data did not match input");

    dest_index += packet_size;

    if (dest_index == BUFFER_SIZE)
      test_print ("Read: Complete at %d bytes\n", dest_index);
  }
  else
  {
    test_end (TEST_RESULT_FAILED, "got unknown packet format");
  }

  flow_packet_unref (packet);

  pause_ms = get_pause_interval_ms ();
  if (pause_ms > 0)
  {
    test_print ("Blocking reads for %.2fs\n", (float) pause_ms / 1000.0);
    reads_are_blocked = TRUE;
    flow_shunt_block_reads (shunt);
    g_timeout_add (pause_ms, (GSourceFunc) read_pause_ended, shunt);
  }
}

static gboolean
write_pause_ended (FlowShunt *shunt)
{
  test_print ("Resuming writes\n");
  writes_are_blocked = FALSE;
  flow_shunt_unblock_writes (shunt);
  return FALSE;
}

static FlowPacket *
write_to_shunt (FlowShunt *shunt, gpointer data)
{
  FlowPacket *packet;
  guint       pause_ms;

  if (writes_are_blocked)
    test_end (TEST_RESULT_FAILED, "got write while blocked");

  if (data != shunt)
    test_end (TEST_RESULT_FAILED, "write callback user_data does not match");

  if (finished_writing)
    test_end (TEST_RESULT_FAILED, "got write callback after sending end-of-stream");

  if (src_index == BUFFER_SIZE)
  {
    packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
    finished_writing = TRUE;
    flow_shunt_block_writes (shunt);

    test_print ("Write: End of stream marker\n");

    /* Wait a bit before quitting, so shunts have a chance to generate invalid events */
    g_timeout_add (1000, (GSourceFunc) test_quit_main_loop, NULL);
  }
  else
  {
    guint len;

    len = g_random_int_range (PACKET_MIN_SIZE, PACKET_MAX_SIZE + 1);
    if (src_index + len > BUFFER_SIZE)
      len = BUFFER_SIZE - src_index;

    packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buffer + src_index, len);

    test_print ("Write: %d byte packet at offset %d\n", len, src_index);

    src_index += len;
  }

  if (!packet)
    test_end (TEST_RESULT_FAILED, "failed to create a packet");

  pause_ms = get_pause_interval_ms ();
  if (pause_ms > 0)
  {
    test_print ("Blocking writes for %.2fs\n", (float) pause_ms / 1000.0);
    writes_are_blocked = TRUE;
    flow_shunt_block_writes (shunt);
    g_timeout_add (pause_ms, (GSourceFunc) write_pause_ended, shunt);
  }

  return packet;
}

static FlowPacket *
write_segment_request_to_shunt (FlowShunt *shunt, gpointer data)
{
  test_print ("Writing segment request\n");

  flow_shunt_set_write_func (shunt, NULL, NULL);

  return flow_packet_new_take_object (flow_segment_request_new (-1), 0);
}

static void
test_run (void)
{
  gchar *test_file_name;
  gint   i;

  g_random_set_seed (time (NULL));

  /* Set up a buffer with random data */

  buffer = g_malloc (BUFFER_SIZE);

  for (i = 0; i < BUFFER_SIZE; )
  {
    guchar *p = buffer + i;

    if (i < BUFFER_SIZE - 4)
    {
      *((guint32 *) p) = g_random_int ();
      i += 4;
    }
    else
    {
      *p = (guchar) g_random_int ();
      i++;
    }
  }

  test_print ("Probability of pause is %d out of %d\n", PAUSE_PROBABILITY, PROBABILITY_MULTIPLIER);

  /* Create and write out scratch file */

  src_index          = 0;
  dest_index         = 0;
  finished_reading   = FALSE;
  finished_writing   = FALSE;
  reads_are_blocked  = FALSE;
  writes_are_blocked = FALSE;

  test_file_name = g_strdup_printf ("test-shunt-data-%08x", g_random_int ());

  write_shunt = flow_create_file (test_file_name, FLOW_WRITE_ACCESS, TRUE,
                                  FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                                  FLOW_NO_ACCESS,
                                  FLOW_NO_ACCESS);

  flow_shunt_set_write_func (write_shunt, write_to_shunt, write_shunt);

  test_run_main_loop ();

  flow_shunt_destroy (write_shunt);

  /* Read in and validate scratch file */

  src_index          = 0;
  dest_index         = 0;
  finished_reading   = FALSE;
  finished_writing   = FALSE;
  reads_are_blocked  = FALSE;
  writes_are_blocked = FALSE;

  read_shunt = flow_open_file (test_file_name, FLOW_READ_ACCESS);
  flow_shunt_set_read_func (read_shunt, read_from_shunt, read_shunt);
  flow_shunt_set_write_func (read_shunt, write_segment_request_to_shunt, read_shunt);

  test_run_main_loop ();

  flow_shunt_destroy (read_shunt);

  /* Cleanup */

  unlink (test_file_name);
  g_free (test_file_name);
  g_free (buffer);
}
