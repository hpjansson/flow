/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tls-tcp-io.h - A prefab I/O class for TCP connections with TLS support.
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

#ifndef _FLOW_TLS_TCP_IO_H
#define _FLOW_TLS_TCP_IO_H

#include <flow/flow-tcp-io.h>
#include <flow/flow-tls-protocol.h>

G_BEGIN_DECLS

#define FLOW_TYPE_TLS_TCP_IO            (flow_tls_tcp_io_get_type ())
#define FLOW_TLS_TCP_IO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_TLS_TCP_IO, FlowTlsTcpIO))
#define FLOW_TLS_TCP_IO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_TLS_TCP_IO, FlowTlsTcpIOClass))
#define FLOW_IS_TLS_TCP_IO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_TLS_TCP_IO))
#define FLOW_IS_TLS_TCP_IO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_TLS_TCP_IO))
#define FLOW_TLS_TCP_IO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_TLS_TCP_IO, FlowTlsTcpIOClass))
GType   flow_tls_tcp_io_get_type        (void) G_GNUC_CONST;

typedef struct _FlowTlsTcpIO      FlowTlsTcpIO;
typedef struct _FlowTlsTcpIOClass FlowTlsTcpIOClass;

struct _FlowTlsTcpIO
{
  FlowTcpIO         parent;

  /* --- Private --- */

  gpointer          priv;
};

struct _FlowTlsTcpIOClass
{
  FlowTcpIOClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowTlsTcpIO        *flow_tls_tcp_io_new              (void);

FlowTlsProtocol     *flow_tls_tcp_io_get_tls_protocol (FlowTlsTcpIO *tls_tcp_io);
void                 flow_tls_tcp_io_set_tls_protocol (FlowTlsTcpIO *tls_tcp_io, FlowTlsProtocol *tls_protocol);

G_END_DECLS

#endif  /* _FLOW_TLS_TCP_IO_H */
