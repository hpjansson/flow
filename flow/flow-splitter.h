/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-splitter.h - A one-to-many unidirectional element.
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

#ifndef _FLOW_SPLITTER_H
#define _FLOW_SPLITTER_H

#include <flow/flow-element.h>
#include <flow/flow-input-pad.h>
#include <flow/flow-output-pad.h>

G_BEGIN_DECLS

#define FLOW_TYPE_SPLITTER            (flow_splitter_get_type ())
#define FLOW_SPLITTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_SPLITTER, FlowSplitter))
#define FLOW_SPLITTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_SPLITTER, FlowSplitterClass))
#define FLOW_IS_SPLITTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_SPLITTER))
#define FLOW_IS_SPLITTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_SPLITTER))
#define FLOW_SPLITTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_SPLITTER, FlowSplitterClass))
GType   flow_splitter_get_type        (void) G_GNUC_CONST;

typedef struct _FlowSplitter      FlowSplitter;
typedef struct _FlowSplitterClass FlowSplitterClass;

struct _FlowSplitter
{
  FlowElement    parent;

  gpointer       priv;
};

struct _FlowSplitterClass
{
  FlowElementClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

G_END_DECLS

FlowSplitter  *flow_splitter_new               (void);

FlowInputPad  *flow_splitter_get_input_pad     (FlowSplitter *splitter);

FlowOutputPad *flow_splitter_add_output_pad    (FlowSplitter *splitter);
void           flow_splitter_remove_output_pad (FlowSplitter *splitter, FlowOutputPad *output_pad);

guint64        flow_splitter_get_buffer_limit  (FlowSplitter *splitter);
void           flow_splitter_set_buffer_limit  (FlowSplitter *splitter, guint64 limit);

#endif  /* _FLOW_SPLITTER_H */
