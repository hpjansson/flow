/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-io.c - A prefab I/O class based on a bin of processing elements.
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

#define USER_ADAPTER_NAME "user-adapter"

#define return_if_invalid_bin(io) \
  G_STMT_START { \
    if G_UNLIKELY (io->need_to_check_bin) \
      flow_io_check_bin (io); \
\
    if G_UNLIKELY (!io->user_adapter) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter."); \
      return; \
    } \
  } G_STMT_END

#define return_val_if_invalid_bin(io, val) \
  G_STMT_START { \
    if G_UNLIKELY (io->need_to_check_bin) \
      flow_io_check_bin (io); \
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

static gboolean
handle_object (FlowIO *io, gpointer object)
{
  GType    type;
  gboolean result = FALSE;

  /* Call the class handlers, from most derived to least derived, stopping
   * if a handler returns TRUE. We return the return value from the last
   * handler run. */

  /* FIXME: If necessary, we can speed this up by caching the class pointers, or
   * even the function pointers if we're feeling frisky. */

  for (type = G_OBJECT_TYPE (io); g_type_is_a (type, FLOW_TYPE_IO); type = g_type_parent (type))
  {
    FlowIOClass *klass = g_type_class_peek (type);

    if (klass->handle_input_object)
      result = klass->handle_input_object (io, object);

    if (result)
      break;
  }

  return result;
}

static void
user_adapter_input (FlowIO *io)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet = NULL;

  /* Check for events first */

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  while (flow_packet_queue_peek_packet (packet_queue, &packet, NULL))
  {
    if G_LIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
      break;

    if (!handle_object (io, flow_packet_get_data (packet)))
      break;

    flow_packet_queue_drop_packet (packet_queue);
    packet = NULL;
  }

  /* If we ran into a packet that's not handled by us, and the user wants
   * to hear about it, notify. */

  if (packet && io->read_notify_func && !io->reads_are_blocked)
    io->read_notify_func (io->read_notify_data);
}

static void
user_adapter_output (FlowIO *io)
{
  if (io->write_notify_func && !io->writes_are_blocked)
    io->write_notify_func (io->write_notify_data);
}

static void
bin_changed (FlowIO *io)
{
  io->need_to_check_bin = TRUE;
}

static void
flow_io_check_bin_internal (FlowIO *io)
{
  FlowBin *bin = FLOW_BIN (io);

  flow_gobject_unref_clear (io->user_adapter);
  io->user_adapter = (FlowUserAdapter *) flow_bin_get_element (bin, USER_ADAPTER_NAME);

  if (io->user_adapter)
  {
    if (FLOW_IS_USER_ADAPTER (io->user_adapter))
    {
      g_object_ref (io->user_adapter);

      flow_user_adapter_set_input_notify (io->user_adapter, (FlowNotifyFunc) user_adapter_input, io);
      flow_user_adapter_set_output_notify (io->user_adapter, (FlowNotifyFunc) user_adapter_output, io);
    }
    else
      io->user_adapter = NULL;
  }

  io->need_to_check_bin = FALSE;
}

static gboolean
flow_io_handle_input_object (FlowIO *io, gpointer object)
{
  /* We don't do much with objects */

  return FALSE;
}

static void
flow_io_type_init (GType type)
{
}

static void
flow_io_class_init (FlowIOClass *klass)
{
  klass->check_bin           = flow_io_check_bin_internal;
  klass->handle_input_object = flow_io_handle_input_object;
}

static void
flow_io_init (FlowIO *io)
{
  FlowBin *bin = FLOW_BIN (io);

  io->min_read_buffer = 1;

  io->allow_blocking_read  = TRUE;
  io->allow_blocking_write = TRUE;

  io->user_adapter = flow_user_adapter_new ();
  flow_bin_add_element (bin, FLOW_ELEMENT (io->user_adapter), USER_ADAPTER_NAME);

  flow_user_adapter_block_input (io->user_adapter);
  flow_user_adapter_block_output (io->user_adapter);

  flow_user_adapter_set_input_notify (io->user_adapter, (FlowNotifyFunc) user_adapter_input, io);
  flow_user_adapter_set_output_notify (io->user_adapter, (FlowNotifyFunc) user_adapter_output, io);

  g_signal_connect (io, "element-added",   G_CALLBACK (bin_changed), NULL);
  g_signal_connect (io, "element-removed", G_CALLBACK (bin_changed), NULL);
}

static void
flow_io_construct (FlowIO *io)
{
}

static void
flow_io_dispose (FlowIO *io)
{
  flow_gobject_unref_clear (io->user_adapter);
}

static void
flow_io_finalize (FlowIO *io)
{
}

static void
set_minimum_read_buffer (FlowIO *io, gint min_len)
{
  gint old_min_len;

  old_min_len = io->min_read_buffer;
  io->min_read_buffer = min_len;

  if (min_len > old_min_len)
  {
    FlowPacketQueue *packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

    if (flow_packet_queue_get_length_data_bytes (packet_queue) < min_len)
      flow_user_adapter_unblock_input (io->user_adapter);
  }
}

static gint
try_read_data (FlowIO *io, gpointer dest_buffer, gint max_len)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet = NULL;
  gint             packet_offset;
  gint             result = 0;

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  while (flow_packet_queue_peek_packet (packet_queue, &packet, &packet_offset))
  {
    if G_LIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
      break;

    handle_object (io, flow_packet_get_data (packet));
    flow_packet_queue_drop_packet (packet_queue);
    packet = NULL;
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

static gboolean
try_read_object (FlowIO *io, gpointer *object_dest)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;
  gpointer         object     = NULL;
  gboolean         conclusive = FALSE;

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  while (flow_packet_queue_peek_packet (packet_queue, &packet, NULL))
  {
    if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_OBJECT)
    {
      conclusive = TRUE;
      break;
    }

    object = flow_packet_get_data (packet);

    if (!handle_object (io, object))
    {
      conclusive = TRUE;
      g_object_ref (object);
      flow_packet_queue_drop_packet (packet_queue);
      break;
    }

    object = NULL;
    flow_packet_queue_drop_packet (packet_queue);
  }

  *object_dest = object;
  return conclusive;
}

