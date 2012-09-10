/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-shell-op.h - Operation: Execute a shell command.
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

#ifndef _FLOW_SHELL_OP_H
#define _FLOW_SHELL_OP_H

#include <glib-object.h>

#define FLOW_TYPE_SHELL_OP            (flow_shell_op_get_type ())
#define FLOW_SHELL_OP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_SHELL_OP, FlowShellOp))
#define FLOW_SHELL_OP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_SHELL_OP, FlowShellOpClass))
#define FLOW_IS_SHELL_OP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_SHELL_OP))
#define FLOW_IS_SHELL_OP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_SHELL_OP))
#define FLOW_SHELL_OP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_SHELL_OP, FlowShellOpClass))
GType   flow_shell_op_get_type        (void) G_GNUC_CONST;

typedef struct _FlowShellOp        FlowShellOp;
typedef struct _FlowShellOpPrivate FlowShellOpPrivate;
typedef struct _FlowShellOpClass   FlowShellOpClass;

struct _FlowShellOp
{
  FlowEvent   parent;

  /*< private >*/

  FlowShellOpPrivate *priv;
};

struct _FlowShellOpClass
{
  FlowEventClass parent_class;

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowShellOp       *flow_shell_op_new        (const gchar *cmd);

const gchar       *flow_shell_op_get_cmd    (FlowShellOp *shell_op);

#endif /* _FLOW_SHELL_OP_H */
