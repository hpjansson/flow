/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-serializable.c - An interface for serializable objects.
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

#include "config.h"
#include "flow-serializable.h"

G_DEFINE_INTERFACE (FlowSerializable, flow_serializable, G_TYPE_OBJECT)

void
flow_serializable_default_init (FlowSerializableInterface *iface)
{
  /* Install properties here */
}

gpointer
flow_serializable_begin (FlowSerializable *serializable)
{
  FlowSerializableInterface *iface;

  g_return_val_if_fail (FLOW_IS_SERIALIZABLE (serializable), NULL);

  iface = FLOW_SERIALIZABLE_GET_IFACE (serializable);

  if (iface->create_context)
    return iface->create_context (serializable);

  return NULL;
}

gboolean
flow_serializable_step (FlowSerializable *serializable, FlowPad *target_pad,
                        gpointer context)
{
  FlowSerializableInterface *iface;
  FlowPacket *packet;

  g_return_val_if_fail (FLOW_IS_SERIALIZABLE (serializable), FALSE);
  g_return_val_if_fail (FLOW_IS_PAD (target_pad), FALSE);

  iface = FLOW_SERIALIZABLE_GET_IFACE (serializable);

  packet = iface->step (serializable, context);

  if (packet)
  {
    flow_pad_push (target_pad, packet);
    return TRUE;
  }

  return FALSE;
}

void
flow_serializable_finish (FlowSerializable *serializable, FlowPad *target_pad,
                          gpointer context)
{
  FlowSerializableInterface *iface;
  FlowPacket *packet;

  g_return_if_fail (FLOW_IS_SERIALIZABLE (serializable));
  g_return_if_fail (FLOW_IS_PAD (target_pad));

  iface = FLOW_SERIALIZABLE_GET_IFACE (serializable);

  while ((packet = iface->step (serializable, context)))
    flow_pad_push (target_pad, packet);

  if (iface->destroy_context)
    iface->destroy_context (serializable, context);
}

void
flow_serializable_abort (FlowSerializable *serializable, gpointer context)
{
  FlowSerializableInterface *iface;

  g_return_if_fail (FLOW_IS_SERIALIZABLE (serializable));

  iface = FLOW_SERIALIZABLE_GET_IFACE (serializable);

  if (iface->destroy_context)
    iface->destroy_context (serializable, context);
}

void
flow_serializable_serialize (FlowSerializable *serializable, FlowPad *target_pad)
{
  FlowSerializableInterface *iface;
  gpointer context = NULL;
  FlowPacket *packet;

  g_return_if_fail (FLOW_IS_SERIALIZABLE (serializable));
  g_return_if_fail (FLOW_IS_PAD (target_pad));

  iface = FLOW_SERIALIZABLE_GET_IFACE (serializable);

  if (iface->create_context)
    context = iface->create_context (serializable);

  while ((packet = iface->step (serializable, context)))
    flow_pad_push (target_pad, packet);

  if (iface->destroy_context)
    iface->destroy_context (serializable, context);
}
