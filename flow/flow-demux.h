/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-demux.h - A one-to-many unidirectional element.
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

#ifndef _FLOW_DEMUX_H
#define _FLOW_DEMUX_H

#include <flow/flow-element.h>
#include <flow/flow-input-pad.h>
#include <flow/flow-output-pad.h>

G_BEGIN_DECLS

#define FLOW_TYPE_DEMUX            (flow_demux_get_type ())
#define FLOW_DEMUX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_DEMUX, FlowDemux))
#define FLOW_DEMUX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_DEMUX, FlowDemuxClass))
#define FLOW_IS_DEMUX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_DEMUX))
#define FLOW_IS_DEMUX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_DEMUX))
#define FLOW_DEMUX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_DEMUX, FlowDemuxClass))
GType   flow_demux_get_type        (void) G_GNUC_CONST;

typedef struct _FlowDemux      FlowDemux;
typedef struct _FlowDemuxClass FlowDemuxClass;

struct _FlowDemux
{
  FlowElement    parent;
};

struct _FlowDemuxClass
{
  FlowElementClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

G_END_DECLS

FlowInputPad  *flow_demux_get_input_pad     (FlowDemux *demux);

FlowOutputPad *flow_demux_add_output_pad    (FlowDemux *demux);
void           flow_demux_remove_output_pad (FlowDemux *demux, FlowOutputPad *output_pad);

#endif  /* _FLOW_DEMUX_H */
