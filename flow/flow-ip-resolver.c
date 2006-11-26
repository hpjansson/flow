/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-resolver.c - IP address resolver.
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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "flow-marshallers.h"
#include "flow-gobject-util.h"
#include "flow-ip-addr.h"
#include "flow-ip-resolver.h"
#include "flow-context-mgmt.h"

#define DEBUG(x)

/* Max lookup attempts when server is returning "temporary failure" */
#define MAX_LOOKUP_ATTEMPTS 2

#define ID_BITS 24

/* --- FlowIPResolver private data --- */

typedef struct
{
  GMutex      *mutex;
  GThreadPool *thread_pool;
  GHashTable  *lookup_table;

  guint        next_lookup_id;
}
FlowIPResolverPrivate;

typedef struct
{
  FlowIPResolver   *resolver;              /* Resolver we belong to */

  guint             id         : ID_BITS;  /* ID of this request */
  guint             is_reverse : 1;        /* TRUE if we're translating IP -> name */
  guint             is_running : 1;        /* TRUE if thread picked it up */
  guint             is_wanted  : 1;        /* TRUE unless cancelled */

  gpointer          arg;                   /* (gchar *), or (FlowIPAddr *) on reverse lookup */
  GList            *results;

  GThread          *dispatch_thread;       /* Thread to dispatch result in */
}
Lookup;

/* --- Implementation prototypes --- */

static GList *flow_ip_resolver_impl_lookup_by_name (const gchar *name);
static GList *flow_ip_resolver_impl_lookup_by_addr (FlowIPAddr *ip_addr);

/* Select an implementation */
#include "flow-ip-resolver-impl-unix.c"

static FlowIPResolver *default_resolver = NULL;

static void
free_object_list (GList *list)
{
  GList *l;

  for (l = list; l; l = g_list_next (l))
    g_object_unref (l->data);

  g_list_free (list);
}

static void
free_deep_list (GList *list)
{
  GList *l;

  for (l = list; l; l = g_list_next (l))
    g_free (l->data);

  g_list_free (list);
}

static void
destroy_lookup (Lookup *lookup)
{
  FlowIPResolver        *ip_resolver = lookup->resolver;
  FlowIPResolverPrivate *priv        = ip_resolver->priv;
  guint                  id          = lookup->id;

  /* Remove from lookup table */

  g_hash_table_remove (priv->lookup_table, GUINT_TO_POINTER (id));

  /* Free memory */

  if (lookup->is_reverse)
  {
    g_object_unref (lookup->arg);
    free_deep_list (lookup->results);
  }
  else
  {
    g_free (lookup->arg);
    free_object_list (lookup->results);
  }

  g_free (lookup);
}

static gboolean
dispatch_lookup (Lookup *lookup)
{
  FlowIPResolver        *resolver = lookup->resolver;
  FlowIPResolverPrivate *priv     = resolver->priv;

  g_object_ref (resolver);

  g_mutex_lock (priv->mutex);

  if (lookup->is_wanted)
  {
    gboolean  is_reverse;
    GList    *arg_list = NULL;
    GList    *result_list;

    is_reverse = lookup->is_reverse;

    arg_list    = g_list_append (arg_list, lookup->arg);
    result_list = lookup->results;

    g_mutex_unlock (priv->mutex);

    if (is_reverse)
      g_signal_emit_by_name (resolver, "resolved", arg_list, result_list);
    else
      g_signal_emit_by_name (resolver, "resolved", result_list, arg_list);

    g_mutex_lock (priv->mutex);

    g_list_free (arg_list);
  }

  destroy_lookup (lookup);
  g_mutex_unlock (priv->mutex);

  g_object_unref (resolver);

  return FALSE;
}

static void
do_lookup (Lookup *lookup, FlowIPResolver *resolver)
{
  FlowIPResolverPrivate *priv = resolver->priv;

  g_mutex_lock (priv->mutex);

  if (lookup->is_wanted)
  {
    GThread  *dispatch_thread;
    gboolean  is_reverse;
    gpointer  arg;

    lookup->is_running = TRUE;

    dispatch_thread = lookup->dispatch_thread;
    is_reverse      = lookup->is_reverse;
    arg             = lookup->arg;

    g_mutex_unlock (priv->mutex);

    if (is_reverse)
    {
      /* Address in, names out */
      lookup->results = flow_ip_resolver_impl_lookup_by_addr (lookup->arg);
    }
    else
    {
      /* Name in, addresses out */
      lookup->results = flow_ip_resolver_impl_lookup_by_name (lookup->arg);
    }

    /* FIXME: Might need a destroy notify, in case the context goes away while we
     * have outstanding events. */
    flow_idle_add_full (dispatch_thread, G_PRIORITY_DEFAULT_IDLE,
                        (GSourceFunc) dispatch_lookup, lookup, NULL);
  }
  else
  {
    destroy_lookup (lookup);
    g_mutex_unlock (priv->mutex);
  }
}

