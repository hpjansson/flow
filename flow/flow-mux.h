/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-mux.h - A stream multiplexer
 *
 * Copyright (C) 2007 Andreas Rottmann
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
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors: Andreas Rottmann <a.rottmann@gmx.at>
 */
#if !defined(_FLOW_MUX_H)
#define _FLOW_MUX_H

#include <flow/flow-mux-event.h>
#include <flow/flow-joiner.h>

G_BEGIN_DECLS

/* FlowMux */
#define FLOW_TYPE_MUX	        (flow_mux_get_type ())
#define FLOW_MUX(obj)	        (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_MUX, FlowMux))
#define FLOW_MUX_CLASS(vtable)  (G_TYPE_CHECK_CLASS_CAST ((vtable), FLOW_TYPE_MUX, FlowMuxClass))
#define FLOW_IS_MUX(obj)        (G_TYPE_CHECK_TYPE ((obj), FLOW_TYPE_MUX))
#define FLOW_MUX_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_MUX, FlowMuxClass))

typedef struct _FlowMux FlowMux;
typedef struct _FlowMuxPrivate FlowMuxPrivate;
typedef struct _FlowMuxClass FlowMuxClass;

struct _FlowMux
{
  FlowJoiner base;

  /*< private >*/

  FlowMuxPrivate *priv;
};

struct _FlowMuxClass
{
  FlowJoinerClass base_class;

  /*< private >*/
};

GType flow_mux_get_type (void) G_GNUC_CONST;

FlowMux        *flow_mux_new (void);
FlowInputPad   *flow_mux_add_channel (FlowMux *mux, FlowMuxEvent *event);
FlowInputPad   *flow_mux_add_channel_id (FlowMux *mux, guint channel_id);

G_END_DECLS

#endif
