/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-property-event.c - An event that sets properties on elements.
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
#include "flow-property-event.h"
#include <gobject/gvaluecollector.h>
#include <string.h>

typedef struct
{
  gchar  *name;
  GValue  value;
}
SetProperty;

/* --- FlowPropertyEvent private data --- */

typedef struct
{
  guint      is_instance : 1;
  gpointer   instance_or_class;
  GArray    *props;
}
FlowPropertyEventPrivate;

/* --- FlowPropertyEvent properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_property_event)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowPropertyEvent definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_property_event, FlowPropertyEvent, FLOW_TYPE_EVENT, 0)

/* --- FlowPropertyEvent implementation --- */

static gchar *
strconcat_replace (gchar *first_toreplace, const gchar *last)
{
  gchar *joined;

  joined = g_strconcat (first_toreplace, last, NULL);

  g_free (first_toreplace);
  return joined;
}

static void
update_description (FlowEvent *event)
{
  FlowPropertyEvent        *property_event = (FlowPropertyEvent *) event;
  FlowPropertyEventPrivate *priv           = property_event->priv;
  gchar                    *all_values_str;
  const gchar              *type_name;
  gchar                    *instance_name;
  guint                     i;

  if (event->description)
    return;

  all_values_str = NULL;

  for (i = 0; i < priv->props->len; i++)
  {
    SetProperty *sprop = &g_array_index (priv->props, SetProperty, i);
    gchar       *value_str;
    gchar       *temp_str;

    if (i > 0)
      all_values_str = strconcat_replace (all_values_str, ", ");

    value_str = g_strdup_value_contents (&sprop->value);
    temp_str = g_strdup_printf ("[%s = %s]", sprop->name, value_str);
    g_free (value_str);

    all_values_str = strconcat_replace (all_values_str, temp_str);
    g_free (temp_str);
  }

  if (priv->is_instance)
  {
    type_name = G_OBJECT_TYPE_NAME (priv->instance_or_class);
    instance_name = g_strdup_printf ("%p", priv->instance_or_class);
  }
  else
  {
    type_name = G_OBJECT_CLASS_NAME (priv->instance_or_class);
    instance_name = g_strdup ("*");
  }

  event->description =
    g_strdup_printf ("For %s(%s): %s", type_name, instance_name, all_values_str);

  g_free (instance_name);
  g_free (all_values_str);
}

static void
flow_property_event_type_init (GType type)
{
}

static void
flow_property_event_class_init (FlowPropertyEventClass *klass)
{
  FlowEventClass *event_klass = (FlowEventClass *) klass;

  event_klass->update_description = update_description;
}

static void
flow_property_event_init (FlowPropertyEvent *property_event)
{
  FlowPropertyEventPrivate *priv = property_event->priv;

  priv->props = g_array_new (FALSE, FALSE, sizeof (SetProperty));
}

static void
flow_property_event_construct (FlowPropertyEvent *property_event)
{
}

static void
flow_property_event_dispose (FlowPropertyEvent *property_event)
{
}

static void
flow_property_event_finalize (FlowPropertyEvent *property_event)
{
  FlowPropertyEventPrivate *priv = property_event->priv;
  guint                     i;

  for (i = 0; i < priv->props->len; i++)
  {
    SetProperty *sprop = &g_array_index (priv->props, SetProperty, i);

    g_free (sprop->name);
    g_value_unset (&sprop->value);
  }

  g_array_free (priv->props, TRUE);
  priv->props = NULL;
}

