/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-segment-request.c - A request to read a segment of data from a stream.
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
#include "flow-enum-types.h"
#include "flow-segment-request.h"

/* --- FlowSegmentRequest private data --- */

struct _FlowSegmentRequestPrivate
{
  gint64 length;
};

/* --- FlowSegmentRequest properties --- */

static gint64
flow_segment_request_get_length_internal (FlowSegmentRequest *segment_request)
{
  FlowSegmentRequestPrivate *priv = segment_request->priv;

  return priv->length;
}

static void
flow_segment_request_set_length_internal (FlowSegmentRequest *segment_request, gint64 length)
{
  FlowSegmentRequestPrivate *priv = segment_request->priv;

  priv->length = length;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_segment_request)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_INT64, "length", "Length", "Number of bytes to read",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_segment_request_get_length_internal, flow_segment_request_set_length_internal,
                               G_MININT64, G_MAXINT64, -1)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowSegmentRequest definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_segment_request, FlowSegmentRequest, FLOW_TYPE_EVENT, 0)

/* --- FlowSegmentRequest implementation --- */

static void
flow_segment_request_type_init (GType type)
{
}

static void
flow_segment_request_class_init (FlowSegmentRequestClass *klass)
{
}

static void
flow_segment_request_init (FlowSegmentRequest *segment_request)
{
}

static void
flow_segment_request_construct (FlowSegmentRequest *segment_request)
{
}

static void
flow_segment_request_dispose (FlowSegmentRequest *segment_request)
{
}

static void
flow_segment_request_finalize (FlowSegmentRequest *segment_request)
{
}

/* --- FlowSegmentRequest public API --- */

FlowSegmentRequest *
flow_segment_request_new (gint64 length)
{
  return g_object_new (FLOW_TYPE_SEGMENT_REQUEST, "length", length, NULL);
}

gint64
flow_segment_request_get_length (FlowSegmentRequest *segment_request)
{
  gint64 length;

  g_return_val_if_fail (FLOW_IS_SEGMENT_REQUEST (segment_request), 0);

  g_object_get (segment_request, "length", &length, NULL);
  return length;
}
