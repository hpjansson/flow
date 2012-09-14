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

#include <unistd.h>  /* getpid */
#include <glib/gstdio.h>  /* g_remove */
#include "flow-gobject-util.h"
#include "flow-context-mgmt.h"
#include "flow-shunt.h"
#include "flow-ssh-master.h"

#define EXTRA_SSH_MASTER_OPTIONS "-q -M -N -o 'ServerAliveInterval 10' -o 'ServerAliveCountMax 6'"

/* --- FlowSshMaster private data --- */

struct _FlowSshMasterPrivate
{
  GMutex *mutex;
  FlowIPService *remote_ip_service;
  gchar *remote_user;
  gchar *control_path;
  FlowShunt *shunt;
  GError *connect_error;
  guint fake_connect_idle_id;
  guint is_connecting : 1;
  guint is_connected : 1;
};

/* --- FlowSshMaster properties --- */

static FlowIPService *
flow_ssh_master_get_remote_ip_service_internal (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;

  return priv->remote_ip_service;
}

static void
flow_ssh_master_set_remote_ip_service_internal (FlowSshMaster *ssh_master, FlowIPService *remote_ip_service)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;

  g_object_ref (remote_ip_service);
  if (priv->remote_ip_service)
    g_object_unref (priv->remote_ip_service);

  priv->remote_ip_service = remote_ip_service;
}

static gchar *
flow_ssh_master_get_remote_user_internal (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;

  return g_strdup (priv->remote_user);
}

static void
flow_ssh_master_set_remote_user_internal (FlowSshMaster *ssh_master, const gchar *remote_user)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;

  g_free (priv->remote_user);
  priv->remote_user = g_strdup (remote_user);
}

static gboolean
flow_ssh_master_get_is_connected_internal (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;

  return priv->is_connected ? TRUE : FALSE;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ssh_master)
FLOW_GOBJECT_PROPERTY         (G_TYPE_OBJECT, "remote-service", "Remote service",
                               "Remote IP service", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_ssh_master_get_remote_ip_service_internal,
                               flow_ssh_master_set_remote_ip_service_internal,
                               flow_ip_service_get_type)
FLOW_GOBJECT_PROPERTY_STRING  ("remote-user", "Remote user", "Remote user name",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_ssh_master_get_remote_user_internal,
                               flow_ssh_master_set_remote_user_internal,
                               NULL)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("is-connected", "Is connected", "If the master is connected to the remote service",
                               G_PARAM_READABLE,
                               flow_ssh_master_get_is_connected_internal,
                               NULL,
                               FALSE)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowSshMaster definition --- */

FLOW_GOBJECT_MAKE_IMPL_NO_PRIVATE (flow_ssh_master, FlowSshMaster, G_TYPE_OBJECT, 0)

/* --- FlowSshMaster implementation --- */

static gchar *
generate_control_path (FlowSshMaster *ssh_master)
{
  gchar *cache_dir;
  gchar *path;

  cache_dir = g_strdup_printf ("%s/flow", g_get_user_cache_dir ());
  g_mkdir_with_parents (cache_dir, 0770);

  path = g_strdup_printf ("%s/ssh-master-%d-%016" G_GINT64_MODIFIER "x",
                          cache_dir,
                          getpid (),
                          (guint64) ssh_master);
  g_remove (path);

  g_free (cache_dir);
  return path;
}

