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

#define SOCKETS_NUM            15
#define SOCKETS_CONCURRENT_MAX 5

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

typedef struct
{
  gint read_offset;
  gint write_offset;
}
TransferInfo;

static guchar            *buffer                 = NULL;

static FlowIPService     *loopback_service       = NULL;
static FlowIPService     *bad_loopback_service   = NULL;
static FlowTcpIOListener *tcp_listener           = NULL;
static FlowTcpIO         *tcp_reader             = NULL;
static FlowTcpIO         *tcp_writer             = NULL;

static GHashTable        *transfer_info_table    = NULL;

static GMainLoop         *main_loop              = NULL;
static gint               sockets_done           = 0;
static gint               sockets_running        = 0;

static FlowTcpIO         *main_tcp_io            = NULL;

static void
transfer_info_free (TransferInfo *transfer_info)
{
  g_slice_free (TransferInfo, transfer_info);
}

static void
subthread_main (void)
{
  FlowTcpIO    *tcp_io;
  TransferInfo  transfer_info;
  guchar        temp_buffer [PACKET_MAX_SIZE];

  test_print ("Subthread connecting to main thread\n");

  tcp_io = flow_tcp_io_new ();
  flow_tcp_io_sync_connect (tcp_io, loopback_service);

  test_print ("Subthread connected to main thread\n");

  transfer_info.read_offset  = 0;
  transfer_info.write_offset = 0;

  while (transfer_info.read_offset  < BUFFER_SIZE ||
         transfer_info.write_offset < BUFFER_SIZE)
  {
    if (transfer_info.write_offset < BUFFER_SIZE)
    {
      gint len;

      len = g_random_int_range (PACKET_MIN_SIZE, PACKET_MAX_SIZE);
      len = MIN (len, BUFFER_SIZE - transfer_info.write_offset);

      flow_io_sync_write (FLOW_IO (tcp_io), buffer + transfer_info.write_offset, len);
      test_print ("Subthread wrote %d bytes\n", len);

      transfer_info.write_offset += len;
    }

    if (transfer_info.read_offset < BUFFER_SIZE)
    {
      gint len;

      len = g_random_int_range (PACKET_MIN_SIZE, PACKET_MAX_SIZE);
      len = MIN (len, BUFFER_SIZE - transfer_info.read_offset);

      if (!flow_io_sync_read_exact (FLOW_IO (tcp_io), temp_buffer, len))
        test_end (TEST_RESULT_FAILED, "subthread short read");

      test_print ("Subthread read %d bytes\n", len);

      if (memcmp (buffer + transfer_info.read_offset, temp_buffer, len))
        test_end (TEST_RESULT_FAILED, "subthread read mismatch");

      transfer_info.read_offset += len;
    }
  }

  test_print ("Subthread disconnecting\n");
  flow_tcp_io_sync_disconnect (tcp_io);
  test_print ("Subthread disconnected\n");

  g_object_unref (tcp_io);
  test_print ("Subthread cleaned up\n");
}

static gboolean
spawn_subthread (void)
{
  if (sockets_running >= SOCKETS_CONCURRENT_MAX || sockets_done >= SOCKETS_NUM)
    return TRUE;

  test_print ("Spawning new subthread\n");
  g_thread_create ((GThreadFunc) subthread_main, NULL, FALSE, NULL);

  sockets_running++;
  return TRUE;
}

static void
read_notify (FlowTcpIO *tcp_io)
{
  TransferInfo *transfer_info;
  guchar        temp_buffer [PACKET_MAX_SIZE];
  gint          len;

  test_print ("Main thread read notify (%p)\n", tcp_io);

  transfer_info = g_hash_table_lookup (transfer_info_table, tcp_io);
  g_assert (transfer_info != NULL);

  len = flow_io_read (FLOW_IO (tcp_io), temp_buffer, PACKET_MAX_SIZE);
  test_print ("Main thread read %d bytes\n", len);

  if (memcmp (buffer + transfer_info->read_offset, temp_buffer, len))
    test_end (TEST_RESULT_FAILED, "main thread read mismatch");

  transfer_info->read_offset += len;

  if (transfer_info->read_offset > BUFFER_SIZE)
    test_end (TEST_RESULT_FAILED, "main thread read past buffer length");
}

static void
write_notify (FlowTcpIO *tcp_io)
{
  TransferInfo *transfer_info;
  gint          len;

  test_print ("Main thread write notify (%p)\n", tcp_io);

  transfer_info = g_hash_table_lookup (transfer_info_table, tcp_io);
  g_assert (transfer_info != NULL);

  len = g_random_int_range (PACKET_MIN_SIZE, PACKET_MAX_SIZE);
  len = MIN (len, BUFFER_SIZE - transfer_info->write_offset);

  flow_io_write (FLOW_IO (tcp_io), buffer + transfer_info->write_offset, len);
  test_print ("Main thread wrote %d bytes\n", len);

  transfer_info->write_offset += len;

  if (transfer_info->write_offset == BUFFER_SIZE)
  {
    flow_io_set_write_notify (FLOW_IO (tcp_io), NULL, NULL);
    test_print ("Main thread read done\n");
  }
}