static FlowPropertyEvent *
flow_property_event_new_valist (gpointer instance_or_class, gboolean is_instance,
                                const gchar *first_property_name, va_list var_args)
{
  FlowPropertyEvent        *property_event;
  FlowPropertyEventPrivate *priv;
  GObjectClass             *klass;
  const gchar              *name;

  property_event = g_object_new (FLOW_TYPE_PROPERTY_EVENT, NULL);
  priv = property_event->priv;

  priv->is_instance       = is_instance;
  priv->instance_or_class = instance_or_class;

  if (is_instance)
    klass = instance_or_class;
  else
    klass = G_OBJECT_GET_CLASS (instance_or_class);

  for (name = first_property_name; name; name = va_arg (var_args, char *))
  {
    SetProperty  sprop;
    GParamSpec  *pspec;
    gchar       *error = NULL;

    pspec = g_object_class_find_property (klass, name);

    if G_UNLIKELY (!pspec)
    {
      g_warning ("%s: object class `%s' has no property named `%s'",
                 G_STRFUNC,
                 G_OBJECT_CLASS_NAME (klass),
                 name);
      break;
    }

    if G_UNLIKELY (!(pspec->flags & G_PARAM_WRITABLE))
    {
      g_warning ("%s: property `%s' of object class `%s' is not writable",
                 G_STRFUNC, pspec->name, G_OBJECT_CLASS_NAME (klass));
      break;
    }

    if G_UNLIKELY (pspec->flags & G_PARAM_CONSTRUCT_ONLY)
    {
      g_warning ("%s: construct property \"%s\" for object `%s' can't be set after construction",
                 G_STRFUNC, pspec->name, G_OBJECT_CLASS_NAME (klass));
      break;
    }

    memset (&sprop, 0, sizeof (SetProperty));
    g_value_init (&sprop.value, G_PARAM_SPEC_VALUE_TYPE (pspec));

    G_VALUE_COLLECT (&sprop.value, var_args, 0, &error);
    if (error)
    {
      g_warning ("%s: %s", G_STRFUNC, error);
      g_free (error);
      g_value_unset (&sprop.value);
      break;
    }

    sprop.name = g_strdup (name);
    g_array_append_val (priv->props, sprop);
  }

  return property_event;
}

static void
apply_properties (FlowPropertyEvent *property_event, GObject *instance)
{
  FlowPropertyEventPrivate *priv = property_event->priv;
  guint                     i;

  for (i = 0; i < priv->props->len; i++)
  {
    SetProperty *sprop = &g_array_index (priv->props, SetProperty, i);

    g_object_set_property (instance, sprop->name, &sprop->value);
  }
}

/* --- FlowPropertyEvent public API --- */

FlowPropertyEvent *
flow_property_event_new_for_class_valist (GType gtype, const gchar *first_property_name, va_list var_args)
{
  FlowPropertyEvent *property_event;
  GObjectClass      *klass;

  klass = g_type_class_ref (gtype);
  g_return_val_if_fail (klass != NULL, NULL);

  property_event = flow_property_event_new_valist (klass, FALSE, first_property_name, var_args);

  g_type_class_unref (klass);
  return property_event;
}

/**
 * flow_property_event_new_for_class:
 * 
 * Creates a new #FlowPropertyEvent.
 * 
 * Return value: a new #FlowPropertyEvent
 **/
FlowPropertyEvent *
flow_property_event_new_for_class (GType gtype, const gchar *first_property_name, ...)
{
  FlowPropertyEvent *property_event;
  GObjectClass      *klass;
  va_list            var_args;

  klass = g_type_class_ref (gtype);
  g_return_val_if_fail (klass != NULL, NULL);

  va_start (var_args, first_property_name);
  property_event = flow_property_event_new_valist (klass, FALSE, first_property_name, var_args);
  va_end (var_args);

  g_type_class_unref (klass);
  return property_event;
}

FlowPropertyEvent *
flow_property_event_new_for_instance_valist (gpointer instance, const gchar *first_property_name, va_list var_args)
{
  g_return_val_if_fail (G_IS_OBJECT (instance), NULL);

  return flow_property_event_new_valist (instance, TRUE, first_property_name, var_args);
}

/**
 * flow_property_event_new_for_instance:
 * 
 * Creates a new #FlowPropertyEvent.
 * 
 * Return value: a new #FlowPropertyEvent
 **/
FlowPropertyEvent *
flow_property_event_new_for_instance (gpointer instance, const gchar *first_property_name, ...)
{
  FlowPropertyEvent *property_event;
  va_list            var_args;

  g_return_val_if_fail (G_IS_OBJECT (instance), NULL);

  va_start (var_args, first_property_name);
  property_event = flow_property_event_new_valist (instance, TRUE, first_property_name, var_args);
  va_end (var_args);

  return property_event;
}

gboolean
flow_property_event_try_apply (FlowPropertyEvent *property_event, gpointer instance)
{
  FlowPropertyEventPrivate *priv;
  gboolean                  result = FALSE;

  g_return_val_if_fail (FLOW_IS_PROPERTY_EVENT (property_event), FALSE);
  g_return_val_if_fail (G_IS_OBJECT (instance), FALSE);

  priv = property_event->priv;

  if (priv->is_instance)
  {
    if (instance == priv->instance_or_class)
    {
      apply_properties (property_event, instance);
      result = TRUE;
    }
  }
  else if (G_OBJECT_GET_CLASS (instance) == priv->instance_or_class)
  {
    apply_properties (property_event, instance);
    result = TRUE;
  }

  return result;
}
