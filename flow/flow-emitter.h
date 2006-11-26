/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-emitter.h - A packet emitter.
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

#ifndef _FLOW_EMITTER_H
#define _FLOW_EMITTER_H

#include <flow/flow-element.h>
#include <flow/flow-output-pad.h>

G_BEGIN_DECLS

#define FLOW_TYPE_EMITTER            (flow_emitter_get_type ())
#define FLOW_EMITTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_EMITTER, FlowEmitter))
#define FLOW_EMITTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_EMITTER, FlowEmitterClass))
#define FLOW_IS_EMITTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_EMITTER))
#define FLOW_IS_EMITTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_EMITTER))
#define FLOW_EMITTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_EMITTER, FlowEmitterClass))
GType   flow_emitter_get_type        (void) G_GNUC_CONST;

typedef struct _FlowEmitter      FlowEmitter;
typedef struct _FlowEmitterClass FlowEmitterClass;

struct _FlowEmitter
{
  FlowElement    parent;

  gpointer       priv;
};

struct _FlowEmitterClass
{
  FlowElementClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

G_END_DECLS

FlowOutputPad  *flow_emitter_get_output_pad     (FlowEmitter *emitter);

#endif  /* _FLOW_EMITTER_H */
