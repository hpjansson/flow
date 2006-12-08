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
#include "flow-property-event.h"
#include "flow-messages.h"
#include "flow-util.h"

#define STRERROR_MAX_LEN 256

void
flow_connect_simplex__simplex (FlowSimplexElement *output_simplex,
                               FlowSimplexElement *input_simplex)
{
  if (output_simplex)
  {
    if (input_simplex)
    {
      flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (output_simplex)),
                        FLOW_PAD (flow_simplex_element_get_input_pad  (input_simplex)));
    }
    else
    {
      flow_pad_disconnect (FLOW_PAD (flow_simplex_element_get_output_pad (output_simplex)));
    }
  }
  else if (input_simplex)
  {
    flow_pad_disconnect (FLOW_PAD (flow_simplex_element_get_input_pad (input_simplex)));
  }
}

void
flow_connect_duplex__duplex (FlowDuplexElement *downstream_duplex,
                             FlowDuplexElement *upstream_duplex)
{
  FlowPad *downstream_duplex_upstream_pads [2];
  FlowPad *upstream_duplex_downstream_pads [2];

  if (upstream_duplex)
    flow_duplex_element_get_upstream_pads (upstream_duplex,
                                           (FlowInputPad **) &upstream_duplex_downstream_pads [0],
                                           (FlowOutputPad **) &upstream_duplex_downstream_pads [1]);

  if (downstream_duplex)
  {
    flow_duplex_element_get_upstream_pads (downstream_duplex,
                                           (FlowInputPad **) &downstream_duplex_upstream_pads [0],
                                           (FlowOutputPad **) &downstream_duplex_upstream_pads [1]);

    if (upstream_duplex)
    {
      flow_pad_connect (downstream_duplex_upstream_pads [0],
                        upstream_duplex_downstream_pads [1]);
      flow_pad_connect (downstream_duplex_upstream_pads [1],
                        upstream_duplex_downstream_pads [0]);
    }
    else
    {
      flow_pad_disconnect (downstream_duplex_upstream_pads [0]);
      flow_pad_disconnect (downstream_duplex_upstream_pads [1]);
    }
  }
  else if (upstream_duplex)
  {
    flow_pad_disconnect (upstream_duplex_downstream_pads [0]);
    flow_pad_disconnect (upstream_duplex_downstream_pads [1]);
  }
}

void
flow_connect_simplex_simplex__duplex (FlowSimplexElement *downstream_simplex_output,
                                      FlowSimplexElement *downstream_simplex_input,
                                      FlowDuplexElement  *upstream_duplex)
{
  FlowPad *duplex_downstream_pads [2];
  FlowPad *simplex_pad;

  if (upstream_duplex)
  {
    flow_duplex_element_get_downstream_pads (upstream_duplex,
                                             (FlowInputPad **)  &duplex_downstream_pads [0],
                                             (FlowOutputPad **) &duplex_downstream_pads [1]);

    if (downstream_simplex_output)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_output_pad (downstream_simplex_output));
      flow_pad_connect (simplex_pad, duplex_downstream_pads [0]);
    }
    else
    {
      flow_pad_disconnect (duplex_downstream_pads [0]);
    }

    if (downstream_simplex_input)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_input_pad (downstream_simplex_input));
      flow_pad_connect (simplex_pad, duplex_downstream_pads [1]);
    }
    else
    {
      flow_pad_disconnect (duplex_downstream_pads [1]);
    }
  }
  else
  {
    if (downstream_simplex_output)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_output_pad (downstream_simplex_output));
      flow_pad_disconnect (simplex_pad);
    }

    if (downstream_simplex_input)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_input_pad (downstream_simplex_input));
      flow_pad_disconnect (simplex_pad);
    }
  }
}

void
flow_connect_duplex__simplex_simplex (FlowDuplexElement  *downstream_duplex,
                                      FlowSimplexElement *upstream_simplex_output,
                                      FlowSimplexElement *upstream_simplex_input)
{
  FlowPad *duplex_upstream_pads [2];
  FlowPad *simplex_pad;

  if (downstream_duplex)
  {
    flow_duplex_element_get_upstream_pads (downstream_duplex,
                                           (FlowInputPad **)  &duplex_upstream_pads [0],
                                           (FlowOutputPad **) &duplex_upstream_pads [1]);

    if (upstream_simplex_output)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_output_pad (upstream_simplex_output));
      flow_pad_connect (duplex_upstream_pads [0], simplex_pad);
    }
    else
    {
      flow_pad_disconnect (duplex_upstream_pads [0]);
    }

    if (upstream_simplex_input)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_input_pad (upstream_simplex_input));
      flow_pad_connect (duplex_upstream_pads [1], simplex_pad);
    }
    else
    {
      flow_pad_disconnect (duplex_upstream_pads [1]);
    }
  }
  else
  {
    if (upstream_simplex_output)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_output_pad (upstream_simplex_output));
      flow_pad_disconnect (simplex_pad);
    }

    if (upstream_simplex_input)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_input_pad (upstream_simplex_input));
      flow_pad_disconnect (simplex_pad);
    }
  }
}

void
flow_disconnect_element (FlowElement *element)
{
  GPtrArray *pads;
  guint      i;

  pads = flow_element_get_input_pads (element);

  for (i = 0; i < pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (pads, i);
    flow_pad_disconnect (pad);
  }

  pads = flow_element_get_output_pads (element);

  for (i = 0; i < pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (pads, i);
    flow_pad_disconnect (pad);
  }
}

FlowPacket *
flow_create_simple_event_packet (const gchar *domain, gint code)
{
  FlowDetailedEvent *detailed_event;

  detailed_event = flow_detailed_event_new_literal (flow_get_event_message (domain, code));
  flow_detailed_event_add_code (detailed_event, domain, code);

  return flow_packet_new_take_object (detailed_event, 0);
}

gboolean
flow_handle_universal_events (FlowElement *element, FlowPacket *packet)
{
  gboolean result = FALSE;

  if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
  {
    gpointer instance = flow_packet_get_data (packet);

    /* Try to apply property events to element */

    if (FLOW_IS_PROPERTY_EVENT (instance))
      result = flow_property_event_try_apply (instance, element);
  }

  return result;
}

static void
shunt_read_object (FlowShunt *shunt, FlowPacket *packet, gpointer **object_dest)
{
  FlowPacketFormat packet_format = flow_packet_get_format (packet);
  gpointer         packet_data   = flow_packet_get_data (packet);

  g_assert (packet_format == FLOW_PACKET_FORMAT_OBJECT);

  flow_shunt_set_read_func (shunt, NULL, NULL);
  *object_dest = g_object_ref (packet_data);
  flow_packet_free (packet);
}

gpointer
flow_read_object_from_shunt (FlowShunt *shunt)
{
  FlowShuntReadFunc *old_read_func;
  gpointer           old_user_data;
  gpointer           object = NULL;

  flow_shunt_get_read_func (shunt, &old_read_func, &old_user_data);

  flow_shunt_set_read_func (shunt, (FlowShuntReadFunc *) shunt_read_object, &object);
  flow_shunt_dispatch_now (shunt, NULL, NULL);

  flow_shunt_set_read_func (shunt, old_read_func, old_user_data);
  return object;
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
