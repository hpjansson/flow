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

#define TEST_UNIT_NAME "FlowShunt (complex file)"
#define TEST_TIMEOUT_S 60

/* Test variables; adjustable */

#define BUFFER_SIZE            20000000  /* Amount of data to transfer */
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

static guchar    *pre_buffer;   /* Updated when we send the request */
static guchar    *post_buffer;  /* Updated when we receive data */
static gint       pre_pos;
static gint       post_pos;

static gint       n_packets_written = 0;
static gint       n_segments_remaining = 0;

static gboolean   finished_writing;
static gboolean   quitting_main_loop;
static gboolean   writes_are_blocked;

static gboolean   started_reading;
static gboolean   finished_reading;
static gboolean   in_segment;
static gboolean   reads_are_blocked;

static FlowShunt *global_shunt;

static void
randomize_buffer (gpointer buf, gint len_bytes)
{
  gint i;

  /* Fill buffer with bytes 0x02 - 0xff */

  for (i = 0; i < len_bytes; )
  {
    guchar *p = buf + i;

    if (i < len_bytes - 4)
    {
      *((guint32 *) p) = (guint32) g_random_int () | 0x02020202;
      i += 4;
    }
    else
    {
      *p = (guchar) g_random_int () | 0x02;
      i++;
    }
  }
}

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
read_stream (FlowShunt *shunt, FlowPacket *packet, gpointer data)
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

        if (post_pos != BUFFER_SIZE)
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
    else if (FLOW_IS_POSITION (packet_data))
    {
      FlowPosition *position = packet_data;

      if (flow_position_get_anchor (position) != FLOW_OFFSET_ANCHOR_BEGIN)
        test_end (TEST_RESULT_FAILED, "got relative position from shunt");

      post_pos = flow_position_get_offset (position);

      test_print ("Read: Seek to %d\n", post_pos);
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

    test_print ("Read: %d byte packet at offset %d\n", packet_size, post_pos);

    if (!started_reading)
      test_end (TEST_RESULT_FAILED, "got data before start of stream");

    if (finished_reading)
      test_end (TEST_RESULT_FAILED, "got data after end of stream");

    if (!in_segment)
      test_end (TEST_RESULT_FAILED, "got data outside segment");

    if (post_pos + packet_size > BUFFER_SIZE)
      test_end (TEST_RESULT_FAILED, "read too much data");

    memcpy ((gchar *) post_buffer + post_pos, packet_data, packet_size);
    post_pos += packet_size;

    if (post_pos == BUFFER_SIZE)
    {
      test_print ("Read: Complete at %d bytes\n", post_pos);
    }
  }
  else
  {
    test_end (TEST_RESULT_FAILED, "got unknown packet format");
  }

  flow_packet_free (packet);

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
  if (finished_writing)
    return FALSE;

  test_print ("Resuming writes\n");
  writes_are_blocked = FALSE;
  flow_shunt_unblock_writes (shunt);
  return FALSE;
}

static FlowPacket *
write_stream (FlowShunt *shunt, gpointer data)
{
  FlowPacket *packet = NULL;
  guint       pause_ms;
  guint       len;

  if (writes_are_blocked)
    test_end (TEST_RESULT_FAILED, "got write while blocked");

  if (data != shunt)
    test_end (TEST_RESULT_FAILED, "write callback user_data does not match");

  if (finished_writing)
    test_end (TEST_RESULT_FAILED, "got write callback after sending end-of-stream");

  len = g_random_int_range (PACKET_MIN_SIZE, PACKET_MAX_SIZE + 1);
  if (pre_pos + len > BUFFER_SIZE)
    len = BUFFER_SIZE - pre_pos;

  packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, pre_buffer + pre_pos, len);
  if (!packet)
    test_end (TEST_RESULT_FAILED, "failed to create a packet");

  test_print ("Write: %d byte packet at offset %d\n", len, pre_pos);

  pre_pos += len;

  if (pre_pos == BUFFER_SIZE)
  {
    finished_writing = TRUE;
    test_print ("Write: File creation complete\n");

    flow_shunt_block_writes (shunt);

    /* Wait a bit before quitting, so shunts have a chance to generate invalid events */
    g_timeout_add (1000, (GSourceFunc) test_quit_main_loop, NULL);
  }

  if (!finished_writing)
  {
    pause_ms = get_pause_interval_ms ();
    if (pause_ms > 0)
    {
      test_print ("Blocking writes for %.2fs\n", (float) pause_ms / 1000.0);
      writes_are_blocked = TRUE;
      flow_shunt_block_writes (shunt);
      g_timeout_add (pause_ms, (GSourceFunc) write_pause_ended, shunt);
    }
  }

  return packet;
}

