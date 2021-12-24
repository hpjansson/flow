/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-gobject-util.c - GObject utilities.
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

void
flow_gobject_class_install_properties (GObjectClass *object_class, const FlowGObjectPropElem *props,
                                       gint n_props)
{
  GObjectClass *parent_class;
  gint          i;

  parent_class = g_type_class_peek_parent (object_class);

  for (i = 1; i < n_props; i++)
  {
    const FlowGObjectPropElem *elem = &props [i];
    GParamSpec                *param_spec;

    /* FIXME: Do we need the parent for this? Shouldn't we be able to find the properties on ourselves? */
    if (g_object_class_find_property (parent_class, elem->name))
    {
      /* Override parent class' property */
      g_object_class_override_property (object_class, i, elem->name);
      continue;
    }

    switch (elem->fundamental_type)
    {
      case G_TYPE_CHAR:
        param_spec = g_param_spec_char (elem->name, elem->nick, elem->blurb,
                                        elem->int_min, elem->int_max, elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_UCHAR:
        param_spec = g_param_spec_uchar (elem->name, elem->nick, elem->blurb,
                                         elem->int_min, elem->int_max, elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_BOOLEAN:
        param_spec = g_param_spec_boolean (elem->name, elem->nick, elem->blurb,
                                           elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_INT:
        param_spec = g_param_spec_int (elem->name, elem->nick, elem->blurb,
                                       elem->int_min, elem->int_max, elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_UINT:
        param_spec = g_param_spec_uint (elem->name, elem->nick, elem->blurb,
                                        elem->int_min, elem->int_max, elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_LONG:
        param_spec = g_param_spec_long (elem->name, elem->nick, elem->blurb,
                                        elem->int_min, elem->int_max, elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_ULONG:
        param_spec = g_param_spec_ulong (elem->name, elem->nick, elem->blurb,
                                         elem->int_min, elem->int_max, elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_INT64:
        param_spec = g_param_spec_int64 (elem->name, elem->nick, elem->blurb,
                                         elem->int_min, elem->int_max, elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_UINT64:
        param_spec = g_param_spec_uint64 (elem->name, elem->nick, elem->blurb,
                                          elem->int_min, elem->int_max, elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_ENUM:
        g_assert (elem->type_getter != NULL);
        param_spec = g_param_spec_enum (elem->name, elem->nick, elem->blurb,
                                        elem->type_getter (), elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_FLAGS:
        g_assert (elem->type_getter != NULL);
        param_spec = g_param_spec_flags (elem->name, elem->nick, elem->blurb,
                                         elem->type_getter (), elem->int_bool_default, elem->param_flags);
        break;

      case G_TYPE_FLOAT:
        param_spec = g_param_spec_float (elem->name, elem->nick, elem->blurb,
                                         elem->float_min, elem->float_max, elem->float_default, elem->param_flags);
        break;

      case G_TYPE_DOUBLE:
        param_spec = g_param_spec_double (elem->name, elem->nick, elem->blurb,
                                          elem->float_min, elem->float_max, elem->float_default, elem->param_flags);
        break;

      case G_TYPE_STRING:
        param_spec = g_param_spec_string (elem->name, elem->nick, elem->blurb,
                                          elem->string_default, elem->param_flags);
        break;

      case G_TYPE_POINTER:
        param_spec = g_param_spec_pointer (elem->name, elem->nick, elem->blurb,
                                           elem->param_flags);
        break;

      case G_TYPE_BOXED:
        g_assert (elem->type_getter != NULL);
        param_spec = g_param_spec_boxed (elem->name, elem->nick, elem->blurb,
                                         elem->type_getter (), elem->param_flags);
        break;

      case G_TYPE_PARAM:
        g_assert (elem->type_getter != NULL);
        param_spec = g_param_spec_param (elem->name, elem->nick, elem->blurb,
                                         elem->type_getter (), elem->param_flags);
        break;

      case G_TYPE_OBJECT:
        g_assert (elem->type_getter != NULL);
        param_spec = g_param_spec_object (elem->name, elem->nick, elem->blurb,
                                          elem->type_getter (), elem->param_flags);
        break;

      default:
        param_spec = NULL;  /* Whatever */
        g_assert_not_reached ();
        break;
    }

    g_object_class_install_property (object_class, i, param_spec);
  }
}

void
flow_gobject_get_property (GObject *object, gint prop_id, GValue *value, GParamSpec *pspec,
                           const FlowGObjectPropElem *props, gint n_props)
{
  const FlowGObjectPropElem *elem;

  g_return_if_fail (prop_id > 0);
  g_return_if_fail (prop_id < n_props);

  elem = &props [prop_id];

  if (!elem->value_getter)
  {
    g_warning ("No getter for property '%s'!", elem->name);
    return;
  }

  switch (elem->fundamental_type)
  {
    case G_TYPE_CHAR:
      g_value_set_schar (value, ((gchar (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_UCHAR:
      g_value_set_uchar (value, ((guchar (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, ((gboolean (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_INT:
      g_value_set_int (value, ((gint (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_UINT:
      g_value_set_uint (value, ((guint (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_LONG:
      g_value_set_long (value, ((glong (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_ULONG:
      g_value_set_ulong (value, ((gulong (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_INT64:
      g_value_set_int64 (value, ((gint64 (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_UINT64:
      g_value_set_uint64 (value, ((guint64 (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_ENUM:
      g_value_set_enum (value, ((gint (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_FLAGS:
      g_value_set_flags (value, ((guint (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_FLOAT:
      g_value_set_float (value, ((gfloat (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_DOUBLE:
      g_value_set_double (value, ((gdouble (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_STRING:
      g_value_take_string (value, ((gchar *(*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_POINTER:
      g_value_set_pointer (value, ((gpointer (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_BOXED:
      g_value_take_boxed (value, ((gconstpointer (*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_PARAM:
      g_value_take_param (value, ((GParamSpec *(*)(GObject *)) elem->value_getter) (object));
      break;

    case G_TYPE_OBJECT:
      g_value_set_object (value, ((GObject *(*)(GObject *)) elem->value_getter) (object));
      break;

    default:
      g_assert_not_reached ();
      break;
  }
}

void
flow_gobject_set_property (GObject *object, gint prop_id, const GValue *value, GParamSpec *pspec,
                           const FlowGObjectPropElem *props, gint n_props)
{
  const FlowGObjectPropElem *elem;

  g_return_if_fail (prop_id > 0);
  g_return_if_fail (prop_id < n_props);

  elem = &props [prop_id];

  if (!elem->value_setter)
  {
    g_warning ("No setter for property '%s'!", elem->name);
    return;
  }

  switch (elem->fundamental_type)
  {
    case G_TYPE_CHAR:
      ((void (*)(GObject *, gchar)) elem->value_setter) (object, g_value_get_schar (value));
      break;

    case G_TYPE_UCHAR:
      ((void (*)(GObject *, guchar)) elem->value_setter) (object, g_value_get_uchar (value));
      break;

    case G_TYPE_BOOLEAN:
      ((void (*)(GObject *, gboolean)) elem->value_setter) (object, g_value_get_boolean (value));
      break;

    case G_TYPE_INT:
      ((void (*)(GObject *, gint)) elem->value_setter) (object, g_value_get_int (value));
      break;

    case G_TYPE_UINT:
      ((void (*)(GObject *, guint)) elem->value_setter) (object, g_value_get_uint (value));
      break;

    case G_TYPE_LONG:
      ((void (*)(GObject *, glong)) elem->value_setter) (object, g_value_get_long (value));
      break;

    case G_TYPE_ULONG:
      ((void (*)(GObject *, gulong)) elem->value_setter) (object, g_value_get_ulong (value));
      break;

    case G_TYPE_INT64:
      ((void (*)(GObject *, gint64)) elem->value_setter) (object, g_value_get_int64 (value));
      break;

    case G_TYPE_UINT64:
      ((void (*)(GObject *, guint64)) elem->value_setter) (object, g_value_get_uint64 (value));
      break;

    case G_TYPE_ENUM:
      ((void (*)(GObject *, gint)) elem->value_setter) (object, g_value_get_enum (value));
      break;

    case G_TYPE_FLAGS:
      ((void (*)(GObject *, guint)) elem->value_setter) (object, g_value_get_flags (value));
      break;

    case G_TYPE_FLOAT:
      ((void (*)(GObject *, gfloat)) elem->value_setter) (object, g_value_get_float (value));
      break;

    case G_TYPE_DOUBLE:
      ((void (*)(GObject *, gdouble)) elem->value_setter) (object, g_value_get_double (value));
      break;

    case G_TYPE_STRING:
      ((void (*)(GObject *, const gchar *)) elem->value_setter) (object, g_value_get_string (value));
      break;

    case G_TYPE_POINTER:
      ((void (*)(GObject *, gpointer)) elem->value_setter) (object, g_value_get_pointer (value));
      break;

    case G_TYPE_BOXED:
      ((void (*)(GObject *, gpointer)) elem->value_setter) (object, g_value_get_boxed (value));
      break;

    case G_TYPE_PARAM:
      ((void (*)(GObject *, GParamSpec *)) elem->value_setter) (object, g_value_get_param (value));
      break;

    case G_TYPE_OBJECT:
      ((void (*)(GObject *, GObject *)) elem->value_setter) (object, g_value_get_object (value));
      break;

    default:
      g_assert_not_reached ();
      break;
  }
}

void
flow_gobject_unref_clear_real (gpointer *ptr_to_ref)
{
  if (!*ptr_to_ref)
    return;

  g_object_unref (*ptr_to_ref);
  *ptr_to_ref = NULL;
}
