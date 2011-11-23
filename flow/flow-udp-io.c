/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-udp-io.c - A prefab I/O class for UDP communications.
 *
 * Copyright (C) 2008 Hans Petter Jansson
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
#include "flow-gerror-util.h"
#include "flow-event-codes.h"
#include "flow-udp-connect-op.h"
#include "flow-udp-io.h"

#define UDP_CONNECTOR_NAME "udp-connector"
#define IP_PROCESSOR_NAME  "ip-processor"

#define return_if_invalid_bin(udp_io) \
  G_STMT_START { \
    FlowUdpIOPrivate *priv = udp_io->priv; \
\
    if G_UNLIKELY (((FlowIO *) udp_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) udp_io); \
\
    if G_UNLIKELY (!priv->user_adapter || !priv->udp_connector) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter and a FlowUdpConnector."); \
      return; \
    } \
  } G_STMT_END

#define return_val_if_invalid_bin(udp_io, val) \
  G_STMT_START { \
    FlowUdpIOPrivate *priv = udp_io->priv; \
\
    if G_UNLIKELY (((FlowIO *) udp_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) udp_io); \
\
    if G_UNLIKELY (!priv->user_adapter || !priv->udp_connector) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowUserAdapter and a FlowUdpConnector."); \
      return val; \
    } \
  } G_STMT_END

#define on_error_propagate(x) \
  G_STMT_START { \
    if (io->error) \
    { \
      if (error) \
        *error = io->error; \
      else \
        g_error_free (io->error); \
\
      io->error = NULL; \
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

/* --- FlowUdpIO private data --- */

struct _FlowUdpIOPrivate
{
  FlowConnectivity  connectivity;
  FlowConnectivity  last_connectivity;

  FlowUdpConnector *udp_connector;
  FlowUserAdapter  *user_adapter;
  FlowIPProcessor  *ip_processor;

  FlowIPService    *remote_service;

  guint             wrote_stream_begin     : 1;
  guint             setting_local_service  : 1;
  guint             setting_remote_service : 1;
  guint             name_resolution_id;
};

/* --- FlowUdpIO properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_udp_io)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowUdpIO definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_udp_io, FlowUdpIO, FLOW_TYPE_IO, 0)

/* --- FlowUdpIO implementation --- */

static void
write_stream_begin (FlowUdpIO *udp_io)
{
  FlowUdpIOPrivate  *priv = udp_io->priv;
  FlowDetailedEvent *detailed_event;

  g_assert (priv->wrote_stream_begin == FALSE);

  priv->wrote_stream_begin = TRUE;

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN);

  flow_io_write_object (FLOW_IO (udp_io), detailed_event);

  g_object_unref (detailed_event);
}

static void
write_stream_end (FlowUdpIO *udp_io, gboolean close_both_directions)
{
  FlowUdpIOPrivate  *priv = udp_io->priv;
  FlowDetailedEvent *detailed_event;

  detailed_event = flow_detailed_event_new (NULL);
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);

  if (close_both_directions)
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END_CONVERSE);

  flow_io_write_object (FLOW_IO (udp_io), detailed_event);

  priv->wrote_stream_begin = FALSE;

  g_object_unref (detailed_event);
}

static void
query_remote_connectivity (FlowUdpIO *udp_io)
{
  FlowUdpIOPrivate *priv = udp_io->priv;
  FlowIO           *io   = FLOW_IO (udp_io);
  FlowConnectivity  before;
  FlowConnectivity  after;

  before = flow_connector_get_last_state (FLOW_CONNECTOR (priv->udp_connector));
  after  = flow_connector_get_state      (FLOW_CONNECTOR (priv->udp_connector));

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
remote_connectivity_changed (FlowUdpIO *udp_io)
{
  return_if_invalid_bin (udp_io);

  query_remote_connectivity (udp_io);
}

static void
flow_udp_io_check_bin (FlowUdpIO *udp_io)
{
  FlowUdpIOPrivate *priv = udp_io->priv;
  FlowBin          *bin  = FLOW_BIN (udp_io);

  if (priv->udp_connector)
  {
    g_signal_handlers_disconnect_by_func (priv->udp_connector, remote_connectivity_changed, udp_io);
  }

  flow_gobject_unref_clear (priv->user_adapter);
  flow_gobject_unref_clear (priv->udp_connector);
  flow_gobject_unref_clear (priv->ip_processor);

  priv->user_adapter  = flow_io_get_user_adapter (FLOW_IO (udp_io));
  priv->udp_connector = (FlowUdpConnector *) flow_bin_get_element (bin, UDP_CONNECTOR_NAME);
  priv->ip_processor  = (FlowIPProcessor *) flow_bin_get_element (bin, IP_PROCESSOR_NAME);

  if (priv->user_adapter)
  {
    if (FLOW_IS_USER_ADAPTER (priv->user_adapter))
      g_object_ref (priv->user_adapter);
    else
      priv->user_adapter = NULL;
  }

  if (priv->udp_connector)
  {
    if (FLOW_IS_UDP_CONNECTOR (priv->udp_connector))
    {
      g_object_ref (priv->udp_connector);
      g_signal_connect_swapped (priv->udp_connector, "connectivity-changed",
                                G_CALLBACK (remote_connectivity_changed), udp_io);
      query_remote_connectivity (udp_io);
    }
    else
      priv->udp_connector = NULL;
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
set_connectivity (FlowUdpIO *udp_io, FlowConnectivity new_connectivity)
{
  FlowUdpIOPrivate *priv = udp_io->priv;

  if (new_connectivity == priv->connectivity)
    return;

  priv->last_connectivity = priv->connectivity;
  priv->connectivity = new_connectivity;
}

static gboolean
check_for_errors (FlowUdpIO *udp_io, FlowDetailedEvent *detailed_event)
{
  FlowIO *io = FLOW_IO (udp_io);
  GError *error;

  error = flow_gerror_from_detailed_event (detailed_event, FLOW_SOCKET_DOMAIN,
                                           FLOW_SOCKET_ADDRESS_PROTECTED,
                                           FLOW_SOCKET_ADDRESS_IN_USE,
                                           FLOW_SOCKET_ADDRESS_DOES_NOT_EXIST,
                                           FLOW_SOCKET_CONNECTION_REFUSED,
                                           FLOW_SOCKET_CONNECTION_RESET,
                                           FLOW_SOCKET_NETWORK_UNREACHABLE,
                                           FLOW_SOCKET_ACCEPT_ERROR,
                                           FLOW_SOCKET_OVERSIZED_PACKET,
                                           -1);

  if (error)
  {
    g_clear_error (&io->error);
    io->error = error;
    return TRUE;
  }

  return FALSE;
}

static gboolean
flow_udp_io_handle_input_object (FlowUdpIO *udp_io, gpointer object)
{
  FlowUdpIOPrivate *priv   = udp_io->priv;
  gboolean          result = FALSE;

  if (FLOW_IS_DETAILED_EVENT (object))
  {
    FlowDetailedEvent *detailed_event = object;

    result = check_for_errors (udp_io, detailed_event);

    if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
    {
      FlowIO *io = FLOW_IO (udp_io);

      g_assert (priv->connectivity != FLOW_CONNECTIVITY_CONNECTED);

      io->read_stream_is_open  = TRUE;
      io->write_stream_is_open = TRUE;

      set_connectivity (udp_io, FLOW_CONNECTIVITY_CONNECTED);

      result = TRUE;
    }
    else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
             flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
    {
      FlowIO *io = FLOW_IO (udp_io);

      g_assert (priv->connectivity != FLOW_CONNECTIVITY_DISCONNECTED);

      io->read_stream_is_open  = FALSE;
      io->write_stream_is_open = FALSE;

      write_stream_end (udp_io, FALSE);

      set_connectivity (udp_io, FLOW_CONNECTIVITY_DISCONNECTED);

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
flow_udp_io_type_init (GType type)
{
}

static void
flow_udp_io_class_init (FlowUdpIOClass *klass)
{
  FlowIOClass *io_klass = FLOW_IO_CLASS (klass);

  io_klass->check_bin           = (void (*) (FlowIO *)) flow_udp_io_check_bin;
  io_klass->handle_input_object = (gboolean (*) (FlowIO *, gpointer)) flow_udp_io_handle_input_object;
}

static void
flow_udp_io_init (FlowUdpIO *udp_io)
{
  FlowUdpIOPrivate *priv = udp_io->priv;
  FlowIO           *io   = FLOW_IO  (udp_io);
  FlowBin          *bin  = FLOW_BIN (udp_io);

  priv->user_adapter = flow_io_get_user_adapter (io);
  g_object_ref (priv->user_adapter);

  priv->udp_connector = flow_udp_connector_new ();
  flow_bin_add_element (bin, FLOW_ELEMENT (priv->udp_connector), UDP_CONNECTOR_NAME);

  priv->ip_processor = flow_ip_processor_new ();
  flow_bin_add_element (bin, FLOW_ELEMENT (priv->ip_processor), IP_PROCESSOR_NAME);

  flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (priv->udp_connector),
                                 FLOW_SIMPLEX_ELEMENT (priv->user_adapter));
  flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (priv->user_adapter),
                                 FLOW_SIMPLEX_ELEMENT (priv->ip_processor));
  flow_connect_simplex__simplex (FLOW_SIMPLEX_ELEMENT (priv->ip_processor),
                                 FLOW_SIMPLEX_ELEMENT (priv->udp_connector));

  g_signal_connect_swapped (priv->udp_connector, "connectivity-changed",
                            G_CALLBACK (remote_connectivity_changed), udp_io);

  io->read_stream_is_open  = FALSE;
  io->write_stream_is_open = FALSE;

  priv->connectivity      = FLOW_CONNECTIVITY_DISCONNECTED;
  priv->last_connectivity = FLOW_CONNECTIVITY_DISCONNECTED;
}

static void
flow_udp_io_construct (FlowUdpIO *udp_io)
{
}

static void
flow_udp_io_dispose (FlowUdpIO *udp_io)
{
  FlowUdpIOPrivate *priv = udp_io->priv;

  flow_gobject_unref_clear (priv->user_adapter);
  flow_gobject_unref_clear (priv->udp_connector);
  flow_gobject_unref_clear (priv->ip_processor);
  flow_gobject_unref_clear (priv->remote_service);
}

static void
flow_udp_io_finalize (FlowUdpIO *udp_io)
{
}

/* --- FlowUdpIO public API --- */

FlowUdpIO *
flow_udp_io_new (void)
{
  return g_object_new (FLOW_TYPE_UDP_IO, NULL);
}

void
flow_udp_io_connect (FlowUdpIO *udp_io, FlowIPService *ip_service)
{
  FlowUdpIOPrivate *priv;
  FlowUdpConnectOp *op;
  FlowIO           *io;

  g_return_if_fail (FLOW_IS_UDP_IO (udp_io));
  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));
  return_if_invalid_bin (udp_io);

  priv = udp_io->priv;

  g_return_if_fail (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);

  io = FLOW_IO (udp_io);

  op = flow_udp_connect_op_new (ip_service, NULL);
  flow_io_write_object (io, op);
  g_object_unref (op);

  write_stream_begin (udp_io);
  set_connectivity (udp_io, FLOW_CONNECTIVITY_CONNECTING);
}

void
flow_udp_io_connect_by_name (FlowUdpIO *udp_io, const gchar *name, gint port)
{
  FlowUdpIOPrivate *priv;
  FlowIPService    *ip_service;

  g_return_if_fail (FLOW_IS_UDP_IO (udp_io));
  g_return_if_fail (name != NULL);
  g_return_if_fail (port > 0);
  return_if_invalid_bin (udp_io);

  priv = udp_io->priv;

  g_return_if_fail (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);

  ip_service = flow_ip_service_new ();
  flow_ip_service_set_name (ip_service, name);
  flow_ip_service_set_port (ip_service, port);

  flow_udp_io_connect (udp_io, ip_service);

  g_object_unref (ip_service);
}

void
flow_udp_io_disconnect (FlowUdpIO *udp_io, gboolean close_both_directions)
{
  FlowUdpIOPrivate *priv;

  g_return_if_fail (FLOW_IS_UDP_IO (udp_io));
  return_if_invalid_bin (udp_io);

  priv = udp_io->priv;

  if (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED ||
      priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTING)
    return;

  write_stream_end (udp_io, close_both_directions);

  set_connectivity (udp_io, FLOW_CONNECTIVITY_DISCONNECTING);
}

gboolean
flow_udp_io_sync_connect (FlowUdpIO *udp_io, FlowIPService *ip_service, GError **error)
{
  FlowUdpIOPrivate *priv;
  FlowUdpConnectOp *op;
  FlowIO           *io;

  g_return_val_if_fail (FLOW_IS_UDP_IO (udp_io), FALSE);
  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), FALSE);
  return_val_if_invalid_bin (udp_io, FALSE);

  priv = udp_io->priv;

  g_return_val_if_fail (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED, FALSE);

  io = FLOW_IO (udp_io);

  g_assert (io->error == NULL);

  op = flow_udp_connect_op_new (ip_service, NULL);
  flow_io_write_object (io, op);
  g_object_unref (op);

  write_stream_begin (udp_io);
  set_connectivity (udp_io, FLOW_CONNECTIVITY_CONNECTING);

  while (priv->connectivity == FLOW_CONNECTIVITY_CONNECTING)
  {
    flow_user_adapter_wait_for_input (priv->user_adapter);
    flow_io_check_events (io);
  }

  if (priv->connectivity == FLOW_CONNECTIVITY_CONNECTED)
  {
    g_assert (io->error == NULL);
    return TRUE;
  }

  g_assert (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);
  g_assert (io->error != NULL);
  on_error_propagate ();
  return FALSE;
}

gboolean
flow_udp_io_sync_disconnect (FlowUdpIO *udp_io, GError **error)
{
  FlowUdpIOPrivate *priv;
  FlowIO           *io;
  gboolean          result;

  g_return_val_if_fail (FLOW_IS_UDP_IO (udp_io), FALSE);
  return_val_if_invalid_bin (udp_io, FALSE);

  priv = udp_io->priv;

  if (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED)
    return TRUE;

  if (priv->connectivity != FLOW_CONNECTIVITY_DISCONNECTING)
  {
    write_stream_end (udp_io, TRUE);
    set_connectivity (udp_io, FLOW_CONNECTIVITY_DISCONNECTING);
  }

  io = FLOW_IO (udp_io);

  while (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTING)
  {
    flow_user_adapter_wait_for_input (priv->user_adapter);
    flow_io_check_events (io);
  }

  result = io->error ? FALSE : TRUE;

  /* We may have to report an error even though we got disconnected, if
   * the disconnect was unclean. */

  if (io->error)
  {
    result = FALSE;
  }
  else
  {
    g_assert (priv->connectivity == FLOW_CONNECTIVITY_DISCONNECTED);
    result = TRUE;
  }

  on_error_propagate ();
  return result;
}

FlowIPService *
flow_udp_io_get_remote_service (FlowUdpIO *udp_io)
{
  FlowUdpIOPrivate *priv;

  g_return_val_if_fail (FLOW_IS_UDP_IO (udp_io), NULL);
  return_val_if_invalid_bin (udp_io, NULL);

  priv = udp_io->priv;

  /* return_val_if_invalid_bin () takes care of this... */
  g_assert (priv->udp_connector != NULL);

  /* We can't use flow_udp_connector_get_remote_service (priv->udp_connector)
   * because we may have sent one that hasn't arrived at the UdpConnector
   * yet. */
  return priv->remote_service;
}

gboolean
flow_udp_io_set_remote_service (FlowUdpIO *udp_io, FlowIPService *ip_service, GError **error)
{
  FlowUdpIOPrivate *priv;

  g_return_val_if_fail (FLOW_IS_UDP_IO (udp_io), FALSE);
  return_val_if_invalid_bin (udp_io, FALSE);

  priv = udp_io->priv;

  if (!flow_ip_service_have_addresses (ip_service))
  {
    /* Do a sync lookup */
    if (!flow_ip_service_sync_resolve (ip_service, error))
      return FALSE;
  }

  priv->remote_service = g_object_ref (ip_service);
  flow_io_write_object (FLOW_IO (udp_io), ip_service);
  return TRUE;
}

gboolean
flow_udp_io_sync_set_remote_by_name (FlowUdpIO *udp_io, const gchar *name, gint port, GError **error)
{
  FlowUdpIOPrivate *priv;
  FlowIPService    *ip_service;
  gboolean          result;

  g_return_val_if_fail (FLOW_IS_UDP_IO (udp_io), FALSE);
  g_return_val_if_fail (name != NULL, FALSE);
  g_return_val_if_fail (port > 0, FALSE);
  return_val_if_invalid_bin (udp_io, FALSE);

  priv = udp_io->priv;

  ip_service = flow_ip_service_new ();
  flow_ip_service_set_name (ip_service, name);
  flow_ip_service_set_port (ip_service, port);

  result = flow_udp_io_sync_connect (udp_io, ip_service, error);

  g_object_unref (ip_service);
  return result;
}

FlowUdpConnector *
flow_udp_io_get_udp_connector (FlowUdpIO *udp_io)
{
  g_return_val_if_fail (FLOW_IS_UDP_IO (udp_io), NULL);

  return FLOW_UDP_CONNECTOR (flow_bin_get_element (FLOW_BIN (udp_io), UDP_CONNECTOR_NAME));
}

void
flow_udp_io_set_udp_connector (FlowUdpIO *udp_io, FlowUdpConnector *udp_connector)
{
  FlowElement *old_udp_connector;
  FlowBin     *bin;

  g_return_if_fail (FLOW_IS_UDP_IO (udp_io));
  g_return_if_fail (FLOW_IS_UDP_CONNECTOR (udp_connector));

  bin = FLOW_BIN (udp_io);

  old_udp_connector = flow_bin_get_element (bin, UDP_CONNECTOR_NAME);

  if ((FlowElement *) udp_connector == old_udp_connector)
    return;

  /* Changes to the bin will trigger an update of our internal pointers */

  if (old_udp_connector)
  {
    flow_replace_element (old_udp_connector, (FlowElement *) udp_connector);
    flow_bin_remove_element (bin, old_udp_connector);
  }

  flow_bin_add_element (bin, FLOW_ELEMENT (udp_connector), UDP_CONNECTOR_NAME);
}

FlowIPProcessor *
flow_udp_io_get_ip_processor (FlowUdpIO *udp_io)
{
  g_return_val_if_fail (FLOW_IS_UDP_IO (udp_io), NULL);

  return FLOW_IP_PROCESSOR (flow_bin_get_element (FLOW_BIN (udp_io), IP_PROCESSOR_NAME));
}

void
flow_udp_io_set_ip_processor (FlowUdpIO *udp_io, FlowIPProcessor *ip_processor)
{
  FlowElement *old_ip_processor;
  FlowBin     *bin;

  g_return_if_fail (FLOW_IS_UDP_IO (udp_io));
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  bin = FLOW_BIN (udp_io);

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
