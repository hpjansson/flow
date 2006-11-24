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

#include "config.h"
#include "flow-context-mgmt.h"

static GStaticMutex    global_context_mutex       = G_STATIC_MUTEX_INIT;
static GHashTable     *global_context_table       = NULL;
static GStaticPrivate  context_for_current_thread = G_STATIC_PRIVATE_INIT;
static GStaticPrivate  atexit_for_current_thread  = G_STATIC_PRIVATE_INIT;

static GMainContext **
get_context_storage_global (GThread *thread)
{
  GMainContext **context_storage = NULL;

  g_static_mutex_lock (&global_context_mutex);

  if G_LIKELY (global_context_table)
  {
    context_storage = g_hash_table_lookup (global_context_table, thread);
  }

  g_static_mutex_unlock (&global_context_mutex);

  return context_storage;
}

static void
set_context_storage_global (GThread *thread, GMainContext **context_storage)
{
  g_static_mutex_lock (&global_context_mutex);

  if G_UNLIKELY (!global_context_table)
    global_context_table = g_hash_table_new (g_direct_hash, g_direct_equal);

  if (context_storage)
    g_hash_table_insert (global_context_table, thread, context_storage);
  else
    g_hash_table_remove (global_context_table, thread);

  g_static_mutex_unlock (&global_context_mutex);
}

static GMainContext **
create_context_storage (GThread *thread, GMainContext *main_context)
{
  GMainContext **main_context_storage;

  if (!main_context)
  {
    if (thread->func == NULL)
    {
      /* Only the main thread and non-glib threads will have
       * a NULL func. Assume this is the main thread. */

      main_context = g_main_context_default ();
      g_main_context_ref (main_context);
    }
    else
    {
      main_context = g_main_context_new ();
    }
  }

  main_context_storage = g_new (GMainContext *, 1);
  *main_context_storage = main_context;

  return main_context_storage;
}

static void
free_thread_data (GThread *thread)
{
  GMainContext **main_context_storage;

  /* NOTE: g_thread_self () won't work in here */

  main_context_storage = get_context_storage_global (thread);
  set_context_storage_global (thread, NULL);

  g_assert (main_context_storage != NULL);

  g_main_context_unref (*main_context_storage);
  *main_context_storage = NULL;
  g_free (main_context_storage);
}

static GMainContext **
ensure_context_storage (GThread *thread, GMainContext *desired_context)
{
  GMainContext **main_context_storage = NULL;

  main_context_storage = get_context_storage_global (thread);

  if (!main_context_storage)
  {
    GThread *thread_self;

    main_context_storage = create_context_storage (thread, desired_context);
    set_context_storage_global (thread, main_context_storage);

    thread_self = g_thread_self ();

    if (thread == thread_self)
    {
      g_static_private_set (&atexit_for_current_thread, thread_self, (GDestroyNotify) free_thread_data);
      g_static_private_set (&context_for_current_thread, main_context_storage, NULL);
    }
  }
  else if (desired_context)
  {
    /* We found an existing main_context_storage, but the user requested a
     * desired_context be set. This is an error. */

    g_warning ("The main context can only be set once per thread. Getting "
               "the main context will automatically create it if it hasn't "
               "already been set. Please set the main context before using "
               "anything that depends on it.");
  }

  return main_context_storage;
}

static GMainContext **
ensure_context_storage_self (void)
{
  GMainContext **main_context_storage;

  main_context_storage = g_static_private_get (&context_for_current_thread);

  if (!main_context_storage)
  {
    GThread *thread_self;

    /* No context in local storage; look in global table in case our
     * context was created by another thread. */

    thread_self = g_thread_self ();
    main_context_storage = get_context_storage_global (thread_self);

    if (!main_context_storage)
    {
      main_context_storage = create_context_storage (thread_self, NULL);
      set_context_storage_global (thread_self, main_context_storage);
    }

    g_static_private_set (&atexit_for_current_thread, thread_self, (GDestroyNotify) free_thread_data);
    g_static_private_set (&context_for_current_thread, main_context_storage, NULL);
  }

  return main_context_storage;
}

