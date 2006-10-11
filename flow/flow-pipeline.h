/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-pipeline.h - A unidirectional multi-element pipeline.
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

#ifndef _FLOW_PIPELINE_H
#define _FLOW_PIPELINE_H

#include <glib-object.h>
#include <flow/flow-element.h>

G_BEGIN_DECLS

#define FLOW_TYPE_PIPELINE            (flow_pipeline_get_type ())
#define FLOW_PIPELINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_PIPELINE, FlowPipeline))
#define FLOW_PIPELINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_PIPELINE, FlowPipelineClass))
#define FLOW_IS_PIPELINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_PIPELINE))
#define FLOW_IS_PIPELINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_PIPELINE))
#define FLOW_PIPELINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_PIPELINE, FlowPipelineClass))
GType   flow_pipeline_get_type        (void) G_GNUC_CONST;

typedef struct _FlowPipeline      FlowPipeline;
typedef struct _FlowPipelineClass FlowPipelineClass;

struct _FlowPipeline
{
  GObject parent;

};

struct _FlowPipelineClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowPipeline *flow_pipeline_new (void);

G_END_DECLS

#endif  /* _FLOW_PIPELINE_H */