static void
prepare_read (FlowIO *io, guint request_len)
{
  FlowIOClass *io_klass;

  io_klass = FLOW_IO_GET_CLASS (io);
  if (io_klass->prepare_read)
    io_klass->prepare_read (io, request_len);
}

static void
prepare_write (FlowIO *io, guint request_len)
{
  FlowIOClass *io_klass;

  io_klass = FLOW_IO_GET_CLASS (io);
  if (io_klass->prepare_write)
    io_klass->prepare_write (io, request_len);
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
  g_return_val_if_fail (FLOW_IS_IO (io), 0);
  g_return_val_if_fail (dest_buffer != NULL, 0);
  g_return_val_if_fail (max_len >= 0, 0);
  return_val_if_invalid_bin (io, 0);

  set_minimum_read_buffer (io, 1);
  prepare_read (io, max_len);

  return try_read_data (io, dest_buffer, max_len);
}

gboolean
flow_io_read_exact (FlowIO *io, gpointer dest_buffer, gint exact_len)
{
  FlowPacketQueue *packet_queue;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  g_return_val_if_fail (dest_buffer != NULL, FALSE);
  g_return_val_if_fail (exact_len >= 0, FALSE);
  return_val_if_invalid_bin (io, FALSE);

  set_minimum_read_buffer (io, exact_len);
  prepare_read (io, exact_len);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  if (flow_packet_queue_get_length_data_bytes (packet_queue) < exact_len)
    return FALSE;

  while (!flow_packet_queue_pop_bytes (packet_queue, dest_buffer, exact_len))
  {
    FlowPacket *packet;

    packet = flow_packet_queue_pop_first_object (packet_queue);
    g_assert (packet != NULL);

    handle_object (io, flow_packet_get_data (packet));
    flow_packet_free (packet);
  }

  return TRUE;
}

