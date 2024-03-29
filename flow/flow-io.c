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
#include "flow-element-util.h"
#include "flow-gobject-util.h"
#include "flow-event-codes.h"
#include "flow-io.h"

#define USER_ADAPTER_NAME "user-adapter"

#define return_if_invalid_bin(io) \
  G_STMT_START { \
    FlowIOPrivate *priv = io->priv; \
\
    if G_UNLIKELY (io->need_to_check_bin) \
      flow_io_check_bin (io); \
\
    if G_UNLIKELY (!priv->user_adapter) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter."); \
      return; \
    } \
  } G_STMT_END

#define return_val_if_invalid_bin(io, val) \
  G_STMT_START { \
    FlowIOPrivate *priv = io->priv; \
\
    if G_UNLIKELY (io->need_to_check_bin) \
      flow_io_check_bin (io); \
\
    if G_UNLIKELY (!priv->user_adapter) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter."); \
      return val; \
    } \
  } G_STMT_END

#define on_error_propagate_and_assert(x) \
  G_STMT_START { \
    if (io->error) \
    { \
      g_assert (x); \
\
      if (error) \
        *error = io->error; \
      else \
        g_error_free (io->error); \
\
      io->error = NULL; \
    } \
  } G_STMT_END

/* --- FlowIO private data --- */

struct _FlowIOPrivate
{
  guint            reads_are_blocked       : 1;
  guint            writes_are_blocked      : 1;
  guint            wrote_stream_begin      : 1;

  FlowNotifyFunc   read_notify_func;
  gpointer         read_notify_data;

  FlowNotifyFunc   write_notify_func;
  gpointer         write_notify_data;

  gint             min_read_buffer;
  FlowUserAdapter *user_adapter;
};

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
  FlowIOPrivate   *priv   = io->priv;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet = NULL;

  /* Check for events first */

  packet_queue = flow_user_adapter_get_input_queue (priv->user_adapter);

  while (flow_packet_queue_peek_packet (packet_queue, &packet, NULL))
  {
    if (G_LIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER))
    {
      /* Subclasses can set the drop_read_data flag, e.g. if we're currently receiving
       * a segment of data they want to ignore silently. */

      if (io->drop_read_data)
      {
        flow_packet_queue_drop_packet (packet_queue);
        packet = NULL;
        continue;
      }

      break;
    }

    if (!handle_object (io, flow_packet_get_data (packet)))
      break;

    flow_packet_queue_drop_packet (packet_queue);
    packet = NULL;
  }

  /* If we ran into a packet that's not handled by us, and the user wants
   * to hear about it, notify. */

  if (packet && priv->read_notify_func && !priv->reads_are_blocked)
    priv->read_notify_func (priv->read_notify_data);
}

static void
user_adapter_output (FlowIO *io)
{
  FlowIOPrivate *priv = io->priv;

  if (priv->write_notify_func && !priv->writes_are_blocked)
    priv->write_notify_func (priv->write_notify_data);
}

static void
update_user_adapter_blocks (FlowIO *io)
{
  FlowIOPrivate *priv = io->priv;

  if (!priv->read_notify_func || priv->reads_are_blocked)
    flow_user_adapter_block_input (priv->user_adapter);
  else
    flow_user_adapter_unblock_input (priv->user_adapter);

  if (!priv->write_notify_func || priv->writes_are_blocked)
    flow_user_adapter_block_output (priv->user_adapter);
  else
    flow_user_adapter_unblock_output (priv->user_adapter);
}

static void
install_user_adapter (FlowIO *io, FlowUserAdapter *user_adapter)
{
  FlowIOPrivate *priv = io->priv;

  flow_gobject_unref_clear (priv->user_adapter);

  if (!user_adapter)
    return;

  if (!FLOW_IS_USER_ADAPTER (user_adapter))
    return;

  priv->user_adapter = g_object_ref (user_adapter);

  flow_user_adapter_set_input_notify  (user_adapter, (FlowNotifyFunc) user_adapter_input,  io);
  flow_user_adapter_set_output_notify (user_adapter, (FlowNotifyFunc) user_adapter_output, io);

  update_user_adapter_blocks (io);
}

