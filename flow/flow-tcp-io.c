/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tcp-io.c - A prefab I/O class for TCP connections.
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
#include "flow-tcp-io.h"

#define USER_ADAPTER_NAME  "user-adapter"
#define TCP_CONNECTOR_NAME "tcp-connector"

#define return_if_invalid_bin(tcp_io) \
  G_STMT_START { \
    if G_UNLIKELY (((FlowIO *) tcp_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) tcp_io); \
\
    if G_UNLIKELY (!tcp_io->user_adapter || !tcp_io->tcp_connector) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter and a FlowTcpConnector."); \
      return; \
    } \
  } G_STMT_END

#define return_val_if_invalid_bin(tcp_io, val) \
  G_STMT_START { \
    if G_UNLIKELY (((FlowIO *) tcp_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) tcp_io); \
\
    if G_UNLIKELY (!tcp_io->user_adapter || !tcp_io->tcp_connector) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter and a FlowTcpConnector."); \
      return val; \
    } \
  } G_STMT_END

/* --- FlowTcpIO properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_tcp_io)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowTcpIO definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_tcp_io, FlowTcpIO, FLOW_TYPE_IO, 0)

/* --- FlowTcpIO implementation --- */

static void
remote_connectivity_changed (FlowTcpIO *tcp_io)
{
  FlowIO           *io = FLOW_IO (tcp_io);
  FlowConnectivity  before;
  FlowConnectivity  after;

  return_if_invalid_bin (tcp_io);

  before = flow_connector_get_last_state (FLOW_CONNECTOR (tcp_io->tcp_connector));
  after  = flow_connector_get_state      (FLOW_CONNECTOR (tcp_io->tcp_connector));

  if (after == FLOW_CONNECTIVITY_DISCONNECTED)
  {
    /* When the connection is closed, we may be in a blocking write call. We have to
     * interrupt it, or it might wait forever. Blocking reads are okay, since we'll get
     * the end-of-stream packet back through the read pipeline. */

    io->allow_blocking_write = FALSE;
    flow_user_adapter_interrupt_output (tcp_io->user_adapter);
  }
  else
  {
    io->allow_blocking_read  = TRUE;
    io->allow_blocking_write = TRUE;
  }
}

static void
flow_tcp_io_check_bin (FlowTcpIO *tcp_io)
{
  FlowBin *bin = FLOW_BIN (tcp_io);

  if (tcp_io->tcp_connector)
  {
    g_signal_handlers_disconnect_by_func (tcp_io->tcp_connector, remote_connectivity_changed, tcp_io);
  }

  flow_gobject_unref_clear (tcp_io->user_adapter);
  flow_gobject_unref_clear (tcp_io->tcp_connector);

  tcp_io->user_adapter  = (FlowUserAdapter *)  flow_bin_get_element (bin, USER_ADAPTER_NAME);
  tcp_io->tcp_connector = (FlowTcpConnector *) flow_bin_get_element (bin, TCP_CONNECTOR_NAME);

  if (tcp_io->user_adapter)
  {
    if (FLOW_IS_USER_ADAPTER (tcp_io->user_adapter))
      g_object_ref (tcp_io->user_adapter);
    else
      tcp_io->user_adapter = NULL;
  }

  if (tcp_io->tcp_connector)
  {
    if (FLOW_IS_TCP_CONNECTOR (tcp_io->tcp_connector))
    {
      g_object_ref (tcp_io->tcp_connector);
      g_signal_connect_swapped (tcp_io->tcp_connector, "connectivity-changed",
                                G_CALLBACK (remote_connectivity_changed), tcp_io);
      remote_connectivity_changed (tcp_io);
    }
    else
      tcp_io->tcp_connector = NULL;
  }
}

static gboolean
flow_tcp_io_handle_input_object (FlowTcpIO *tcp_io, gpointer object)
{
  if (FLOW_IS_DETAILED_EVENT (object))
  {
    FlowDetailedEvent *detailed_event = object;

    if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
    {
      FlowIO *io = FLOW_IO (tcp_io);

      g_assert (tcp_io->connectivity != FLOW_CONNECTIVITY_CONNECTED);

      tcp_io->connectivity = FLOW_CONNECTIVITY_CONNECTED;

      io->allow_blocking_read  = TRUE;
      io->allow_blocking_write = TRUE;

      /* FIXME: Issue signal */
    }
    else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
    {
      FlowIO *io = FLOW_IO (tcp_io);

      g_assert (tcp_io->connectivity != FLOW_CONNECTIVITY_DISCONNECTED);

      tcp_io->connectivity = FLOW_CONNECTIVITY_DISCONNECTED;

      io->allow_blocking_read  = FALSE;
      io->allow_blocking_write = FALSE;

      /* FIXME: Issue signal */
    }
  }

  return FALSE;
}

static void
flow_tcp_io_type_init (GType type)
{
}

static void
flow_tcp_io_class_init (FlowTcpIOClass *klass)
{
  FlowIOClass *io_klass = FLOW_IO_CLASS (klass);

  io_klass->check_bin           = (void (*) (FlowIO *)) flow_tcp_io_check_bin;
  io_klass->handle_input_object = (gboolean (*) (FlowIO *, gpointer)) flow_tcp_io_handle_input_object;
}

static void
flow_tcp_io_init (FlowTcpIO *tcp_io)
{
  FlowIO  *io  = FLOW_IO  (tcp_io);
  FlowBin *bin = FLOW_BIN (tcp_io);

  tcp_io->user_adapter = FLOW_USER_ADAPTER (flow_bin_get_element (bin, USER_ADAPTER_NAME));
  g_object_ref (tcp_io->user_adapter);

  tcp_io->tcp_connector = flow_tcp_connector_new ();
  flow_bin_add_element (bin, FLOW_ELEMENT (tcp_io->tcp_connector), TCP_CONNECTOR_NAME);

  flow_pad_connect (FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (tcp_io->tcp_connector))),
                    FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (tcp_io->user_adapter))));
  flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (tcp_io->tcp_connector))),
                    FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (tcp_io->user_adapter))));

  tcp_io->connectivity = FLOW_CONNECTIVITY_DISCONNECTED;

  io->allow_blocking_read  = FALSE;
  io->allow_blocking_write = FALSE;

  g_signal_connect_swapped (tcp_io->tcp_connector, "connectivity-changed",
                            G_CALLBACK (remote_connectivity_changed), tcp_io);
}

