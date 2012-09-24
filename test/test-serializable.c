/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-serializable.c - FlowSerializable test.
 *
 * Copyright (C) 2012 Hans Petter Jansson
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

#define TEST_UNIT_NAME "FlowSerializable"
#define TEST_TIMEOUT_S 20

#include "test-common.c"
#include <flow/flow-serializable.h>

/* --- Test class --- */

#define FLOW_TYPE_FOO            (flow_foo_get_type ())
#define FLOW_FOO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_FOO, FlowFoo))
#define FLOW_FOO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_FOO, FlowFooClass))
#define FLOW_IS_FOO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_FOO))
#define FLOW_IS_FOO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_FOO))
#define FLOW_FOO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_FOO, FlowFooClass))
GType   flow_foo_get_type        (void) G_GNUC_CONST;

typedef struct _FlowFoo        FlowFoo;
typedef struct _FlowFooPrivate FlowFooPrivate;
typedef struct _FlowFooClass   FlowFooClass;

struct _FlowFoo
{
  GObject     object;

  /*< private >*/

  FlowFooPrivate *priv;
};

struct _FlowFooClass
{
  GObjectClass parent_class;

  /*< private >*/

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

struct _FlowFooPrivate
{
  guint32 test_int;
};

static guint32
flow_foo_get_test_int_internal (FlowFoo *foo)
{
  FlowFooPrivate *priv = foo->priv;

  return priv->test_int;
}

static void
flow_foo_set_test_int_internal (FlowFoo *foo, guint32 test_int)
{
  FlowFooPrivate *priv = foo->priv;

  priv->test_int = test_int;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_foo)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT, "test", "Test", "Test integer",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_foo_get_test_int_internal,
                               flow_foo_set_test_int_internal,
                               0, G_MAXUINT32, 0)
FLOW_GOBJECT_PROPERTIES_END   ()
FLOW_GOBJECT_MAKE_IMPL (flow_foo, FlowFoo, G_TYPE_OBJECT, 0)

typedef struct
{
  gint n;
}
SerializeContext;

static gpointer
flow_foo_create_serialize_context (FlowFoo *foo)
{
  SerializeContext *context = g_slice_new (SerializeContext);
  context->n = 0;
  return context;
}

static void
flow_foo_destroy_serialize_context (FlowFoo *foo, gpointer data)
{
  g_slice_free (SerializeContext, data);
}

static FlowPacket *
flow_foo_serialize_step (FlowFoo *foo, gpointer data)
{
  FlowPacket *packet;
  FlowFooPrivate *priv = foo->priv;
  SerializeContext *context = data;
  union
  {
    guint32 n_be;
    guchar b [4];
  }
  u;

  u.n_be = GUINT32_TO_BE (priv->test_int);
  packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, &u.b [context->n], 1);
  context->n++;
  return packet;
}

typedef struct
{
  union
  {
    guint32 n_be;
    guchar b [4];
  }
  u;
  gint n;
}
DeserializeContext;

static gpointer
flow_foo_create_deserialize_context (void)
{
  DeserializeContext *context = g_slice_new (DeserializeContext);
  return context;
}

static void
flow_foo_destroy_deserialize_context (gpointer data)
{
  g_slice_free (DeserializeContext, data);
}

static gboolean
flow_foo_deserialize_step (FlowPacketQueue *packet_queue, DeserializeContext *context,
                           FlowSerializable **serializable_out, GError **error)
{
  gint n_dequeued;

  n_dequeued = flow_packet_queue_pop_bytes (packet_queue, &context->u.b [context->n], 4 - context->n);
  context->n += n_dequeued;
  if (context->n < 4)
    return TRUE;

  *serializable_out = g_object_new (FLOW_TYPE_FOO,
                                    "test", GUINT32_FROM_BE (context->u.n_be),
                                    NULL);
  return TRUE;
}

static void
flow_foo_serializable_iface_init (gpointer g_iface, gpointer iface_data)
{
  FlowSerializableInterface *iface = g_iface;

  iface->create_serialize_context = (gpointer (*) (FlowSerializable *))
    flow_foo_create_serialize_context;
  iface->destroy_serialize_context = (void (*) (FlowSerializable *, gpointer))
    flow_foo_destroy_serialize_context;
  iface->serialize_step = (FlowPacket *(*) (FlowSerializable *, gpointer))
    flow_foo_serialize_step;

  iface->create_deserialize_context = (gpointer (*) (void))
    flow_foo_create_deserialize_context;
  iface->destroy_deserialize_context = (void (*) (gpointer))
    flow_foo_destroy_deserialize_context;
  iface->deserialize_step = (gboolean (*) (FlowPacketQueue *, gpointer, FlowSerializable **, GError **))
    flow_foo_deserialize_step;
}

static void
flow_foo_type_init (GType type)
{
  const GInterfaceInfo iface_info =
  {
    (GInterfaceInitFunc) flow_foo_serializable_iface_init,  /* interface_init */
    NULL,  /* interface_finalize */
    NULL   /* interface_data */
  };

  g_type_add_interface_static (type, FLOW_TYPE_SERIALIZABLE, &iface_info);
}

static void
flow_foo_class_init (FlowFooClass *klass)
{
}

static void
flow_foo_init (FlowFoo *foo)
{
}

static void
flow_foo_construct (FlowFoo *foo)
{
}

static void
flow_foo_dispose (FlowFoo *foo)
{
}

static void
flow_foo_finalize (FlowFoo *foo)
{
}

/* --- Test main --- */

static void
test_run (void)
{
  FlowFoo *foo;
  gint i;

  for (i = 0; i < 1000; i++)
  {
    foo = g_object_new (FLOW_TYPE_FOO,
                        "test", i,
                        NULL);



    g_object_unref (foo);
  }
}
