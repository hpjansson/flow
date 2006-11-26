/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-emitter.c - A packet emitter.
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
#include "flow-emitter.h"

/* --- FlowEmitter private data --- */

typedef struct
{
}
FlowEmitterPrivate;

/* --- FlowEmitter properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_emitter)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowEmitter definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_emitter, FlowEmitter, FLOW_TYPE_ELEMENT, G_TYPE_FLAG_ABSTRACT)

/* --- FlowEmitter implementation --- */

static void
flow_emitter_type_init (GType type)
{
}

static void
flow_emitter_class_init (FlowEmitterClass *klass)
{
}

static void
flow_emitter_init (FlowEmitter *emitter)
{
  FlowElement *element = (FlowElement *) emitter;

  g_ptr_array_add (element->output_pads, g_object_new (FLOW_TYPE_OUTPUT_PAD, "owner-element", emitter, NULL));
}

static void
flow_emitter_construct (FlowEmitter *emitter)
{
}

static void
flow_emitter_dispose (FlowEmitter *emitter)
{
}

static void
flow_emitter_finalize (FlowEmitter *emitter)
{
}

/* --- FlowEmitter public API --- */

FlowOutputPad *
flow_emitter_get_output_pad (FlowEmitter *emitter)
{
  FlowElement *element = (FlowElement *) emitter;

  g_return_val_if_fail (FLOW_IS_EMITTER (emitter), NULL);

  return g_ptr_array_index (element->output_pads, 0);
}
