/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-destructor.h - An endpoint that destroys packets.
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

#ifndef _FLOW_DESTRUCTOR_H
#define _FLOW_DESTRUCTOR_H

#include <flow/flow-collector.h>

G_BEGIN_DECLS

#define FLOW_TYPE_DESTRUCTOR            (flow_destructor_get_type ())
#define FLOW_DESTRUCTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_DESTRUCTOR, FlowDestructor))
#define FLOW_DESTRUCTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_DESTRUCTOR, FlowDestructorClass))
#define FLOW_IS_DESTRUCTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_DESTRUCTOR))
#define FLOW_IS_DESTRUCTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_DESTRUCTOR))
#define FLOW_DESTRUCTOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_DESTRUCTOR, FlowDestructorClass))
GType   flow_destructor_get_type        (void) G_GNUC_CONST;

typedef struct _FlowDestructor      FlowDestructor;
typedef struct _FlowDestructorClass FlowDestructorClass;

struct _FlowDestructor
{
  FlowCollector parent;
};

struct _FlowDestructorClass
{
  FlowCollectorClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowDestructor *flow_destructor_new (void);

G_END_DECLS

#endif  /* _FLOW_DESTRUCTOR_H */
