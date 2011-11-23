/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-mux-event.h - Events for the multiplexer (flow-{de}mux).
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

#ifndef _FLOW_MUX_EVENT_H
#define _FLOW_MUX_EVENT_H

#include <glib-object.h>
#include <flow/flow-event.h>

G_BEGIN_DECLS

#define FLOW_TYPE_MUX_EVENT            (flow_mux_event_get_type ())
#define FLOW_MUX_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_MUX_EVENT, FlowMuxEvent))
#define FLOW_MUX_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_MUX_EVENT, FlowMuxEventClass))
#define FLOW_IS_MUX_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_MUX_EVENT))
#define FLOW_IS_MUX_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_MUX_EVENT))
#define FLOW_MUX_EVENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_MUX_EVENT, FlowMuxEventClass))

typedef struct _FlowMuxEvent        FlowMuxEvent;
typedef struct _FlowMuxEventPrivate FlowMuxEventPrivate;
typedef struct _FlowMuxEventClass   FlowMuxEventClass;

struct _FlowMuxEvent
{
  FlowEvent parent;

  /*< private >*/

  FlowMuxEventPrivate *priv;
};

struct _FlowMuxEventClass
{
  FlowEventClass parent_class;

  /*< private >*/
};

GType flow_mux_event_get_type (void) G_GNUC_CONST;

FlowMuxEvent *flow_mux_event_new (guint channel_id);
guint         flow_mux_event_get_channel_id (FlowMuxEvent *event);

G_END_DECLS

#endif  /* _FLOW_MUX_EVENT_H */