static void
flow_io_check_bin_internal (FlowIO *io)
{
  FlowBin         *bin  = FLOW_BIN (io);
  FlowUserAdapter *user_adapter;

  user_adapter = (FlowUserAdapter *) flow_bin_get_element (bin, USER_ADAPTER_NAME);
  install_user_adapter (io, user_adapter);

  io->need_to_check_bin = FALSE;
}

static void
bin_changed (FlowIO *io)
{
  io->need_to_check_bin = TRUE;
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
  FlowIOPrivate *priv = io->priv;
  FlowBin       *bin  = FLOW_BIN (io);

  priv->min_read_buffer = 1;

  io->read_stream_is_open  = TRUE;
  io->write_stream_is_open = TRUE;

  g_signal_connect (io, "element-added",   G_CALLBACK (bin_changed), NULL);
  g_signal_connect (io, "element-removed", G_CALLBACK (bin_changed), NULL);

  /* Adding the element will eventually trigger flow_io_check_bin_internal, which
   * does the rest of the setup */

  priv->user_adapter = flow_user_adapter_new ();
  flow_bin_add_element (bin, FLOW_ELEMENT (priv->user_adapter), USER_ADAPTER_NAME);
}

static void
flow_io_construct (FlowIO *io)
{
}

static void
flow_io_dispose (FlowIO *io)
{
  FlowIOPrivate *priv = io->priv;

  flow_gobject_unref_clear (priv->user_adapter);
  g_clear_error (&io->error);
}

static void
flow_io_finalize (FlowIO *io)
{
}

static void
successful_read (FlowIO *io, gint len)
{
  FlowIOClass *io_klass;

  io_klass = FLOW_IO_GET_CLASS (io);
  if (io_klass->successful_read)
    io_klass->successful_read (io, len);
}

static void
set_minimum_read_buffer (FlowIO *io, gint min_len)
{
  FlowIOPrivate *priv = io->priv;
  gint           old_min_len;

  old_min_len = priv->min_read_buffer;
  priv->min_read_buffer = min_len;

  if (min_len > old_min_len)
  {
    FlowPacketQueue *packet_queue = flow_user_adapter_get_input_queue (priv->user_adapter);

    if (flow_packet_queue_get_length_data_bytes (packet_queue) < min_len)
      flow_user_adapter_unblock_input (priv->user_adapter);
  }
}

static gint
try_read_data (FlowIO *io, gpointer dest_buffer, gint max_len)
{
  FlowIOPrivate   *priv   = io->priv;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet = NULL;
  gint             packet_offset;
  gint             result = 0;

  packet_queue = flow_user_adapter_get_input_queue (priv->user_adapter);

  /* Process objects until we get some data */

  while (flow_packet_queue_peek_packet (packet_queue, &packet, &packet_offset))
  {
    if (G_LIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER))
    {
      if (io->drop_read_data)
      {
        flow_packet_queue_drop_packet (packet_queue);
        packet = NULL;
        continue;
      }

      break;
    }

    handle_object (io, flow_packet_get_data (packet));
    flow_packet_queue_drop_packet (packet_queue);
    packet = NULL;
  }

  /* If we found some data, pop it off the queue */

  if G_LIKELY (packet)
  {
    gint len = flow_packet_get_size (packet) - packet_offset;

    /* If the packet contains less data than requested, don't try to process
     * additional packets. */

    if (len <= max_len)
    {
      gpointer src_buffer = flow_packet_get_data (packet);

      memcpy (dest_buffer, (guint8 *) src_buffer + packet_offset, len);
      flow_packet_queue_drop_packet (packet_queue);
      result = len;
    }
    else
    {
      flow_packet_queue_pop_bytes_exact (packet_queue, dest_buffer, max_len);
      result = max_len;
    }
  }

  if (result > 0)
    successful_read (io, result);

  return result;
}

/**
 * try_read_object:
 * 
 * @io: A #FlowIO object.
 * @object_dest: Location to store the pointer to found object.
 * 
 * Try to read an object from the queue that we need to handle. Let subclasses
 * handle objects if possible. If we bump into data, stop and return a
 * %NULL object.
 * 
 * Return value: %TRUE if there was something in the queue, %FALSE otherwise.
 **/