gpointer
flow_io_read_object (FlowIO *io)
{
  gpointer object;

  g_return_val_if_fail (FLOW_IS_IO (io), NULL);
  return_val_if_invalid_bin (io, NULL);

  set_minimum_read_buffer (io, 1);
  prepare_read (io, 0);
  try_read_object (io, &object);

  return object;
}

void
flow_io_write (FlowIO *io, gpointer src_buffer, gint exact_len)
{
  FlowPacketQueue *packet_queue;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (src_buffer != NULL);
  g_return_if_fail (exact_len >= 0);
  return_if_invalid_bin (io);

  prepare_write (io, exact_len);

  packet_queue = flow_user_adapter_get_output_queue (io->user_adapter);

  flow_packet_queue_push_bytes (packet_queue, src_buffer, exact_len);
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

  prepare_write (io, 0);

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

static void
update_user_adapter_blocks (FlowIO *io)
{
  if (!io->read_notify_func || io->reads_are_blocked)
    flow_user_adapter_block_input (io->user_adapter);
  else
    flow_user_adapter_unblock_input (io->user_adapter);

  if (!io->write_notify_func || io->writes_are_blocked)
    flow_user_adapter_block_output (io->user_adapter);
  else
    flow_user_adapter_unblock_output (io->user_adapter);
}

void
flow_io_set_read_notify (FlowIO *io, FlowNotifyFunc func, gpointer user_data)
{
  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  io->read_notify_func = func;
  io->read_notify_data = user_data;

  update_user_adapter_blocks (io);

  /* TODO? */
}

void
flow_io_set_write_notify (FlowIO *io, FlowNotifyFunc func, gpointer user_data)
{
  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  io->write_notify_func = func;
  io->write_notify_data = user_data;

  update_user_adapter_blocks (io);

  /* TODO? */
}

void
flow_io_block_reads (FlowIO *io)
{
  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  if (io->reads_are_blocked)
    return;

  io->reads_are_blocked = TRUE;
  update_user_adapter_blocks (io);

  /* TODO? */
}

void
flow_io_unblock_reads (FlowIO *io)
{
  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  if (!io->reads_are_blocked)
    return;

  io->reads_are_blocked = FALSE;
  update_user_adapter_blocks (io);

  /* TODO? */
}

void
flow_io_block_writes (FlowIO *io)
{
  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  if (io->writes_are_blocked)
    return;

  io->writes_are_blocked = TRUE;
  update_user_adapter_blocks (io);

  /* TODO? */
}

void
flow_io_unblock_writes (FlowIO *io)
{
  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  if (!io->writes_are_blocked)
    return;

  io->writes_are_blocked = FALSE;
  update_user_adapter_blocks (io);

  /* TODO? */
}

gint
flow_io_sync_read (FlowIO *io, gpointer dest_buffer, gint max_len)
{
  FlowPacketQueue *packet_queue;
  gint             result;

  g_return_val_if_fail (FLOW_IS_IO (io), 0);
  g_return_val_if_fail (dest_buffer != NULL, 0);
  g_return_val_if_fail (max_len >= 0, 0);
  return_val_if_invalid_bin (io, 0);

  set_minimum_read_buffer (io, 1);
  prepare_read (io, max_len);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  while (!(result = try_read_data (io, dest_buffer, max_len)) && io->allow_blocking_read)
  {
    flow_user_adapter_wait_for_input (io->user_adapter);
  }

  return result;
}

gboolean
flow_io_sync_read_exact (FlowIO *io, gpointer dest_buffer, gint exact_len)
{
  FlowPacketQueue *packet_queue;
  gboolean         result;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  g_return_val_if_fail (dest_buffer != NULL, FALSE);
  g_return_val_if_fail (exact_len >= 0, FALSE);
  return_val_if_invalid_bin (io, FALSE);

  set_minimum_read_buffer (io, exact_len);
  prepare_read (io, exact_len);

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  while (!(result = flow_packet_queue_pop_bytes (packet_queue, dest_buffer, exact_len)))
  {
    FlowPacket *packet;

    packet = flow_packet_queue_pop_first_object (packet_queue);
    if (packet)
    {
      handle_object (io, flow_packet_get_data (packet));
      flow_packet_free (packet);
      continue;
    }

    if (!io->allow_blocking_read)
      break;

    flow_user_adapter_wait_for_input (io->user_adapter);
  }

  return result;
}

gpointer
flow_io_sync_read_object (FlowIO *io)
{
  gpointer object;

  g_return_val_if_fail (FLOW_IS_IO (io), NULL);
  return_val_if_invalid_bin (io, NULL);

  set_minimum_read_buffer (io, 1);
  prepare_read (io, 0);

  while (!try_read_object (io, &object) && io->allow_blocking_read)
  {
    flow_user_adapter_wait_for_input (io->user_adapter);
  }

  return object;
}

void
flow_io_sync_write (FlowIO *io, gpointer src_buffer, gint exact_len)
{
  FlowPacketQueue *packet_queue;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (src_buffer != NULL);
  g_return_if_fail (exact_len >= 0);
  return_if_invalid_bin (io);

  prepare_write (io, exact_len);

  packet_queue = flow_user_adapter_get_output_queue (io->user_adapter);

  flow_packet_queue_push_bytes (packet_queue, src_buffer, exact_len);
  flow_user_adapter_push (io->user_adapter);

  while (io->allow_blocking_write && flow_packet_queue_get_length_packets (packet_queue))
  {
    flow_user_adapter_wait_for_output (io->user_adapter);
  }
}

void
flow_io_sync_write_object (FlowIO *io, gpointer object)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (object != NULL);
  return_if_invalid_bin (io);

  prepare_write (io, 0);

  packet_queue = flow_user_adapter_get_output_queue (io->user_adapter);

  packet = flow_packet_new (FLOW_PACKET_FORMAT_OBJECT, object, 0);
  flow_packet_queue_push_packet (packet_queue, packet);

  flow_user_adapter_push (io->user_adapter);

  while (io->allow_blocking_write && flow_packet_queue_get_length_packets (packet_queue))
  {
    flow_user_adapter_wait_for_output (io->user_adapter);
  }
}

