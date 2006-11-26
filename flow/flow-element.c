/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-element.c - A pipeline processing element.
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
#include "flow-gobject-util.h"
#include "flow-element.h"

/* --- FlowElement private data --- */

typedef struct
{
}
FlowElementPrivate;

/* --- FlowElement properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_element)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowElement definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_element, FlowElement, G_TYPE_OBJECT, G_TYPE_FLAG_ABSTRACT)

/* --- FlowElement implementation --- */

static void
flow_element_type_init (GType type)
{
}

static void
flow_element_class_init (FlowElementClass *klass)
{
}

static void
flow_element_init (FlowElement *element)
{
  element->input_pads  = g_ptr_array_new ();
  element->output_pads = g_ptr_array_new ();
}

static void
flow_element_construct (FlowElement *element)
{
}

static void
flow_element_dispose (FlowElement *element)
{
  /* The user may destroy us in one of our callbacks. When this happens, make
   * sure we don't generate any more output (disconnect all pads), but stay
   * alive until the callback dispatcher can exit. */

  g_slist_free (element->pending_inputs);
  element->pending_inputs = NULL;

  if (element->dispatch_depth)
  {
    guint i;

    for (i = 0; i < element->input_pads->len; i++)
    {
      FlowPad *pad = g_ptr_array_index (element->input_pads, i);
      flow_pad_disconnect (pad);
    }

    for (i = 0; i < element->output_pads->len; i++)
    {
      FlowPad *pad = g_ptr_array_index (element->output_pads, i);
      flow_pad_disconnect (pad);
    }

    /* This is matched in flow-input-pad.c and flow-output-pad.c */
    element->was_disposed = TRUE;
    g_object_ref (element);
  }
}

static void
flow_element_finalize (FlowElement *element)
{
  guint i;

  for (i = 0; i < element->input_pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (element->input_pads, i);
    g_object_unref (pad);
  }

  for (i = 0; i < element->output_pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (element->output_pads, i);
    g_object_unref (pad);
  }

  g_ptr_array_free (element->input_pads, TRUE);
  g_ptr_array_free (element->output_pads, TRUE);
}

/* --- FlowElement public API --- */

GPtrArray *
flow_element_get_input_pads (FlowElement *element)
{
  g_return_val_if_fail (FLOW_IS_ELEMENT (element), NULL);

  return element->input_pads;
}

GPtrArray *
flow_element_get_output_pads (FlowElement *element)
{
  g_return_val_if_fail (FLOW_IS_ELEMENT (element), NULL);

  return element->output_pads;
}
