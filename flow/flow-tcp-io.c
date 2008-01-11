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
#include "flow-element-util.h"
#include "flow-gobject-util.h"
#include "flow-event-codes.h"
#include "flow-tcp-connect-op.h"
#include "flow-tcp-io.h"

#define TCP_CONNECTOR_NAME "tcp-connector"
#define IP_PROCESSOR_NAME  "ip-processor"

#define return_if_invalid_bin(tcp_io) \
  G_STMT_START { \
    FlowTcpIOPrivate *priv = tcp_io->priv; \
\
    if G_UNLIKELY (((FlowIO *) tcp_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) tcp_io); \
\
    if G_UNLIKELY (!priv->user_adapter || !priv->tcp_connector) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter and a FlowTcpConnector."); \
      return; \
    } \
  } G_STMT_END

#define return_val_if_invalid_bin(tcp_io, val) \
  G_STMT_START { \
    FlowTcpIOPrivate *priv = tcp_io->priv; \
\
    if G_UNLIKELY (((FlowIO *) tcp_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) tcp_io); \
\
    if G_UNLIKELY (!priv->user_adapter || !priv->tcp_connector) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter and a FlowTcpConnector."); \
      return val; \
    } \
  } G_STMT_END

/* --- FlowTcpIO private data --- */

typedef struct
{
  FlowConnectivity  connectivity;
  FlowConnectivity  last_connectivity;

  FlowTcpConnector *tcp_connector;
  FlowUserAdapter  *user_adapter;
  FlowIPProcessor  *ip_processor;

  guint             wrote_stream_begin : 1;
  guint             name_resolution_id;
}
FlowTcpIOPrivate;

/* --- FlowTcpIO properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_tcp_io)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowTcpIO definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_tcp_io, FlowTcpIO, FLOW_TYPE_IO, 0)

/* --- FlowTcpIO implementation --- */

static void
write_stream_begin (FlowTcpIO *tcp_io)
{
  FlowTcpIOPrivate  *priv = tcp_io->priv;
  FlowDetailedEvent *detailed_event;

  g_assert (priv->wrote_stream_begin == FALSE);

  priv->wrote_stream_begin = TRUE;

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);

  flow_io_write_object (FLOW_IO (tcp_io), detailed_event);

  g_object_unref (detailed_event);
}

static void
write_stream_end (FlowTcpIO *tcp_io, gboolean close_both_directions)
{
  FlowTcpIOPrivate  *priv = tcp_io->priv;
  FlowDetailedEvent *detailed_event;

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);

  if (close_both_directions)
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END_CONVERSE);

  flow_io_write_object (FLOW_IO (tcp_io), detailed_event);

  priv->wrote_stream_begin = FALSE;

  g_object_unref (detailed_event);
}

static void
query_remote_connectivity (FlowTcpIO *tcp_io)
{
  FlowTcpIOPrivate *priv = tcp_io->priv;
  FlowIO           *io   = FLOW_IO (tcp_io);
  FlowConnectivity  before;
  FlowConnectivity  after;

  before = flow_connector_get_last_state (FLOW_CONNECTOR (priv->tcp_connector));
  after  = flow_connector_get_state      (FLOW_CONNECTOR (priv->tcp_connector));

  if (after == FLOW_CONNECTIVITY_DISCONNECTED)
  {
    /* When the connection is closed, we may be in a blocking write call. We have to
     * interrupt it, or it might wait forever. Blocking reads are okay, since we'll get
     * the end-of-stream packet back through the read pipeline. */

    io->write_stream_is_open = FALSE;
    flow_user_adapter_interrupt_output (priv->user_adapter);
  }
  else
  {
    io->read_stream_is_open  = TRUE;
    io->write_stream_is_open = TRUE;
  }
}

static void
remote_connectivity_changed (FlowTcpIO *tcp_io)
{
  return_if_invalid_bin (tcp_io);

  query_remote_connectivity (tcp_io);
}

