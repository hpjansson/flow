/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-destructor.c - An endpoint that destroys packets.
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

#include "config.h"
#include "flow-gobject-util.h"
#include "flow-destructor.h"

/* --- FlowDestructor properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_destructor)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowDestructor definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_destructor, FlowDestructor, FLOW_TYPE_COLLECTOR, 0)

/* --- FlowDestructor implementation --- */

static void
flow_destructor_type_init (GType type)
{
}

static void
flow_destructor_class_init (FlowDestructorClass *klass)
{
  /* We don't have to implement anything - the FlowCollector class
   * defaults to exactly what we want. */
}

static void
flow_destructor_init (FlowDestructor *destructor)
{
}

static void
flow_destructor_construct (FlowDestructor *destructor)
{
}

static void
flow_destructor_dispose (FlowDestructor *destructor)
{
}

static void
flow_destructor_finalize (FlowDestructor *destructor)
{
}

/* --- FlowDestructor public API --- */

FlowDestructor *
flow_destructor_new (void)
{
  return g_object_new (FLOW_TYPE_DESTRUCTOR, NULL);
}
