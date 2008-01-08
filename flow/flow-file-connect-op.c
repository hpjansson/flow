/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-file-connect-op.c - Operation: Connect to local file.
 *
 * Copyright (C) 2007 Hans Petter Jansson
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
#include "flow-shunt.h"
#include "flow-event.h"
#include "flow-enum-types.h"
#include "flow-file-connect-op.h"

/* --- FlowFileConnectOp private data --- */

typedef struct
{
  gchar          *path;
  FlowAccessMode  access_mode;

  guint           do_create  : 1;
  guint           do_replace : 1;

  FlowAccessMode  create_mode_user;
  FlowAccessMode  create_mode_group;
  FlowAccessMode  create_mode_others;
}
FlowFileConnectOpPrivate;

/* --- FlowFileConnectOp properties --- */

static gchar *
flow_file_connect_op_get_path_internal (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  return g_strdup (priv->path);
}

static void
flow_file_connect_op_set_path_internal (FlowFileConnectOp *file_connect_op, const gchar *path)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  g_assert (priv->path == NULL);
  priv->path = g_strdup (path);
}

static guint
flow_file_connect_op_get_access_mode_internal (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  return priv->access_mode;
}

static void
flow_file_connect_op_set_access_mode_internal (FlowFileConnectOp *file_connect_op, guint access_mode)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  priv->access_mode = access_mode;
}

static gboolean
flow_file_connect_op_get_create_internal (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  return priv->do_create;
}

static void
flow_file_connect_op_set_create_internal (FlowFileConnectOp *file_connect_op, gboolean do_create)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  priv->do_create = do_create;
}

static gboolean
flow_file_connect_op_get_replace_internal (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  return priv->do_replace;
}

static void
flow_file_connect_op_set_replace_internal (FlowFileConnectOp *file_connect_op, gboolean do_replace)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  priv->do_replace = do_replace;
}

static FlowAccessMode
flow_file_connect_op_get_create_mode_user_internal (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  return priv->create_mode_user;
}

static void
flow_file_connect_op_set_create_mode_user_internal (FlowFileConnectOp *file_connect_op, FlowAccessMode create_mode_user)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  priv->create_mode_user = create_mode_user;
}

static FlowAccessMode
flow_file_connect_op_get_create_mode_group_internal (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  return priv->create_mode_group;
}

static void
flow_file_connect_op_set_create_mode_group_internal (FlowFileConnectOp *file_connect_op, FlowAccessMode create_mode_group)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  priv->create_mode_group = create_mode_group;
}

static FlowAccessMode
flow_file_connect_op_get_create_mode_others_internal (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  return priv->create_mode_others;
}

static void
flow_file_connect_op_set_create_mode_others_internal (FlowFileConnectOp *file_connect_op, FlowAccessMode create_mode_others)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  priv->create_mode_others = create_mode_others;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_file_connect_op)
FLOW_GOBJECT_PROPERTY_STRING  ("path", "Local Path", "Path to local file to open",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_file_connect_op_get_path_internal,
                               flow_file_connect_op_set_path_internal,
                               NULL)
FLOW_GOBJECT_PROPERTY_INT     (G_TYPE_UINT, "access-mode", "Access Mode", "File access mode",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_file_connect_op_get_access_mode_internal,
                               flow_file_connect_op_set_access_mode_internal,
                               0, G_MAXUINT, 0)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("replace", "Replace", "Whether to replace file if it exists",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_file_connect_op_get_replace_internal,
                               flow_file_connect_op_set_replace_internal,
                               FALSE)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("create", "Create", "Whether to create file if it does not exist",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_file_connect_op_get_create_internal,
                               flow_file_connect_op_set_create_internal,
                               FALSE)
FLOW_GOBJECT_PROPERTY_FLAGS   ("create-mode-user", "Create Mode User", "New file permissions for owning user",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_file_connect_op_get_create_mode_user_internal,
                               flow_file_connect_op_set_create_mode_user_internal,
                               FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                               flow_access_mode_get_type)
FLOW_GOBJECT_PROPERTY_FLAGS   ("create-mode-group", "Create Mode Group", "New file permissions for owning group",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_file_connect_op_get_create_mode_group_internal,
                               flow_file_connect_op_set_create_mode_group_internal,
                               FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                               flow_access_mode_get_type)
FLOW_GOBJECT_PROPERTY_FLAGS   ("create-mode-others", "Create Mode Others", "New file permissions for other users",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_file_connect_op_get_create_mode_others_internal,
                               flow_file_connect_op_set_create_mode_others_internal,
                               FLOW_READ_ACCESS | FLOW_WRITE_ACCESS,
                               flow_access_mode_get_type)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowFileConnectOp definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_file_connect_op, FlowFileConnectOp, FLOW_TYPE_EVENT, 0)

