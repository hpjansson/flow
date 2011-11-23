/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-event.c - An event that can be propagated along a pipeline.
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
#include "flow-event.h"

/* --- FlowEvent private data --- */

struct _FlowEventPrivate
{
};

/* --- FlowEvent properties --- */

static FlowElement *
flow_event_get_source_element_internal (FlowEvent *event)
{
  return event->source_element;
}

static void
flow_event_set_source_element_internal (FlowEvent *event, FlowElement *element)
{
  if (event->source_element == element)
    return;

  if (event->source_element)
    g_object_remove_weak_pointer ((GObject *) event->source_element, (gpointer) &event->source_element);

  event->source_element = element;

  if (element)
    g_object_add_weak_pointer ((GObject *) element, (gpointer) &event->source_element);
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_event)
FLOW_GOBJECT_PROPERTY         (G_TYPE_OBJECT, "source-element", "Source element",
                               "Element that generated this event", G_PARAM_READWRITE,
                               flow_event_get_source_element_internal,
                               flow_event_set_source_element_internal,
                               flow_element_get_type)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowEvent definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_event, FlowEvent, G_TYPE_OBJECT, G_TYPE_FLAG_ABSTRACT)

/* --- FlowEvent implementation --- */

static void
flow_event_type_init (GType type)
{
}

static void
flow_event_class_init (FlowEventClass *klass)
{
}

static void
flow_event_init (FlowEvent *event)
{
}

static void
flow_event_construct (FlowEvent *event)
{
}

static void
flow_event_dispose (FlowEvent *event)
{
  if (event->source_element)
  {
    g_object_remove_weak_pointer ((GObject *) event->source_element, (gpointer) &event->source_element);
    event->source_element = NULL;
  }
}

static void
flow_event_finalize (FlowEvent *event)
{
  g_free (event->description);
  event->description = NULL;
}

/* --- FlowEvent public API --- */

/**
 * flow_event_get_source_element:
 * @event: A #FlowEvent.
 *
 * Returns a pointer to the #FlowElement that generated this event. The source
 * element may be unset, in which case %NULL will be returned. The source
 * element may be set multiple times.
 *
 * Return value: The #FlowElement that generated the event, or %NULL if unset.
 **/
FlowElement *
flow_event_get_source_element (FlowEvent *event)
{
  g_return_val_if_fail (FLOW_IS_EVENT (event), NULL);

  return event->source_element;
}

/**
 * flow_event_get_description:
 * @event: A #FlowEvent.
 *
 * Causes @event to generate a human-readable description of itself and return
 * a pointer to it. The string belongs to @event and must not be freed.
 *
 * Return value: A pointer to a human-readable description of the event.
 **/
const gchar *
flow_event_get_description (FlowEvent *event)
{
  FlowEventClass *klass;

  g_return_val_if_fail (FLOW_IS_EVENT (event), NULL);

  klass = FLOW_EVENT_GET_CLASS (event);
  if (klass->update_description)
    klass->update_description (event);

  if (!event->description)
    return G_OBJECT_CLASS_NAME (event);

  return event->description;
}
