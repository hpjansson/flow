/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-gobject-util.h - GObject utilities.
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

#ifndef _FLOW_GOBJECT_UTIL_H
#define _FLOW_GOBJECT_UTIL_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct
{
  guint        fundamental_type;
  const gchar *name;
  const gchar *nick;
  const gchar *blurb;
  GParamFlags  param_flags;
  void         (*value_getter) (void);
  void         (*value_setter) (void);
  GType        (*type_getter)  (void);

  /* TODO: Could we use unions here? */

  gint64       int_min;
  gint64       int_max;
  gint64       int_bool_default;

  gdouble      float_min;
  gdouble      float_max;
  gdouble      float_default;

  const gchar *string_default;
}
FlowGObjectPropElem;

#define FLOW_GOBJECT_PROPERTY_OVERRIDE(name, value_getter, value_setter)                \
{ 0, name, NULL, NULL, 0,                                                               \
  (void (*)(void)) value_getter, (void (*)(void)) value_setter,                         \
  NULL, 0, 0, 0, 0.0, 0.0, 0.0, NULL }

#define FLOW_GOBJECT_PROPERTY(fundamental_type, name, nick, blurb, param_flags,         \
                              value_getter, value_setter, type_getter)                  \
{ fundamental_type, name, nick, blurb, param_flags,                                     \
  (void (*)(void)) value_getter, (void (*)(void)) value_setter, type_getter,            \
  0, 0, 0, 0.0, 0.0, 0.0, NULL },

#define FLOW_GOBJECT_PROPERTY_BOOLEAN(fundamental_type, name, nick, blurb, param_flags, \
                                      value_getter, value_setter, defval)               \
{ fundamental_type, name, nick, blurb, param_flags,                                     \
  (void (*)(void)) value_getter, (void (*)(void)) value_setter, NULL,                   \
  0, 0, defval, 0.0, 0.0, 0.0, NULL },

#define FLOW_GOBJECT_PROPERTY_INT(fundamental_type, name, nick, blurb, param_flags,     \
                                  value_getter, value_setter, min, max, defval)         \
{ fundamental_type, name, nick, blurb, param_flags,                                     \
  (void (*)(void)) value_getter, (void (*)(void)) value_setter, NULL,                   \
  min, max, defval, 0.0, 0.0, 0.0, NULL },

#define FLOW_GOBJECT_PROPERTY_ENUM(name, nick, blurb, param_flags,                      \
                                   value_getter, value_setter, defval, type_getter)     \
{ G_TYPE_ENUM, name, nick, blurb, param_flags,                                          \
  (void (*)(void)) value_getter, (void (*)(void)) value_setter, type_getter,            \
  0, 0, defval, 0.0, 0.0, 0.0, NULL },

#define FLOW_GOBJECT_PROPERTY_FLAGS(name, nick, blurb, param_flags,                     \
                                   value_getter, value_setter, defval, type_getter)     \
{ G_TYPE_FLAGS, name, nick, blurb, param_flags,                                         \
  (void (*)(void)) value_getter, (void (*)(void)) value_setter, type_getter,            \
  0, 0, defval, 0.0, 0.0, 0.0, NULL },

#define FLOW_GOBJECT_PROPERTY_FLOAT(fundamental_type, name, nick, blurb, param_flags,   \
                                    value_getter, value_setter, min, max, defval)       \
{ fundamental_type, name, nick, blurb, param_flags,                                     \
  (void (*)(void)) value_getter, (void (*)(void)) value_setter, NULL,                   \
  0, 0, 0, min, max, defval, NULL },

#define FLOW_GOBJECT_PROPERTY_STRING(fundamental_type, name, nick, blurb, param_flags,  \
                                     value_getter, value_setter, defval)                \
{ fundamental_type, name, nick, blurb, param_flags,                                     \
  (void (*)(void)) value_getter, (void (*)(void)) value_setter, NULL,                   \
  0, 0, 0, 0.0, 0.0, 0.0, defval },

#define FLOW_GOBJECT_PROPERTY_POINTER(fundamental_type, name, nick, blurb, param_flags, \
                                      value_getter, value_setter)                       \
{ fundamental_type, name, nick, blurb, param_flags,                                     \
  (void (*)(void)) value_getter, (void (*)(void)) value_setter,                         \
  NULL, 0, 0, 0, 0.0, 0.0, 0.0, NULL },

