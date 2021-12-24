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

static void
main_context_destroy_notify (GMainContext *main_context)
{
  if (main_context)
    g_main_context_unref (main_context);
}

static GPrivate context_for_current_thread = G_PRIVATE_INIT ((GDestroyNotify) main_context_destroy_notify);

static GMainContext *
get_main_context_for_current_thread (void)
{
  GMainContext *main_context;

  main_context = g_private_get (&context_for_current_thread);

  if (!main_context)
  {
    GThread *thread_self;

    /* This thread has not been assigned a main context yet; do it now */

    thread_self = g_thread_self ();
    g_assert (thread_self != NULL);

    if (thread_self->func == NULL)
    {
      /* Ref the default main context */

      /* Only the main thread and non-glib threads will have
       * a NULL func. Assume this is the main thread. */

      main_context = g_main_context_default ();
      g_main_context_ref (main_context);
    }
    else
    {
      /* Create a new main context for this thread */

      main_context = g_main_context_new ();
    }

    g_private_set (&context_for_current_thread, main_context);
  }

  return main_context;
}

static void
set_main_context_for_current_thread (GMainContext *main_context)
{
  GMainContext *old_main_context;

  old_main_context = g_private_get (&context_for_current_thread);
  if (old_main_context)
  {
    /* The thread has already been assigned a main context. This is an error. */

    g_warning ("The main context can only be set once per thread. Getting "
               "the main context will automatically create it if it hasn't "
               "already been set. Please set the main context before using "
               "anything that depends on it.");
    return;
  }

  g_main_context_ref (main_context);
  g_private_set (&context_for_current_thread, main_context);
}

/**
 * flow_get_main_context_for_current_thread:
 * 
 * Gets the current thread's main context. If the context
 * does not exist, it will be created. All asynchronous callbacks for
 * the current thread will be dispatched from this context.
 * 
 * Return value: A #GMainContext.
 **/
GMainContext *
flow_get_main_context_for_current_thread (void)
{
  return get_main_context_for_current_thread ();
}

/**
 * flow_set_main_context_for_current_thread:
 * @main_context: A #GMainContext.
 *
 * Sets the current thread's main context. This can only be done once
 * per thread, before any calls to flow_get_main_context_for_current_thread ().
 * You should do this before invoking any other Flow function in the thread.
 * 
 * Return value: A #GMainContext.
 **/
void
flow_set_main_context_for_current_thread (GMainContext *main_context)
{
  g_return_if_fail (main_context != NULL);

  set_main_context_for_current_thread (main_context);
}

/**
 * flow_idle_add_full:
 * @dispatch_context: A #GMainContext.
 * @priority: The priority of the idle source. Typically this will be in the range betweeen #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE.
 * @func: Function to call.
 * @data: Data to pass to @func.
 * @notify: Function to call when the idle is removed, or %NULL.
 * 
 * Adds a function to be called whenever there are no higher priority events
 * pending. If the function returns %FALSE it is automatically removed from the
 * list of event sources and will not be called again.
 *
 * The callback is dispatched from @dispatch_context, in the thread running it.
 * 
 * Return value: The ID (greater than 0) of the event source.
 **/
guint
flow_idle_add_full (GMainContext *dispatch_context, gint priority, GSourceFunc func, gpointer data,
                    GDestroyNotify notify)
{
  GSource *idle_source;
  guint    id;

  if (!dispatch_context)
    dispatch_context = get_main_context_for_current_thread ();

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, priority);
  g_source_set_callback (idle_source, func, data, notify);

  id = g_source_attach (idle_source, dispatch_context);
  g_source_unref (idle_source);

  return id;
}

