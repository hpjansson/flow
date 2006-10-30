/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-user-adapter.h - Lets arbitrary code interface with a stream.
 *
 * Copyright (C) 2006 Hans Petter Jansson
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

#ifndef _FLOW_USER_ADAPTER_H
#define _FLOW_USER_ADAPTER_H

#include <flow/flow-simplex-element.h>

G_BEGIN_DECLS

#define FLOW_TYPE_USER_ADAPTER            (flow_user_adapter_get_type ())
#define FLOW_USER_ADAPTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_USER_ADAPTER, FlowUserAdapter))
#define FLOW_USER_ADAPTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_USER_ADAPTER, FlowUserAdapterClass))
#define FLOW_IS_USER_ADAPTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_USER_ADAPTER))
#define FLOW_IS_USER_ADAPTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_USER_ADAPTER))
#define FLOW_USER_ADAPTER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_USER_ADAPTER, FlowUserAdapterClass))
GType   flow_user_adapter_get_type        (void) G_GNUC_CONST;

typedef struct _FlowUserAdapter      FlowUserAdapter;
typedef struct _FlowUserAdapterClass FlowUserAdapterClass;

struct _FlowUserAdapter
{
  /* --- Private --- */

  FlowSimplexElement  parent;

  FlowPacketQueue    *input_queue;
  FlowPacketQueue    *output_queue;

  GMainLoop          *input_loop;
  GMainLoop          *output_loop;

  FlowNotifyFunc      user_notify_func;
  gpointer            user_notify_data;

  /* waiting_for_... means the user is in a blocking call. Blocking calls can
   * be recursive if the GMainContext is shared with other facilities, so we
   * maintain a depth count.
   *
   * This is used to prevent async notifications from firing while we're in
   * a blocking call (for apps that mix sync and async access). */

  guint16             waiting_for_input;
  guint16             waiting_for_output;
};

struct _FlowUserAdapterClass
{
  FlowSimplexElementClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowUserAdapter *flow_user_adapter_new              (void);

void             flow_user_adapter_set_notify_func  (FlowUserAdapter *user_adapter, FlowNotifyFunc func, gpointer user_data);

FlowPacketQueue *flow_user_adapter_get_input_queue  (FlowUserAdapter *user_adapter);
FlowPacketQueue *flow_user_adapter_get_output_queue (FlowUserAdapter *user_adapter);

void             flow_user_adapter_push             (FlowUserAdapter *user_adapter);

void             flow_user_adapter_block            (FlowUserAdapter *user_adapter);
void             flow_user_adapter_unblock          (FlowUserAdapter *user_adapter);

void             flow_user_adapter_wait_for_input   (FlowUserAdapter *user_adapter);
void             flow_user_adapter_wait_for_output  (FlowUserAdapter *user_adapter);

void             flow_user_adapter_interrupt_input  (FlowUserAdapter *user_adapter);
void             flow_user_adapter_interrupt_output (FlowUserAdapter *user_adapter);

G_END_DECLS

#endif  /* _FLOW_USER_ADAPTER_H */
