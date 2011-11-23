/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-element.h - A pipeline processing element.
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

#ifndef _FLOW_ELEMENT_H
#define _FLOW_ELEMENT_H

/* FlowPad and FlowElement are co-dependent types */
typedef struct _FlowElement        FlowElement;
typedef struct _FlowElementPrivate FlowElementPrivate;
typedef struct _FlowElementClass   FlowElementClass;

#include <glib-object.h>
#include <flow/flow-pad.h>

G_BEGIN_DECLS

#define FLOW_TYPE_ELEMENT            (flow_element_get_type ())
#define FLOW_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_ELEMENT, FlowElement))
#define FLOW_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_ELEMENT, FlowElementClass))
#define FLOW_IS_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_ELEMENT))
#define FLOW_IS_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_ELEMENT))
#define FLOW_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_ELEMENT, FlowElementClass))
GType   flow_element_get_type        (void) G_GNUC_CONST;

struct _FlowElement
{
  GObject     parent;

  /*< private >*/

  /* --- Protected --- */

  GPtrArray  *input_pads;
  GPtrArray  *output_pads;

  /* Used in input processing */

  guint       was_disposed       : 1;
  guint       input_pad_removed  : 1;
  guint       output_pad_removed : 1;
  guint       dispatch_depth     : 15;
  FlowPad    *current_input;
  GSList     *pending_inputs;

  /*< private >*/

  FlowElementPrivate *priv;
};

struct _FlowElementClass
{
  GObjectClass parent_class;

  /* Methods */

  void     (*output_pad_blocked)       (FlowElement *element, FlowPad *output_pad);
  void     (*output_pad_unblocked)     (FlowElement *element, FlowPad *output_pad);
  void     (*process_input)            (FlowElement *element, FlowPad *input_pad);

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

GPtrArray *flow_element_get_input_pads  (FlowElement *element);
GPtrArray *flow_element_get_output_pads (FlowElement *element);

G_END_DECLS

#endif  /* _FLOW_ELEMENT_H */