static gboolean
try_read_object (FlowIO *io, gpointer *object_dest)
{
  FlowIOPrivate   *priv       = io->priv;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;
  gpointer         object     = NULL;
  gboolean         conclusive = FALSE;

  packet_queue = flow_user_adapter_get_input_queue (priv->user_adapter);

  while (flow_packet_queue_peek_packet (packet_queue, &packet, NULL))
  {
    /* If we find data, leave it alone and return a NULL object. */

    if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_OBJECT)
    {
      /* Unless we're dropping incoming data */

      if (io->drop_read_data)
      {
        flow_packet_queue_drop_packet (packet_queue);
        continue;
      }

      conclusive = TRUE;
      break;
    }

    object = flow_packet_get_data (packet);

    /* If we find an object not handled by subclasses, return that. */

    if (!handle_object (io, object))
    {
      conclusive = TRUE;
      g_object_ref (object);
      flow_packet_queue_drop_packet (packet_queue);
      break;
    }

    /* Object was handled by subclasses. Process next packet. */

    object = NULL;
    flow_packet_queue_drop_packet (packet_queue);
  }

  *object_dest = object;
  return conclusive;
}

static void
prepare_read (FlowIO *io, gint request_len)
{
  FlowIOClass *io_klass;

  io_klass = FLOW_IO_GET_CLASS (io);
  if (io_klass->prepare_read)
    io_klass->prepare_read (io, request_len);
}

static void
prepare_write (FlowIO *io, gint request_len)
{
  FlowIOClass *io_klass;

  io_klass = FLOW_IO_GET_CLASS (io);
  if (io_klass->prepare_write)
    io_klass->prepare_write (io, request_len);
}

/* This is called before every data write function actually writes data, to ensure
 * we'll send FLOW_STREAM_BEGIN first. However, we don't enforce it for objects, as
 * they may be needed to set parameters before the stream is opened. */
static void
ensure_downstream_open (FlowIO *io)
{
  FlowIOPrivate     *priv = io->priv;
  FlowDetailedEvent *detailed_event;
  FlowPacketQueue   *packet_queue;
  FlowPacket        *packet;

  if (priv->wrote_stream_begin)
    return;

  priv->wrote_stream_begin = TRUE;

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);

  packet_queue = flow_user_adapter_get_output_queue (priv->user_adapter);

  packet = flow_packet_new (FLOW_PACKET_FORMAT_OBJECT, detailed_event, 0);
  flow_packet_queue_push_packet (packet_queue, packet);

  g_object_unref (detailed_event);
}

/* Checks if the user wrote a stream begin/end event, so we know not to send ours
 * in ensure_downstream_open (). */
static void
check_downstream_state_change (FlowIO *io, gpointer object)
{
  FlowIOPrivate *priv = io->priv;

  /* Monitor events written by user to see if we need to write a FLOW_STREAM_BEGIN */

  if (FLOW_IS_DETAILED_EVENT (object))
  {
    if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
    {
      priv->wrote_stream_begin = TRUE;
    }
    else if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
    {
      priv->wrote_stream_begin = FALSE;
    }
  }
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

  /* Must check after invalid_bin test */
  g_return_val_if_fail (io->read_stream_is_open == TRUE, 0);

  set_minimum_read_buffer (io, 1);
  prepare_read (io, max_len);

  return try_read_data (io, dest_buffer, max_len);
}

