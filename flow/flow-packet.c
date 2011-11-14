/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-packet.c - A data packet.
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
#include "flow-packet.h"

#include <string.h>  /* memcpy */

#define DATA_ALIGN_TO      (sizeof (gpointer))
#define PACKET_HEADER_SIZE (((sizeof (FlowPacket) + DATA_ALIGN_TO - 1) / DATA_ALIGN_TO) * DATA_ALIGN_TO)

#if GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 10
# define packet_alloc(size)        g_slice_alloc (size)
# define packet_free(packet, size) g_slice_free1 (size, packet)
#else
# define packet_alloc(size)        g_malloc (size)
# define packet_free(packet, size) g_free (packet)
#endif

/**
 * flow_packet_new:
 * @format: Format of this packet's contents, e.g. #FLOW_PACKET_FORMAT_BUFFER or
 *          #FLOW_PACKET_FORMAT_OBJECT.
 * @data:   For packets of type #FLOW_PACKET_FORMAT_BUFFER, a pointer to the data
 *          that will be copied into the packet. For #FLOW_PACKET_FORMAT_OBJECT,
 *          a pointer to the object that will be referenced.
 * @size:   For #FLOW_PACKET_FORMAT_BUFFER, the size of @data to copy, in bytes.
 *          For #FLOW_PACKET_FORMAT_OBJECT, an approximate measure of the memory
 *          consumed by the object.
 * 
 * Creates a new packet that can be queued and handled by Flow elements and
 * pipelines.
 * 
 * Return value: A new #FlowPacket.
 **/
