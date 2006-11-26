/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-connector.c - Traffic origin/endpoint which may be disconnected.
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

#include "flow-gobject-util.h"
#include "flow-connector.h"

/* --- FlowConnector private data --- */

typedef struct
{
  FlowConnectivity   state;
  FlowConnectivity   last_state;
}
FlowConnectorPrivate;

/* --- FlowConnector properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_connector)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowConnector definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_connector, FlowConnector, FLOW_TYPE_SIMPLEX_ELEMENT, G_TYPE_FLAG_ABSTRACT)

/* --- FlowConnector implementation --- */

static void
flow_connector_type_init (GType type)
{
}

static void
flow_connector_class_init (FlowConnectorClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  /* The FlowSimplexElement default implementation just maps
   * input to output. That doesn't make sense for us. */

  element_klass->output_pad_blocked   = NULL;
  element_klass->output_pad_unblocked = NULL;
  element_klass->process_input        = NULL;

  g_signal_newv ("connectivity-changed",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 NULL,                                   /* Class closure */
                 NULL, NULL,                             /* Accumulator, accu data */
                 g_cclosure_marshal_VOID__VOID,          /* Marshaller */
                 G_TYPE_NONE,                            /* Return type */
                 0, NULL);                               /* Number of params, param types */
}

static void
flow_connector_init (FlowConnector *connector)
{
  FlowConnectorPrivate *priv = connector->priv;

  priv->state      = FLOW_CONNECTIVITY_DISCONNECTED;
  priv->last_state = FLOW_CONNECTIVITY_DISCONNECTED;
}

static void
flow_connector_construct (FlowConnector *connector)
{
}

static void
flow_connector_dispose (FlowConnector *connector)
{
}

static void
flow_connector_finalize (FlowConnector *connector)
{
}

/* --- FlowConnector public API --- */

FlowConnectivity
flow_connector_get_state (FlowConnector *connector)
{
  FlowConnectorPrivate *priv;

  g_return_val_if_fail (FLOW_IS_CONNECTOR (connector), FLOW_CONNECTIVITY_DISCONNECTED);

  priv = connector->priv;

  return priv->state;
}

FlowConnectivity
flow_connector_get_last_state (FlowConnector *connector)
{
  FlowConnectorPrivate *priv;

  g_return_val_if_fail (FLOW_IS_CONNECTOR (connector), FLOW_CONNECTIVITY_DISCONNECTED);

  priv = connector->priv;

  return priv->last_state;
}

void
flow_connector_set_state_internal (FlowConnector *connector, FlowConnectivity state)
{
  FlowConnectorPrivate *priv;

  g_return_if_fail (FLOW_IS_CONNECTOR (connector));

  priv = connector->priv;

  if (state == priv->state)
    return;

  priv->last_state = priv->state;
  priv->state      = state;

  g_signal_emit_by_name (connector, "connectivity-changed");
}