static void
master_shunt_read (FlowShunt *shunt, FlowPacket *packet, FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv           = ssh_master->priv;
  FlowPacketFormat      packet_format  = flow_packet_get_format (packet);
  gpointer              packet_data    = flow_packet_get_data (packet);
  const gchar          *signal_to_emit = NULL;

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT && FLOW_IS_DETAILED_EVENT (packet_data))
  {
    FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

    g_mutex_lock (priv->mutex);

    if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
    {
      priv->is_connected = TRUE;
      priv->is_connecting = FALSE;
      signal_to_emit = "connect-finished";
    }
    else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
    {
      flow_shunt_destroy (priv->shunt);
      priv->shunt = NULL;

      priv->is_connected = FALSE;
      signal_to_emit = "disconnected";
    }
    else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
    {
      flow_shunt_destroy (priv->shunt);
      priv->shunt = NULL;

      if (priv->connect_error)
        g_error_free (priv->connect_error);
      priv->connect_error = g_error_new_literal (FLOW_SSH_DOMAIN_QUARK, FLOW_SSH_MASTER_FAILED,
                                                 "Could not start ssh master");

      priv->is_connecting = FALSE;
      signal_to_emit = "connect-finished";
    }

    g_mutex_unlock (priv->mutex);
  }

  flow_packet_unref (packet);

  if (signal_to_emit)
    g_signal_emit_by_name (ssh_master, signal_to_emit);
}

static void
connect_begin (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;
  gchar *remote_name;
  gint remote_port;
  gchar *cmd;

  g_assert (!priv->is_connecting);
  priv->is_connecting = TRUE;

  remote_port = flow_ip_service_get_port (priv->remote_ip_service);
  remote_name = flow_ip_service_get_name (priv->remote_ip_service);
  g_assert (remote_name != NULL);

  if (!priv->control_path)
    priv->control_path = generate_control_path (ssh_master);

  if (remote_port > 0)
  {
    cmd = g_strdup_printf ("ssh " EXTRA_SSH_MASTER_OPTIONS " -o 'ControlPath %s' -p %d %s",
                           priv->control_path,
                           remote_port,
                           remote_name);
  }
  else
  {
    cmd = g_strdup_printf ("ssh " EXTRA_SSH_MASTER_OPTIONS " -o 'ControlPath %s' %s",
                           priv->control_path,
                           remote_name);
  }

  priv->shunt = flow_spawn_command_line (cmd);

  flow_shunt_set_read_func (priv->shunt, (FlowShuntReadFunc *) master_shunt_read, ssh_master);

  g_free (cmd);
  g_free (remote_name);
}

static gboolean
confirm_already_connected (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;
  gboolean reconnect = FALSE;

  g_mutex_lock (priv->mutex);

  if (!priv->is_connected && !priv->is_connecting)
  {
    /* The existing connection failed while waiting for this callback
     * to fire. We need to connect for real. */
    reconnect = TRUE;
  }

  g_mutex_unlock (priv->mutex);

  if (reconnect)
    flow_ssh_master_connect (ssh_master);
  else
    g_signal_emit_by_name (ssh_master, "connect-finished");

  return FALSE;
}

static void
flow_ssh_master_type_init (GType type)
{
}

static void
flow_ssh_master_class_init (FlowSshMasterClass *klass)
{
  if (!g_thread_supported ())
    g_thread_init (NULL);

  g_signal_newv ("connect-finished",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 NULL,                                   /* Class closure */
                 NULL, NULL,                             /* Accumulator, accu data */
                 g_cclosure_marshal_VOID__VOID,          /* Marshaller */
                 G_TYPE_NONE,                            /* Return type */
                 0, NULL);                               /* Number of params, param types */

  g_signal_newv ("disconnected",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 NULL,                                   /* Class closure */
                 NULL, NULL,                             /* Accumulator, accu data */
                 g_cclosure_marshal_VOID__VOID,          /* Marshaller */
                 G_TYPE_NONE,                            /* Return type */
                 0, NULL);                               /* Number of params, param types */
}

static void
flow_ssh_master_init (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;

  priv->control_path = generate_control_path (ssh_master);
}

static void
flow_ssh_master_construct (FlowSshMaster *ssh_master)
{
}

static void
flow_ssh_master_dispose (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;

  if (priv->fake_connect_idle_id)
  {
    flow_source_remove_from_current_thread (priv->fake_connect_idle_id);
    priv->fake_connect_idle_id = 0;
  }

  flow_gobject_unref_clear (priv->remote_ip_service);
}