gboolean
flow_io_read_exact (FlowIO *io, gpointer dest_buffer, gint exact_len)
{
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  g_return_val_if_fail (dest_buffer != NULL, FALSE);
  g_return_val_if_fail (exact_len >= 0, FALSE);
  return_val_if_invalid_bin (io, FALSE);

  /* Must check after invalid_bin test */
  g_return_val_if_fail (io->read_stream_is_open == TRUE, FALSE);

  priv = io->priv;

  set_minimum_read_buffer (io, exact_len);
  prepare_read (io, exact_len);

  packet_queue = flow_user_adapter_get_input_queue (priv->user_adapter);

  if (flow_packet_queue_get_length_data_bytes (packet_queue) < exact_len)
    return FALSE;

  while (!flow_packet_queue_pop_bytes_exact (packet_queue, dest_buffer, exact_len))
  {
    FlowPacket *packet;

    packet = flow_packet_queue_pop_first_object (packet_queue);
    g_assert (packet != NULL);

    handle_object (io, flow_packet_get_data (packet));
    flow_packet_unref (packet);
  }

  successful_read (io, exact_len);

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
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (src_buffer != NULL);
  g_return_if_fail (exact_len >= 0);
  return_if_invalid_bin (io);

  /* Must check after invalid_bin test */
  g_return_if_fail (io->write_stream_is_open == TRUE);

  priv = io->priv;

  ensure_downstream_open (io);
  prepare_write (io, exact_len);

  packet_queue = flow_user_adapter_get_output_queue (priv->user_adapter);

  flow_packet_queue_push_bytes (packet_queue, src_buffer, exact_len);
  flow_user_adapter_push (priv->user_adapter);
}

void
flow_io_write_object (FlowIO *io, gpointer object)
{
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (object != NULL);
  return_if_invalid_bin (io);

  priv = io->priv;

  prepare_write (io, 0);
  check_downstream_state_change (io, object);

  packet_queue = flow_user_adapter_get_output_queue (priv->user_adapter);

  packet = flow_packet_new (FLOW_PACKET_FORMAT_OBJECT, object, 0);
  flow_packet_queue_push_packet (packet_queue, packet);

  flow_user_adapter_push (priv->user_adapter);
}

void
flow_io_flush (FlowIO *io)
{
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  /* Must check after invalid_bin test */
  g_return_if_fail (io->write_stream_is_open == TRUE);

  priv = io->priv;

  ensure_downstream_open (io);

  packet_queue = flow_user_adapter_get_output_queue (priv->user_adapter);

  packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_FLUSH);
  flow_packet_queue_push_packet (packet_queue, packet);

  flow_user_adapter_push (priv->user_adapter);
}

void
flow_io_set_read_notify (FlowIO *io, FlowNotifyFunc func, gpointer user_data)
{
  FlowIOPrivate   *priv;

  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  priv = io->priv;

  priv->read_notify_func = func;
  priv->read_notify_data = user_data;

  update_user_adapter_blocks (io);
}

void
flow_io_set_write_notify (FlowIO *io, FlowNotifyFunc func, gpointer user_data)
{
  FlowIOPrivate   *priv;

  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  priv = io->priv;

  priv->write_notify_func = func;
  priv->write_notify_data = user_data;

  update_user_adapter_blocks (io);
}

void
flow_io_block_reads (FlowIO *io)
{
  FlowIOPrivate   *priv;

  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  priv = io->priv;

  if (priv->reads_are_blocked)
    return;

  priv->reads_are_blocked = TRUE;
  update_user_adapter_blocks (io);
}

void
flow_io_unblock_reads (FlowIO *io)
{
  FlowIOPrivate   *priv;

  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  priv = io->priv;

  if (!priv->reads_are_blocked)
    return;

  priv->reads_are_blocked = FALSE;
  update_user_adapter_blocks (io);
}

void
flow_io_block_writes (FlowIO *io)
{
  FlowIOPrivate   *priv;

  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  priv = io->priv;

  if (priv->writes_are_blocked)
    return;

  priv->writes_are_blocked = TRUE;
  update_user_adapter_blocks (io);
}

void
flow_io_unblock_writes (FlowIO *io)
{
  FlowIOPrivate   *priv;

  g_return_if_fail (FLOW_IS_IO (io));
  return_if_invalid_bin (io);

  priv = io->priv;

  if (!priv->writes_are_blocked)
    return;

  priv->writes_are_blocked = FALSE;
  update_user_adapter_blocks (io);
}

