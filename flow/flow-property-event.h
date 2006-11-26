/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-property-event.h - An event that sets properties on elements.
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

#ifndef _FLOW_PROPERTY_EVENT_H
#define _FLOW_PROPERTY_EVENT_H

#include <flow/flow-event.h>

G_BEGIN_DECLS

#define FLOW_TYPE_PROPERTY_EVENT            (flow_property_event_get_type ())
#define FLOW_PROPERTY_EVENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_PROPERTY_EVENT, FlowPropertyEvent))
#define FLOW_PROPERTY_EVENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_PROPERTY_EVENT, FlowPropertyEventClass))
#define FLOW_IS_PROPERTY_EVENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_PROPERTY_EVENT))
#define FLOW_IS_PROPERTY_EVENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_PROPERTY_EVENT))
#define FLOW_PROPERTY_EVENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_PROPERTY_EVENT, FlowPropertyEventClass))
GType   flow_property_event_get_type        (void) G_GNUC_CONST;

typedef struct _FlowPropertyEvent      FlowPropertyEvent;
typedef struct _FlowPropertyEventClass FlowPropertyEventClass;

struct _FlowPropertyEvent
{
  FlowEvent  parent;

  /* --- Private --- */

  gpointer   priv;
};

struct _FlowPropertyEventClass
{
  FlowEventClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowPropertyEvent *flow_property_event_new_for_class_valist    (GType klass_type,
                                                                const gchar *first_property_name, va_list var_args);
FlowPropertyEvent *flow_property_event_new_for_instance_valist (gpointer instance,
                                                                const gchar *first_property_name, va_list var_args);

FlowPropertyEvent *flow_property_event_new_for_class           (GType klass_type, const gchar *first_property_name, ...);
FlowPropertyEvent *flow_property_event_new_for_instance        (gpointer instance, const gchar *first_property_name, ...);

gboolean           flow_property_event_try_apply               (FlowPropertyEvent *property_event, gpointer instance);

G_END_DECLS

#endif  /* _FLOW_PROPERTY_EVENT_H */
