/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ssh-master.c - An SSH master for multiplexing connections.
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include "flow-gobject-util.h"
#include "flow-ssh-master.h"

/* --- Class properties --- */
FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ssh_master)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- Class definition --- */
FLOW_GOBJECT_MAKE_IMPL_NO_PRIVATE (flow_ssh_master, FlowSshMaster, G_TYPE_OBJECT, 0)

static void
flow_ssh_master_type_init (GType type)
{
}

static void
flow_ssh_master_class_init (FlowSshMasterClass *klass)
{
}

static void
flow_ssh_master_init (FlowSshMaster *ssh_master)
{
}

static void
flow_ssh_master_construct (FlowSshMaster *ssh_master)
{
}

static void
flow_ssh_master_dispose (FlowSshMaster *ssh_master)
{
}

static void
flow_ssh_master_finalize (FlowSshMaster *ssh_master)
{
}

FlowSshMaster *
flow_ssh_master_new (void)
{
  return g_object_new (FLOW_TYPE_SSH_MASTER, NULL);
}