gint
flow_io_sync_read (FlowIO *io, gpointer dest_buffer, gint max_len, GError **error)
{
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;
  gint             result;

  g_return_val_if_fail (FLOW_IS_IO (io), 0);
  g_return_val_if_fail (dest_buffer != NULL, 0);
  g_return_val_if_fail (max_len >= 0, 0);
  return_val_if_invalid_bin (io, 0);

  /* Must check after invalid_bin test */
  g_return_val_if_fail (io->read_stream_is_open == TRUE, 0);

  priv = io->priv;

  g_assert (io->error == NULL);

  set_minimum_read_buffer (io, 1);
  prepare_read (io, max_len);

  packet_queue = flow_user_adapter_get_input_queue (priv->user_adapter);

  while (!(result = try_read_data (io, dest_buffer, max_len)) &&
         io->read_stream_is_open && !io->error)
  {
    flow_user_adapter_wait_for_input (priv->user_adapter);
  }

  g_assert (result > 0 || !io->read_stream_is_open || io->error);
  on_error_propagate_and_assert (result == 0);
  return result;
}

gboolean
flow_io_sync_read_exact (FlowIO *io, gpointer dest_buffer, gint exact_len, GError **error)
{
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;
  gboolean         result;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  g_return_val_if_fail (dest_buffer != NULL, FALSE);
  g_return_val_if_fail (exact_len >= 0, FALSE);
  return_val_if_invalid_bin (io, FALSE);

  /* Must check after invalid_bin test */
  g_return_val_if_fail (io->read_stream_is_open == TRUE, FALSE);

  priv = io->priv;

  g_assert (io->error == NULL);

  set_minimum_read_buffer (io, exact_len);
  prepare_read (io, exact_len);

  packet_queue = flow_user_adapter_get_input_queue (priv->user_adapter);

  while (!(result = flow_packet_queue_pop_bytes_exact (packet_queue, dest_buffer, exact_len)))
  {
    FlowPacket *packet;

    packet = flow_packet_queue_pop_first_object (packet_queue);
    if (packet)
    {
      handle_object (io, flow_packet_get_data (packet));
      flow_packet_unref (packet);
      continue;
    }

    if (!io->read_stream_is_open || io->error)
      break;

    flow_user_adapter_wait_for_input (priv->user_adapter);
  }

  if (result)
    successful_read (io, exact_len);

  g_assert (result || !io->read_stream_is_open || io->error);
  on_error_propagate_and_assert (!result);
  return result;
}

gpointer
flow_io_sync_read_object (FlowIO *io, GError **error)
{
  FlowIOPrivate   *priv;
  gpointer         object;

  g_return_val_if_fail (FLOW_IS_IO (io), NULL);
  return_val_if_invalid_bin (io, NULL);

  priv = io->priv;

  g_assert (io->error == NULL);

  set_minimum_read_buffer (io, 1);
  prepare_read (io, 0);

  while (!try_read_object (io, &object) && io->read_stream_is_open && !io->error)
  {
    flow_user_adapter_wait_for_input (priv->user_adapter);
  }

  g_assert (object || !io->read_stream_is_open || io->error);
  on_error_propagate_and_assert (!object);
  return object;
}

gboolean
flow_io_sync_write (FlowIO *io, gpointer src_buffer, gint exact_len, GError **error)
{
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;
  gboolean         result = FALSE;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  g_return_val_if_fail (src_buffer != NULL, FALSE);
  g_return_val_if_fail (exact_len >= 0, FALSE);
  return_val_if_invalid_bin (io, FALSE);

  /* Must check after invalid_bin test */
  g_return_val_if_fail (io->write_stream_is_open == TRUE, FALSE);

  priv = io->priv;

  g_assert (io->error == NULL);

  ensure_downstream_open (io);
  prepare_write (io, exact_len);

  packet_queue = flow_user_adapter_get_output_queue (priv->user_adapter);

  flow_packet_queue_push_bytes (packet_queue, src_buffer, exact_len);
  flow_user_adapter_push (priv->user_adapter);

  while (io->write_stream_is_open && !io->error)
  {
    if (!flow_packet_queue_get_length_packets (packet_queue))
    {
      result = TRUE;
      break;
    }

    flow_user_adapter_wait_for_output (priv->user_adapter);
  }

  g_assert (result || !io->write_stream_is_open || io->error);
  on_error_propagate_and_assert (!result);
  return result;
}

