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

#define TEST_UNIT_NAME "FlowFileIO"
#define TEST_TIMEOUT_S 180

/* Test variables; adjustable */

#define THREADS_TOTAL          15
#define THREADS_CONCURRENT_MAX 5

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
  gint offset;
  gint len;
}
TransferInfo;

static guchar            *buffer                 = NULL;

static GMutex             global_mutex;

static gint               threads_started        = 0;
static gint               threads_running        = 0;
static gint               threads_ended          = 0;

static void
subthread_main (void)
{
  FlowFileIO    *file_io;
  TransferInfo   transfer_info = { 0, 0 };
  gchar         *file_name;
  guchar         temp_buffer [PACKET_MAX_SIZE];
  gboolean       result;
  GError        *error = NULL;

  test_print ("Subthread opening file\n");

  file_name = g_strdup_printf ("test-file-io-scratch-%p-%08x", g_thread_self (), g_random_int ());
  file_io = flow_file_io_new ();
  result = flow_file_io_sync_create (file_io, file_name, FLOW_READ_ACCESS | FLOW_WRITE_ACCESS, TRUE,
                                     FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                                     FLOW_NO_ACCESS,
                                     FLOW_NO_ACCESS,
                                     &error);

  if (!result)
  {
    test_end (TEST_RESULT_FAILED, "failed to create scratch file");
    g_error_free (error);
    g_free (file_name);
    return;
  }

  test_print ("Subthread opened file\n");

  transfer_info.offset = 0;

  while (transfer_info.len < BUFFER_SIZE)
  {
    gint op = g_random_int_range (0, 3);

    g_assert (transfer_info.offset <= transfer_info.len);

    if (op == 0)
    {
      gint len;

      /* Append some data (seek to EOF if not already there) */

      if (transfer_info.offset < transfer_info.len)
      {
        test_print ("Append: Seeking to EOF\n");
        flow_file_io_seek (file_io, FLOW_OFFSET_ANCHOR_END, 0);
      }

      len = g_random_int_range (PACKET_MIN_SIZE, PACKET_MAX_SIZE);
      len = MIN (len, BUFFER_SIZE - transfer_info.len);

      test_print ("Append: Appending %d bytes\n", len);

      flow_io_sync_write (FLOW_IO (file_io), buffer + transfer_info.len, len, NULL);

      transfer_info.len += len;
      transfer_info.offset = transfer_info.len;
    }
    else if (op == 1 && transfer_info.len > 0)
    {
      gint len;

      /* Read some data (seek back if at EOF) */

      if (transfer_info.offset == transfer_info.len)
      {
        gint offset = g_random_int_range (0, transfer_info.len);

        test_print ("Read: Seeking to %d\n", offset);
        flow_file_io_seek_to (file_io, offset);
        transfer_info.offset = offset;
      }

      len = g_random_int_range (PACKET_MIN_SIZE, PACKET_MAX_SIZE);
      len = MIN (len, transfer_info.len - transfer_info.offset);

      test_print ("Read: Reading %d bytes\n", len);

      /* TODO: Verify return value? */
      flow_io_sync_read_exact (FLOW_IO (file_io), temp_buffer, len, NULL);

      if (memcmp (temp_buffer, buffer + transfer_info.offset, len))
        test_end (TEST_RESULT_FAILED, "Data verification error");

      test_print ("Verified %d bytes ok\n", len);

      transfer_info.offset += len;
    }
    else if (transfer_info.len > 0)
    {
      gint offset = g_random_int_range (0, transfer_info.len);

      /* Seek to random offset */

      test_print ("Seek: Seeking to %d\n", offset);

      flow_file_io_seek_to (file_io, offset);
      transfer_info.offset = offset;
    }
  }

  test_print ("Subthread disconnecting\n");
  flow_file_io_sync_close (file_io, NULL);
  test_print ("Subthread disconnected\n");

  g_object_unref (file_io);
  test_print ("Subthread cleaned up\n");

  threads_ended++;
  threads_running--;

  remove (file_name);
  g_free (file_name);
}

static gboolean
spawn_subthread (void)
{
  gboolean run_again = TRUE;

  g_mutex_lock (&global_mutex);

  if (threads_ended == THREADS_TOTAL)
  {
    test_quit_main_loop ();
    run_again = FALSE;
  }
  else if (threads_started == THREADS_TOTAL ||
           threads_running == THREADS_CONCURRENT_MAX)
  {
    /* Wait for threads to finish */
  }
  else
  {
    threads_started++;
    threads_running++;

    test_print ("Spawning new subthread\n");
    g_thread_new (NULL, (GThreadFunc) subthread_main, NULL);
  }

  g_mutex_unlock (&global_mutex);

  return TRUE;
}

static void
long_test (void)
{
  guint id;

  test_print ("Long test begin\n");

  id = g_timeout_add (250, (GSourceFunc) spawn_subthread, NULL);
  test_run_main_loop ();
  g_source_remove (id);

  test_print ("Long test end\n");
}

static void
test_run (void)
{
  gint i;

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

  long_test ();

  g_free (buffer);
  buffer = NULL;
}
