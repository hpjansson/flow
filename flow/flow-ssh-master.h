/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ssh-master.h - An SSH master for multiplexing connections.
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

#ifndef _FLOW_SSH_MASTER_H
#define _FLOW_SSH_MASTER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FLOW_TYPE_SSH_MASTER            (flow_ssh_master_get_type ())
#define FLOW_SSH_MASTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_SSH_MASTER, FlowSshMaster))
#define FLOW_SSH_MASTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_SSH_MASTER, FlowSshMasterClass))
#define FLOW_IS_SSH_MASTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_SSH_MASTER))
#define FLOW_IS_SSH_MASTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_SSH_MASTER))
#define FLOW_SSH_MASTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_SSH_MASTER, FlowSshMasterClass))
GType   flow_ssh_master_get_type        (void) G_GNUC_CONST;

typedef struct _FlowSshMaster        FlowSshMaster;
typedef struct _FlowSshMasterPrivate FlowSshMasterPrivate;
typedef struct _FlowSshMasterClass   FlowSshMasterClass;

struct _FlowSshMaster
{
  GObject          object;

  /*< private >*/

  FlowSshMasterPrivate *priv;
};

struct _FlowSshMasterClass
{
  GObjectClass parent_class;

  /*< private >*/

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowSshMaster *flow_ssh_master_new                   (FlowIPService *remote_ip_service, const gchar *remote_user);

FlowIPService *flow_ssh_master_get_remote_ip_service (FlowSshMaster *ssh_master);
gchar         *flow_ssh_master_get_remote_user       (FlowSshMaster *ssh_master);
gboolean       flow_ssh_master_get_is_connected      (FlowSshMaster *ssh_master);
GError        *flow_ssh_master_get_last_error        (FlowSshMaster *ssh_master);

gchar         *flow_ssh_master_get_control_path      (FlowSshMaster *ssh_master);

void           flow_ssh_master_connect               (FlowSshMaster *ssh_master);
gboolean       flow_ssh_master_sync_connect          (FlowSshMaster *ssh_master, GError **error);

G_END_DECLS

#endif /* _FLOW_IP_ADDR_H */