#define FLOW_GOBJECT_PROPERTIES_BEGIN(self_prefix) \
  static const FlowGObjectPropElem self_prefix##_prop_list [] = {                       \
{  0, NULL, NULL, NULL, 0, NULL, NULL, NULL, 0, 0, 0, 0.0, 0.0, 0.0, NULL },

#define FLOW_GOBJECT_PROPERTIES_END() };

/* Usage: FLOW_GOBJECT_MAKE_IMPL (flow_gadget, FlowGadget, G_TYPE_OBJECT, G_TYPE_FLAG_ABSTRACT) */

#define FLOW_GOBJECT_MAKE_IMPL(self_prefix, self_type, parent_type, reg_flags)    \
static GObjectClass *self_prefix##_parent_class;                                  \
                                                                                  \
/* User funcs; user will have to define these */                                  \
static void self_prefix##_type_init (GType type);                                 \
static void self_prefix##_class_init (self_type##Class *object_class);            \
static void self_prefix##_init (self_type *object);                               \
static void self_prefix##_construct (self_type *object);                          \
static void self_prefix##_dispose (self_type *object);                            \
static void self_prefix##_finalize (self_type *object);                           \
                                                                                  \
static void self_prefix##_simple_get_property (GObject *object, guint prop_id,    \
                                               GValue *value, GParamSpec *pspec)  \
{                                                                                 \
  flow_gobject_get_property (object, prop_id, value, pspec,                       \
                             self_prefix##_prop_list,                             \
                             G_N_ELEMENTS (self_prefix##_prop_list));             \
}                                                                                 \
                                                                                  \
static void self_prefix##_simple_set_property (GObject *object, guint prop_id,    \
                                               const GValue *value, GParamSpec *pspec)  \
{                                                                                 \
  flow_gobject_set_property (object, prop_id, value, pspec,                       \
                             self_prefix##_prop_list,                             \
                             G_N_ELEMENTS (self_prefix##_prop_list));             \
}                                                                                 \
                                                                                  \
static GObject *self_prefix##_simple_construct (GType type, guint n_cons_props,   \
                                                GObjectConstructParam *cons_props)  \
{                                                                                 \
  GObject      *object;                                                           \
  GObjectClass *parent_class;                                                     \
                                                                                  \
  parent_class = g_type_class_peek (parent_type);                                 \
  object = parent_class->constructor (type, n_cons_props, cons_props);            \
  self_prefix##_construct ((self_type *) object);  /* User func */                \
                                                                                  \
  return object;                                                                  \
}                                                                                 \
                                                                                  \
static void self_prefix##_simple_dispose (GObject *object)                        \
{                                                                                 \
  self_prefix##_dispose ((self_type *) object);  /* User func */                  \
                                                                                  \
  if (G_OBJECT_CLASS (self_prefix##_parent_class)->dispose)                       \
    (* G_OBJECT_CLASS (self_prefix##_parent_class)->dispose) (object);            \
}                                                                                 \
                                                                                  \
static void self_prefix##_simple_finalize (GObject *object)                       \
{                                                                                 \
  self_prefix##_finalize ((self_type *) object);  /* User func */                 \
                                                                                  \
  if (G_OBJECT_CLASS (self_prefix##_parent_class)->finalize)                      \
    (* G_OBJECT_CLASS (self_prefix##_parent_class)->finalize) (object);           \
}                                                                                 \
                                                                                  \
static void self_prefix##_class_simple_init (GObjectClass *object_class)          \
{                                                                                 \
  self_prefix##_parent_class = g_type_class_ref (parent_type);                    \
  object_class->get_property = self_prefix##_simple_get_property;                 \
  object_class->set_property = self_prefix##_simple_set_property;                 \
  object_class->constructor = self_prefix##_simple_construct;                     \
  object_class->dispose = self_prefix##_simple_dispose;                           \
  object_class->finalize = self_prefix##_simple_finalize;                         \
  flow_gobject_class_install_properties (object_class, self_prefix##_prop_list,   \
                                         G_N_ELEMENTS (self_prefix##_prop_list)); \
  self_prefix##_class_init ((self_type##Class *) object_class);  /* User func */  \
}                                                                                 \
                                                                                  \
static void self_prefix##_simple_init (GObject *object)                           \
{                                                                                 \
  self_prefix##_init ((self_type *) object);  /* User func */                     \
}                                                                                 \
                                                                                  \
GType self_prefix##_get_type (void)                                               \
{                                                                                 \
  static GType type = 0;                                                          \
  if (!type)                                                                      \
  {                                                                               \
    static GTypeInfo const object_info =                                          \
    {                                                                             \
      sizeof (self_type##Class),                                                  \
                                                                                  \
      (GBaseInitFunc) NULL,                                                       \
      (GBaseFinalizeFunc) NULL,                                                   \
                                                                                  \
      (GClassInitFunc) self_prefix##_class_simple_init,                           \
      (GClassFinalizeFunc) NULL,                                                  \
      NULL,  /* class_data */                                                     \
                                                                                  \
      sizeof (self_type),                                                         \
      0,  /* n_preallocs */                                                       \
      (GInstanceInitFunc) self_prefix##_simple_init,                              \
      NULL                                                                        \
    };                                                                            \
    type = g_type_register_static (parent_type, #self_type, &object_info, reg_flags); \
    self_prefix##_type_init (type);  /* User func */                              \
  }                                                                               \
  return type;                                                                    \
}

/* Usage: FLOW_GINTERFACE_MAKE_IMPL (flow_gadget, FlowGadget) */
#define FLOW_GINTERFACE_MAKE_IMPL(self_prefix, self_type)                         \
                                                                                  \
/* User funcs; user will have to define these */                                  \
static void self_prefix##_type_init (GType type);                                 \
static void self_prefix##_base_init (self_type##Interface *interface);            \
                                                                                  \
static void self_prefix##_base_simple_init (gpointer interface)                   \
{                                                                                 \
  static gboolean initialized = FALSE;                                            \
  if (!initialized)                                                               \
  {                                                                               \
    self_prefix##_base_init ((self_type##Interface *) interface);  /* User func */ \
    initialized = TRUE;                                                           \
  }                                                                               \
}                                                                                 \
                                                                                  \
GType self_prefix##_get_type (void)                                               \
{                                                                                 \
  static GType type = 0;                                                          \
  if (!type)                                                                      \
  {                                                                               \
    static GTypeInfo const object_info =                                          \
    {                                                                             \
      sizeof (self_type##Interface),                                              \
                                                                                  \
      (GBaseInitFunc) self_prefix##_base_simple_init,                             \
      (GBaseFinalizeFunc) NULL,                                                   \
                                                                                  \
      (GClassInitFunc) NULL,                                                      \
      (GClassFinalizeFunc) NULL,                                                  \
      NULL,  /* class_data */                                                     \
                                                                                  \
      0,     /* sizeof (self_type) */                                             \
      0,     /* n_preallocs */                                                    \
      NULL,  /* instance_init */                                                  \
    };                                                                            \
    type = g_type_register_static (G_TYPE_INTERFACE, #self_type, &object_info, 0); \
    self_prefix##_type_init (type);  /* User func */                              \
  }                                                                               \
  return type;                                                                    \
}

void flow_gobject_class_install_properties (GObjectClass *object_class, const FlowGObjectPropElem *props,
                                            gint n_props) G_GNUC_INTERNAL;
void flow_gobject_get_property             (GObject *object, gint prop_id, GValue *value,
                                            GParamSpec *pspec, const FlowGObjectPropElem *props, gint n_props) G_GNUC_INTERNAL;
void flow_gobject_set_property             (GObject *object, gint prop_id, const GValue *value,
                                            GParamSpec *pspec, const FlowGObjectPropElem *props, gint n_props) G_GNUC_INTERNAL;

#define flow_gobject_unref_clear(ref) flow_gobject_unref_clear_real ((gpointer) &ref)
void flow_gobject_unref_clear_real   (gpointer *ptr_to_ref) G_GNUC_INTERNAL;

G_END_DECLS

#endif  /* _FLOW_GOBJECT_UTIL_H */
