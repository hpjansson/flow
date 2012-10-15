/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-process-result.c - A process result that can be propagated along a pipeline.
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

#include "config.h"
#include "flow-gobject-util.h"
#include "flow-enum-types.h"
#include "flow-process-result.h"

/* --- FlowProcessResult private data --- */

struct _FlowProcessResultPrivate
{
  gint result;
};

/* --- FlowProcessResult properties --- */

static gint
flow_process_result_get_result_internal (FlowProcessResult *process_result)
{
  FlowProcessResultPrivate *priv = process_result->priv;

  return priv->result;
}

static void
flow_process_result_set_result_internal (FlowProcessResult *process_result, gint result)
{
  FlowProcessResultPrivate *priv = process_result->priv;

  priv->result = result;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_process_result)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_INT, "result", "Result", "Return value of process",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_process_result_get_result_internal, flow_process_result_set_result_internal,
                               G_MININT, G_MAXINT, 0)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowProcessResult definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_process_result, FlowProcessResult, FLOW_TYPE_EVENT, 0)

/* --- FlowProcessResult implementation --- */

static void
flow_process_result_type_init (GType type)
{
}

static void
flow_process_result_class_init (FlowProcessResultClass *klass)
{
}

static void
flow_process_result_init (FlowProcessResult *process_result)
{
  FlowProcessResultPrivate *priv = process_result->priv;

  priv->result = -1;
}

static void
flow_process_result_construct (FlowProcessResult *process_result)
{
}

static void
flow_process_result_dispose (FlowProcessResult *process_result)
{
}

static void
flow_process_result_finalize (FlowProcessResult *process_result)
{
}

/* --- FlowProcessResult public API --- */

FlowProcessResult *
flow_process_result_new (gint result)
{
  return g_object_new (FLOW_TYPE_PROCESS_RESULT, "result", result, NULL);
}

gint
flow_process_result_get_result (FlowProcessResult *process_result)
{
  g_return_val_if_fail (FLOW_IS_PROCESS_RESULT (process_result), -1);

  return flow_process_result_get_result_internal (process_result);
}
