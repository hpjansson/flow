/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tcp-listener.h - Connection listener that spawns FlowTcpConnectors.
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

#ifndef _FLOW_TCP_LISTENER_H
#define _FLOW_TCP_LISTENER_H

#include <glib-object.h>
#include <flow-tcp-connector.h>
#include <flow-ip-service.h>
#include <flow-shunt.h>

G_BEGIN_DECLS

#define FLOW_TYPE_TCP_LISTENER            (flow_tcp_listener_get_type ())
#define FLOW_TCP_LISTENER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_TCP_LISTENER, FlowTcpListener))
#define FLOW_TCP_LISTENER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_TCP_LISTENER, FlowTcpListenerClass))
#define FLOW_IS_TCP_LISTENER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_TCP_LISTENER))
#define FLOW_IS_TCP_LISTENER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_TCP_LISTENER))
#define FLOW_TCP_LISTENER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_TCP_LISTENER, FlowTcpListenerClass))
GType   flow_tcp_listener_get_type        (void) G_GNUC_CONST;

typedef struct _FlowTcpListener        FlowTcpListener;
typedef struct _FlowTcpListenerPrivate FlowTcpListenerPrivate;
typedef struct _FlowTcpListenerClass   FlowTcpListenerClass;

struct _FlowTcpListener
{
  GObject          parent;

  /*< private >*/

  FlowTcpListenerPrivate *priv;
};

struct _FlowTcpListenerClass
{
  GObjectClass parent_class;

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowTcpListener  *flow_tcp_listener_new                 (void);

FlowIPService    *flow_tcp_listener_get_local_service   (FlowTcpListener *tcp_listener);
gboolean          flow_tcp_listener_set_local_service   (FlowTcpListener *tcp_listener, FlowIPService *ip_service,
                                                         FlowDetailedEvent **error_event);

FlowTcpConnector *flow_tcp_listener_pop_connection      (FlowTcpListener *tcp_listener);
FlowTcpConnector *flow_tcp_listener_sync_pop_connection (FlowTcpListener *tcp_listener);

G_END_DECLS

#endif  /* _FLOW_TCP_LISTENER_H */
