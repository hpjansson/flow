/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-shell-op.c - Operation: Execute a shell command.
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
#include "flow-event.h"
#include "flow-enum-types.h"
#include "flow-shell-op.h"

/* --- FlowShellOp private data --- */

struct _FlowShellOpPrivate
{
  gchar          *cmd;
};

/* --- FlowShellOp properties --- */

static gchar *
flow_shell_op_get_cmd_internal (FlowShellOp *shell_op)
{
  FlowShellOpPrivate *priv = shell_op->priv;

  return g_strdup (priv->cmd);
}

static void
flow_shell_op_set_cmd_internal (FlowShellOp *shell_op, const gchar *cmd)
{
  FlowShellOpPrivate *priv = shell_op->priv;

  g_assert (priv->cmd == NULL);
  priv->cmd = g_strdup (cmd);
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_shell_op)
FLOW_GOBJECT_PROPERTY_STRING  ("cmd", "Command", "Command to run",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_shell_op_get_cmd_internal,
                               flow_shell_op_set_cmd_internal,
                               NULL)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowShellOp definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_shell_op, FlowShellOp, FLOW_TYPE_EVENT, 0)

/* --- FlowShellOp implementation --- */

static void
flow_shell_op_update_description (FlowShellOp *shell_op)
{
  FlowShellOpPrivate *priv                      = shell_op->priv;
  FlowEvent          *event                     = FLOW_EVENT (shell_op);

  if (event->description)
    return;

  event->description = g_strdup_printf ("Run shell command '%s'", priv->cmd);
}

static void
flow_shell_op_type_init (GType type)
{
}

static void
flow_shell_op_class_init (FlowShellOpClass *klass)
{
  FlowEventClass *event_klass = FLOW_EVENT_CLASS (klass);

  event_klass->update_description = (void (*) (FlowEvent *)) flow_shell_op_update_description;
}

static void
flow_shell_op_init (FlowShellOp *shell_op)
{
}

static void
flow_shell_op_construct (FlowShellOp *shell_op)
{
  FlowShellOpPrivate *priv = shell_op->priv;

  g_assert (priv->cmd != NULL);
}

static void
flow_shell_op_dispose (FlowShellOp *shell_op)
{
}

static void
flow_shell_op_finalize (FlowShellOp *shell_op)
{
  FlowShellOpPrivate *priv = shell_op->priv;

  g_free (priv->cmd);
  priv->cmd = NULL;
}

/* --- FlowShellOp public API --- */

/**
 * flow_shell_op_new:
 * @cmd:               The local file system cmd to open or create.
 *
 * Creates a new #FlowShellOp.
 *
 * Return value: A new #FlowShellOp.
 **/
FlowShellOp *
flow_shell_op_new (const gchar *cmd)
{
  g_return_val_if_fail (cmd != NULL, NULL);

  return g_object_new (FLOW_TYPE_SHELL_OP,
                       "cmd",               cmd,
                       NULL);
}

/**
 * flow_shell_op_get_cmd:
 * @shell_op: A #FlowShellOp.
 *
 * Returns the local file system cmd to open or create. The string
 * belongs to @shell_op, and should not be freed.
 *
 * Return value: The local cmd to open or create.
 **/
const gchar *
flow_shell_op_get_cmd (FlowShellOp *shell_op)
{
  FlowShellOpPrivate *priv;

  g_return_val_if_fail (FLOW_IS_SHELL_OP (shell_op), NULL);

  priv = shell_op->priv;
  return priv->cmd;
}
