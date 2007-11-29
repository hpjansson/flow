/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-mux-event.c - Events for the multiplexer (flow-{de}mux).
 *
 * Copyright (C) 2007 Andreas Rottmann
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
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors: Andreas Rottmann <a.rottmann@gmx.at>
 */

#include "config.h"
#include "flow-gobject-util.h"
#include "flow-mux-event.h"

typedef struct
{
  guint channel_id;
} FlowMuxEventPrivate;

static guint flow_mux_event_set_channel_id_internal (FlowMuxEvent *event, guint channel_id);

  
FLOW_GOBJECT_PROPERTIES_BEGIN (flow_mux_event)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT, "channel-id", "Channel ID", "Channel ID",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_mux_event_get_channel_id,
                               flow_mux_event_set_channel_id_internal,
                               0, G_MAXUINT, 0)
FLOW_GOBJECT_PROPERTIES_END   ()

FLOW_GOBJECT_MAKE_IMPL        (flow_mux_event, FlowMuxEvent, FLOW_TYPE_EVENT, 0)

static void
flow_mux_event_type_init (GType type)
{
}

static void
flow_mux_event_class_init (FlowMuxEventClass *klass)
{
}

static void
flow_mux_event_init (FlowMuxEvent *mux_event)
{
}

static void
flow_mux_event_construct (FlowMuxEvent *mux_event)
{
}

static void
flow_mux_event_dispose (FlowMuxEvent *mux_event)
{
}

static void
flow_mux_event_finalize (FlowMuxEvent *mux_event)
{
}

FlowMuxEvent *
flow_mux_event_new (guint channel_id)
{
  return g_object_new (FLOW_TYPE_MUX_EVENT, "channel-id", channel_id, NULL);
}

guint
flow_mux_event_get_channel_id (FlowMuxEvent *event)
{
  return ((FlowMuxEventPrivate *) event->priv)->channel_id;
}

static guint
flow_mux_event_set_channel_id_internal (FlowMuxEvent *event, guint channel_id)
{
  return ((FlowMuxEventPrivate *) event->priv)->channel_id = channel_id;
}
