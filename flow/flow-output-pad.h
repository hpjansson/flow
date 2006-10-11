/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-output-pad.h - An element output connector.
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

#ifndef _FLOW_OUTPUT_PAD_H
#define _FLOW_OUTPUT_PAD_H

#include <flow/flow-pad.h>

G_BEGIN_DECLS

#define FLOW_TYPE_OUTPUT_PAD            (flow_output_pad_get_type ())
#define FLOW_OUTPUT_PAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_OUTPUT_PAD, FlowOutputPad))
#define FLOW_OUTPUT_PAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_OUTPUT_PAD, FlowOutputPadClass))
#define FLOW_IS_OUTPUT_PAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_OUTPUT_PAD))
#define FLOW_IS_OUTPUT_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_OUTPUT_PAD))
#define FLOW_OUTPUT_PAD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_OUTPUT_PAD, FlowOutputPadClass))
GType   flow_output_pad_get_type        (void) G_GNUC_CONST;

typedef struct _FlowOutputPad      FlowOutputPad;
typedef struct _FlowOutputPadClass FlowOutputPadClass;

struct _FlowOutputPad
{
  FlowPad parent;
};

struct _FlowOutputPadClass
{
  FlowPadClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

G_END_DECLS

#endif  /* _FLOW_OUTPUT_PAD_H */