FlowPacket *
flow_packet_new (FlowPacketFormat format, gpointer data, guint size)
{
  FlowPacket *packet;
  guint       body_size;

  switch (format)
  {
    case FLOW_PACKET_FORMAT_BUFFER:
      body_size = size;
      break;

    case FLOW_PACKET_FORMAT_OBJECT:
      body_size = sizeof (gpointer);
      break;

    default:
      g_assert_not_reached ();
      body_size = 0;
      break;
  }

  packet            = packet_alloc (PACKET_HEADER_SIZE + body_size);
  packet->format    = format;
  packet->size      = size;
  packet->ref_count = 1;

  switch (format)
  {
    case FLOW_PACKET_FORMAT_BUFFER:
      if (size > 0)
      {
        g_assert (data != NULL);
        memcpy ((guint8 *) packet + PACKET_HEADER_SIZE, data, size);
      }
      break;

    case FLOW_PACKET_FORMAT_OBJECT:
      g_assert (data != NULL);
      g_object_ref (data);
      *((gpointer *) ((guint8 *) packet + PACKET_HEADER_SIZE)) = data;
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  return packet;
}

FlowPacket *
flow_packet_alloc_for_data (guint size, gpointer *data_ptr_out)
{
  FlowPacket *packet;

  g_return_val_if_fail (size > 0, NULL);

  packet            = packet_alloc (PACKET_HEADER_SIZE + size);
  packet->format    = FLOW_PACKET_FORMAT_BUFFER;
  packet->size      = size;
  packet->ref_count = 1;

  *data_ptr_out = (gpointer *) ((guint8 *) packet + PACKET_HEADER_SIZE);

  return packet;
}

/**
 * flow_packet_new_take_object:
 * @object: A pointer to the object that will be referenced.
 * @size:   An approximate measure of the memory consumed by the object.
 * 
 * Creates a new packet that can be queued and handled by Flow elements and
 * pipelines. This function does not increment @object's reference count;
 * it takes the reference the caller was holding. It's faster than the
 * more generic flow_packet_new ().
 * 
 * Return value: A new #FlowPacket.
 **/
FlowPacket *
flow_packet_new_take_object (gpointer object, guint size)
{
  FlowPacket *packet;

  packet            = packet_alloc (PACKET_HEADER_SIZE + sizeof (gpointer));
  packet->format    = FLOW_PACKET_FORMAT_OBJECT;
  packet->size      = size;
  packet->ref_count = 1;

  g_assert (object != NULL);
  *((gpointer *) ((guint8 *) packet + PACKET_HEADER_SIZE)) = object;

  return packet;
}

FlowPacket *
flow_packet_copy (FlowPacket *packet)
{
  FlowPacket *packet_copy;

  g_return_val_if_fail (packet != NULL, NULL);

  switch (packet->format)
  {
    case FLOW_PACKET_FORMAT_BUFFER:
      packet_copy = packet_alloc (PACKET_HEADER_SIZE + packet->size);
      memcpy (packet_copy, packet, PACKET_HEADER_SIZE + packet->size);
      break;

    case FLOW_PACKET_FORMAT_OBJECT:
      {
        GObject *object;

        object = *((gpointer *) ((guint8 *) packet + PACKET_HEADER_SIZE));
        g_object_ref (object);
        packet_copy = packet_alloc (PACKET_HEADER_SIZE + sizeof (gpointer));
        memcpy (packet_copy, packet, PACKET_HEADER_SIZE + sizeof (gpointer));
      }
      break;

    default:
      g_assert_not_reached ();
      packet_copy = NULL;
      break;
  }

  packet_copy->ref_count = 1;

  return packet_copy;
}

static void
free_packet (FlowPacket *packet)
{
  switch (packet->format)
  {
    case FLOW_PACKET_FORMAT_BUFFER:
      packet_free (packet, PACKET_HEADER_SIZE + packet->size);
      break;

    case FLOW_PACKET_FORMAT_OBJECT:
      {
        GObject *object;

        object = *((gpointer *) ((guint8 *) packet + PACKET_HEADER_SIZE));
        g_object_unref (object);

        packet_free (packet, PACKET_HEADER_SIZE + sizeof (gpointer));
      }
      break;

    default:
      g_assert_not_reached ();
      break;
  }
}

/**
 * flow_packet_ref:
 * @packet: The packet to add a reference to.
 * 
 * Adds a reference to the packet. The packet is freed when its reference count
 * drops to zero.
 *
 * As a convenience, the passed-in pointer is returned, so you can use it in statements
 * like the following: flow_pad_push (pad, flow_packet_ref (packet));
 *
 * Return value: The passed-in #FlowPacket.
 **/
FlowPacket *
flow_packet_ref (FlowPacket *packet)
{
  g_return_val_if_fail (packet != NULL, NULL);

  g_atomic_int_inc (&packet->ref_count);
  return packet;
}

/**
 * flow_packet_unref:
 * @packet: The packet to subtract a reference from.
 * 
 * Removes a reference from the packet. The packet is freed when its reference count
 * drops to zero. If the packet contains data, it will be freed with the packet. If a
 * freed packet references an object, the object will be unreferenced.
 **/
void
flow_packet_unref (FlowPacket *packet)
{
  gboolean is_zero;

  g_return_if_fail (packet != NULL);

  is_zero = g_atomic_int_dec_and_test (&packet->ref_count);
  if (is_zero)
    free_packet (packet);
}

/**
 * flow_packet_get_format:
 * @packet: A packet.
 * 
 * Gets the format of the packet's contents.
 * 
 * Return value: The format of the packet's contents.
 **/
FlowPacketFormat
flow_packet_get_format (FlowPacket *packet)
{
  g_return_val_if_fail (packet != NULL, FLOW_PACKET_FORMAT_BUFFER);

  return packet->format;
}

/**
 * flow_packet_get_size:
 * @packet: A packet.
 * 
 * Gets the size of a packet specified upon its creation.
 * 
 * Return value: The packet's size.
 **/
guint
flow_packet_get_size (FlowPacket *packet)
{
  g_return_val_if_fail (packet != NULL, 0);

  return packet->size;
}

/**
 * flow_packet_get_data:
 * @packet: A packet.
 * 
 * Gets the data assigned to a packet. For buffers, this is a pointer to
 * an array of bytes owned by the packet. For objects, it is a pointer to
 * the object.
 * 
 * Return value: The packet's data pointer.
 **/
gpointer
flow_packet_get_data (FlowPacket *packet)
{
  gpointer data;

  g_return_val_if_fail (packet != NULL, NULL);

  switch (packet->format)
  {
    case FLOW_PACKET_FORMAT_BUFFER:
      data = (guint8 *) packet + PACKET_HEADER_SIZE;
      break;

    case FLOW_PACKET_FORMAT_OBJECT:
      data = *((gpointer *) ((guint8 *) packet + PACKET_HEADER_SIZE));
      break;

    default:
      g_assert_not_reached ();
      data = NULL;
      break;
  }

  return data;
}
