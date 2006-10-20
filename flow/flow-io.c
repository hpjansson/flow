/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-io.c - A simple I/O class based on a bin of processing elements.
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

#include <string.h>
#include "config.h"
#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-event-codes.h"
#include "flow-io.h"

#define USER_ELEMENT_NAME "user-element"

#define return_if_invalid_bin(io) \
  G_STMT_START { \
    if G_UNLIKELY (io->need_to_rescan_children) \
      rescan_children (io); \
\
    if G_UNLIKELY (!io->user_adapter) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter."); \
      return; \
    } \
  } G_STMT_END

#define return_val_if_invalid_bin(io, val) \
  G_STMT_START { \
    if G_UNLIKELY (io->need_to_rescan_children) \
      rescan_children (io); \
\
    if G_UNLIKELY (!io->user_adapter) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter."); \
      return val; \
    } \
  } G_STMT_END

/* --- FlowIO properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_io)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowIO definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_io, FlowIO, FLOW_TYPE_BIN, 0)

/* --- FlowIO implementation --- */

static void
flow_io_type_init (GType type)
{
}

static void
flow_io_class_init (FlowIOClass *klass)
{
}

static void
flow_io_init (FlowIO *io)
{
  FlowBin     *bin = FLOW_BIN (io);
  FlowElement *element;

  element = FLOW_ELEMENT (flow_user_adapter_new ());
  flow_bin_add_element (bin, element, USER_ELEMENT_NAME);
  g_object_unref (element);

  io->need_to_rescan_children = TRUE;
}

static void
flow_io_construct (FlowIO *io)
{
}

static void
flow_io_dispose (FlowIO *io)
{
}

static void
flow_io_finalize (FlowIO *io)
{
}

static void
rescan_children (FlowIO *io)
{
  FlowBin *bin = FLOW_BIN (io);

  io->user_adapter = FLOW_USER_ADAPTER (flow_bin_get_element (bin, USER_ELEMENT_NAME));

  io->need_to_rescan_children = FALSE;
}

/* --- FlowIO public API --- */

FlowIO *
flow_io_new (void)
{
  return g_object_new (FLOW_TYPE_IO, NULL);
}

gint
flow_io_read (FlowIO *io, gpointer dest_buffer, gint max_len)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;
  guint            packet_offset;
  gint             result = 0;

  g_return_val_if_fail (FLOW_IS_IO (io), 0);
  g_return_val_if_fail (dest_buffer != NULL, 0);
  g_return_val_if_fail (max_len >= 0, 0);
  return_val_if_invalid_bin (io, 0);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  while (flow_packet_queue_peek_packet (packet_queue, &packet, &packet_offset))
  {
    if G_LIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
      break;

    /* TODO: Handle events */

    flow_packet_queue_pop_packet (packet_queue);
  }

  if G_LIKELY (packet)
  {
    gint len = flow_packet_get_size (packet) - packet_offset;

    if (len <= max_len)
    {
      gpointer src_buffer = flow_packet_get_data (packet);

      memcpy (dest_buffer, (guint8 *) src_buffer + packet_offset, len);
      flow_packet_queue_drop_packet (packet_queue);
      result = len;
    }
    else
    {
      flow_packet_queue_pop_bytes (packet_queue, dest_buffer, max_len);
      result = max_len;
    }
  }

  return result;
}

gboolean
flow_io_read_exact (FlowIO *io, gpointer dest_buffer, gint exact_len)
{
  FlowPacketQueue *packet_queue;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  g_return_val_if_fail (dest_buffer != NULL, FALSE);
  g_return_val_if_fail (exact_len >= 0, FALSE);
  return_val_if_invalid_bin (io, FALSE);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  /* TODO */
}

gpointer
flow_io_read_object (FlowIO *io)
{
  FlowPacketQueue *packet_queue;

  g_return_val_if_fail (FLOW_IS_IO (io), NULL);
  return_val_if_invalid_bin (io, NULL);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  /* TODO */
}

void
flow_io_write (FlowIO *io, gpointer src_buffer, gint exact_len)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (src_buffer != NULL);
  g_return_if_fail (exact_len >= 0);
  return_if_invalid_bin (io);

  packet_queue = flow_user_adapter_get_output_queue (io->user_adapter);

  packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, src_buffer, exact_len);
  flow_packet_queue_push_packet (packet_queue, packet);

  flow_user_adapter_push (io->user_adapter);
}

void
flow_io_write_object (FlowIO *io, gpointer object)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (object != NULL);
  return_if_invalid_bin (io);

  packet_queue = flow_user_adapter_get_output_queue (io->user_adapter);

  packet = flow_packet_new (FLOW_PACKET_FORMAT_OBJECT, object, 0);
  flow_packet_queue_push_packet (packet_queue, packet);

  flow_user_adapter_push (io->user_adapter);
}

void
flow_io_flush (FlowIO *io)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  packet_queue = flow_user_adapter_get_output_queue (io->user_adapter);

  packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_FLUSH);
  flow_packet_queue_push_packet (packet_queue, packet);

  flow_user_adapter_push (io->user_adapter);
}

gint
flow_io_sync_read (FlowIO *io, gpointer dest_buffer, gint max_len)
{
  FlowPacketQueue *packet_queue;

  g_return_val_if_fail (FLOW_IS_IO (io), 0);
  g_return_val_if_fail (dest_buffer != NULL, 0);
  g_return_val_if_fail (max_len >= 0, 0);
  return_val_if_invalid_bin (io, 0);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);


  /* TODO */
}

gboolean
flow_io_sync_read_exact (FlowIO *io, gpointer dest_buffer, gint exact_len)
{
  FlowPacketQueue *packet_queue;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  g_return_val_if_fail (dest_buffer != NULL, FALSE);
  g_return_val_if_fail (exact_len >= 0, FALSE);
  return_val_if_invalid_bin (io, FALSE);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);


  /* TODO */
}

gpointer
flow_io_sync_read_object (FlowIO *io)
{
  FlowPacketQueue *packet_queue;

  g_return_val_if_fail (FLOW_IS_IO (io), NULL);
  return_val_if_invalid_bin (io, NULL);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);


  /* TODO */
}

void
flow_io_sync_write (FlowIO *io, gpointer src_buffer, gint exact_len)
{
  FlowPacketQueue *packet_queue;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (src_buffer != NULL);
  g_return_if_fail (exact_len >= 0);
  return_if_invalid_bin (io);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);


  /* TODO */
}

void
flow_io_sync_write_object (FlowIO *io, gpointer object)
{
  FlowPacketQueue *packet_queue;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (object != NULL);
  return_if_invalid_bin (io);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);


  /* TODO */
}

gboolean
flow_io_sync_flush (FlowIO *io)
{
  FlowPacketQueue *packet_queue;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  return_val_if_invalid_bin (io, FALSE);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);


  /* TODO */
}
