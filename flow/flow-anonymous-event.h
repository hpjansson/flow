/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-anonymous-event.h - An event containing just a pointer.
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

#ifndef _FLOW_ANONYMOUS_EVENT_H
#define _FLOW_ANONYMOUS_EVENT_H

#include <flow/flow-event.h>

G_BEGIN_DECLS

#define FLOW_TYPE_ANONYMOUS_EVENT            (flow_anonymous_event_get_type ())
#define FLOW_ANONYMOUS_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_ANONYMOUS_EVENT, FlowAnonymousEvent))
#define FLOW_ANONYMOUS_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_ANONYMOUS_EVENT, FlowAnonymousEventClass))
#define FLOW_IS_ANONYMOUS_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_ANONYMOUS_EVENT))
#define FLOW_IS_ANONYMOUS_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_ANONYMOUS_EVENT))
#define FLOW_ANONYMOUS_EVENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_ANONYMOUS_EVENT, FlowAnonymousEventClass))
GType   flow_anonymous_event_get_type        (void) G_GNUC_CONST;

typedef struct _FlowAnonymousEvent        FlowAnonymousEvent;
typedef struct _FlowAnonymousEventClass   FlowAnonymousEventClass;

struct _FlowAnonymousEvent
{
  FlowEvent      parent;

  /*< private >*/

  gpointer       data;
  GDestroyNotify notify;
};

struct _FlowAnonymousEventClass
{
  FlowEventClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowAnonymousEvent *flow_anonymous_event_new                (void);

gpointer            flow_anonymous_event_get_data           (FlowAnonymousEvent *anonymous_event);
void                flow_anonymous_event_set_data           (FlowAnonymousEvent *anonymous_event, gpointer data);

void                flow_anonymous_event_set_destroy_notify (FlowAnonymousEvent *anonymous_event, GDestroyNotify notify);

G_END_DECLS

#endif  /* _FLOW_ANONYMOUS_EVENT_H */