static FlowPacket *
write_random (FlowShunt *shunt, gpointer data)
{
  FlowPacket *packet;
  gint        r;

  r = g_random_int_range (0, PROBABILITY_MULTIPLIER);

  if (pre_pos > BUFFER_SIZE || r < PROBABILITY_MULTIPLIER / 3)
  {
    /* Seek to a random position */

    r = g_random_int_range (0, BUFFER_SIZE);
    packet = flow_packet_new_take_object (flow_position_new (FLOW_OFFSET_ANCHOR_BEGIN, r), 0);
    pre_pos = r;

    test_print ("Write: Seek to offset %d\n", r);
  }
  else if (pre_pos == BUFFER_SIZE || r < (PROBABILITY_MULTIPLIER * 2) / 3)
  {
    /* Request a read */

    r = g_random_int_range (1, PACKET_MAX_SIZE);
    memset (post_buffer + pre_pos, 1, MIN (r, BUFFER_SIZE - pre_pos));  /* To-be-synced segment */
    packet = flow_packet_new_take_object (flow_segment_request_new (r), 0);
    pre_pos += MIN (r, BUFFER_SIZE - pre_pos);

    n_segments_remaining++;

    test_print ("Write: Request %d bytes\n", r);
  }
  else
  {
    /* Write some data */

    r = g_random_int_range (1, PACKET_MAX_SIZE);
    r = MIN (r, BUFFER_SIZE - pre_pos);

    g_assert (r > 0);

    randomize_buffer (pre_buffer + pre_pos, r);
    memset (post_buffer + pre_pos, 0, r);  /* Out-of-sync segment */
    packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, pre_buffer + pre_pos, r);
    pre_pos += r;

    test_print ("Write: Write %d bytes\n", r);

    n_packets_written++;

    test_print ("Write: %d / %d packets\n", n_packets_written, TOTAL_EXPECTED_PACKETS);

    if (n_packets_written >= TOTAL_EXPECTED_PACKETS)
    {
      /* Done writing */

      test_print ("Done writing.");
      flow_shunt_block_writes (shunt);
      finished_writing = TRUE;

      if (n_segments_remaining == 0 && !quitting_main_loop)
      {
        test_print (" No outstanding requests - quitting after short delay...\n");
        quitting_main_loop = TRUE;
        g_timeout_add (1000, (GSourceFunc) test_quit_main_loop, NULL);
      }
      else
      {
        test_print ("\n");
      }
    }
  }

  return packet;
}

static void
memcpy_to_be_synced_only (guchar *dest, const guchar *src, gint n)
{
  gint i;

  for (i = 0; i < n; i++)
  {
    if (*dest == 1)
      *dest = *src;

    src++;
    dest++;
  }
}

static void
read_random (FlowShunt *shunt, FlowPacket *packet, gpointer data)
{
  guint    packet_size;
  gpointer packet_data;

  if (data != shunt)
    test_end (TEST_RESULT_FAILED, "read callback user_data does not match");

  if (!packet)
    test_end (TEST_RESULT_FAILED, "got read with NULL packet");

  packet_data = flow_packet_get_data (packet);
  if (!packet_data)
    test_end (TEST_RESULT_FAILED, "got NULL packet data");

  if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = packet_data;

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
        n_segments_remaining--;
      }
    }
    else if (FLOW_IS_POSITION (packet_data))
    {
      FlowPosition *position = packet_data;

      if (flow_position_get_anchor (position) != FLOW_OFFSET_ANCHOR_BEGIN)
        test_end (TEST_RESULT_FAILED, "got relative position from shunt");

      post_pos = flow_position_get_offset (position);

      test_print ("Read: Seek to %d\n", post_pos);
    }
    else if (!FLOW_IS_EVENT (packet_data))
    {
      test_end (TEST_RESULT_FAILED, "got a weird object from read shunt");
    }    
  }
  else if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
  {
    packet_size = flow_packet_get_size (packet);
    if (packet_size == 0)
      test_end (TEST_RESULT_FAILED, "got zero-size buffer packet");

    test_print ("Read: %d byte packet at offset %d\n", packet_size, post_pos);

    if (!started_reading)
      test_end (TEST_RESULT_FAILED, "got data before start of stream");

    if (finished_reading)
      test_end (TEST_RESULT_FAILED, "got data after end of stream");

    if (!in_segment)
      test_end (TEST_RESULT_FAILED, "got data outside segment");

    if (post_pos + packet_size > BUFFER_SIZE)
      test_end (TEST_RESULT_FAILED, "read out of bounds data");

    memcpy_to_be_synced_only ((guchar *) post_buffer + post_pos, packet_data, packet_size);
    post_pos += packet_size;
  }
  else
  {
    test_end (TEST_RESULT_FAILED, "got unknown packet format");
  }

  if (finished_writing && n_segments_remaining == 0 && !quitting_main_loop)
  {
    /* Done reading. Wait a little bit to see if shunt will generate any bogus events. */

    test_print ("Done reading. Quitting after short delay...\n");
    quitting_main_loop = TRUE;
    g_timeout_add (1000, (GSourceFunc) test_quit_main_loop, NULL);
  }

  flow_packet_free (packet);
}

