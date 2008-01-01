/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-file-io.c - A prefab I/O class for local file connections.
 *
 * Copyright (C) 2007 Hans Petter Jansson
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
#include "flow-position.h"
#include "flow-file-connect-op.h"
#include "flow-segment-request.h"
#include "flow-file-io.h"

#define FILE_CONNECTOR_NAME "file-connector"

#define return_if_invalid_bin(file_io) \
  G_STMT_START { \
    FlowFileIOPrivate *priv = file_io->priv; \
\
    if G_UNLIKELY (((FlowIO *) file_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) file_io); \
\
    if G_UNLIKELY (!priv->user_adapter || !priv->file_connector) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter and a FlowTcpConnector."); \
      return; \
    } \
  } G_STMT_END

#define return_val_if_invalid_bin(file_io, val) \
  G_STMT_START { \
    FlowFileIOPrivate *priv = file_io->priv; \
\
    if G_UNLIKELY (((FlowIO *) file_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) file_io); \
\
    if G_UNLIKELY (!priv->user_adapter || !priv->file_connector) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter and a FlowTcpConnector."); \
      return val; \
    } \
  } G_STMT_END

/* --- FlowFileIO private data --- */

typedef struct
{
  FlowConnectivity   connectivity;
  FlowConnectivity   last_connectivity;

  FlowFileConnector *file_connector;
  FlowUserAdapter   *user_adapter;

  guint              wrote_stream_begin : 1;

  /* We get back one segment per request; if the user does a non-blocking read
   * request, then seeks or writes before the data arrives, we have to discard the
   * resulting segment. This may happen repeatedly, resulting in multiple outstanding 
   * segments to be discarded. These variables refer to data that's "in the pipeline",
   * i.e. requested from the backend but not read by the user yet. */

  guint              n_segments_to_drop;
  guint              n_segments_requested;
  guint              n_bytes_requested;

  /* The offset at which the next operation should take place, i.e. where the
   * user thinks we are. In reality, we might have outstanding behind-the-scenes
   * read requests that put us at a different offset, and we may need to
   * compensate for these when issuing a write. */

  FlowOffsetAnchor   user_anchor;
  goffset            user_offset;
}
FlowFileIOPrivate;

/* --- FlowFileIO properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_file_io)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowFileIO definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_file_io, FlowFileIO, FLOW_TYPE_IO, 0)

/* --- FlowFileIO implementation --- */

static void
write_stream_begin (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv = file_io->priv;
  FlowDetailedEvent *detailed_event;

  g_assert (priv->wrote_stream_begin == FALSE);

  priv->wrote_stream_begin = TRUE;

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);
  flow_io_write_object (FLOW_IO (file_io), detailed_event);
  g_object_unref (detailed_event);
}

static void
write_stream_end (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv = file_io->priv;
  FlowDetailedEvent *detailed_event;

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
  flow_io_write_object (FLOW_IO (file_io), detailed_event);
  g_object_unref (detailed_event);

  priv->wrote_stream_begin = FALSE;
}

static void
query_remote_connectivity (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv = file_io->priv;
  FlowIO            *io   = FLOW_IO (file_io);
  FlowConnectivity   before;
  FlowConnectivity   after;

  before = flow_connector_get_last_state (FLOW_CONNECTOR (priv->file_connector));
  after  = flow_connector_get_state      (FLOW_CONNECTOR (priv->file_connector));

  if (after == FLOW_CONNECTIVITY_DISCONNECTED)
  {
    /* When the connection is closed, we may be in a blocking write call. We have to
     * interrupt it, or it might wait forever. Blocking reads are okay, since we'll get
     * the end-of-stream packet back through the read pipeline. */

    io->allow_blocking_write = FALSE;
    flow_user_adapter_interrupt_output (priv->user_adapter);
  }
  else
  {
    io->allow_blocking_read  = TRUE;
    io->allow_blocking_write = TRUE;
  }
}

static void
remote_connectivity_changed (FlowFileIO *file_io)
{
  return_if_invalid_bin (file_io);

  query_remote_connectivity (file_io);
}

static void
flow_file_io_check_bin (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv = file_io->priv;
  FlowBin           *bin  = FLOW_BIN (file_io);

  if (priv->file_connector)
  {
    g_signal_handlers_disconnect_by_func (priv->file_connector, remote_connectivity_changed, file_io);
  }

  flow_gobject_unref_clear (priv->user_adapter);
  flow_gobject_unref_clear (priv->file_connector);

  priv->user_adapter  = flow_io_get_user_adapter (FLOW_IO (file_io));
  priv->file_connector = (FlowFileConnector *) flow_bin_get_element (bin, FILE_CONNECTOR_NAME);

  if (priv->user_adapter)
  {
    if (FLOW_IS_USER_ADAPTER (priv->user_adapter))
      g_object_ref (priv->user_adapter);
    else
      priv->user_adapter = NULL;
  }

  if (priv->file_connector)
  {
    if (FLOW_IS_FILE_CONNECTOR (priv->file_connector))
    {
      g_object_ref (priv->file_connector);
      g_signal_connect_swapped (priv->file_connector, "connectivity-changed",
                                G_CALLBACK (remote_connectivity_changed), file_io);
      query_remote_connectivity (file_io);
    }
    else
      priv->file_connector = NULL;
  }
}

static void
set_connectivity (FlowFileIO *file_io, FlowConnectivity new_connectivity)
{
  FlowFileIOPrivate *priv = file_io->priv;

  if (new_connectivity == priv->connectivity)
    return;

  priv->last_connectivity = priv->connectivity;
  priv->connectivity = new_connectivity;

  g_signal_emit_by_name (file_io, "connectivity-changed");
}

static gboolean
flow_file_io_handle_input_object (FlowFileIO *file_io, gpointer object)
{
  FlowFileIOPrivate *priv   = file_io->priv;
  gboolean           result = FALSE;

  if (FLOW_IS_DETAILED_EVENT (object))
  {
    FlowDetailedEvent *detailed_event = object;

    if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
    {
      FlowIO *io = FLOW_IO (file_io);

      g_assert (priv->connectivity != FLOW_CONNECTIVITY_CONNECTED);

      io->allow_blocking_read  = TRUE;
      io->allow_blocking_write = TRUE;

      set_connectivity (file_io, FLOW_CONNECTIVITY_CONNECTED);

      result = TRUE;
    }
    else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
             flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
    {
      FlowIO *io = FLOW_IO (file_io);

      g_assert (priv->connectivity != FLOW_CONNECTIVITY_DISCONNECTED);

      io->allow_blocking_read  = FALSE;
      io->allow_blocking_write = FALSE;

      set_connectivity (file_io, FLOW_CONNECTIVITY_DISCONNECTED);

      result = TRUE;
    }
    else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_BEGIN))
    {
      result = TRUE;
    }
    else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_END))
    {
      if (priv->n_segments_to_drop > 0)
        priv->n_segments_to_drop--;

      result = TRUE;
    }
  }
  else if (FLOW_IS_POSITION (object))
  {
    result = TRUE;
  }

  return result;
}

