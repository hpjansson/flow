/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-tcp-io.c - FlowTcpIO test.
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

#define TEST_UNIT_NAME "FlowTcpIO"
#define TEST_TIMEOUT_S 60

/* Test variables; adjustable */

#define BUFFER_SIZE            5000000   /* Amount of data to transfer */
#define PACKET_MAX_SIZE        8192      /* Max transfer unit */
#define PACKET_MIN_SIZE        1         /* Min transfer unit */

#define TOTAL_PAUSE_TIME_MS    3000      /* Total time to spend *not* reading or writing */
#define PAUSE_MIN_LENGTH_MS    10        /* Min pause unit */
#define PAUSE_MAX_LENGTH_MS    200       /* Max pause unit */

/* Calculations to determine the probability of pausing for
 * each packet processed. No user serviceable parts inside. */

#define PROBABILITY_MULTIPLIER 1000000   /* For fixed-point fractions */
#define PACKET_AVG_SIZE        (PACKET_MIN_SIZE + ((PACKET_MAX_SIZE - PACKET_MIN_SIZE) / 2))
#define PAUSE_AVG_LENGTH_MS    (PAUSE_MIN_LENGTH_MS + ((PAUSE_MAX_LENGTH_MS - PAUSE_MIN_LENGTH_MS) / 2))
#define NUM_PAUSES             (TOTAL_PAUSE_TIME_MS / PAUSE_AVG_LENGTH_MS)
#define TOTAL_EXPECTED_PACKETS (BUFFER_SIZE / PACKET_AVG_SIZE)
#define TOTAL_EXPECTED_EVENTS  (TOTAL_EXPECTED_PACKETS * 2)  /* Account for both reads and writes */
#define PAUSE_PROBABILITY      ((NUM_PAUSES * PROBABILITY_MULTIPLIER) / TOTAL_EXPECTED_EVENTS)

#include "test-common.c"

static guchar            *buffer;
static guint              src_index;
static guint              dest_index;

static FlowTcpIOListener *tcp_listener = NULL;
static FlowTcpIO         *tcp_reader   = NULL;
static FlowTcpIO         *tcp_writer   = NULL;

static void
short_tests (void)
{
  FlowIPService *loopback_service;
  guchar         read_buffer [2048];

  loopback_service = flow_ip_service_new ();
  flow_ip_addr_set_string (FLOW_IP_ADDR (loopback_service), "127.0.0.1");
  flow_ip_service_set_port (loopback_service, 2533);

  tcp_listener = flow_tcp_io_listener_new ();
  if (!flow_tcp_listener_set_local_service (FLOW_TCP_LISTENER (tcp_listener), loopback_service, NULL))
    test_end (TEST_RESULT_FAILED, "could not bind short-test listener");

  tcp_writer = flow_tcp_io_new ();
  if (!flow_tcp_io_sync_connect (tcp_writer, loopback_service))
    test_end (TEST_RESULT_FAILED, "could not connect to short-test listener");

  tcp_reader = flow_tcp_io_listener_sync_pop_connection (tcp_listener);
  if (!tcp_reader)
    test_end (TEST_RESULT_FAILED, "missed connection on listener end");

  flow_io_write (FLOW_IO (tcp_writer), buffer,        512);
  flow_io_write (FLOW_IO (tcp_writer), buffer +  512, 512);
  flow_io_write (FLOW_IO (tcp_writer), buffer + 1024, 512);
  flow_io_write (FLOW_IO (tcp_writer), buffer + 1536, 512);

  /* Partial read */

  flow_io_sync_read_exact (FLOW_IO (tcp_reader), read_buffer, 2000);

  /* Check read */

  if (memcmp (buffer, read_buffer, 2000))
    test_end (TEST_RESULT_FAILED, "data mismatch in short transfer (1)");

  /* Read remaining bytes with an oversized request */

  if (flow_io_sync_read (FLOW_IO (tcp_reader), read_buffer, 100) != 48)
    test_end (TEST_RESULT_FAILED, "oversized read request returned wrong count");

  /* Make sure remaining bytes match */

  if (memcmp (buffer + 2000, read_buffer, 48))
    test_end (TEST_RESULT_FAILED, "data mismatch in short transfer (2)");

  /* Make sure nothing got clobbered */

  if (memcmp (buffer + 48, read_buffer + 48, 1000))
    test_end (TEST_RESULT_FAILED, "data mismatch in short transfer (3)");

  g_object_unref (loopback_service);
}

static void
test_run (void)
{
  gint           i;

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

  src_index          = 0;
  dest_index         = 0;

  short_tests ();

  g_free (buffer);
}
