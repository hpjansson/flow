/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-connector.h - Traffic origin/endpoint which may be disconnected.
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

#ifndef _FLOW_CONNECTOR_H
#define _FLOW_CONNECTOR_H

#include <glib-object.h>
#include <flow-element.h>

G_BEGIN_DECLS

typedef enum
{
  FLOW_CONNECTIVITY_CONNECTED,
  FLOW_CONNECTIVITY_CONNECTING,
  FLOW_CONNECTIVITY_DISCONNECTED,
  FLOW_CONNECTIVITY_DISCONNECTING
}
FlowConnectivity;

#define FLOW_TYPE_CONNECTOR            (flow_connector_get_type ())
#define FLOW_CONNECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_CONNECTOR, FlowConnector))
#define FLOW_CONNECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_CONNECTOR, FlowConnectorClass))
#define FLOW_IS_CONNECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_CONNECTOR))
#define FLOW_IS_CONNECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_CONNECTOR))
#define FLOW_CONNECTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_CONNECTOR, FlowConnectorClass))
GType   flow_connector_get_type        (void) G_GNUC_CONST;

typedef struct _FlowConnector      FlowConnector;
typedef struct _FlowConnectorClass FlowConnectorClass;

struct _FlowConnector
{
  FlowElement        parent;

  FlowConnectivity   state;
  FlowConnectivity   last_state;
};

struct _FlowConnectorClass
{
  FlowElementClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

G_END_DECLS

FlowConnectivity   flow_connector_get_state          (FlowConnector *connector);
FlowConnectivity   flow_connector_get_last_state     (FlowConnector *connector);

/* For FlowConnector implementations only */

void               flow_connector_set_state_internal (FlowConnector *connector, FlowConnectivity state);

#endif  /* _FLOW_CONNECTOR_H */
