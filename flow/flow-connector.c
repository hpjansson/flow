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

#define DEFAULT_BUFFER_SIZE 4096

/* --- FlowConnector private data --- */

typedef struct
{
  FlowConnectivity   state;
  FlowConnectivity   last_state;

  guint              io_buffer_size;
  guint              read_queue_limit;
  guint              write_queue_limit;
}
FlowConnectorPrivate;

/* --- FlowConnector properties --- */

static guint
flow_connector_get_io_buffer_size_internal (FlowConnector *connector)
{
  FlowConnectorPrivate *priv = connector->priv;

  return priv->io_buffer_size;
}

static void
flow_connector_set_io_buffer_size_internal (FlowConnector *connector, guint io_buffer_size)
{
  FlowConnectorPrivate *priv = connector->priv;

  priv->io_buffer_size = io_buffer_size;
}

static guint
flow_connector_get_read_queue_limit_internal (FlowConnector *connector)
{
  FlowConnectorPrivate *priv = connector->priv;

  return priv->read_queue_limit;
}

static void
flow_connector_set_read_queue_limit_internal (FlowConnector *connector, guint read_queue_limit)
{
  FlowConnectorPrivate *priv = connector->priv;

  priv->read_queue_limit = read_queue_limit;
}

static guint
flow_connector_get_write_queue_limit_internal (FlowConnector *connector)
{
  FlowConnectorPrivate *priv = connector->priv;

  return priv->write_queue_limit;
}

static void
flow_connector_set_write_queue_limit_internal (FlowConnector *connector, guint write_queue_limit)
{
  FlowConnectorPrivate *priv = connector->priv;

  priv->write_queue_limit = write_queue_limit;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_connector)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT, "io-buffer-size", "I/O Buffer Size", "I/O buffer size",
                               G_PARAM_READWRITE,
                               flow_connector_get_io_buffer_size_internal,
                               flow_connector_set_io_buffer_size_internal,
                               1, G_MAXUINT, DEFAULT_BUFFER_SIZE)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT, "read-queue-limit", "Read Queue Limit", "Read queue size limit",
                               G_PARAM_READWRITE,
                               flow_connector_get_read_queue_limit_internal,
                               flow_connector_set_read_queue_limit_internal,
                               1, G_MAXUINT, DEFAULT_BUFFER_SIZE)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT, "write-queue-limit", "Write Queue Limit", "Write queue size limit",
                               G_PARAM_READWRITE,
                               flow_connector_get_write_queue_limit_internal,
                               flow_connector_set_write_queue_limit_internal,
                               1, G_MAXUINT, DEFAULT_BUFFER_SIZE)
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

  priv->io_buffer_size = DEFAULT_BUFFER_SIZE;
  priv->read_queue_limit = DEFAULT_BUFFER_SIZE;
  priv->write_queue_limit = DEFAULT_BUFFER_SIZE;
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

/**
 * flow_connector_get_state:
 * @connector: A #FlowConnector.
 * 
 * Gets the current connectivity state of @connector.
 * 
 * Return value: The current #FlowConnectivity of @connector.
 **/
FlowConnectivity
flow_connector_get_state (FlowConnector *connector)
{
  FlowConnectorPrivate *priv;

  g_return_val_if_fail (FLOW_IS_CONNECTOR (connector), FLOW_CONNECTIVITY_DISCONNECTED);

  priv = connector->priv;

  return priv->state;
}

/**
 * flow_connector_get_last_state:
 * @connector: A #FlowConnector.
 * 
 * Gets the last connectivity state of @connector. By using this
 * and the state from flow_connector_get_state(), you can determine
 * the last state transition.
 * 
 * Return value: The previous #FlowConnectivity of @connector.
 **/
FlowConnectivity
flow_connector_get_last_state (FlowConnector *connector)
{
  FlowConnectorPrivate *priv;

  g_return_val_if_fail (FLOW_IS_CONNECTOR (connector), FLOW_CONNECTIVITY_DISCONNECTED);

  priv = connector->priv;

  return priv->last_state;
}

/**
 * flow_connector_set_state_internal:
 * @connector: A #FlowConnector.
 * @state: The new #FlowConnectivity of @connector.
 * 
 * This function is for #FlowConnector implementations only. It handles
 * the details of a state transition, like storing the previous state and
 * emitting a signal.
 **/
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

guint
flow_connector_get_io_buffer_size (FlowConnector *connector)
{
  FlowConnectorPrivate *priv;

  g_return_val_if_fail (FLOW_IS_CONNECTOR (connector), 0);

  priv = connector->priv;
  return priv->io_buffer_size;
}

void
flow_connector_set_io_buffer_size (FlowConnector *connector, guint io_buffer_size)
{
  g_return_if_fail (FLOW_IS_CONNECTOR (connector));
  g_return_if_fail (io_buffer_size > 0);

  g_object_set (G_OBJECT (connector), "io-buffer-size", io_buffer_size, NULL);
}

guint
flow_connector_get_read_queue_limit (FlowConnector *connector)
{
  FlowConnectorPrivate *priv;

  g_return_val_if_fail (FLOW_IS_CONNECTOR (connector), 0);

  priv = connector->priv;
  return priv->read_queue_limit;
}

void
flow_connector_set_read_queue_limit (FlowConnector *connector, guint read_queue_limit)
{
  g_return_if_fail (FLOW_IS_CONNECTOR (connector));
  g_return_if_fail (read_queue_limit > 0);

  g_object_set (G_OBJECT (connector), "read-queue-limit", read_queue_limit, NULL);
}

guint
flow_connector_get_write_queue_limit (FlowConnector *connector)
{
  FlowConnectorPrivate *priv;

  g_return_val_if_fail (FLOW_IS_CONNECTOR (connector), 0);

  priv = connector->priv;
  return priv->write_queue_limit;
}

void
flow_connector_set_write_queue_limit (FlowConnector *connector, guint write_queue_limit)
{
  g_return_if_fail (FLOW_IS_CONNECTOR (connector));
  g_return_if_fail (write_queue_limit > 0);

  g_object_set (G_OBJECT (connector), "write-queue-limit", write_queue_limit, NULL);
}
