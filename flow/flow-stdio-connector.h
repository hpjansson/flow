/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-stdio-connector.h - Origin/endpoint for stdio.
 *
 * Copyright (C) 2012 Hans Petter Jansson
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

#ifndef _FLOW_STDIO_CONNECTOR_H
#define _FLOW_STDIO_CONNECTOR_H

#include <glib-object.h>
#include <flow-connector.h>

G_BEGIN_DECLS

#define FLOW_TYPE_STDIO_CONNECTOR            (flow_stdio_connector_get_type ())
#define FLOW_STDIO_CONNECTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_STDIO_CONNECTOR, FlowStdioConnector))
#define FLOW_STDIO_CONNECTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_STDIO_CONNECTOR, FlowStdioConnectorClass))
#define FLOW_IS_STDIO_CONNECTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_STDIO_CONNECTOR))
#define FLOW_IS_STDIO_CONNECTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_STDIO_CONNECTOR))
#define FLOW_STDIO_CONNECTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_STDIO_CONNECTOR, FlowStdioConnectorClass))
GType   flow_stdio_connector_get_type        (void) G_GNUC_CONST;

typedef struct _FlowStdioConnector        FlowStdioConnector;
typedef struct _FlowStdioConnectorPrivate FlowStdioConnectorPrivate;
typedef struct _FlowStdioConnectorClass   FlowStdioConnectorClass;

struct _FlowStdioConnector
{
  FlowConnector    parent;

  /*< private >*/

  FlowStdioConnectorPrivate *priv;
};

struct _FlowStdioConnectorClass
{
  FlowConnectorClass parent_class;

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowStdioConnector *flow_stdio_connector_new      (void);

G_END_DECLS

#endif  /* _FLOW_STDIO_CONNECTOR_H */
