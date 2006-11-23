/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-context-mgmt.h - Manage per-thread GMainContexts.
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

#ifndef _FLOW_CONTEXT_MGMT_H
#define _FLOW_CONTEXT_MGMT_H

#include <glib.h>

G_BEGIN_DECLS

GMainContext *flow_get_main_context_for_thread         (GThread *thread);
void          flow_set_main_context_for_thread         (GThread *thread, GMainContext *main_context);

GMainContext *flow_get_main_context_for_current_thread (void);
void          flow_set_main_context_for_current_thread (GMainContext *main_context);

guint         flow_idle_add_full                       (GThread *dispatch_thread, gint priority,
                                                        GSourceFunc func, gpointer data, GDestroyNotify notify);
guint         flow_idle_add_to_current_thread          (GSourceFunc func, gpointer data);

guint         flow_timeout_add_full                    (GThread *dispatch_thread, gint priority, guint interval,
                                                        GSourceFunc func, gpointer data, GDestroyNotify notify);
guint         flow_timeout_add_to_current_thread       (guint interval, GSourceFunc func, gpointer data);

void          flow_source_remove                       (GThread *dispatch_thread, guint source_id);
void          flow_source_remove_from_current_thread   (guint source_id);

G_END_DECLS

#endif  /* _FLOW_CONTEXT_MGMT_H */