/**
 * flow_idle_add_to_current_thread:
 * @func: Function to call.
 * @data: Data to pass to @func.
 * 
 * Adds a function to be called whenever there are no higher priority events
 * pending to the current thread's main loop. The function is given the default
 * idle priority, #G_PRIORITY_DEFAULT_IDLE. If the function returns %FALSE it
 * is automatically removed from the list of event sources and will not be called again.
 *
 * The callback is dispatched from the current thread's main context.
 * 
 * Return value: The ID (greater than 0) of the event source.
 **/
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

/**
 * flow_timeout_add_full:
 * @dispatch_context: A #GMainContext.
 * @priority: The priority of the timeout source. Typically this will be in the range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE.
 * @interval: The time between calls to the function, in milliseconds (1/1000ths of a second).
 * @func: Function to call.
 * @data: Data to pass to @func.
 * @notify: Function to call when the timeout is removed, or %NULL.
 * 
 * Sets a function to be called at regular intervals, with the given priority.
 * The function is called repeatedly until it returns %FALSE, at which point the
 * timeout is automatically destroyed and the function will not be called
 * again. The notify function is called when the timeout is destroyed. The
 * first call to the function will be at the end of the first interval.
 *
 * Note that timeout functions may be delayed, due to the processing of other
 * event sources. Thus they should not be relied on for precise timing. After
 * each call to the timeout function, the time of the next timeout is recalculated
 * based on the current time and the given interval (it does not try to 'catch up'
 * time lost in delays).
 * 
 * The callback is dispatched from @dispatch_context, in the thread running it.
 *
 * Return value: The ID (greater than 0) of the event source.
 **/
guint
flow_timeout_add_full (GMainContext *dispatch_context, gint priority, guint interval,
                       GSourceFunc func, gpointer data, GDestroyNotify notify)
{
  GSource      *timeout_source;
  guint         id;

  if (!dispatch_context)
    dispatch_context = get_main_context_for_current_thread ();

  timeout_source = g_timeout_source_new (interval);
  g_source_set_priority (timeout_source, priority);
  g_source_set_callback (timeout_source, func, data, notify);

  id = g_source_attach (timeout_source, dispatch_context);
  g_source_unref (timeout_source);

  return id;
}

/**
 * flow_timeout_add_seconds_full:
 * @dispatch_context: A #GMainContext.
 * @priority: The priority of the timeout source. Typically this will be in the range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE.
 * @interval: The time between calls to the function, in seconds.
 * @func: Function to call.
 * @data: Data to pass to @func.
 * @notify: Function to call when the timeout is removed, or %NULL.
 * 
 * Sets a function to be called at regular intervals, with the given priority.
 * The function is called repeatedly until it returns %FALSE, at which point the
 * timeout is automatically destroyed and the function will not be called
 * again. The notify function is called when the timeout is destroyed. The
 * first call to the function will be at the end of the first interval.
 *
 * Note that timeout functions may be delayed, due to the processing of other
 * event sources. Thus they should not be relied on for precise timing. After
 * each call to the timeout function, the time of the next timeout is recalculated
 * based on the current time and the given interval (it does not try to 'catch up'
 * time lost in delays).
 * 
 * Unlike flow_timeout_add_full(), this function operates at whole second granularity.
 * The initial starting point of the timer is determined by the implementation and the
 * implementation is expected to group multiple timers together so that they fire
 * all at the same time. To allow this grouping, the interval to the first timer
 * is rounded and can deviate up to one second from the specified interval.
 * Subsequent timer iterations will generally run at the specified interval.
 * 
 * The callback is dispatched from @dispatch_context, in the thread running it.
 *
 * Return value: The ID (greater than 0) of the event source.
 **/
guint
flow_timeout_add_seconds_full (GMainContext *dispatch_context, gint priority, guint interval,
                               GSourceFunc func, gpointer data, GDestroyNotify notify)
{
  GSource      *timeout_source;
  guint         id;

  if (!dispatch_context)
    dispatch_context = get_main_context_for_current_thread ();

  timeout_source = g_timeout_source_new_seconds (interval);
  g_source_set_priority (timeout_source, priority);
  g_source_set_callback (timeout_source, func, data, notify);

  id = g_source_attach (timeout_source, dispatch_context);
  g_source_unref (timeout_source);

  return id;
}

