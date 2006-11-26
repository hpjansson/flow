/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-service.c - An Internet address and port number.
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include "flow-gobject-util.h"
#include "flow-ip-service.h"

/* --- FlowIPService private data --- */

typedef struct
{
  gint        port;
  FlowQuality quality;
}
FlowIPServicePrivate;

/* --- FlowIPService properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ip_service)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowIPService definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_ip_service, FlowIPService, FLOW_TYPE_IP_ADDR, 0)

/* --- FlowIPService implementation --- */

static void
flow_ip_service_type_init (GType type)
{
}

static void
flow_ip_service_class_init (FlowIPServiceClass *klass)
{
}

static void
flow_ip_service_init (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv = ip_service->priv;

  priv->quality = FLOW_QUALITY_UNSPECIFIED;
}

static void
flow_ip_service_construct (FlowIPService *ip_service)
{
}

static void
flow_ip_service_dispose (FlowIPService *ip_service)
{
}

static void
flow_ip_service_finalize (FlowIPService *ip_service)
{
}

FlowIPService *
flow_ip_service_new (void)
{
  return g_object_new (FLOW_TYPE_IP_SERVICE, NULL);
}

gint
flow_ip_service_get_port (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv;

  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), 0);

  priv = ip_service->priv;

  return priv->port;
}

void
flow_ip_service_set_port (FlowIPService *ip_service, gint port)
{
  FlowIPServicePrivate *priv;

  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));
  g_return_if_fail (port >= 0 && port <= 65535);

  priv = ip_service->priv;

  priv->port = port;
}

FlowQuality
flow_ip_service_get_quality (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv;

  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), FLOW_QUALITY_UNSPECIFIED);

  priv = ip_service->priv;

  return priv->quality;
}

void
flow_ip_service_set_quality (FlowIPService *ip_service, FlowQuality quality)
{
  FlowIPServicePrivate *priv;

  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));

  priv = ip_service->priv;

  priv->quality = quality;
}