static void
flow_ssh_master_finalize (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv = ssh_master->priv;

  g_free (priv->control_path);
  if (priv->shunt)
    flow_shunt_destroy (priv->shunt);
  if (priv->connect_error)
    g_error_free (priv->connect_error);
}

/* --- FlowSshMaster public API --- */

FlowSshMaster *
flow_ssh_master_new (FlowIPService *remote_ip_service, const gchar *remote_user)
{
  return g_object_new (FLOW_TYPE_SSH_MASTER,
                       "remote-service", remote_ip_service,
                       "remote-user", remote_user,
                       NULL);
}

FlowIPService *
flow_ssh_master_get_remote_ip_service (FlowSshMaster *ssh_master)
{
  g_return_val_if_fail (FLOW_IS_SSH_MASTER (ssh_master), NULL);

  return flow_ssh_master_get_remote_ip_service_internal (ssh_master);
}

const gchar *
flow_ssh_master_get_remote_user (FlowSshMaster *ssh_master)
{
  g_return_val_if_fail (FLOW_IS_SSH_MASTER (ssh_master), NULL);

  return flow_ssh_master_get_remote_user_internal (ssh_master);
}

void
flow_ssh_master_connect (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv;

  g_return_if_fail (FLOW_IS_SSH_MASTER (ssh_master));

  priv = ssh_master->priv;

  g_mutex_lock (priv->mutex);

  if (priv->is_connecting)
    goto out;

  if (priv->is_connected)
  {
    g_assert (priv->fake_connect_idle_id == 0);
    priv->fake_connect_idle_id = flow_idle_add_to_current_thread ((GSourceFunc) confirm_already_connected, ssh_master);
  }
  else
  {
    g_clear_error (&priv->connect_error);
    connect_begin (ssh_master);
  }

out:
  g_mutex_unlock (priv->mutex);
}

gboolean
flow_ssh_master_sync_connect (FlowSshMaster *ssh_master, GError **error)
{
  FlowSshMasterPrivate *priv;
  GMainLoop            *main_loop;
  guint                 signal_id;
  gboolean              result = FALSE;

  g_return_val_if_fail (FLOW_IS_SSH_MASTER (ssh_master), FALSE);

  priv = ssh_master->priv;

  main_loop = g_main_loop_new (flow_get_main_context_for_current_thread (), FALSE);
  signal_id = g_signal_connect_swapped (ssh_master, "connect-finished", (GCallback) g_main_loop_quit, main_loop);

  flow_ssh_master_connect (ssh_master);
  g_main_loop_run (main_loop);

  g_signal_handler_disconnect (ssh_master, signal_id);
  g_main_loop_unref (main_loop);

  g_mutex_lock (priv->mutex);

  if (priv->is_connected)
  {
    g_assert (priv->connect_error == NULL);
    result = TRUE;
  }
  else
  {
    GError *error_copy;

    g_assert (priv->connect_error != NULL);

    error_copy = g_error_copy (priv->connect_error);
    g_propagate_error (error, error_copy);
  }

  g_mutex_unlock (priv->mutex);

  return result;
}

gboolean
flow_ssh_master_get_is_connected (FlowSshMaster *ssh_master)
{
  g_return_val_if_fail (FLOW_IS_SSH_MASTER (ssh_master), FALSE);

  return flow_ssh_master_get_is_connected_internal (ssh_master);
}

GError *
flow_ssh_master_get_last_error (FlowSshMaster *ssh_master)
{
  FlowSshMasterPrivate *priv;
  GError               *error = NULL;

  g_return_val_if_fail (FLOW_IS_SSH_MASTER (ssh_master), NULL);

  priv = ssh_master->priv;

  g_mutex_lock (priv->mutex);
  if (priv->connect_error)
    error = g_error_copy (priv->connect_error);
  g_mutex_unlock (priv->mutex);

  return error;
}
