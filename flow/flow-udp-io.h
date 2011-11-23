/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-udp-io.h - A prefab I/O class for UDP communications.
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

#ifndef _FLOW_UDP_IO_H
#define _FLOW_UDP_IO_H

#include <flow/flow-detailed-event.h>
#include <flow/flow-io.h>
#include <flow/flow-udp-connector.h>
#include <flow/flow-ip-processor.h>

G_BEGIN_DECLS

#define FLOW_TYPE_UDP_IO            (flow_udp_io_get_type ())
#define FLOW_UDP_IO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_UDP_IO, FlowUdpIO))
#define FLOW_UDP_IO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_UDP_IO, FlowUdpIOClass))
#define FLOW_IS_UDP_IO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_UDP_IO))
#define FLOW_IS_UDP_IO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_UDP_IO))
#define FLOW_UDP_IO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_UDP_IO, FlowUdpIOClass))
GType   flow_udp_io_get_type        (void) G_GNUC_CONST;

typedef struct _FlowUdpIO        FlowUdpIO;
typedef struct _FlowUdpIOPrivate FlowUdpIOPrivate;
typedef struct _FlowUdpIOClass   FlowUdpIOClass;

struct _FlowUdpIO
{
  FlowIO            parent;

  /*< private >*/

  FlowUdpIOPrivate *priv;
};

struct _FlowUdpIOClass
{
  FlowIOClass parent_class;

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowUdpIO        *flow_udp_io_new                     (void);

FlowIPService    *flow_udp_io_get_local_service       (FlowUdpIO *udp_io);
gboolean          flow_udp_io_set_local_service       (FlowUdpIO *udp_io, FlowIPService *ip_service,
                                                       GError **error);

FlowIPService    *flow_udp_io_get_remote_service      (FlowUdpIO *udp_io);
gboolean          flow_udp_io_set_remote_service      (FlowUdpIO *udp_io, FlowIPService *ip_service,
                                                       GError **error);
gboolean          flow_udp_io_sync_set_remote_by_name (FlowUdpIO *udp_io, const gchar *name, gint port,
                                                       GError **error);

FlowUdpConnector *flow_udp_io_get_udp_connector       (FlowUdpIO *io);
void              flow_udp_io_set_udp_connector       (FlowUdpIO *io, FlowUdpConnector *udp_connector);

FlowIPProcessor  *flow_udp_io_get_ip_processor        (FlowUdpIO *io);
void              flow_udp_io_set_ip_processor        (FlowUdpIO *io, FlowIPProcessor *ip_processor);

G_END_DECLS

#endif  /* _FLOW_UDP_IO_H */
