/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-common.c - Common code for tests.
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

/* This file should be included directly in each test program */

#define __USE_GNU 1

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <glib/gprintf.h>

#include <flow/flow-gobject-util.h>
#include <flow/flow.h>

typedef enum
{
  TEST_RESULT_OK,
  TEST_RESULT_SYSTEM_ERROR,
  TEST_RESULT_FAILED,
  TEST_RESULT_CRASH,
  TEST_RESULT_TIMEOUT
}
TestResult;

struct timeval begin_tv;

static void test_run (void);

static const gchar *result_text [] =
{
  "passed",
  "not run - system error",
  "FAILED",
  "FAILED - crash",
  "FAILED - timeout"
};

static TestResult global_result_code  = TEST_RESULT_OK;

static gboolean   test_is_verbose       = FALSE;
static gboolean   test_trap             = TRUE;
static gboolean   test_abort_on_error   = FALSE;
static gboolean   test_suspend_on_error = FALSE;
static GMainLoop *main_loop;

static gchar *
test_print_timestamp (void)
{
  struct timeval  now_tv;
  struct timeval  diff_tv;

  gettimeofday (&now_tv, NULL);

  diff_tv.tv_sec = now_tv.tv_sec - begin_tv.tv_sec;
  if (now_tv.tv_usec < begin_tv.tv_usec)
  {
    diff_tv.tv_sec -= 1;
    now_tv.tv_usec += 1000000;
  }

  diff_tv.tv_usec = now_tv.tv_usec - begin_tv.tv_usec;

  return g_strdup_printf ("%02lu:%02lu.%02lu", diff_tv.tv_sec / 60, diff_tv.tv_sec % 60,
                          diff_tv.tv_usec / 10000);
}

G_GNUC_UNUSED static void
test_print (const gchar *format, ...)
{
  va_list  args;
  gchar   *retval;
  gchar   *joined;
  gchar   *ts;

  if (!test_is_verbose)
    return;

  ts = test_print_timestamp ();

  va_start (args, format);
  retval = g_strdup_vprintf (format, args);
  va_end (args);

  joined = g_strjoin (" ", ts, retval, NULL);

  fwrite (joined, 1, strlen (joined), stdout);
  fflush (stdout);

  g_free (joined);
  g_free (retval);
  g_free (ts);
}

static void
test_end (TestResult result_code, const gchar *result_description)
{
  gchar *ts;

  ts = test_print_timestamp ();

  g_print ("%s%s%s%s%s%s\n",
           test_is_verbose ? "\n" : "",
           test_is_verbose ? ts : "",
           test_is_verbose ? " Result: " : "",
           result_text [result_code],
           result_description ? " - " : "",
           result_description ? result_description : "");

  g_free (ts);
  g_main_loop_unref (main_loop);

  global_result_code = result_code;

  if (result_code == TEST_RESULT_OK)
    exit (0);

  if (test_abort_on_error)
    G_BREAKPOINT ();

  if (test_suspend_on_error)
  {
    g_print ("Going to sleep - you can attach a debugger now.\n");
    sleep (30 * 24 * 60 * 60);  /* 1 month should do it */
  }

  exit (1);
}

static void
test_crash (void)
{
  test_end (TEST_RESULT_CRASH, NULL);
}

static void
test_timeout (void)
{
  test_end (TEST_RESULT_TIMEOUT, NULL);
}

static void
test_begin (const gchar *name)
{
  g_print ("Testing %s%s", name, test_is_verbose ? "...\n\n" : ": ");
  fflush (stdout);

  g_type_init ();
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR);

  main_loop = g_main_loop_new (NULL, FALSE);

  if (test_trap)
  {
    /* In case the test crashes, bail out and notify */
    signal (SIGSEGV, (sig_t) test_crash);

    /* In case the test hangs, bail out and notify */
    signal (SIGALRM, (sig_t) test_timeout);
    alarm (TEST_TIMEOUT_S);
  }

  gettimeofday (&begin_tv, NULL);
}

G_GNUC_UNUSED static void
test_run_main_loop (void)
{
  g_main_loop_run (main_loop);
}

G_GNUC_UNUSED static void
test_quit_main_loop (void)
{
  g_main_loop_quit (main_loop);
}

static void
print_help (const gchar *prog_name)
{
  g_printerr ("%s tests %s. It takes the following arguments:\n\n"
              "-h --help      Print this help and exit.\n"
              "-v --verbose   Show test debug information.\n"
              "-n --no-trap   Don't trap timeout or segv.\n"
              "-a --abort     Abort on error, don't exit.\n"
              "-s --suspend   Emit a message and go to sleep on error.\n\n",
              prog_name, TEST_UNIT_NAME);
}

gint
main (gint argc, gchar *argv [])
{
  gint i;

  for (i = 1; i < argc; i++)
  {
    gchar *arg = argv [i];

    if (!strcmp (arg, "-h") || !strcmp (arg, "--help"))
    {
      print_help (argv [0]);
      exit (0);
    }
    else if (!strcmp (arg, "-v") || !strcmp (arg, "--verbose"))
    {
      test_is_verbose = TRUE;
    }
    else if (!strcmp (arg, "-n") || !strcmp (arg, "--no-trap"))
    {
      test_trap = FALSE;
    }
    else if (!strcmp (arg, "-a") || !strcmp (arg, "--abort"))
    {
      test_abort_on_error = TRUE;
    }
    else if (!strcmp (arg, "-s") || !strcmp (arg, "--suspend"))
    {
      test_suspend_on_error = TRUE;
    }
    else
    {
      g_printerr ("%s: Unknown argument '%s'.\n", argv [0], arg);
      print_help (argv [0]);
      exit (1);
    }
  }

  if (test_abort_on_error && test_suspend_on_error)
  {
    g_printerr ("%s: Options --abort and --suspend conflict.\n", argv [0]);
    print_help (argv [0]);
    exit (1);
  }

  test_begin (TEST_UNIT_NAME);
  test_run ();
  test_end (TEST_RESULT_OK, NULL);

  return 0;
}
