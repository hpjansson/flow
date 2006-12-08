/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tls-tcp-io-listener.h - Connection listener that spawns FlowTlsTcpIOs.
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

#ifndef _FLOW_TLS_TCP_IO_LISTENER_H
#define _FLOW_TLS_TCP_IO_LISTENER_H

#include <glib-object.h>
#include <flow-tcp-io-listener.h>
#include <flow-tls-tcp-io.h>

G_BEGIN_DECLS

#define FLOW_TYPE_TLS_TCP_IO_LISTENER            (flow_tls_tcp_io_listener_get_type ())
#define FLOW_TLS_TCP_IO_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_TLS_TCP_IO_LISTENER, FlowTlsTcpIOListener))
#define FLOW_TLS_TCP_IO_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_TLS_TCP_IO_LISTENER, FlowTlsTcpIOListenerClass))
#define FLOW_IS_TLS_TCP_IO_LISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_TLS_TCP_IO_LISTENER))
#define FLOW_IS_TLS_TCP_IO_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_TLS_TCP_IO_LISTENER))
#define FLOW_TLS_TCP_IO_LISTENER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_TLS_TCP_IO_LISTENER, FlowTlsTcpIOListenerClass))
GType   flow_tls_tcp_io_listener_get_type        (void) G_GNUC_CONST;

typedef struct _FlowTlsTcpIOListener      FlowTlsTcpIOListener;
typedef struct _FlowTlsTcpIOListenerClass FlowTlsTcpIOListenerClass;

struct _FlowTlsTcpIOListener
{
  FlowTcpIOListener  parent;

  /* --- Private --- */

  gpointer           priv;
};

struct _FlowTlsTcpIOListenerClass
{
  FlowTcpIOListenerClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowTlsTcpIOListener  *flow_tls_tcp_io_listener_new                 (void);

FlowTlsTcpIO          *flow_tls_tcp_io_listener_pop_connection      (FlowTlsTcpIOListener *tls_tcp_io_listener);
FlowTlsTcpIO          *flow_tls_tcp_io_listener_sync_pop_connection (FlowTlsTcpIOListener *tls_tcp_io_listener);

G_END_DECLS

#endif  /* _FLOW_TLS_TCP_IO_LISTENER_H */