static void
flow_tcp_io_construct (FlowTcpIO *tcp_io)
{
}

static void
flow_tcp_io_dispose (FlowTcpIO *tcp_io)
{
  flow_gobject_unref_clear (tcp_io->user_adapter);
  flow_gobject_unref_clear (tcp_io->tcp_connector);
}

static void
flow_tcp_io_finalize (FlowTcpIO *tcp_io)
{
}

static void
write_stream_begin (FlowTcpIO *tcp_io)
{
  FlowDetailedEvent *detailed_event;

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);

  flow_io_write_object (FLOW_IO (tcp_io), detailed_event);

  g_object_unref (detailed_event);
}

static void
write_stream_end (FlowTcpIO *tcp_io)
{
  FlowDetailedEvent *detailed_event;

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);

  flow_io_write_object (FLOW_IO (tcp_io), detailed_event);

  g_object_unref (detailed_event);
}

/* --- FlowTcpIO public API --- */

FlowTcpIO *
flow_tcp_io_new (void)
{
  return g_object_new (FLOW_TYPE_TCP_IO, NULL);
}

void
flow_tcp_io_connect (FlowTcpIO *tcp_io, FlowIPService *ip_service)
{
  FlowIO *io;

  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));
  return_if_invalid_bin (tcp_io);
  g_return_if_fail (tcp_io->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);

  io = FLOW_IO (tcp_io);

  flow_io_write_object (io, ip_service);
  write_stream_begin (tcp_io);

  tcp_io->connectivity = FLOW_CONNECTIVITY_CONNECTING;
}

void
flow_tcp_io_connect_by_name (FlowTcpIO *tcp_io, const gchar *name, gint port)
{
  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  g_return_if_fail (name != NULL);
  g_return_if_fail (port > 0);
  return_if_invalid_bin (tcp_io);
  g_return_if_fail (tcp_io->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);


  /* TODO */
}

void
flow_tcp_io_disconnect (FlowTcpIO *tcp_io)
{
  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  return_if_invalid_bin (tcp_io);
  g_return_if_fail (tcp_io->connectivity != FLOW_CONNECTIVITY_DISCONNECTED);
  g_return_if_fail (tcp_io->connectivity != FLOW_CONNECTIVITY_DISCONNECTING);

  write_stream_end (tcp_io);

  tcp_io->connectivity = FLOW_CONNECTIVITY_DISCONNECTING;
}

gboolean
flow_tcp_io_sync_connect (FlowTcpIO *tcp_io, FlowIPService *ip_service)
{
  FlowIO *io;

  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), FALSE);
  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), FALSE);
  return_val_if_invalid_bin (tcp_io, FALSE);
  g_return_val_if_fail (tcp_io->connectivity == FLOW_CONNECTIVITY_DISCONNECTED, FALSE);

  io = FLOW_IO (tcp_io);

  flow_io_write_object (io, ip_service);
  write_stream_begin (tcp_io);

  tcp_io->connectivity = FLOW_CONNECTIVITY_CONNECTING;

  while (tcp_io->connectivity == FLOW_CONNECTIVITY_CONNECTING)
  {
    flow_user_adapter_wait_for_input (tcp_io->user_adapter);
  }

  return tcp_io->connectivity == FLOW_CONNECTIVITY_CONNECTED ? TRUE : FALSE;
}

gboolean
flow_tcp_io_sync_connect_by_name (FlowTcpIO *tcp_io, const gchar *name, gint port)
{
  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (port > 0, FALSE);
  return_val_if_invalid_bin (tcp_io, FALSE);
  g_return_val_if_fail (tcp_io->connectivity == FLOW_CONNECTIVITY_DISCONNECTED, FALSE);


  /* TODO */
}

void
flow_tcp_io_sync_disconnect (FlowTcpIO *tcp_io)
{
  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  return_if_invalid_bin (tcp_io);
  g_return_if_fail (tcp_io->connectivity != FLOW_CONNECTIVITY_DISCONNECTED);
  g_return_if_fail (tcp_io->connectivity != FLOW_CONNECTIVITY_DISCONNECTING);

  write_stream_end (tcp_io);

  tcp_io->connectivity = FLOW_CONNECTIVITY_DISCONNECTING;
  
  while (tcp_io->connectivity == FLOW_CONNECTIVITY_DISCONNECTING)
  {
    flow_user_adapter_wait_for_input (tcp_io->user_adapter);
  }

  g_assert (tcp_io->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);
}

FlowIPService *
flow_tcp_io_get_remote_service (FlowTcpIO *tcp_io)
{
  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), NULL);
  return_val_if_invalid_bin (tcp_io, NULL);

  if (!tcp_io->tcp_connector)
    return NULL;

  return flow_tcp_connector_get_remote_service (tcp_io->tcp_connector);
}

FlowConnectivity
flow_tcp_io_get_connectivity (FlowTcpIO *tcp_io)
{
  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), FLOW_CONNECTIVITY_DISCONNECTED);
  return_val_if_invalid_bin (tcp_io, FLOW_CONNECTIVITY_DISCONNECTED);

  return tcp_io->connectivity;
}
