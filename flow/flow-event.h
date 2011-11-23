/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-event.h - An event that can be propagated along a pipeline.
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

#ifndef _FLOW_EVENT_H
#define _FLOW_EVENT_H

#include <flow/flow-element.h>

G_BEGIN_DECLS

#define FLOW_TYPE_EVENT            (flow_event_get_type ())
#define FLOW_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_EVENT, FlowEvent))
#define FLOW_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_EVENT, FlowEventClass))
#define FLOW_IS_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_EVENT))
#define FLOW_IS_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_EVENT))
#define FLOW_EVENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_EVENT, FlowEventClass))
GType   flow_event_get_type        (void) G_GNUC_CONST;

typedef struct _FlowEvent        FlowEvent;
typedef struct _FlowEventPrivate FlowEventPrivate;
typedef struct _FlowEventClass   FlowEventClass;

struct _FlowEvent
{
  GObject      parent;

  /*< private >*/

  /* --- Protected --- */

  FlowElement *source_element;
  gchar       *description;

  /* --- Private --- */

  FlowEventPrivate *priv;
};

struct _FlowEventClass
{
  GObjectClass parent_class;

  void (*update_description) (FlowEvent *event);

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

G_END_DECLS

FlowElement *flow_event_get_source_element (FlowEvent *event);
const gchar *flow_event_get_description    (FlowEvent *event);

#endif  /* _FLOW_EVENT_H */