void
flow_io_sync_flush (FlowIO *io)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  prepare_write (io, 0);

  packet_queue = flow_user_adapter_get_output_queue (io->user_adapter);

  packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_FLUSH);
  flow_packet_queue_push_packet (packet_queue, packet);

  flow_user_adapter_push (io->user_adapter);

  while (io->allow_blocking_write && flow_packet_queue_get_length_packets (packet_queue))
  {
    flow_user_adapter_wait_for_output (io->user_adapter);
  }
}

void
flow_io_check_bin (FlowIO *io)
{
  GType type;

  g_return_if_fail (FLOW_IS_IO (io));

  /* Call the class handlers, from most derived to least derived */

  /* FIXME: If necessary, we can speed this up by caching the class pointers, or
   * even the function pointers if we're feeling frisky. */

  for (type = G_OBJECT_TYPE (io); g_type_is_a (type, FLOW_TYPE_IO); type = g_type_parent (type))
  {
    FlowIOClass *klass = g_type_class_peek (type);

    if (klass->check_bin)
      klass->check_bin (io);
  }
}

void
flow_io_check_events (FlowIO *io)
{
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  g_return_if_fail (FLOW_IS_IO (io));

  packet_queue = flow_user_adapter_get_input_queue (io->user_adapter);

  while (flow_packet_queue_peek_packet (packet_queue, &packet, NULL))
  {
    gpointer object;

    if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_OBJECT)
      break;

    object = flow_packet_get_data (packet);

    if (!handle_object (io, object))
      break;

    flow_packet_queue_drop_packet (packet_queue);
  }
}
