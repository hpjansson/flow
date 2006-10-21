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

static GStaticPrivate context_for_current_thread = G_STATIC_PRIVATE_INIT;

static void
free_context_storage (GMainContext **main_context_storage)
{
  g_main_context_unref (*main_context_storage);
  g_free (main_context_storage);
}

static inline GMainContext **
get_context_storage (void)
{
  GMainContext **main_context_storage = g_static_private_get (&context_for_current_thread);

  if G_UNLIKELY (!main_context_storage)
  {
    main_context_storage = g_new (GMainContext *, 1);
    *main_context_storage = NULL;
    g_static_private_set (&context_for_current_thread, main_context_storage, (GDestroyNotify) free_context_storage);
  }

  return main_context_storage;
}

GMainContext *
flow_get_main_context_for_current_thread (void)
{
  GMainContext **main_context_storage = get_context_storage ();

  if G_UNLIKELY (*main_context_storage == NULL)
  {
    GThread *self_thread = g_thread_self ();

    if (self_thread->func == NULL)
    {
      /* Only the main thread and non-glib threads will have
       * a NULL func. Assume this is the main thread. */

      *main_context_storage = g_main_context_default ();
      g_main_context_ref (*main_context_storage);
    }
    else
    {
      /* For all other threads that haven't been assigned a
       * context by the user, create a context. */

      *main_context_storage = g_main_context_new ();
    }
  }

  return *main_context_storage;
}

void
flow_set_main_context_for_current_thread (GMainContext *main_context)
{
  GMainContext **main_context_storage;

  g_return_if_fail (main_context != NULL);

  main_context_storage = get_context_storage ();

  if G_UNLIKELY (*main_context_storage)
  {
    g_warning ("The main context can only be set once per thread. Getting "
               "the main context will automatically create it if it hasn't "
               "already been set. Please set the main context before using "
               "anything that depends on it.");
    return;
  }

  *main_context_storage = main_context;
}
