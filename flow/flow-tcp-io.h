/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tcp-io.h - A prefab I/O class for TCP connections.
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

#ifndef _FLOW_TCP_IO_H
#define _FLOW_TCP_IO_H

#include <flow/flow-detailed-event.h>
#include <flow/flow-io.h>
#include <flow/flow-user-adapter.h>
#include <flow/flow-tcp-connector.h>

G_BEGIN_DECLS

#define FLOW_TYPE_TCP_IO            (flow_tcp_io_get_type ())
#define FLOW_TCP_IO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_TCP_IO, FlowTcpIO))
#define FLOW_TCP_IO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_TCP_IO, FlowTcpIOClass))
#define FLOW_IS_TCP_IO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_TCP_IO))
#define FLOW_IS_TCP_IO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_TCP_IO))
#define FLOW_TCP_IO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_TCP_IO, FlowTcpIOClass))
GType   flow_tcp_io_get_type        (void) G_GNUC_CONST;

typedef struct _FlowTcpIO      FlowTcpIO;
typedef struct _FlowTcpIOClass FlowTcpIOClass;

struct _FlowTcpIO
{
  FlowIO            parent;

  /* --- Private --- */

  FlowConnectivity  connectivity;
  FlowConnectivity  last_connectivity;

  FlowTcpConnector *tcp_connector;
  FlowUserAdapter  *user_adapter;

  guint             name_resolution_id;
};

struct _FlowTcpIOClass
{
  FlowIOClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowTcpIO        *flow_tcp_io_new                   (void);

void              flow_tcp_io_connect               (FlowTcpIO *tcp_io, FlowIPService *ip_service);
void              flow_tcp_io_connect_by_name       (FlowTcpIO *tcp_io, const gchar *name, gint port);
void              flow_tcp_io_disconnect            (FlowTcpIO *tcp_io, gboolean close_both_directions);

gboolean          flow_tcp_io_sync_connect          (FlowTcpIO *tcp_io, FlowIPService *ip_service);
gboolean          flow_tcp_io_sync_connect_by_name  (FlowTcpIO *tcp_io, const gchar *name, gint port);
void              flow_tcp_io_sync_disconnect       (FlowTcpIO *tcp_io);

FlowIPService    *flow_tcp_io_get_remote_service    (FlowTcpIO *tcp_io);
FlowConnectivity  flow_tcp_io_get_connectivity      (FlowTcpIO *tcp_io);
FlowConnectivity  flow_tcp_io_get_last_connectivity (FlowTcpIO *tcp_io);

G_END_DECLS

#endif  /* _FLOW_TCP_IO_H */
