/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-service.h - An Internet address and port number.
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

#ifndef _FLOW_IP_SERVICE_H
#define _FLOW_IP_SERVICE_H

#include <glib-object.h>
#include <flow/flow-ip-addr.h>

typedef enum
{
  FLOW_QUALITY_UNSPECIFIED,
  FLOW_QUALITY_LOW_COST,
  FLOW_QUALITY_HIGH_RELIABILITY,
  FLOW_QUALITY_HIGH_THROUGHPUT,
  FLOW_QUALITY_LOW_LATENCY
}
FlowQuality;

#define FLOW_TYPE_IP_SERVICE            (flow_ip_service_get_type ())
#define FLOW_IP_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_IP_SERVICE, FlowIPService))
#define FLOW_IP_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_IP_SERVICE, FlowIPServiceClass))
#define FLOW_IS_IP_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_IP_SERVICE))
#define FLOW_IS_IP_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_IP_SERVICE))
#define FLOW_IP_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_IP_SERVICE, FlowIPServiceClass))
GType   flow_ip_service_get_type        (void) G_GNUC_CONST;

typedef struct _FlowIPService      FlowIPService;
typedef struct _FlowIPServiceClass FlowIPServiceClass;

struct _FlowIPService
{
  GObject     parent;

  /* --- Private --- */

  gpointer    priv;
};

struct _FlowIPServiceClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowIPService       *flow_ip_service_new              (void);

gboolean             flow_ip_service_have_name        (FlowIPService *ip_service);

gchar               *flow_ip_service_get_name         (FlowIPService *ip_service);
void                 flow_ip_service_set_name         (FlowIPService *ip_service, const gchar *name);

gint                 flow_ip_service_get_port         (FlowIPService *ip_service);
void                 flow_ip_service_set_port         (FlowIPService *ip_service, gint port);

FlowQuality          flow_ip_service_get_quality      (FlowIPService *ip_service);
void                 flow_ip_service_set_quality      (FlowIPService *ip_service, FlowQuality quality);

gint                 flow_ip_service_get_n_addresses  (FlowIPService *ip_service);
FlowIPAddr          *flow_ip_service_get_nth_address  (FlowIPService *ip_service, gint n);
FlowIPAddr          *flow_ip_service_find_address     (FlowIPService *ip_service, FlowIPAddrFamily family);
GList               *flow_ip_service_list_addresses   (FlowIPService *ip_service);
void                 flow_ip_service_add_address      (FlowIPService *ip_service, FlowIPAddr *ip_addr);
void                 flow_ip_service_remove_address   (FlowIPService *ip_service, FlowIPAddr *ip_addr);

void                 flow_ip_service_resolve          (FlowIPService *ip_service);
gboolean             flow_ip_service_sync_resolve     (FlowIPService *ip_service);

#endif /* _FLOW_IP_SERVICE_H */
