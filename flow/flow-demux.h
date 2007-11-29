/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-demux.h - A stream demultiplexer
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
#if !defined(_FLOW_DEMUX_H)
#define _FLOW_DEMUX_H

#include <flow/flow-mux-event.h>
#include <flow/flow-splitter.h>

G_BEGIN_DECLS

/* FlowDemux */
#define FLOW_TYPE_DEMUX	        (flow_demux_get_type ())
#define FLOW_DEMUX(obj)	        (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_DEMUX, FlowDemux))
#define FLOW_DEMUX_CLASS(vtable)  (G_TYPE_CHECK_CLASS_CAST ((vtable), FLOW_TYPE_DEMUX, FlowDemuxClass))
#define FLOW_IS_DEMUX(obj)        (G_TYPE_CHECK_TYPE ((obj), FLOW_TYPE_DEMUX))
#define FLOW_DEMUX_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_DEMUX, FlowDemuxClass))

typedef struct _FlowDemux FlowDemux;
typedef struct _FlowDemuxClass FlowDemuxClass;

struct _FlowDemux
{
  FlowSplitter base;
};

struct _FlowDemuxClass
{
  FlowSplitterClass base_class;
};

GType flow_demux_get_type (void) G_GNUC_CONST;

FlowDemux      *flow_demux_new (void);
FlowOutputPad  *flow_demux_add_channel (FlowDemux *demux, guint channel_id);

G_END_DECLS

#endif