static void
lost_connection (FlowTcpIO *tcp_io)
{
  TransferInfo *transfer_info;

  if (flow_tcp_io_get_connectivity (tcp_io) != FLOW_CONNECTIVITY_DISCONNECTED)
    return;

  transfer_info = g_hash_table_lookup (transfer_info_table, tcp_io);
  g_assert (transfer_info != NULL);

  if (transfer_info->read_offset < BUFFER_SIZE)
    test_end (TEST_RESULT_FAILED, "main thread did not read all data");

  if (transfer_info->write_offset < BUFFER_SIZE)
    test_end (TEST_RESULT_FAILED, "main thread did not write all data");

  test_print ("Main thread lost connection\n");
  g_hash_table_remove (transfer_info_table, tcp_io);
  g_object_unref (tcp_io);

  sockets_done++;
  sockets_running--;

  if (sockets_done >= SOCKETS_NUM && sockets_running == 0)
    g_main_loop_quit (main_loop);
}

static void
new_connection (void)
{
  FlowTcpIO    *tcp_io;
  TransferInfo *transfer_info;

  tcp_io = flow_tcp_io_listener_pop_connection (tcp_listener);
  if (!tcp_io)
    return;

  main_tcp_io = tcp_io;

  test_print ("Main thread received connection\n");

  transfer_info = g_slice_new0 (TransferInfo);
  g_hash_table_insert (transfer_info_table, tcp_io, transfer_info);

  flow_io_set_read_notify  (FLOW_IO (tcp_io), (FlowNotifyFunc) read_notify, tcp_io);
  flow_io_set_write_notify (FLOW_IO (tcp_io), (FlowNotifyFunc) write_notify, tcp_io);

  g_signal_connect (tcp_io, "connectivity-changed", (GCallback) lost_connection, NULL);
}

static void
long_test (void)
{
  test_print ("Long test begin\n");

  g_timeout_add (250, (GSourceFunc) spawn_subthread, NULL);

  g_signal_connect (tcp_listener, "new-connection", (GCallback) new_connection, NULL);

  main_loop = g_main_loop_new (g_main_context_default (), FALSE);
  g_main_loop_run (main_loop);

  test_print ("Long test end\n");
}

static void
short_tests (void)
{
  guchar read_buffer [2048];

  test_print ("Short tests begin\n");

  tcp_writer = flow_tcp_io_new ();

  if (flow_tcp_io_sync_connect (tcp_writer, bad_loopback_service))
    test_end (TEST_RESULT_FAILED, "bad connect did not fail as expected");

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

  /* Disconnect */

  flow_tcp_io_sync_disconnect (tcp_reader);
  flow_tcp_io_sync_disconnect (tcp_writer);

  g_object_unref (tcp_reader);
  g_object_unref (tcp_writer);

  test_print ("Short tests end\n");
}

static void
test_run (void)
{
  FlowIPAddr *ip_addr;
  gint        i;

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

  transfer_info_table = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                               NULL, (GDestroyNotify) transfer_info_free);

  loopback_service = flow_ip_service_new ();
  ip_addr = flow_ip_addr_new ();
  flow_ip_addr_set_string (ip_addr, "127.0.0.1");
  flow_ip_service_set_port (loopback_service, 2533);
  flow_ip_service_add_address (loopback_service, ip_addr);
  g_object_unref (ip_addr);

  bad_loopback_service = flow_ip_service_new ();
  ip_addr = flow_ip_addr_new ();
  flow_ip_addr_set_string (ip_addr, "127.0.0.1");
  flow_ip_service_set_port (bad_loopback_service, 12505);
  flow_ip_service_add_address (bad_loopback_service, ip_addr);
  g_object_unref (ip_addr);

  tcp_listener = flow_tcp_io_listener_new ();
  if (!flow_tcp_listener_set_local_service (FLOW_TCP_LISTENER (tcp_listener), loopback_service, NULL))
    test_end (TEST_RESULT_FAILED, "could not bind short-test listener");

  short_tests ();
  long_test ();

  g_hash_table_destroy (transfer_info_table);
  transfer_info_table = NULL;

  g_object_unref (tcp_listener);
  tcp_listener = NULL;

  g_object_unref (loopback_service);
  loopback_service = NULL;

  g_free (buffer);
  buffer = NULL;
}
