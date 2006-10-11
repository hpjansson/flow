/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-util.c - General utilities.
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

#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "flow-detailed-event.h"
#include "flow-messages.h"
#include "flow-util.h"

#define STRERROR_MAX_LEN 256

FlowPacket *
flow_create_simple_event_packet (const gchar *domain, gint code)
{
  FlowDetailedEvent *detailed_event;

  detailed_event = flow_detailed_event_new_literal (flow_get_event_message (domain, code));
  flow_detailed_event_add_code (detailed_event, domain, code);

  return flow_packet_new_take_object (detailed_event, 0);
}

gchar *
flow_strerror (gint errnum)
{
  gchar buf [STRERROR_MAX_LEN] = "";

  strerror_r (errnum, buf, STRERROR_MAX_LEN);

  /* Usually, strerror will do this for us. However, its behaviour
   * varies across operating systems, so this is here as a safeguard. */
  if (!*buf)
    return g_strdup_printf ("Unknown error code %d", errnum);

  return g_strdup (buf);
}

static inline gint
ptr_array_find (GPtrArray *array, gpointer data)
{
  guint i;

  for (i = 0; i < array->len; i++)
  {
    if (g_ptr_array_index (array, i) == data)
      return i;
  }

  return -1;
}

static inline gboolean
ptr_array_remove_sparse (GPtrArray *array, gpointer data)
{
  gint i;

  i = ptr_array_find (array, data);
  if (i < 0)
    return FALSE;

  g_ptr_array_index (array, i) = NULL;
  return TRUE;
}

static inline void
ptr_array_add_sparse (GPtrArray *array, gpointer data)
{
  guint i;

  for (i = 0; i < array->len; i++)
  {
    gpointer ptr = g_ptr_array_index (array, i);

    if (!ptr)
    {
      g_ptr_array_index (array, i) = data;
      return;
    }
  }

  g_ptr_array_add (array, data);
}

gint
flow_g_ptr_array_find (GPtrArray *array, gpointer data)
{
  g_return_val_if_fail (array != NULL, -1);

  return ptr_array_find (array, data);
}

gboolean
flow_g_ptr_array_remove_sparse (GPtrArray *array, gpointer data)
{
  g_return_val_if_fail (array != NULL, -1);

  return ptr_array_remove_sparse (array, data);
}

void
flow_g_ptr_array_add_sparse (GPtrArray *array, gpointer data)
{
  g_return_if_fail (array != NULL);

  ptr_array_add_sparse (array, data);
}

guint
flow_g_ptr_array_compress (GPtrArray *array)
{
  guint old_len;
  guint new_len;
  gint  i, j;

  g_return_val_if_fail (array != NULL, -1);

  /* Walk backwards from the end of the array. We find the last valid pointer (j),
   * then the last empty slot to move it into (i). i < j always. For every NULL
   * pointer found, we decrement new_len by 1. The result is a compact array with
   * no holes. The order of the elements may change. Algorithm is O(n) in initial
   * size of the array. */

  old_len = array->len;
  new_len = old_len;
  j       = old_len - 1;
  i       = j - 1;

  for (;;)
  {
    for ( ; j >= 0; j--)
    {
      gpointer ptr = g_ptr_array_index (array, j);

      if (ptr)
        break;

      new_len--;
    }

    if (j < 1)
      break;

    if (i > j)
      i = j - 1;

    for ( ; i >= 0; i--)
    {
      gpointer ptr = g_ptr_array_index (array, i);

      if (!ptr)
      {
        g_ptr_array_index (array, i) = g_ptr_array_index (array, j);
        new_len--;
        j--;
        break;
      }
    }
  }

  g_ptr_array_set_size (array, new_len);
  return old_len - new_len;
}
