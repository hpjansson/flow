/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-position.h - A stream position that can be propagated along a pipeline.
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

#ifndef _FLOW_POSITION_H
#define _FLOW_POSITION_H

#include <flow/flow-event.h>

G_BEGIN_DECLS

typedef enum
{
  FLOW_OFFSET_ANCHOR_CURRENT,
  FLOW_OFFSET_ANCHOR_BEGIN,
  FLOW_OFFSET_ANCHOR_END
}
FlowOffsetAnchor;

#define FLOW_TYPE_POSITION            (flow_position_get_type ())
#define FLOW_POSITION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_POSITION, FlowPosition))
#define FLOW_POSITION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_POSITION, FlowPositionClass))
#define FLOW_IS_POSITION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_POSITION))
#define FLOW_IS_POSITION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_POSITION))
#define FLOW_POSITION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_POSITION, FlowPositionClass))
GType   flow_position_get_type        (void) G_GNUC_CONST;

typedef struct _FlowPosition        FlowPosition;
typedef struct _FlowPositionPrivate FlowPositionPrivate;
typedef struct _FlowPositionClass   FlowPositionClass;

struct _FlowPosition
{
  FlowEvent parent;

  /*< private >*/

  FlowPositionPrivate *priv;
};

struct _FlowPositionClass
{
  FlowEventClass parent_class;

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowPosition    *flow_position_new        (FlowOffsetAnchor anchor, gint64 offset);

gint64           flow_position_get_offset (FlowPosition *position);
FlowOffsetAnchor flow_position_get_anchor (FlowPosition *position);

G_END_DECLS

#endif  /* _FLOW_POSITION_H */