static void
flow_tcp_io_check_bin (FlowTcpIO *tcp_io)
{
  FlowTcpIOPrivate *priv = tcp_io->priv;
  FlowBin          *bin  = FLOW_BIN (tcp_io);

  if (priv->tcp_connector)
  {
    g_signal_handlers_disconnect_by_func (priv->tcp_connector, remote_connectivity_changed, tcp_io);
  }

  flow_gobject_unref_clear (priv->user_adapter);
  flow_gobject_unref_clear (priv->tcp_connector);
  flow_gobject_unref_clear (priv->ip_processor);

  priv->user_adapter  = flow_io_get_user_adapter (FLOW_IO (tcp_io));
  priv->tcp_connector = (FlowTcpConnector *) flow_bin_get_element (bin, TCP_CONNECTOR_NAME);
  priv->ip_processor  = (FlowIPProcessor *) flow_bin_get_element (bin, IP_PROCESSOR_NAME);

  if (priv->user_adapter)
  {
    if (FLOW_IS_USER_ADAPTER (priv->user_adapter))
      g_object_ref (priv->user_adapter);
    else
      priv->user_adapter = NULL;
  }

  if (priv->tcp_connector)
  {
    if (FLOW_IS_TCP_CONNECTOR (priv->tcp_connector))
    {
      g_object_ref (priv->tcp_connector);
      g_signal_connect_swapped (priv->tcp_connector, "connectivity-changed",
                                G_CALLBACK (remote_connectivity_changed), tcp_io);
      query_remote_connectivity (tcp_io);
    }
    else
      priv->tcp_connector = NULL;
  }

  if (priv->ip_processor)
  {
    if (FLOW_IS_IP_PROCESSOR (priv->ip_processor))
      g_object_ref (priv->ip_processor);
    else
      priv->ip_processor = NULL;
  }
}

static void
set_connectivity (FlowTcpIO *tcp_io, FlowConnectivity new_connectivity)
{
  FlowTcpIOPrivate *priv = tcp_io->priv;

  if (new_connectivity == priv->connectivity)
    return;

  priv->last_connectivity = priv->connectivity;
  priv->connectivity = new_connectivity;

  g_signal_emit_by_name (tcp_io, "connectivity-changed");
}

static gboolean
flow_tcp_io_handle_input_object (FlowTcpIO *tcp_io, gpointer object)
{
  FlowTcpIOPrivate *priv   = tcp_io->priv;
  gboolean          result = FALSE;

  if (FLOW_IS_DETAILED_EVENT (object))
  {
    FlowDetailedEvent *detailed_event = object;

    if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
    {
      FlowIO *io = FLOW_IO (tcp_io);

      g_assert (priv->connectivity != FLOW_CONNECTIVITY_CONNECTED);

      io->read_stream_is_open  = TRUE;
      io->write_stream_is_open = TRUE;

      set_connectivity (tcp_io, FLOW_CONNECTIVITY_CONNECTED);

      result = TRUE;
    }
    else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
             flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
    {
      FlowIO *io = FLOW_IO (tcp_io);

      g_assert (priv->connectivity != FLOW_CONNECTIVITY_DISCONNECTED);

      io->read_stream_is_open  = FALSE;
      io->write_stream_is_open = FALSE;

      write_stream_end (tcp_io, FALSE);

      set_connectivity (tcp_io, FLOW_CONNECTIVITY_DISCONNECTED);

      result = TRUE;
    }
    else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_BEGIN) ||
             flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_SEGMENT_END))
    {
      result = TRUE;
    }
  }

  return result;
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
flow_tcp_io_init (FlowTcpIO *tcp_io)
{
  FlowTcpIOPrivate *priv = tcp_io->priv;
  FlowIO           *io   = FLOW_IO  (tcp_io);
  FlowBin          *bin  = FLOW_BIN (tcp_io);

  priv->user_adapter = flow_io_get_user_adapter (io);
  g_object_ref (priv->user_adapter);

  priv->tcp_connector = flow_tcp_connector_new ();
  flow_bin_add_element (bin, FLOW_ELEMENT (priv->tcp_connector), TCP_CONNECTOR_NAME);

  priv->ip_processor = flow_ip_processor_new ();
  flow_bin_add_element (bin, FLOW_ELEMENT (priv->ip_processor), IP_PROCESSOR_NAME);

  flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (priv->tcp_connector),
                                 FLOW_SIMPLEX_ELEMENT (priv->user_adapter));
  flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (priv->user_adapter),
                                 FLOW_SIMPLEX_ELEMENT (priv->ip_processor));
  flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (priv->ip_processor),
                                 FLOW_SIMPLEX_ELEMENT (priv->tcp_connector));

  g_signal_connect_swapped (priv->tcp_connector, "connectivity-changed",
                            G_CALLBACK (remote_connectivity_changed), tcp_io);

  io->read_stream_is_open  = FALSE;
  io->write_stream_is_open = FALSE;

  priv->connectivity      = FLOW_CONNECTIVITY_DISCONNECTED;
  priv->last_connectivity = FLOW_CONNECTIVITY_DISCONNECTED;
}

