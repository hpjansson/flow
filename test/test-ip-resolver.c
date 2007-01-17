/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-ip-resolver.c - FlowIPResolver test.
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

#define TEST_UNIT_NAME "FlowIPResolver"
#define TEST_TIMEOUT_S 60

#include "test-common.c"

static gint to_resolve;

const gchar * const resolve_names [] =
{
  "www.google.com",
  "www.ipv6.digital.com",
  "balle.google.com",
  "www.b-n.com",
  "www.amazon.com",
  "www.6init.org",
  NULL
};

const gchar * const resolve_addrs [] =
{
  "64.233.167.147",
  "64.233.167.99",
  "64.233.167.104",
  "216.201.112.12",
  "207.171.175.29",
  "2001:630:D0:F104:A00:20FF:FEB5:EF1E",
  "152.78.189.239",
  "3FFE:1200:2001:1:8000:0:0:2",
  NULL
};

static void
resolved (GList *addr_list, GList *name_list, FlowIPResolver *ip_resolver)
{
  GList *l;

  if (!name_list && addr_list)
  {
    for (l = addr_list; l; l = g_list_next (l))
    {
      FlowIPAddr *ip_addr     = l->data;
      gchar      *ip_addr_str = flow_ip_addr_get_string (ip_addr);

      test_print ("%-20.20s -> %s\n", "[NULL]", ip_addr_str);
      g_free (ip_addr_str);
    }
  }
  else if (name_list && !addr_list)
  {
    for (l = name_list; l; l = g_list_next (l))
    {
      gchar *name = l->data;

      test_print ("%-20.20s -> %s\n", name, "[NULL]");
    }
  }
  else if (!name_list && !addr_list)
  {
    test_end (TEST_RESULT_FAILED, "none-to-none resolution");
  }
  else  /* name_list && addr_list */
  {
    for ( ; name_list; name_list = g_list_next (name_list))
    {
      gchar *name = name_list->data;

      for (l = addr_list; l; l = g_list_next (l))
      {
        FlowIPAddr *ip_addr     = l->data;
        gchar      *ip_addr_str = flow_ip_addr_get_string (ip_addr);

        test_print ("%-20.20s -> %s\n", name, ip_addr_str);
        g_free (ip_addr_str);
      }
    }
  }

  to_resolve--;

  if (!to_resolve)
    test_quit_main_loop ();
}

static void
test_run (void)
{
  FlowIPResolver *ip_resolver;
  gint            i;

  ip_resolver = flow_ip_resolver_new ();

  for (i = 0; resolve_names [i]; i++)
  {
    flow_ip_resolver_resolve_name (ip_resolver, resolve_names [i],
                                   (FlowIPLookupFunc *) resolved, ip_resolver);
  }

  to_resolve = i;

  for (i = 0; resolve_addrs [i]; i++)
  {
    FlowIPAddr *ip_addr;

    ip_addr = flow_ip_addr_new ();
    if (!flow_ip_addr_set_string (ip_addr, resolve_addrs [i]))
      test_end (TEST_RESULT_FAILED, "IP address parser failed");

    flow_ip_resolver_resolve_ip_addr (ip_resolver, ip_addr,
                                      (FlowIPLookupFunc *) resolved, ip_resolver);

    g_object_unref (ip_addr);
  }

  to_resolve += i;

  test_run_main_loop ();

  g_object_unref (ip_resolver);
}