/* --- FlowFileConnectOp implementation --- */

static gchar *
append_mode_str (gchar *orig, const gchar *appendage)
{
  gchar *new;

  if (!orig)
    return g_strdup (appendage);

  new = g_strjoin ("/", orig, appendage, NULL);

  g_free (orig);
  return new;
}

static void
flow_file_connect_op_update_description (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv                      = file_connect_op->priv;
  FlowEvent                *event                     = FLOW_EVENT (file_connect_op);
  gchar                    *access_mode_str           = NULL;
  gboolean                  must_free_access_mode_str = FALSE;

  if (event->description)
    return;

  if (priv->access_mode == FLOW_NO_ACCESS)
  {
    access_mode_str = "no access";
  }
  else
  {
    must_free_access_mode_str = TRUE;

    if (priv->access_mode & FLOW_READ_ACCESS)
      access_mode_str = append_mode_str (access_mode_str, "read");
    if (priv->access_mode & FLOW_WRITE_ACCESS)
      access_mode_str = append_mode_str (access_mode_str, "write");
    if (priv->access_mode & FLOW_EXECUTE_ACCESS)
      access_mode_str = append_mode_str (access_mode_str, "execute");
  }

  event->description = g_strdup_printf ("Open file '%s' for %s", priv->path, access_mode_str);

  if (must_free_access_mode_str)
    g_free (access_mode_str);
}

static void
flow_file_connect_op_type_init (GType type)
{
}

static void
flow_file_connect_op_class_init (FlowFileConnectOpClass *klass)
{
  FlowEventClass *event_klass = FLOW_EVENT_CLASS (klass);

  event_klass->update_description = (void (*) (FlowEvent *)) flow_file_connect_op_update_description;
}

static void
flow_file_connect_op_init (FlowFileConnectOp *file_connect_op)
{
}

static void
flow_file_connect_op_construct (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  g_assert (priv->path != NULL);
  g_assert (priv->access_mode != FLOW_NO_ACCESS);
}

static void
flow_file_connect_op_dispose (FlowFileConnectOp *file_connect_op)
{
}

static void
flow_file_connect_op_finalize (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv = file_connect_op->priv;

  g_free (priv->path);
  priv->path = NULL;
}

/* --- FlowFileConnectOp public API --- */

FlowFileConnectOp *
flow_file_connect_op_new (const gchar *path, FlowAccessMode access_mode,
                          gboolean do_create, gboolean do_replace,
                          FlowAccessMode create_mode_user,
                          FlowAccessMode create_mode_group,
                          FlowAccessMode create_mode_others)
{
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (access_mode != FLOW_NO_ACCESS, NULL);

  return g_object_new (FLOW_TYPE_FILE_CONNECT_OP,
                       "path",               path,
                       "access-mode",        access_mode,
                       "create",             do_create,
                       "replace",            do_replace,
                       "create-mode-user",   create_mode_user,
                       "create-mode-group",  create_mode_group,
                       "create-mode-others", create_mode_others,
                       NULL);
}

const gchar *
flow_file_connect_op_get_path (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv;

  g_return_val_if_fail (FLOW_IS_FILE_CONNECT_OP (file_connect_op), NULL);

  priv = file_connect_op->priv;
  return priv->path;
}

FlowAccessMode
flow_file_connect_op_get_access_mode (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv;

  g_return_val_if_fail (FLOW_IS_FILE_CONNECT_OP (file_connect_op), FLOW_NO_ACCESS);

  priv = file_connect_op->priv;
  return priv->access_mode;
}

gboolean
flow_file_connect_op_get_create (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv;

  g_return_val_if_fail (FLOW_IS_FILE_CONNECT_OP (file_connect_op), FALSE);

  priv = file_connect_op->priv;
  return priv->do_create;
}

gboolean
flow_file_connect_op_get_replace (FlowFileConnectOp *file_connect_op)
{
  FlowFileConnectOpPrivate *priv;

  g_return_val_if_fail (FLOW_IS_FILE_CONNECT_OP (file_connect_op), FALSE);

  priv = file_connect_op->priv;
  return priv->do_replace;
}

void
flow_file_connect_op_get_create_modes (FlowFileConnectOp *file_connect_op,
                                       FlowAccessMode *create_mode_user,
                                       FlowAccessMode *create_mode_group,
                                       FlowAccessMode *create_mode_others)
{
  FlowFileConnectOpPrivate *priv;

  g_return_if_fail (FLOW_IS_FILE_CONNECT_OP (file_connect_op));

  priv = file_connect_op->priv;

  if (create_mode_user)
    *create_mode_user = priv->create_mode_user;
  if (create_mode_group)
    *create_mode_group = priv->create_mode_group;
  if (create_mode_others)
    *create_mode_others = priv->create_mode_others;
}