static void
flow_tcp_io_construct (FlowTcpIO *tcp_io)
{
}

static void
flow_tcp_io_dispose (FlowTcpIO *tcp_io)
{
  FlowTcpIOPrivate *priv = tcp_io->priv;

  flow_gobject_unref_clear (priv->user_adapter);
  flow_gobject_unref_clear (priv->tcp_connector);
  flow_gobject_unref_clear (priv->ip_processor);
}

static void
flow_tcp_io_finalize (FlowTcpIO *tcp_io)
{
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
  FlowTcpIOPrivate *priv;
  FlowTcpConnectOp *op;
  FlowIO           *io;

  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));
  return_if_invalid_bin (tcp_io);

  priv = tcp_io->priv;

  g_return_if_fail (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);

  io = FLOW_IO (tcp_io);

  op = flow_tcp_connect_op_new (ip_service, -1);
  flow_io_write_object (io, op);
  g_object_unref (op);

  write_stream_begin (tcp_io);
  set_connectivity (tcp_io, FLOW_CONNECTIVITY_CONNECTING);
}

void
flow_tcp_io_connect_by_name (FlowTcpIO *tcp_io, const gchar *name, gint port)
{
  FlowTcpIOPrivate *priv;
  FlowIPService    *ip_service;

  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  g_return_if_fail (name != NULL);
  g_return_if_fail (port > 0);
  return_if_invalid_bin (tcp_io);

  priv = tcp_io->priv;

  g_return_if_fail (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);

  ip_service = flow_ip_service_new ();
  flow_ip_service_set_name (ip_service, name);
  flow_ip_service_set_port (ip_service, port);

  flow_tcp_io_connect (tcp_io, ip_service);

  g_object_unref (ip_service);
}

void
flow_tcp_io_disconnect (FlowTcpIO *tcp_io, gboolean close_both_directions)
{
  FlowTcpIOPrivate *priv;

  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  return_if_invalid_bin (tcp_io);

  priv = tcp_io->priv;

  if (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED ||
      priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTING)
    return;

  write_stream_end (tcp_io, close_both_directions);

  set_connectivity (tcp_io, FLOW_CONNECTIVITY_DISCONNECTING);
}

gboolean
flow_tcp_io_sync_connect (FlowTcpIO *tcp_io, FlowIPService *ip_service)
{
  FlowTcpIOPrivate *priv;
  FlowTcpConnectOp *op;
  FlowIO           *io;

  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), FALSE);
  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), FALSE);
  return_val_if_invalid_bin (tcp_io, FALSE);

  priv = tcp_io->priv;

  g_return_val_if_fail (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED, FALSE);

  io = FLOW_IO (tcp_io);

  op = flow_tcp_connect_op_new (ip_service, -1);
  flow_io_write_object (io, op);
  g_object_unref (op);

  write_stream_begin (tcp_io);
  set_connectivity (tcp_io, FLOW_CONNECTIVITY_CONNECTING);

  while (priv->connectivity == FLOW_CONNECTIVITY_CONNECTING)
  {
    flow_user_adapter_wait_for_input (priv->user_adapter);
    flow_io_check_events (io);
  }

  return priv->connectivity == FLOW_CONNECTIVITY_CONNECTED ? TRUE : FALSE;
}

gboolean
flow_tcp_io_sync_connect_by_name (FlowTcpIO *tcp_io, const gchar *name, gint port)
{
  FlowTcpIOPrivate *priv;
  FlowIPService    *ip_service;
  gboolean          result;

  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (port > 0, FALSE);
  return_val_if_invalid_bin (tcp_io, FALSE);

  priv = tcp_io->priv;

  g_return_val_if_fail (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED, FALSE);

  ip_service = flow_ip_service_new ();
  flow_ip_service_set_name (ip_service, name);
  flow_ip_service_set_port (ip_service, port);

  result = flow_tcp_io_sync_connect (tcp_io, ip_service);

  g_object_unref (ip_service);
  return result;
}

