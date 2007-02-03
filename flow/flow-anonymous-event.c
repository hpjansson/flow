/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-anonymous-event.c - An event containing just a pointer.
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
#include "flow-anonymous-event.h"

/* --- FlowAnonymousEvent properties --- */

static gpointer
flow_anonymous_event_get_data_internal (FlowAnonymousEvent *anonymous_event)
{
  return anonymous_event->data;
}

static void
flow_anonymous_event_set_data_internal (FlowAnonymousEvent *anonymous_event, gpointer data)
{
  FlowEvent *event = (FlowEvent *) anonymous_event;

  anonymous_event->data = data;

  g_free (event->description);
  event->description = NULL;
}

static gpointer
flow_anonymous_event_get_destroy_notify_internal (FlowAnonymousEvent *anonymous_event)
{
  return anonymous_event->notify;
}

static void
flow_anonymous_event_set_destroy_notify_internal (FlowAnonymousEvent *anonymous_event, GDestroyNotify notify)
{
  anonymous_event->notify = notify;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_anonymous_event)
FLOW_GOBJECT_PROPERTY_POINTER ("data", "Data", "Data",
                               G_PARAM_READWRITE,
                               flow_anonymous_event_get_data_internal,
                               flow_anonymous_event_set_data_internal)
FLOW_GOBJECT_PROPERTY_POINTER ("destroy-notify", "Destroy notify", "Destroy notification callback",
                               G_PARAM_READWRITE,
                               flow_anonymous_event_get_destroy_notify_internal,
                               flow_anonymous_event_set_destroy_notify_internal)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowAnonymousEvent definition --- */

FLOW_GOBJECT_MAKE_IMPL_NO_PRIVATE (flow_anonymous_event, FlowAnonymousEvent, FLOW_TYPE_EVENT, 0)

/* --- FlowAnonymousEvent implementation --- */

static void
update_description (FlowEvent *event)
{
  FlowAnonymousEvent *anonymous_event = (FlowAnonymousEvent *) anonymous_event;

  if (event->description)
    return;

  event->description = g_strdup_printf ("Pointer to %p", anonymous_event->data);
}

static void
flow_anonymous_event_type_init (GType type)
{
}

static void
flow_anonymous_event_class_init (FlowAnonymousEventClass *klass)
{
  FlowEventClass *event_klass = (FlowEventClass *) klass;

  event_klass->update_description = update_description;
}

static void
flow_anonymous_event_init (FlowAnonymousEvent *anonymous_event)
{
}

static void
flow_anonymous_event_construct (FlowAnonymousEvent *anonymous_event)
{
}

static void
flow_anonymous_event_dispose (FlowAnonymousEvent *anonymous_event)
{
}

static void
flow_anonymous_event_finalize (FlowAnonymousEvent *anonymous_event)
{
  if (anonymous_event->notify)
    anonymous_event->notify (anonymous_event->data);
}

/* --- FlowAnonymousEvent public API --- */

/**
 * flow_anonymous_event_new:
 * 
 * Creates a new #FlowAnonymousEvent.
 * 
 * Return value: A new #FlowAnonymousEvent
 **/
FlowAnonymousEvent *
flow_anonymous_event_new (void)
{
  FlowAnonymousEvent *anonymous_event;

  anonymous_event = g_object_new (FLOW_TYPE_ANONYMOUS_EVENT, NULL);

  return anonymous_event;
}

/**
 * flow_anonymous_event_get_data:
 * @anonymous_event: A #FlowAnonymousEvent
 * 
 * Gets the generic pointer delivered by this event.
 * 
 * Return value: The generic pointer stored in this event.
 **/
gpointer
flow_anonymous_event_get_data (FlowAnonymousEvent *anonymous_event)
{
  g_return_val_if_fail (FLOW_IS_ANONYMOUS_EVENT (anonymous_event), NULL);

  return anonymous_event->data;
}

/**
 * flow_anonymous_event_set_data:
 * @anonymous_event: A #FlowAnonymousEvent
 * @data: A generic pointer
 * 
 * Stores the @data pointer in @anonymous_event.
 **/
void
flow_anonymous_event_set_data (FlowAnonymousEvent *anonymous_event, gpointer data)
{
  g_return_if_fail (FLOW_IS_ANONYMOUS_EVENT (anonymous_event));

  g_object_set (anonymous_event, "data", data, NULL);
}

/**
 * flow_anonymous_event_set_destroy_notify:
 * @anonymous_event: A #FlowAnonymousEvent
 * @notify: A user callback
 * 
 * Sets up a callback that will run when @anonymous_event is
 * finalized. This can be used to free any data associated with
 * the pointer set by flow_anonymous_event_set_data().
 **/
void
flow_anonymous_event_set_destroy_notify (FlowAnonymousEvent *anonymous_event, GDestroyNotify notify)
{
  g_return_if_fail (FLOW_IS_ANONYMOUS_EVENT (anonymous_event));

  g_object_set (anonymous_event, "destroy-notify", notify, NULL);
}
