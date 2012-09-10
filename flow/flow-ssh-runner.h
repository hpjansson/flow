/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ssh-runner.h - Origin/endpoint for connected SSH session.
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

#ifndef _FLOW_SSH_RUNNER_H
#define _FLOW_SSH_RUNNER_H

#include <glib-object.h>
#include <flow-connector.h>
#include <flow-shunt.h>

G_BEGIN_DECLS

#define FLOW_TYPE_SSH_RUNNER            (flow_ssh_runner_get_type ())
#define FLOW_SSH_RUNNER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_SSH_RUNNER, FlowSshRunner))
#define FLOW_SSH_RUNNER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_SSH_RUNNER, FlowSshRunnerClass))
#define FLOW_IS_SSH_RUNNER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_SSH_RUNNER))
#define FLOW_IS_SSH_RUNNER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_SSH_RUNNER))
#define FLOW_SSH_RUNNER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_SSH_RUNNER, FlowSshRunnerClass))
GType   flow_ssh_runner_get_type        (void) G_GNUC_CONST;

typedef struct _FlowSshRunner        FlowSshRunner;
typedef struct _FlowSshRunnerPrivate FlowSshRunnerPrivate;
typedef struct _FlowSshRunnerClass   FlowSshRunnerClass;

struct _FlowSshRunner
{
  FlowConnector    parent;

  /*< private >*/

  FlowSshRunnerPrivate *priv;
};

struct _FlowSshRunnerClass
{
  FlowConnectorClass parent_class;

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowSshRunner    *flow_ssh_runner_new                (void);

FlowIPService    *flow_ssh_runner_get_remote_service (FlowSshRunner *ssh_runner);

G_END_DECLS

#endif  /* _FLOW_SSH_RUNNER_H */
