/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-detailed-event.h - An event with a public code and description.
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

#ifndef _FLOW_DETAILED_EVENT_H
#define _FLOW_DETAILED_EVENT_H

#include <flow/flow-event.h>

G_BEGIN_DECLS

#define FLOW_TYPE_DETAILED_EVENT            (flow_detailed_event_get_type ())
#define FLOW_DETAILED_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_DETAILED_EVENT, FlowDetailedEvent))
#define FLOW_DETAILED_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_DETAILED_EVENT, FlowDetailedEventClass))
#define FLOW_IS_DETAILED_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_DETAILED_EVENT))
#define FLOW_IS_DETAILED_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_DETAILED_EVENT))
#define FLOW_DETAILED_EVENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_DETAILED_EVENT, FlowDetailedEventClass))
GType   flow_detailed_event_get_type        (void) G_GNUC_CONST;

typedef struct _FlowDetailedEvent      FlowDetailedEvent;
typedef struct _FlowDetailedEventClass FlowDetailedEventClass;

struct _FlowDetailedEvent
{
  FlowEvent    parent;

  /* --- Private --- */

  gpointer     priv;
};

struct _FlowDetailedEventClass
{
  FlowEventClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowDetailedEvent *flow_detailed_event_new             (const gchar *format, ...);
FlowDetailedEvent *flow_detailed_event_new_literal     (const gchar *description);

void               flow_detailed_event_add_code        (FlowDetailedEvent *detailed_event, const gchar *domain, gint code);

gboolean           flow_detailed_event_matches         (FlowDetailedEvent *detailed_event, const gchar *domain, gint code);

G_END_DECLS

#endif  /* _FLOW_DETAILED_EVENT_H */
