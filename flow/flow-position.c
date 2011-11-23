/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-position.c - A stream position that can be propagated along a pipeline.
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
#include "flow-position.h"

/* --- FlowPosition private data --- */

struct _FlowPositionPrivate
{
  guint     anchor    : 4;
  gint64    offset;
};

/* --- FlowPosition properties --- */

static gint64
flow_position_get_offset_internal (FlowPosition *position)
{
  FlowPositionPrivate *priv = position->priv;

  return priv->offset;
}

static void
flow_position_set_offset_internal (FlowPosition *position, gint64 offset)
{
  FlowPositionPrivate *priv = position->priv;

  priv->offset = offset;
}

static FlowOffsetAnchor
flow_position_get_anchor_internal (FlowPosition *position)
{
  FlowPositionPrivate *priv = position->priv;

  return priv->anchor;
}

static void
flow_position_set_anchor_internal (FlowPosition *position, FlowOffsetAnchor anchor)
{
  FlowPositionPrivate *priv = position->priv;

  priv->anchor = anchor;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_position)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_INT64, "offset", "Offset", "Stream offset relative to anchor",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_position_get_offset_internal, flow_position_set_offset_internal,
                               G_MININT64, G_MAXINT64, 0)
FLOW_GOBJECT_PROPERTY_ENUM    ("anchor", "Anchor", "Stream anchor",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_position_get_anchor_internal, flow_position_set_anchor_internal,
                               FLOW_OFFSET_ANCHOR_CURRENT,
                               flow_offset_anchor_get_type)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowPosition definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_position, FlowPosition, FLOW_TYPE_EVENT, 0)

/* --- FlowPosition implementation --- */

static void
flow_position_type_init (GType type)
{
}

static void
flow_position_class_init (FlowPositionClass *klass)
{
}

static void
flow_position_init (FlowPosition *position)
{
  FlowPositionPrivate *priv = position->priv;

  priv->anchor = FLOW_OFFSET_ANCHOR_CURRENT;
  priv->offset = 0;
}

static void
flow_position_construct (FlowPosition *position)
{
}

static void
flow_position_dispose (FlowPosition *position)
{
}

static void
flow_position_finalize (FlowPosition *position)
{
}

/* --- FlowPosition public API --- */

FlowPosition *
flow_position_new (FlowOffsetAnchor anchor, gint64 offset)
{
  return g_object_new (FLOW_TYPE_POSITION, "anchor", anchor, "offset", offset, NULL);
}

gint64
flow_position_get_offset (FlowPosition *position)
{
  gint64 offset;

  g_return_val_if_fail (FLOW_IS_POSITION (position), 0);

  g_object_get (position, "offset", &offset, NULL);
  return offset;
}

FlowOffsetAnchor
flow_position_get_anchor (FlowPosition *position)
{
  FlowOffsetAnchor anchor;

  g_return_val_if_fail (FLOW_IS_POSITION (position), FLOW_OFFSET_ANCHOR_CURRENT);

  g_object_get (position, "anchor", &anchor, NULL);
  return anchor;
}