void
flow_tcp_io_sync_disconnect (FlowTcpIO *tcp_io)
{
  FlowTcpIOPrivate *priv;
  FlowIO           *io;

  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  return_if_invalid_bin (tcp_io);

  priv = tcp_io->priv;

  if (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED)
    return;

  if (priv->connectivity != FLOW_CONNECTIVITY_DISCONNECTING)
  {
    write_stream_end (tcp_io, TRUE);
    set_connectivity (tcp_io, FLOW_CONNECTIVITY_DISCONNECTING);
  }

  io = FLOW_IO (tcp_io);

  while (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTING)
  {
    flow_user_adapter_wait_for_input (priv->user_adapter);
    flow_io_check_events (io);
  }

  g_assert (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);
}

FlowIPService *
flow_tcp_io_get_remote_service (FlowTcpIO *tcp_io)
{
  FlowTcpIOPrivate *priv;

  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), NULL);
  return_val_if_invalid_bin (tcp_io, NULL);

  priv = tcp_io->priv;

  if (!priv->tcp_connector)
    return NULL;

  return flow_tcp_connector_get_remote_service (priv->tcp_connector);
}

FlowConnectivity
flow_tcp_io_get_connectivity (FlowTcpIO *tcp_io)
{
  FlowTcpIOPrivate *priv;

  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), FLOW_CONNECTIVITY_DISCONNECTED);
  return_val_if_invalid_bin (tcp_io, FLOW_CONNECTIVITY_DISCONNECTED);

  priv = tcp_io->priv;

  return priv->connectivity;
}

FlowConnectivity
flow_tcp_io_get_last_connectivity (FlowTcpIO *tcp_io)
{
  FlowTcpIOPrivate *priv;

  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), FLOW_CONNECTIVITY_DISCONNECTED);
  return_val_if_invalid_bin (tcp_io, FLOW_CONNECTIVITY_DISCONNECTED);

  priv = tcp_io->priv;

  return priv->last_connectivity;
}

FlowTcpConnector *
flow_tcp_io_get_tcp_connector (FlowTcpIO *tcp_io)
{
  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), NULL);

  return FLOW_TCP_CONNECTOR (flow_bin_get_element (FLOW_BIN (tcp_io), TCP_CONNECTOR_NAME));
}

void
flow_tcp_io_set_tcp_connector (FlowTcpIO *tcp_io, FlowTcpConnector *tcp_connector)
{
  FlowElement *old_tcp_connector;
  FlowBin     *bin;

  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  g_return_if_fail (FLOW_IS_TCP_CONNECTOR (tcp_connector));

  bin = FLOW_BIN (tcp_io);

  old_tcp_connector = flow_bin_get_element (bin, TCP_CONNECTOR_NAME);

  if ((FlowElement *) tcp_connector == old_tcp_connector)
    return;

  /* Changes to the bin will trigger an update of our internal pointers */

  if (old_tcp_connector)
  {
    flow_replace_element (old_tcp_connector, (FlowElement *) tcp_connector);
    flow_bin_remove_element (bin, old_tcp_connector);
  }

  flow_bin_add_element (bin, FLOW_ELEMENT (tcp_connector), TCP_CONNECTOR_NAME);
}

FlowIPProcessor *
flow_tcp_io_get_ip_processor (FlowTcpIO *tcp_io)
{
  g_return_val_if_fail (FLOW_IS_TCP_IO (tcp_io), NULL);

  return FLOW_IP_PROCESSOR (flow_bin_get_element (FLOW_BIN (tcp_io), IP_PROCESSOR_NAME));
}

void
flow_tcp_io_set_ip_processor (FlowTcpIO *tcp_io, FlowIPProcessor *ip_processor)
{
  FlowElement *old_ip_processor;
  FlowBin     *bin;

  g_return_if_fail (FLOW_IS_TCP_IO (tcp_io));
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  bin = FLOW_BIN (tcp_io);

  old_ip_processor = flow_bin_get_element (bin, IP_PROCESSOR_NAME);

  if ((FlowElement *) ip_processor == old_ip_processor)
    return;

  /* Changes to the bin will trigger an update of our internal pointers */

  if (old_ip_processor)
  {
    flow_replace_element (old_ip_processor, (FlowElement *) ip_processor);
    flow_bin_remove_element (bin, old_ip_processor);
  }

  flow_bin_add_element (bin, FLOW_ELEMENT (ip_processor), IP_PROCESSOR_NAME);
}
