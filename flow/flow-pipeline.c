/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-pipeline.c - A unidirectional multi-element pipeline.
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
#include "flow-pipeline.h"

/* --- FlowPipeline properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_pipeline)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowPipeline definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_pipeline, FlowPipeline, G_TYPE_OBJECT, 0)

static void
flow_pipeline_type_init (GType type)
{
}

static void
flow_pipeline_class_init (FlowPipelineClass *klass)
{
}

static void
flow_pipeline_init (FlowPipeline *pipeline)
{
}

static void
flow_pipeline_construct (FlowPipeline *pipeline)
{
}

static void
flow_pipeline_dispose (FlowPipeline *pipeline)
{
}

static void
flow_pipeline_finalize (FlowPipeline *pipeline)
{
}

/* --- FlowPipeline public API --- */

FlowPipeline *
flow_pipeline_new (void)
{
  return g_object_new (FLOW_TYPE_PIPELINE, NULL);
}
