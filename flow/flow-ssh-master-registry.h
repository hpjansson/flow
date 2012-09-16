/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ssh-master-registry.h - A registry for FlowSshMaster re-use.
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

#ifndef _FLOW_SSH_MASTER_REGISTRY_H
#define _FLOW_SSH_MASTER_REGISTRY_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FLOW_TYPE_SSH_MASTER_REGISTRY            (flow_ssh_master_registry_get_type ())
#define FLOW_SSH_MASTER_REGISTRY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_SSH_MASTER_REGISTRY, FlowSshMasterRegistry))
#define FLOW_SSH_MASTER_REGISTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_SSH_MASTER_REGISTRY, FlowSshMasterRegistryClass))
#define FLOW_IS_SSH_MASTER_REGISTRY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_SSH_MASTER_REGISTRY))
#define FLOW_IS_SSH_MASTER_REGISTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_SSH_MASTER_REGISTRY))
#define FLOW_SSH_MASTER_REGISTRY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_SSH_MASTER_REGISTRY, FlowSshMasterRegistryClass))
GType   flow_ssh_master_registry_get_type        (void) G_GNUC_CONST;

typedef struct _FlowSshMasterRegistry        FlowSshMasterRegistry;
typedef struct _FlowSshMasterRegistryPrivate FlowSshMasterRegistryPrivate;
typedef struct _FlowSshMasterRegistryClass   FlowSshMasterRegistryClass;

struct _FlowSshMasterRegistry
{
  GObject          object;

  /*< private >*/

  FlowSshMasterRegistryPrivate *priv;
};

struct _FlowSshMasterRegistryClass
{
  GObjectClass parent_class;

  /*< private >*/

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowSshMasterRegistry      *flow_ssh_master_registry_get_default          (void);

G_END_DECLS

#endif /* _FLOW_IP_ADDR_H */
