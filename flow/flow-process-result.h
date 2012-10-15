/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-process-result.h - A process result that can be propagated along a pipeline.
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

#ifndef _FLOW_PROCESS_RESULT_H
#define _FLOW_PROCESS_RESULT_H

#include <flow/flow-event.h>

G_BEGIN_DECLS

#define FLOW_TYPE_PROCESS_RESULT            (flow_process_result_get_type ())
#define FLOW_PROCESS_RESULT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_PROCESS_RESULT, FlowProcessResult))
#define FLOW_PROCESS_RESULT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_PROCESS_RESULT, FlowProcessResultClass))
#define FLOW_IS_PROCESS_RESULT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_PROCESS_RESULT))
#define FLOW_IS_PROCESS_RESULT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_PROCESS_RESULT))
#define FLOW_PROCESS_RESULT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_PROCESS_RESULT, FlowProcessResultClass))
GType   flow_process_result_get_type        (void) G_GNUC_CONST;

typedef struct _FlowProcessResult        FlowProcessResult;
typedef struct _FlowProcessResultPrivate FlowProcessResultPrivate;
typedef struct _FlowProcessResultClass   FlowProcessResultClass;

struct _FlowProcessResult
{
  FlowEvent parent;

  /*< private >*/

  FlowProcessResultPrivate *priv;
};

struct _FlowProcessResultClass
{
  FlowEventClass parent_class;

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowProcessResult *flow_process_result_new            (gint result);

gint               flow_process_result_get_result     (FlowProcessResult *process_result);

G_END_DECLS

#endif  /* _FLOW_PROCESS_RESULT_H */
