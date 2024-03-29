/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-joiner.h - A one-to-many unidirectional element.
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

#ifndef _FLOW_JOINER_H
#define _FLOW_JOINER_H

#include <flow/flow-element.h>
#include <flow/flow-input-pad.h>
#include <flow/flow-output-pad.h>

G_BEGIN_DECLS

#define FLOW_TYPE_JOINER            (flow_joiner_get_type ())
#define FLOW_JOINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_JOINER, FlowJoiner))
#define FLOW_JOINER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_JOINER, FlowJoinerClass))
#define FLOW_IS_JOINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_JOINER))
#define FLOW_IS_JOINER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_JOINER))
#define FLOW_JOINER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_JOINER, FlowJoinerClass))
GType   flow_joiner_get_type        (void) G_GNUC_CONST;

typedef struct _FlowJoiner        FlowJoiner;
typedef struct _FlowJoinerPrivate FlowJoinerPrivate;
typedef struct _FlowJoinerClass   FlowJoinerClass;

struct _FlowJoiner
{
  FlowElement    parent;

  /*< private >*/

  FlowJoinerPrivate *priv;
};

struct _FlowJoinerClass
{
  FlowElementClass parent_class;

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

G_END_DECLS

FlowJoiner     *flow_joiner_new              (void);

FlowOutputPad  *flow_joiner_get_output_pad   (FlowJoiner *joiner);

FlowInputPad   *flow_joiner_add_input_pad    (FlowJoiner *joiner);
void            flow_joiner_remove_input_pad (FlowJoiner *joiner, FlowInputPad *input_pad);

#endif  /* _FLOW_JOINER_H */
