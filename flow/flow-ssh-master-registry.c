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

/* --- FlowSshMasterRegistry private data --- */

struct _FlowSshMasterRegistryPrivate
{
  GMutex *mutex;
  GHashTable *masters;
  GHashTable *timeouts;
};

/* --- FlowSshMasterRegistry properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ssh_master_registry)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowSshMasterRegistry definition --- */

FLOW_GOBJECT_MAKE_IMPL (flow_ssh_master_registry, FlowSshMasterRegistry, G_TYPE_OBJECT, 0)

/* --- FlowSshMasterRegistry implementation --- */

static gchar *
generate_key (FlowIPService *remote_ip_service, const gchar *remote_user)
{
  gint remote_port;
  gchar *remote_name;
  gchar *key;

  remote_port = flow_ip_service_get_port (remote_ip_service);
  remote_name = flow_ip_service_get_name (remote_ip_service);

  key = g_strdup_printf ("%s@%s:%d", remote_user ? remote_user : "", remote_name, remote_port);

  g_free (remote_name);
  return key;
}

typedef struct
{
  FlowSshMasterRegistry *registry;
  FlowSshMaster *master;
  guint timeout_id;
}
MasterTimeout;

static void master_timeout_destroy (MasterTimeout *master_timeout);
static void toggle_ref_cb (FlowSshMasterRegistry *ssh_master_registry, FlowSshMaster *ssh_master, gboolean is_last_ref);

static gboolean
master_timeout_timed_out_cb (MasterTimeout *master_timeout)
{
  FlowSshMasterRegistryPrivate *priv = master_timeout->registry->priv;

  g_object_remove_toggle_ref ((GObject *) master_timeout->master, (GToggleNotify) toggle_ref_cb, master_timeout->registry);
  g_hash_table_remove (priv->masters, master_timeout->master);
  g_hash_table_remove (priv->timeouts, master_timeout->master);
  master_timeout_destroy (master_timeout);
  return TRUE;
}

static MasterTimeout *
master_timeout_new (FlowSshMasterRegistry *ssh_master_registry, FlowSshMaster *ssh_master)
{
  MasterTimeout *master_timeout = g_slice_new (MasterTimeout);

  master_timeout->registry = ssh_master_registry;
  master_timeout->master = ssh_master;
  master_timeout->timeout_id =
    g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                                30,
                                (GSourceFunc) master_timeout_timed_out_cb,
                                master_timeout,
                                NULL);

  return master_timeout;
}

static void
master_timeout_destroy (MasterTimeout *master_timeout)
{
  g_source_remove (master_timeout->timeout_id);
  g_slice_free (MasterTimeout, master_timeout);
}

static void
toggle_ref_cb (FlowSshMasterRegistry *ssh_master_registry, FlowSshMaster *ssh_master, gboolean is_last_ref)
{
  FlowSshMasterRegistryPrivate *priv = ssh_master_registry->priv;

  if (is_last_ref)
  {
    g_hash_table_insert (priv->timeouts, ssh_master, master_timeout_new (ssh_master_registry, ssh_master));
  }
  else
  {
    g_hash_table_remove (priv->timeouts, ssh_master);
  }
}

static void
remove_toggle_ref_foreach (gpointer key, FlowSshMaster *ssh_master, FlowSshMasterRegistry *ssh_master_registry)
{
  g_object_remove_toggle_ref ((GObject *) ssh_master, (GToggleNotify) toggle_ref_cb, ssh_master_registry);
}

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
  FlowSshMasterRegistryPrivate *priv = ssh_master_registry->priv;

  priv->masters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  priv->timeouts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify) master_timeout_destroy);
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
  FlowSshMasterRegistryPrivate *priv = ssh_master_registry->priv;

  g_hash_table_foreach (priv->masters, (GHFunc) remove_toggle_ref_foreach, ssh_master_registry);
  g_hash_table_destroy (priv->masters);
  g_hash_table_destroy (priv->timeouts);
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

FlowSshMaster *
flow_ssh_master_registry_get_master (FlowSshMasterRegistry *ssh_master_registry, FlowIPService *remote_ip_service, const gchar *remote_user)
{
  FlowSshMasterRegistryPrivate *priv;
  FlowSshMaster *ssh_master;
  gchar *key;

  g_return_val_if_fail (FLOW_IS_SSH_MASTER_REGISTRY (ssh_master_registry), NULL);
  g_return_val_if_fail (FLOW_IS_IP_SERVICE (remote_ip_service), NULL);

  priv = ssh_master_registry->priv;

  key = generate_key (remote_ip_service, remote_user);
  ssh_master = g_hash_table_lookup (priv->masters, key);

  if (ssh_master)
  {
    g_free (key);
  }
  else
  {
    ssh_master = flow_ssh_master_new (remote_ip_service, remote_user);
    g_hash_table_insert (priv->masters, key, ssh_master);
    g_object_add_toggle_ref ((GObject *) ssh_master, (GToggleNotify) toggle_ref_cb, ssh_master_registry);
    g_object_unref (ssh_master);
  }

  return ssh_master;
}