static void
update_user_offset (FlowFileIO *file_io, FlowOffsetAnchor anchor, goffset offset)
{
  FlowFileIOPrivate *priv = file_io->priv;

  if (anchor == FLOW_OFFSET_ANCHOR_BEGIN)
  {
    priv->user_anchor = FLOW_OFFSET_ANCHOR_BEGIN;
    priv->user_offset = offset;
  }
  else if (anchor == FLOW_OFFSET_ANCHOR_END)
  {
    priv->user_anchor = FLOW_OFFSET_ANCHOR_END;
    priv->user_offset = offset;
  }
  else if (anchor == FLOW_OFFSET_ANCHOR_CURRENT)
  {
    priv->user_offset += offset;
  }
  else
  {
    g_assert_not_reached ();
  }
}

static void
flow_file_io_prepare_read (FlowFileIO *file_io, gint request_len)
{
  FlowFileIOPrivate  *priv = file_io->priv;
  FlowSegmentRequest *segment_request;
  gint                n_bytes_to_request;

  if (priv->n_bytes_requested >= request_len)
    return;

  /* FIXME: Maybe we have to send a FLOW_STREAM_BEGIN event first?
   * See flow-io.c:ensure_downstream_open(). */

  n_bytes_to_request = request_len - priv->n_bytes_requested;

  segment_request = flow_segment_request_new (n_bytes_to_request);
  flow_io_write_object (FLOW_IO (file_io), segment_request);
  g_object_unref (segment_request);

  priv->n_bytes_requested = request_len;
  priv->n_segments_requested++;
}