gboolean
flow_io_sync_write_object (FlowIO *io, gpointer object, GError **error)
{
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;
  gboolean         result = FALSE;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  g_return_val_if_fail (object != NULL, FALSE);
  return_val_if_invalid_bin (io, FALSE);

  priv = io->priv;

  g_assert (io->error == NULL);

  check_downstream_state_change (io, object);
  prepare_write (io, 0);

  packet_queue = flow_user_adapter_get_output_queue (priv->user_adapter);

  packet = flow_packet_new (FLOW_PACKET_FORMAT_OBJECT, object, 0);
  flow_packet_queue_push_packet (packet_queue, packet);

  flow_user_adapter_push (priv->user_adapter);

  while (io->write_stream_is_open && !io->error)
  {
    if (!flow_packet_queue_get_length_packets (packet_queue))
    {
      result = TRUE;
      break;
    }

    flow_user_adapter_wait_for_output (priv->user_adapter);
  }

  g_assert (result || !io->write_stream_is_open || io->error);
  on_error_propagate_and_assert (!result);
  return result;
}

gboolean
flow_io_sync_flush (FlowIO *io, GError **error)
{
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;
  gboolean         result = FALSE;

  g_return_val_if_fail (FLOW_IS_IO (io), FALSE);
  return_val_if_invalid_bin (io, FALSE);

  /* Must check after invalid_bin test */
  g_return_val_if_fail (io->write_stream_is_open == TRUE, FALSE);

  priv = io->priv;

  g_assert (io->error == NULL);

  ensure_downstream_open (io);
  prepare_write (io, 0);

  packet_queue = flow_user_adapter_get_output_queue (priv->user_adapter);

  packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_FLUSH);
  flow_packet_queue_push_packet (packet_queue, packet);

  flow_user_adapter_push (priv->user_adapter);

  while (io->write_stream_is_open && !io->error)
  {
    if (!flow_packet_queue_get_length_packets (packet_queue))
    {
      result = TRUE;
      break;
    }

    flow_user_adapter_wait_for_output (priv->user_adapter);
  }

  g_assert (result || !io->write_stream_is_open || io->error);
  on_error_propagate_and_assert (!result);
  return result;
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
  FlowIOPrivate   *priv;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  g_return_if_fail (FLOW_IS_IO (io));

  priv = io->priv;

  packet_queue = flow_user_adapter_get_input_queue (priv->user_adapter);

  while (flow_packet_queue_peek_packet (packet_queue, &packet, NULL))
  {
    gpointer object;

    if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_OBJECT)
    {
      if (io->drop_read_data)
      {
        flow_packet_queue_drop_packet (packet_queue);
        continue;
      }

      break;
    }

    object = flow_packet_get_data (packet);

    if (!handle_object (io, object))
      break;

    flow_packet_queue_drop_packet (packet_queue);
  }
}

FlowUserAdapter *
flow_io_get_user_adapter (FlowIO *io)
{
  g_return_val_if_fail (FLOW_IS_IO (io), NULL);

  return FLOW_USER_ADAPTER (flow_bin_get_element (FLOW_BIN (io), USER_ADAPTER_NAME));
}

void
flow_io_set_user_adapter (FlowIO *io, FlowUserAdapter *user_adapter)
{
  FlowElement *old_user_adapter;
  FlowBin     *bin;

  g_return_if_fail (FLOW_IS_IO (io));
  g_return_if_fail (FLOW_IS_USER_ADAPTER (user_adapter));

  bin = FLOW_BIN (io);

  old_user_adapter = flow_bin_get_element (bin, USER_ADAPTER_NAME);

  if ((FlowElement *) user_adapter == old_user_adapter)
    return;

  /* Changes to the bin will trigger an update of our internal pointers */

  if (old_user_adapter)
  {
    flow_replace_element (old_user_adapter, (FlowElement *) user_adapter);
    flow_bin_remove_element (bin, old_user_adapter);
  }

  flow_bin_add_element (bin, FLOW_ELEMENT (user_adapter), USER_ADAPTER_NAME);
}

GError *
flow_io_get_last_error (FlowIO *io)
{
  GError *error = NULL;

  g_return_val_if_fail (FLOW_IS_IO (io), NULL);

  if (io->error)
    error = g_error_copy (io->error);

  return error;
}