static FlowPacket *
write_full_request (FlowShunt *shunt, gpointer data)
{
  FlowPacket      *packet;
  static gboolean  wrote_seek = FALSE;

  if (!wrote_seek)
  {
    /* 1. Seek to beginning of file */
    packet = flow_packet_new_take_object (flow_position_new (FLOW_OFFSET_ANCHOR_BEGIN, 0), 0);
    wrote_seek = TRUE;
  }
  else
  {
    /* 2. Request read-to-EOF, no more writes */
    packet = flow_packet_new_take_object (flow_segment_request_new (-1), 0);
    flow_shunt_set_write_func (shunt, NULL, NULL);
  }

  return packet;
}

static void
verify_buffer_sparse (gconstpointer pre_buf, gconstpointer post_buf, gint len_bytes)
{
  gint i;
  gint n_similar = 0;

  for (i = 0; i < len_bytes; i++)
  {
    const guchar *p0 = pre_buf + i;
    const guchar *p1 = post_buf + i;

    if (*p1 == 0)
    {
      /* Known out of sync segment */
    }
    else if (*p1 == 1)
    {
      /* To be synced segment - this means we didn't process a read */
      test_print ("Sparse verify: Incomplete read starting at %d\n", i);
      test_end (TEST_RESULT_FAILED, "missed one or more reads");
    }
    else
    {
      if (*p0 != *p1)
      {
        test_print ("Sparse verify: Differing data starting at %d\n", i);
        test_end (TEST_RESULT_FAILED, "corrupt post-I/O data (sparse)");
      }

      n_similar++;
    }
  }

  test_print ("Similar bytes after random I/O: %d out of %d\n", n_similar, len_bytes);
}

static void
verify_buffer_full (gconstpointer pre_buf, gconstpointer post_buf, gint len_bytes)
{
  gint i;

  for (i = 0; i < len_bytes; i++)
  {
    const gchar *p0 = pre_buf + i;
    const gchar *p1 = post_buf + i;

    if (*p0 != *p1)
    {
      test_print ("Full verify: Differing data starting at %d\n", i);
      test_end (TEST_RESULT_FAILED, "corrupt post-I/O data (full)");
    }
  }
}

static void
test_run (void)
{
  gchar *test_file_name;

  g_random_set_seed (time (NULL));

  /* Set up a buffer with random data */

  pre_buffer = g_malloc (BUFFER_SIZE);
  randomize_buffer (pre_buffer, BUFFER_SIZE);

  post_buffer = g_malloc0 (BUFFER_SIZE);

  test_print ("Probability of pause is %d out of %d\n", PAUSE_PROBABILITY, PROBABILITY_MULTIPLIER);

  /* Create and write out scratch file */

  pre_pos            = 0;
  finished_reading   = FALSE;
  finished_writing   = FALSE;
  reads_are_blocked  = FALSE;
  writes_are_blocked = FALSE;

  test_file_name = g_strdup_printf ("test-shunt-data-%08x", g_random_int ());

  global_shunt = flow_create_file (test_file_name, FLOW_READ_ACCESS | FLOW_WRITE_ACCESS, TRUE,
                                   FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                                   FLOW_NO_ACCESS,
                                   FLOW_NO_ACCESS);

  flow_shunt_set_write_func (global_shunt, write_stream, global_shunt);

  test_run_main_loop ();

#if 0
  /* FIXME: Flawed test */

  /* Do random I/O on file and validate results */

  finished_reading = FALSE;
  finished_writing = FALSE;
  pre_pos  = BUFFER_SIZE;
  post_pos = BUFFER_SIZE;
  flow_shunt_set_read_func  (global_shunt, read_random,  global_shunt);
  flow_shunt_set_write_func (global_shunt, write_random, global_shunt);
  flow_shunt_unblock_writes (global_shunt);

  test_run_main_loop ();

  verify_buffer_sparse (pre_buffer, post_buffer, BUFFER_SIZE);
#endif

  /* Re-read entire file and validate results */

  finished_reading = FALSE;
  finished_writing = FALSE;
  pre_pos  = BUFFER_SIZE;
  post_pos = BUFFER_SIZE;
  flow_shunt_set_read_func  (global_shunt, read_stream,        global_shunt);
  flow_shunt_set_write_func (global_shunt, write_full_request, global_shunt);
  flow_shunt_unblock_writes (global_shunt);

  test_run_main_loop ();

  verify_buffer_full (pre_buffer, post_buffer, BUFFER_SIZE);

  /* Cleanup */

  flow_shunt_destroy (global_shunt);
  unlink (test_file_name);
  g_free (test_file_name);
  g_free (pre_buffer);
  g_free (post_buffer);
}
