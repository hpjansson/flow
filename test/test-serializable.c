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
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT, "test-int", "Test", "Test integer",
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
flow_foo_destroy_serialize_context (FlowFoo *foo, SerializeContext *context)
{
  g_slice_free (SerializeContext, context);
}

static FlowPacket *
flow_foo_serialize_step (FlowFoo *foo, SerializeContext *context)
{
  FlowPacket *packet;
  FlowFooPrivate *priv = foo->priv;
  union
  {
    guint32 n_be;
    guchar b [4];
  }
  u;

  if (context->n == 4)
    return NULL;

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
  context->n = 0;
  return context;
}

static void
flow_foo_destroy_deserialize_context (DeserializeContext *context)
{
  g_slice_free (DeserializeContext, context);
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
                                    "test-int", GUINT32_FROM_BE (context->u.n_be),
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
deserialize_and_check_one (FlowFoo *original_foo, FlowPacketQueue *packet_queue)
{
  FlowFoo *copy_foo;
  gpointer context;
  gint original_int;
  gint copy_int;

  copy_foo = NULL;

  context = flow_serializable_deserialize_begin (FLOW_TYPE_FOO);
  while (flow_serializable_deserialize_step (FLOW_TYPE_FOO, packet_queue, context,
                                             (FlowSerializable **) &copy_foo, NULL) &&
         !copy_foo)
    ;

  g_object_get (original_foo, "test-int", &original_int, NULL);
  g_object_get (copy_foo, "test-int", &copy_int, NULL);
  if (copy_int != original_int)
    test_end (TEST_RESULT_FAILED, "deserialized object does not correspond to original");

  g_object_unref (copy_foo);
}

static void
test_run (void)
{
  FlowSimplexElement *controller;
  FlowPad *input_pad, *output_pad;
  FlowFoo *foo;
  gint i;

  controller = FLOW_SIMPLEX_ELEMENT (flow_controller_new ());
  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (controller));
  output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (controller));

  for (i = 0; i < 1000; i++)
  {
    gpointer context;
    gint j;

    foo = g_object_new (FLOW_TYPE_FOO,
                        "test-int", i,
                        NULL);

    flow_serializable_serialize_all (FLOW_SERIALIZABLE (foo), input_pad);

    context = flow_serializable_serialize_begin (FLOW_SERIALIZABLE (foo));
    flow_serializable_serialize_finish (FLOW_SERIALIZABLE (foo), input_pad, context);

    context = flow_serializable_serialize_begin (FLOW_SERIALIZABLE (foo));
    while (flow_serializable_serialize_step (FLOW_SERIALIZABLE (foo), input_pad, context))
      ;

    for (j = 0; j < 3; j++)
      deserialize_and_check_one (foo, flow_pad_get_packet_queue (output_pad));

    g_object_unref (foo);
  }

  g_object_unref (controller);
}