static guint
create_lookup (FlowIPResolver *resolver, gboolean is_reverse, gpointer arg)
{
  FlowIPResolverPrivate *priv = resolver->priv;
  Lookup                *lookup;
  guint                  id;

  lookup = g_new0 (Lookup, 1);

  lookup->resolver        = resolver;
  lookup->is_reverse      = is_reverse;
  lookup->is_wanted       = TRUE;
  lookup->arg             = arg;
  lookup->dispatch_thread = g_thread_self ();

  /* Make sure this thread has a main context that we can dispatch in */
  flow_get_main_context_for_current_thread ();

  g_mutex_lock (priv->mutex);

  /* Assign an ID that is not 0 and not in use */

  for ( ; ; priv->next_lookup_id++)
  {
    priv->next_lookup_id %= (1 << ID_BITS);
    if (!priv->next_lookup_id)
      continue;

    if (!g_hash_table_lookup (priv->lookup_table, GUINT_TO_POINTER (priv->next_lookup_id)))
      break;
  }

  id = lookup->id = priv->next_lookup_id;

  /* Insert in lookup table */
  g_hash_table_insert (priv->lookup_table, GUINT_TO_POINTER (id), lookup);

  /* Queue request */
  g_thread_pool_push (priv->thread_pool, lookup, NULL);

  g_mutex_unlock (priv->mutex);

  return id;
}

/* --- FlowIPResolver properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ip_resolver)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowIPResolver definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_ip_resolver, FlowIPResolver, G_TYPE_OBJECT, 0)

static void
flow_ip_resolver_type_init (GType type)
{
}

static void
flow_ip_resolver_class_init (FlowIPResolverClass *klass)
{
  static const GType param_types [2] = { G_TYPE_POINTER, G_TYPE_POINTER };

  /* Make sure types are registered before child threads try to use them;
   * type registration is not protected by a mutex. */

  g_type_class_ref (FLOW_TYPE_IP_ADDR);

  /* Signals */

  g_signal_newv ("resolved",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 NULL,                                   /* Class closure */
                 NULL, NULL,                             /* Accumulator, accu data */
                 flow_marshal_VOID__POINTER_POINTER,     /* Marshaller */
                 G_TYPE_NONE,                            /* Return type */
                 2, (GType *) param_types);              /* Number of params, param types */
}

static void
flow_ip_resolver_init (FlowIPResolver *ip_resolver)
{
  FlowIPResolverPrivate *priv = ip_resolver->priv;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  priv->thread_pool = g_thread_pool_new ((GFunc) do_lookup, ip_resolver, 4, FALSE, NULL);
  if (!priv->thread_pool)
    g_error ("Could not create thread pool for FlowIPResolver.");

  priv->mutex = g_mutex_new ();
  priv->lookup_table = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
flow_ip_resolver_construct (FlowIPResolver *ip_resolver)
{
}

static void
flow_ip_resolver_dispose (FlowIPResolver *ip_resolver)
{
}

static void
flow_ip_resolver_finalize (FlowIPResolver *ip_resolver)
{
  FlowIPResolverPrivate *priv = ip_resolver->priv;

  /* TODO: Cancel pending lookups */

  g_thread_pool_free (priv->thread_pool,
                      TRUE /* immediate (ignore queue) */,
                      TRUE /* wait (for workers to finish) */);
  g_mutex_free (priv->mutex);
  g_hash_table_destroy (priv->lookup_table);
}

FlowIPResolver *
flow_ip_resolver_new (void)
{
  return g_object_new (FLOW_TYPE_IP_RESOLVER, NULL);
}

FlowIPResolver *
flow_ip_resolver_get_default (void)
{
  if (!default_resolver)
  {
    default_resolver = flow_ip_resolver_new ();
    g_object_add_weak_pointer ((GObject *) default_resolver, (gpointer) &default_resolver);
  }

  return default_resolver;
}

guint
flow_ip_resolver_resolve_name (FlowIPResolver *resolver, const gchar *name)
{
  g_return_val_if_fail (FLOW_IS_IP_RESOLVER (resolver), 0);
  g_return_val_if_fail (name != NULL, 0);

  return create_lookup (resolver, FALSE, g_strdup (name));
}

guint
flow_ip_resolver_resolve_ip_addr (FlowIPResolver *resolver, FlowIPAddr *addr)
{
  g_return_val_if_fail (FLOW_IS_IP_RESOLVER (resolver), 0);
  g_return_val_if_fail (addr != NULL, 0);

  g_object_ref (addr);

  return create_lookup (resolver, TRUE, addr);
}

void
flow_ip_resolver_cancel_resolution (FlowIPResolver *resolver, guint lookup_id)
{
  FlowIPResolverPrivate *priv;
  Lookup                *lookup;

  g_return_if_fail (FLOW_IS_IP_RESOLVER (resolver));
  g_return_if_fail (lookup_id != 0);

  priv = resolver->priv;

  g_mutex_lock (priv->mutex);

  lookup = g_hash_table_lookup (priv->lookup_table, GUINT_TO_POINTER (lookup_id));
  if (lookup)
    lookup->is_wanted = FALSE;

  g_mutex_unlock (priv->mutex);
}
