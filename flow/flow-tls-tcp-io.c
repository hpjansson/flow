/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tls-tcp-io.c - A prefab I/O class for TCP connections with TLS support.
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
#include "flow-tls-tcp-io.h"

#define TLS_PROTOCOL_NAME "tls-protocol"

#define return_if_invalid_bin(tls_tcp_io) \
  G_STMT_START { \
    FlowTlsTcpIOPrivate *priv = tls_tcp_io->priv; \
\
    if G_UNLIKELY (((FlowIO *) tls_tcp_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) tls_tcp_io); \
\
    if G_UNLIKELY (!priv->tls_protocol) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowTlsProtocol."); \
      return; \
    } \
  } G_STMT_END

#define return_val_if_invalid_bin(tls_tcp_io, val) \
  G_STMT_START { \
    FlowTlsTcpIOPrivate *priv = tls_tcp_io->priv; \
\
    if G_UNLIKELY (((FlowIO *) tls_tcp_io)->need_to_check_bin) \
      flow_io_check_bin ((FlowIO *) tls_tcp_io); \
\
    if G_UNLIKELY (!priv->tls_protocol) \
    { \
      g_warning (G_STRLOC ": Misconfigured bin! Need a FlowTlsProtocol."); \
      return val; \
    } \
  } G_STMT_END

/* --- FlowTlsTcpIO private data --- */

typedef struct
{
  FlowTlsProtocol *tls_protocol;
}
FlowTlsTcpIOPrivate;

/* --- FlowTlsTcpIO properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_tls_tcp_io)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowTlsTcpIO definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_tls_tcp_io, FlowTlsTcpIO, FLOW_TYPE_TCP_IO, 0)

/* --- FlowTlsTcpIO implementation --- */

static void
install_tls_protocol (FlowTlsTcpIO *tls_tcp_io, FlowTlsProtocol *tls_protocol)
{
  FlowTlsTcpIOPrivate *priv = tls_tcp_io->priv;

  flow_gobject_unref_clear (priv->tls_protocol);

  if (!tls_protocol)
    return;

  if (!FLOW_IS_TLS_PROTOCOL (tls_protocol))
    return;

  priv->tls_protocol = g_object_ref (tls_protocol);
}

static void
flow_tls_tcp_io_check_bin (FlowTlsTcpIO *tls_tcp_io)
{
  FlowBin             *bin  = FLOW_BIN (tls_tcp_io);
  FlowTlsProtocol     *tls_protocol;

  tls_protocol = (FlowTlsProtocol *) flow_bin_get_element (bin, TLS_PROTOCOL_NAME);
  install_tls_protocol (tls_tcp_io, tls_protocol);
}

static gboolean
flow_tls_tcp_io_handle_input_object (FlowTlsTcpIO *tls_tcp_io, gpointer object)
{
  FlowTlsTcpIOPrivate *priv   = tls_tcp_io->priv;
  gboolean             result = FALSE;

  if (FLOW_IS_DETAILED_EVENT (object))
  {
    FlowDetailedEvent *detailed_event = object;

    /* We've got nothing to do here. Yet. */
  }

  return result;
}

static void
flow_tls_tcp_io_type_init (GType type)
{
}

static void
flow_tls_tcp_io_class_init (FlowTlsTcpIOClass *klass)
{
  FlowIOClass *io_klass = FLOW_IO_CLASS (klass);

  io_klass->check_bin           = (void (*) (FlowIO *)) flow_tls_tcp_io_check_bin;
  io_klass->handle_input_object = (gboolean (*) (FlowIO *, gpointer)) flow_tls_tcp_io_handle_input_object;
}

static void
flow_tls_tcp_io_init (FlowTlsTcpIO *tls_tcp_io)
{
  FlowTlsTcpIOPrivate *priv = tls_tcp_io->priv;
  FlowSimplexElement  *tcp_connector;
  FlowSimplexElement  *user_adapter;

  priv->tls_protocol = flow_tls_protocol_new (FLOW_AGENT_ROLE_CLIENT);
  flow_bin_add_element (FLOW_BIN (tls_tcp_io), FLOW_ELEMENT (priv->tls_protocol), TLS_PROTOCOL_NAME);

  tcp_connector = FLOW_SIMPLEX_ELEMENT (flow_tcp_io_get_tcp_connector (FLOW_TCP_IO (tls_tcp_io)));
  user_adapter  = FLOW_SIMPLEX_ELEMENT (flow_io_get_user_adapter      (FLOW_IO     (tls_tcp_io)));

  flow_insert_simplex_simplex__Iduplex (tcp_connector, tcp_connector, FLOW_DUPLEX_ELEMENT (priv->tls_protocol));
}

static void
flow_tls_tcp_io_construct (FlowTlsTcpIO *tls_tcp_io)
{
}

static void
flow_tls_tcp_io_dispose (FlowTlsTcpIO *tls_tcp_io)
{
  FlowTlsTcpIOPrivate *priv = tls_tcp_io->priv;

  flow_gobject_unref_clear (priv->tls_protocol);
}

static void
flow_tls_tcp_io_finalize (FlowTlsTcpIO *tls_tcp_io)
{
}

/* --- FlowTlsTcpIO public API --- */

FlowTlsTcpIO *
flow_tls_tcp_io_new (void)
{
  return g_object_new (FLOW_TYPE_TLS_TCP_IO, NULL);
}

FlowTlsProtocol *
flow_tls_tcp_io_get_tls_protocol (FlowTlsTcpIO *tls_tcp_io)
{
  g_return_val_if_fail (FLOW_IS_TLS_TCP_IO (tls_tcp_io), NULL);

  return FLOW_TLS_PROTOCOL (flow_bin_get_element (FLOW_BIN (tls_tcp_io), TLS_PROTOCOL_NAME));
}

void
flow_tls_tcp_io_set_tls_protocol (FlowTlsTcpIO *tls_tcp_io, FlowTlsProtocol *tls_protocol)
{
  FlowElement *old_tls_protocol;
  FlowBin     *bin;

  g_return_if_fail (FLOW_IS_TLS_TCP_IO (tls_tcp_io));
  g_return_if_fail (FLOW_IS_TLS_PROTOCOL (tls_protocol));

  bin = FLOW_BIN (tls_tcp_io);

  old_tls_protocol = flow_bin_get_element (bin, TLS_PROTOCOL_NAME);

  if ((FlowElement *) tls_protocol == old_tls_protocol)
    return;

  /* Changes to the bin will trigger an update of our internal pointers */

  if (old_tls_protocol)
  {
    flow_replace_element (old_tls_protocol, (FlowElement *) tls_protocol);
    flow_bin_remove_element (bin, old_tls_protocol);
  }

  flow_bin_add_element (bin, FLOW_ELEMENT (tls_protocol), TLS_PROTOCOL_NAME);
}
