/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-serializable.h - An interface for serializable objects.
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

#ifndef _FLOW_SERIALIZABLE_H
#define _FLOW_SERIALIZABLE_H

#include <flow/flow-pad.h>

G_BEGIN_DECLS

#define FLOW_TYPE_SERIALIZABLE              (flow_serializable_get_type ())
#define FLOW_SERIALIZABLE(inst)             (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                             FLOW_TYPE_SERIALIZABLE, FlowSerializable))
#define FLOW_IS_SERIALIZABLE(inst)          (G_TYPE_CHECK_INSTANCE_TYPE ((inst), FLOW_TYPE_SERIALIZABLE))
#define FLOW_SERIALIZABLE_GET_IFACE(inst)   (G_TYPE_INSTANCE_GET_INTERFACE ((inst), \
                                             FLOW_TYPE_SERIALIZABLE, FlowSerializableInterface))
GType   flow_serializable_get_type          (void) G_GNUC_CONST;

typedef struct _FlowSerializable          FlowSerializable;  /* Dummy object */
typedef struct _FlowSerializableInterface FlowSerializableInterface;

struct _FlowSerializableInterface
{
  GTypeInterface g_iface;

  /* Virtual functions */

  gpointer    (*create_serialize_context)    (FlowSerializable *serializable);
  void        (*destroy_serialize_context)   (FlowSerializable *serializable, gpointer context);
  FlowPacket *(*serialize_step)              (FlowSerializable *serializable, gpointer context);

  gpointer    (*create_deserialize_context)  (void);
  void        (*destroy_deserialize_context) (gpointer context);
  gboolean    (*deserialize_step)            (FlowPacketQueue *packet_queue, gpointer context,
                                              FlowSerializable **serializable_out, GError **error);
};

gpointer          flow_serializable_serialize_begin     (FlowSerializable *serializable);
gboolean          flow_serializable_serialize_step      (FlowSerializable *serializable, FlowPad *target_pad,
                                                         gpointer context);
void              flow_serializable_serialize_finish    (FlowSerializable *serializable, FlowPad *target_pad,
                                                         gpointer context);
void              flow_serializable_serialize_abort     (FlowSerializable *serializable, gpointer context);

void              flow_serializable_serialize_all       (FlowSerializable *serializable, FlowPad *target_pad);

gpointer          flow_serializable_deserialize_begin   (GType type, FlowPacketQueue *packet_queue);
gboolean          flow_serializable_deserialize_step    (GType type, FlowPacketQueue *packet_queue,
                                                         gpointer context, FlowSerializable **serializable_out,
                                                         GError **error);
void              flow_serializable_deserialize_abort   (GType type, gpointer context);

G_END_DECLS

#endif  /* _FLOW_SERIALIZABLE_H */
