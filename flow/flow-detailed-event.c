/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-detailed-event.c - An event with a public code and description.
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
#include "flow-detailed-event.h"

typedef struct
{
  GSList      *codes;
}
FlowDetailedEventPrivate;

typedef struct
{
  gint         code;
  const gchar *domain;
}
DomainCode;

/* --- FlowDetailedEvent properties --- */

static gchar *
flow_detailed_event_get_description_internal (FlowDetailedEvent *detailed_event)
{
  return g_strdup (detailed_event->parent.description);
}

static void
flow_detailed_event_set_description_internal (FlowDetailedEvent *detailed_event, const gchar *description)
{
  g_free (detailed_event->parent.description);
  detailed_event->parent.description = g_strdup (description);
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_detailed_event)
FLOW_GOBJECT_PROPERTY_STRING  ("description", "Description", "Description",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_detailed_event_get_description_internal, flow_detailed_event_set_description_internal,
                               NULL)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowDetailedEvent definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_detailed_event, FlowDetailedEvent, FLOW_TYPE_EVENT, 0)

/* --- FlowDetailedEvent implementation --- */

static void
flow_detailed_event_type_init (GType type)
{
}

static void
flow_detailed_event_class_init (FlowDetailedEventClass *klass)
{
}

static void
flow_detailed_event_init (FlowDetailedEvent *detailed_event)
{
}

static void
flow_detailed_event_construct (FlowDetailedEvent *detailed_event)
{
}

static void
flow_detailed_event_dispose (FlowDetailedEvent *detailed_event)
{
}

static void
flow_detailed_event_finalize (FlowDetailedEvent *detailed_event)
{
  FlowDetailedEventPrivate *priv = detailed_event->priv;
  GSList                   *l;

  for (l = priv->codes; l; l = g_slist_next (l))
    g_slice_free (DomainCode, l->data);

  g_slist_free (priv->codes);
}

static FlowDetailedEvent *
flow_detailed_event_new_valist (const gchar   *format,
                                va_list        args)
{
  FlowDetailedEvent *detailed_event;
  gchar             *description;

  description = format ? g_strdup_vprintf (format, args) : NULL;

  detailed_event = g_object_new (FLOW_TYPE_DETAILED_EVENT,
                                 "description", description,
                                 NULL);

  g_free (description);

  return detailed_event;
}

/* --- FlowDetailedEvent public API --- */

/**
 * flow_detailed_event_new:
 * @domain: detailed_event domain 
 * @code: detailed_event code
 * @format: printf()-style format for detailed_event description
 * @Varargs: parameters for description format
 * 
 * Creates a new #FlowDetailedEvent with the given @domain and @code,
 * and a description formatted with @format.
 * 
 * Return value: a new #FlowDetailedEvent
 **/
FlowDetailedEvent *
flow_detailed_event_new (const gchar *format,
                         ...)
{
  FlowDetailedEvent *detailed_event;
  va_list            args;

  va_start (args, format);
  detailed_event = flow_detailed_event_new_valist (format, args);
  va_end (args);

  return detailed_event;
}

/**
 * flow_detailed_event_new_literal:
 * @domain: detailed_event domain
 * @code: detailed_event code
 * @description: detailed_event description
 * 
 * Creates a new #FlowDetailedEvent; unlike flow_detailed_event_new(),
 * @description is not a printf()-style format string. Use this 
 * function if @description contains text you don't have control over, 
 * that could include printf() escape sequences.
 * 
 * Return value: a new #FlowDetailedEvent
 **/
FlowDetailedEvent *
flow_detailed_event_new_literal (const gchar *description)
{
  FlowDetailedEvent *detailed_event;

  detailed_event = g_object_new (FLOW_TYPE_DETAILED_EVENT,
                                 "description", description,
                                 NULL);

  return detailed_event;
}

/**
 * flow_detailed_add_code:
 * @detailed_event: a #FlowDetailedEvent
 * @domain: a detailed_event domain
 * @code: a detailed_event code
 * 
 * Adds @code in @domain to the list of codes that this event
 * should match.
 **/
void
flow_detailed_event_add_code (FlowDetailedEvent *detailed_event,
                              const gchar       *domain,
                              gint               code)
{
  FlowDetailedEventPrivate *priv;
  DomainCode               *domain_code;

  g_return_if_fail (FLOW_IS_DETAILED_EVENT (detailed_event));
  g_return_if_fail (domain != NULL);

  priv = detailed_event->priv;

  domain_code = g_slice_new (DomainCode);
  domain_code->domain = g_intern_string (domain);
  domain_code->code   = code;

  priv->codes = g_slist_prepend (priv->codes, domain_code);
}

/**
 * flow_detailed_event_matches:
 * @detailed_event: a #FlowDetailedEvent
 * @domain: a detailed_event domain or NULL
 * @code: a detailed_event code or -1
 * 
 * Returns %TRUE if @detailed_event matches @domain and @code, %FALSE
 * otherwise.
 * 
 * Return value: whether @detailed_event has @domain and @code
 **/
gboolean
flow_detailed_event_matches (FlowDetailedEvent *detailed_event,
                             const gchar       *domain,
                             gint               code)
{
  FlowDetailedEventPrivate *priv;
  GSList                   *l;

  g_return_val_if_fail (FLOW_IS_DETAILED_EVENT (detailed_event), FALSE);

  priv = detailed_event->priv;

  for (l = priv->codes; l; l = g_slist_next (l))
  {
    DomainCode *domain_code = l->data;

    if ((domain_code->code == code || code == -1) &&
        (domain_code->domain == domain || domain_code->domain == g_intern_string (domain)))
    {
      return TRUE;
    }
  }

  return FALSE;
}