/**
 * flow_timeout_add_to_current_thread:
 * @interval: The time between calls to the function, in milliseconds (1/1000ths of a second).
 * @func: Function to call.
 * @data: Data to pass to @func.
 * 
 * Sets a function to be called at regular intervals, with the default
 * priority, #G_PRIORITY_DEFAULT. The function is called repeatedly until
 * it returns %FALSE, at which point the timeout is automatically destroyed
 * and the function will not be called again. The first call to the
 * function will be at the end of the first interval.
 * 
 * Note that timeout functions may be delayed, due to the processing of
 * other event sources. Thus they should not be relied on for precise timing.
 * After each call to the timeout function, the time of the next timeout
 * is recalculated based on the current time and the given interval (it does
 * not try to 'catch up' time lost in delays). 
 * 
 * The callback is dispatched from the current thread's main context.
 * 
 * Return value: The ID (greater than 0) of the event source.
 **/
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

/**
 * flow_timeout_add_seconds_to_current_thread:
 * @interval: The time between calls to the function, in milliseconds (1/1000ths of a second).
 * @func: Function to call.
 * @data: Data to pass to @func.
 * 
 * Sets a function to be called at regular intervals, with the default
 * priority, #G_PRIORITY_DEFAULT. The function is called repeatedly until
 * it returns %FALSE, at which point the timeout is automatically destroyed
 * and the function will not be called again. The first call to the
 * function will be at the end of the first interval.
 * 
 * Note that timeout functions may be delayed, due to the processing of
 * other event sources. Thus they should not be relied on for precise timing.
 * After each call to the timeout function, the time of the next timeout
 * is recalculated based on the current time and the given interval (it does
 * not try to 'catch up' time lost in delays). 
 * 
 * Unlike flow_timeout_add_to_current_thread(), this function operates at
 * whole second granularity. The initial starting point of the timer is
 * determined by the implementation and the implementation is expected to group
 * multiple timers together so that they fire all at the same time. To allow this
 * grouping, the interval to the first timer is rounded and can deviate up to one
 * second from the specified interval. Subsequent timer iterations will generally
 * run at the specified interval.
 * 
 * The callback is dispatched from the current thread's main context.
 * 
 * Return value: The ID (greater than 0) of the event source.
 **/
guint
flow_timeout_add_seconds_to_current_thread (guint interval, GSourceFunc func, gpointer data)
{
  GMainContext *main_context;
  GSource      *timeout_source;
  guint         id;

  main_context = get_main_context_for_current_thread ();

  timeout_source = g_timeout_source_new_seconds (interval);
  g_source_set_callback (timeout_source, func, data, NULL);

  id = g_source_attach (timeout_source, main_context);
  g_source_unref (timeout_source);

  return id;
}

/**
 * flow_source_remove:
 * @dispatch_context: A #GMainContext.
 * @source_id: The ID of the source to remove.
 * 
 * Removes the source identified by @source_id from @dispatch_context. If
 * @dispatch_context belongs to another thread, you must hold a reference
 * to it.
 **/
void
flow_source_remove (GMainContext *dispatch_context, guint source_id)
{
  GSource *source;

  if (!dispatch_context)
    dispatch_context = get_main_context_for_current_thread ();

  source = g_main_context_find_source_by_id (dispatch_context, source_id);

  if G_UNLIKELY (!source)
  {
    g_warning ("Tried to remove non-existent source %u from context %p.",
               source_id, dispatch_context);
    return;
  }

  g_source_destroy (source);
}

/**
 * flow_source_remove_from_current_thread:
 * @source_id: The ID of the source to remove.
 * 
 * Removes the source identified by @source_id from the current thread's
 * main context.
 **/
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
