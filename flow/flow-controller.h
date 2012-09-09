/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-controller.h - Flow rate measurement and control element.
 *
 * Copyright (C) 2012 Hans Petter Jansson
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

#ifndef _FLOW_CONTROLLER_H
#define _FLOW_CONTROLLER_H

#include <glib-object.h>
#include <flow-simplex-element.h>

G_BEGIN_DECLS

#define FLOW_TYPE_CONTROLLER            (flow_controller_get_type ())
#define FLOW_CONTROLLER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_CONTROLLER, FlowController))
#define FLOW_CONTROLLER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_CONTROLLER, FlowControllerClass))
#define FLOW_IS_CONTROLLER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_CONTROLLER))
#define FLOW_IS_CONTROLLER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_CONTROLLER))
#define FLOW_CONTROLLER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_CONTROLLER, FlowControllerClass))
GType   flow_controller_get_type        (void) G_GNUC_CONST;

typedef struct _FlowController        FlowController;
typedef struct _FlowControllerPrivate FlowControllerPrivate;
typedef struct _FlowControllerClass   FlowControllerClass;

struct _FlowController
{
  FlowSimplexElement parent;

  /*< private >*/

  FlowControllerPrivate *priv;
};

struct _FlowControllerClass
{
  FlowSimplexElementClass parent_class;

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowController *flow_controller_new                   (void);

guint64         flow_controller_get_total_bytes       (FlowController *controller);

G_END_DECLS

#endif  /* _FLOW_CONTROLLER_H */
