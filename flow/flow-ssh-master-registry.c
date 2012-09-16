/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ssh-master-registry.c - A registry for FlowSshMaster re-use.
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
#include "flow-ssh-master-registry.h"

/* --- FlowSshMasterRegistry properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ssh_master_registry)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowSshMasterRegistry definition --- */

FLOW_GOBJECT_MAKE_IMPL_NO_PRIVATE (flow_ssh_master_registry, FlowSshMasterRegistry, G_TYPE_OBJECT, 0)

/* --- FlowSshMasterRegistry implementation --- */

static void
flow_ssh_master_registry_type_init (GType type)
{
}

static void
flow_ssh_master_registry_class_init (FlowSshMasterRegistryClass *klass)
{
}

static void
flow_ssh_master_registry_init (FlowSshMasterRegistry *ssh_master_registry)
{
}

static void
flow_ssh_master_registry_construct (FlowSshMasterRegistry *ssh_master_registry)
{
}

static void
flow_ssh_master_registry_dispose (FlowSshMasterRegistry *ssh_master_registry)
{
}

static void
flow_ssh_master_registry_finalize (FlowSshMasterRegistry *ssh_master_registry)
{
}

static FlowSshMasterRegistry *
instantiate_singleton (void)
{
  return g_object_new (FLOW_TYPE_SSH_MASTER_REGISTRY, NULL);
}

/* --- FlowSshMasterRegistry public API --- */

FlowSshMasterRegistry *
flow_ssh_master_registry_get_default (void)
{
  static GOnce my_once = G_ONCE_INIT;

  g_once (&my_once, (GThreadFunc) instantiate_singleton, NULL);
  return my_once.retval;
}