static void
flow_file_io_successful_read (FlowFileIO *file_io, gint len)
{
  FlowFileIOPrivate *priv = file_io->priv;

  g_assert (priv->n_bytes_requested >= len);

  priv->n_bytes_requested -= len;
  priv->user_offset += len;
}

static void
drop_read_requests (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv = file_io->priv;

  priv->n_segments_to_drop += priv->n_segments_requested;
  priv->n_segments_requested = 0;
  priv->n_bytes_requested = 0;
}

static void
flow_file_io_prepare_write (FlowFileIO *file_io, gint request_len)
{
  FlowFileIOPrivate *priv = file_io->priv;

  if (request_len == 0)
    return;

  if (priv->n_bytes_requested > 0)
  {
    FlowPosition *position;

    /* Compensate for requested but not received reads */

    position = flow_position_new (priv->user_anchor, priv->user_offset);
    flow_io_write_object (FLOW_IO (file_io), position);
    g_object_unref (position);
  }

  drop_read_requests (file_io);
  priv->user_offset += request_len;

  /* If we wrote past end-of-file, we are now at end-of-file */

  if (priv->user_anchor == FLOW_OFFSET_ANCHOR_END && priv->user_offset > 0)
    priv->user_offset = 0;
}

static void
flow_file_io_type_init (GType type)
{
}

static void
flow_file_io_class_init (FlowFileIOClass *klass)
{
  FlowIOClass *io_klass = FLOW_IO_CLASS (klass);

  io_klass->check_bin           = (void (*) (FlowIO *)) flow_file_io_check_bin;
  io_klass->handle_input_object = (gboolean (*) (FlowIO *, gpointer)) flow_file_io_handle_input_object;
  io_klass->prepare_read        = (void (*) (FlowIO *io, gint request_len)) flow_file_io_prepare_read;
  io_klass->successful_read     = (void (*) (FlowIO *io, gint len)) flow_file_io_successful_read;
  io_klass->prepare_write       = (void (*) (FlowIO *io, gint request_len)) flow_file_io_prepare_write;

  g_signal_newv ("connectivity-changed",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 NULL,                                   /* Class closure */
                 NULL, NULL,                             /* Accumulator, accu data */
                 g_cclosure_marshal_VOID__VOID,          /* Marshaller */
                 G_TYPE_NONE,                            /* Return type */
                 0, NULL);                               /* Number of params, param types */
}

static void
flow_file_io_init (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv = file_io->priv;
  FlowIO            *io   = FLOW_IO  (file_io);
  FlowBin           *bin  = FLOW_BIN (file_io);

  priv->user_adapter = flow_io_get_user_adapter (io);
  g_object_ref (priv->user_adapter);

  priv->file_connector = flow_file_connector_new ();
  flow_bin_add_element (bin, FLOW_ELEMENT (priv->file_connector), FILE_CONNECTOR_NAME);

  flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (priv->file_connector),
                                 FLOW_SIMPLEX_ELEMENT (priv->user_adapter));
  flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (priv->user_adapter),
                                 FLOW_SIMPLEX_ELEMENT (priv->file_connector));

  g_signal_connect_swapped (priv->file_connector, "connectivity-changed",
                            G_CALLBACK (remote_connectivity_changed), file_io);

  io->allow_blocking_read  = FALSE;
  io->allow_blocking_write = FALSE;

  priv->connectivity      = FLOW_CONNECTIVITY_DISCONNECTED;
  priv->last_connectivity = FLOW_CONNECTIVITY_DISCONNECTED;

  priv->user_anchor = FLOW_OFFSET_ANCHOR_BEGIN;
  priv->user_offset = 0;
}