static inline GMainContext *
get_main_context_for_current_thread (void)
{
  GMainContext **main_context_storage;

  main_context_storage = ensure_context_storage_self ();
  return *main_context_storage;
}

static inline GMainContext *
get_main_context_for_thread (GThread *thread)
{
  GMainContext **main_context_storage;

  main_context_storage = ensure_context_storage (thread, NULL);
  return *main_context_storage;
}

GMainContext *
flow_get_main_context_for_current_thread (void)
{
  return get_main_context_for_current_thread ();
}

GMainContext *
flow_get_main_context_for_thread (GThread *thread)
{
  g_return_val_if_fail (thread != NULL, NULL);

  if G_LIKELY (thread)
    return get_main_context_for_thread (thread);
  else
    return get_main_context_for_current_thread ();
}

void
flow_set_main_context_for_thread (GThread *thread, GMainContext *main_context)
{
  g_return_if_fail (thread != NULL);

  ensure_context_storage (thread, main_context);
}

guint
flow_idle_add_full (GThread *dispatch_thread, gint priority, GSourceFunc func, gpointer data,
                    GDestroyNotify notify)
{
  GMainContext *main_context;
  GSource      *idle_source;
  guint         id;

  main_context = get_main_context_for_thread (dispatch_thread);

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, priority);
  g_source_set_callback (idle_source, func, data, notify);

  id = g_source_attach (idle_source, main_context);
  g_source_unref (idle_source);

  return id;
}

guint
flow_idle_add_to_current_thread (GSourceFunc func, gpointer data)
{
  GMainContext *main_context;
  GSource      *idle_source;
  guint         id;

  main_context = get_main_context_for_current_thread ();

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source, func, data, NULL);

  id = g_source_attach (idle_source, main_context);
  g_source_unref (idle_source);

  return id;
}

guint
flow_timeout_add_full (GThread *dispatch_thread, gint priority, guint interval,
                       GSourceFunc func, gpointer data, GDestroyNotify notify)
{
  GMainContext *main_context;
  GSource      *timeout_source;
  guint         id;

  main_context = get_main_context_for_thread (dispatch_thread);

  timeout_source = g_timeout_source_new (interval);
  g_source_set_priority (timeout_source, priority);
  g_source_set_callback (timeout_source, func, data, notify);

  id = g_source_attach (timeout_source, main_context);
  g_source_unref (timeout_source);

  return id;
}

guint
flow_timeout_add_to_current_thread (guint interval, GSourceFunc func, gpointer data)
{
  GMainContext *main_context;
  GSource      *timeout_source;
  guint         id;

  main_context = get_main_context_for_current_thread ();

  timeout_source = g_timeout_source_new (interval);
  g_source_set_callback (timeout_source, func, data, NULL);

  id = g_source_attach (timeout_source, main_context);
  g_source_unref (timeout_source);

  return id;
}

void
flow_source_remove (GThread *dispatch_thread, guint source_id)
{
  GMainContext *main_context;
  GSource      *source;

  if (dispatch_thread)
    main_context = get_main_context_for_thread (dispatch_thread);
  else
    main_context = get_main_context_for_current_thread ();

  source = g_main_context_find_source_by_id (main_context, source_id);

  if G_UNLIKELY (!source)
  {
    g_warning ("Tried to remove non-existent source %u from thread %p's context.",
               source_id, dispatch_thread);
    return;
  }

  g_source_destroy (source);
}

void
flow_source_remove_from_current_thread (guint source_id)
{
  GMainContext *main_context;
  GSource      *source;

  main_context = get_main_context_for_current_thread ();
  source = g_main_context_find_source_by_id (main_context, source_id);

  if G_UNLIKELY (!source)
  {
    g_warning ("Tried to remove non-existent source %u from current thread's context.",
               source_id);
    return;
  }

  g_source_destroy (source);
}