static void
flow_file_io_construct (FlowFileIO *file_io)
{
}

static void
flow_file_io_dispose (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv = file_io->priv;

  flow_gobject_unref_clear (priv->user_adapter);
  flow_gobject_unref_clear (priv->file_connector);
}

static void
flow_file_io_finalize (FlowFileIO *file_io)
{
}

/* --- FlowFileIO public API --- */

FlowFileIO *
flow_file_io_new (void)
{
  return g_object_new (FLOW_TYPE_FILE_IO, NULL);
}

void
flow_file_io_open (FlowFileIO *file_io, const gchar *path, FlowAccessMode access_mode)
{
  FlowFileIOPrivate *priv;
  FlowIO            *io;
  FlowFileConnectOp *file_connect_op;

  g_return_if_fail (FLOW_IS_FILE_IO (file_io));
  return_if_invalid_bin (file_io);

  priv = file_io->priv;
  io = FLOW_IO (file_io);

  g_return_if_fail (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);

  file_connect_op = flow_file_connect_op_new (path, access_mode);
  flow_io_write_object (io, file_connect_op);
  g_object_unref (file_connect_op);

  write_stream_begin (file_io);

  set_connectivity (file_io, FLOW_CONNECTIVITY_CONNECTING);
}

void
flow_file_io_close (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv;

  g_return_if_fail (FLOW_IS_FILE_IO (file_io));
  return_if_invalid_bin (file_io);

  priv = file_io->priv;

  if (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED ||
      priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTING)
    return;

  write_stream_end (file_io);

  set_connectivity (file_io, FLOW_CONNECTIVITY_DISCONNECTING);
}

void
flow_file_io_seek (FlowFileIO *file_io, FlowOffsetAnchor anchor, goffset offset)
{
  FlowFileIOPrivate *priv;
  FlowIO            *io;
  FlowPosition      *position;

  g_return_if_fail (FLOW_IS_FILE_IO (file_io));
  return_if_invalid_bin (file_io);

  priv = file_io->priv;
  io = FLOW_IO (file_io);

  g_return_if_fail (priv->connectivity == FLOW_CONNECTIVITY_CONNECTED || priv->connectivity == FLOW_CONNECTIVITY_CONNECTING);

  drop_read_requests (file_io);
  update_user_offset (file_io, anchor, offset);

  position = flow_position_new (anchor, offset);
  flow_io_write_object (io, position);
  g_object_unref (position);
}

void
flow_file_io_seek_to (FlowFileIO *file_io, goffset offset)
{
  FlowFileIOPrivate *priv;
  FlowIO            *io;
  FlowPosition      *position;

  g_return_if_fail (FLOW_IS_FILE_IO (file_io));
  return_if_invalid_bin (file_io);

  priv = file_io->priv;
  io = FLOW_IO (file_io);

  g_return_if_fail (priv->connectivity == FLOW_CONNECTIVITY_CONNECTED || priv->connectivity == FLOW_CONNECTIVITY_CONNECTING);

  drop_read_requests (file_io);
  update_user_offset (file_io, FLOW_OFFSET_ANCHOR_BEGIN, offset);

  position = flow_position_new (FLOW_OFFSET_ANCHOR_BEGIN, offset);
  flow_io_write_object (io, position);
  g_object_unref (position);
}

gboolean
flow_file_io_sync_open (FlowFileIO *file_io, const gchar *path, FlowAccessMode access_mode)
{
  FlowFileIOPrivate *priv;
  FlowIO            *io;
  FlowFileConnectOp *file_connect_op;

  g_return_val_if_fail (FLOW_IS_FILE_IO (file_io), FALSE);
  return_val_if_invalid_bin (file_io, FALSE);

  priv = file_io->priv;
  io = FLOW_IO (file_io);

  g_return_val_if_fail (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED, FALSE);

  file_connect_op = flow_file_connect_op_new (path, access_mode);
  flow_io_write_object (io, file_connect_op);
  g_object_unref (file_connect_op);

  write_stream_begin (file_io);

  set_connectivity (file_io, FLOW_CONNECTIVITY_CONNECTING);

  while (priv->connectivity == FLOW_CONNECTIVITY_CONNECTING)
  {
    flow_user_adapter_wait_for_input (priv->user_adapter);
    flow_io_check_events (io);
  }

  return priv->connectivity == FLOW_CONNECTIVITY_CONNECTED ? TRUE : FALSE;
}

void
flow_file_io_sync_close (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv;
  FlowIO            *io;

  g_return_if_fail (FLOW_IS_FILE_IO (file_io));
  return_if_invalid_bin (file_io);

  priv = file_io->priv;
  io = FLOW_IO (file_io);

  if (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED)
    return;

  if (priv->connectivity != FLOW_CONNECTIVITY_DISCONNECTING)
  {
    write_stream_end (file_io);
    set_connectivity (file_io, FLOW_CONNECTIVITY_DISCONNECTING);
  }

  while (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTING)
  {
    flow_user_adapter_wait_for_input (priv->user_adapter);
    flow_io_check_events (io);
  }

  g_assert (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);
}

gchar *
flow_file_io_get_path (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv;

  g_return_val_if_fail (FLOW_IS_FILE_IO (file_io), NULL);
  return_val_if_invalid_bin (file_io, NULL);

  priv = file_io->priv;

  if (!priv->file_connector)
    return NULL;

  return flow_file_connector_get_path (priv->file_connector);
}

FlowConnectivity
flow_file_io_get_connectivity (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv;

  g_return_val_if_fail (FLOW_IS_FILE_IO (file_io), FLOW_CONNECTIVITY_DISCONNECTED);
  return_val_if_invalid_bin (file_io, FLOW_CONNECTIVITY_DISCONNECTED);

  priv = file_io->priv;

  return priv->connectivity;
}

FlowConnectivity
flow_file_io_get_last_connectivity (FlowFileIO *file_io)
{
  FlowFileIOPrivate *priv;

  g_return_val_if_fail (FLOW_IS_FILE_IO (file_io), FLOW_CONNECTIVITY_DISCONNECTED);
  return_val_if_invalid_bin (file_io, FLOW_CONNECTIVITY_DISCONNECTED);

  priv = file_io->priv;

  return priv->last_connectivity;
}

FlowFileConnector *
flow_file_io_get_file_connector (FlowFileIO *file_io)
{
  g_return_val_if_fail (FLOW_IS_FILE_IO (file_io), NULL);

  return FLOW_FILE_CONNECTOR (flow_bin_get_element (FLOW_BIN (file_io), FILE_CONNECTOR_NAME));
}

void
flow_file_io_set_file_connector (FlowFileIO *file_io, FlowFileConnector *file_connector)
{
  FlowElement *old_file_connector;
  FlowBin     *bin;

  g_return_if_fail (FLOW_IS_FILE_IO (file_io));
  g_return_if_fail (FLOW_IS_FILE_CONNECTOR (file_connector));

  bin = FLOW_BIN (file_io);

  old_file_connector = flow_bin_get_element (bin, FILE_CONNECTOR_NAME);

  if ((FlowElement *) file_connector == old_file_connector)
    return;

  /* Changes to the bin will trigger an update of our internal pointers */

  if (old_file_connector)
  {
    flow_replace_element (old_file_connector, (FlowElement *) file_connector);
    flow_bin_remove_element (bin, old_file_connector);
  }

  flow_bin_add_element (bin, FLOW_ELEMENT (file_connector), FILE_CONNECTOR_NAME);
}
