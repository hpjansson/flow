/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-service.c - An Internet address and port number.
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
#include "flow-gobject-util.h"
#include "flow-context-mgmt.h"
#include "flow-ip-resolver.h"
#include "flow-ip-service.h"

/* --- FlowIPService private data --- */

typedef struct
{
  GMutex      *mutex;
  gchar       *name;
  GList       *addresses;
  gint         port;
  guint        resolve_id;
  guint        quality               : 3;
  guint        resolve_id_is_idle_id : 1;
}
FlowIPServicePrivate;

/* --- FlowIPService properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ip_service)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowIPService definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_ip_service, FlowIPService, G_TYPE_OBJECT, 0)

/* --- FlowIPService implementation --- */

static void
flow_ip_service_type_init (GType type)
{
}

static void
flow_ip_service_class_init (FlowIPServiceClass *klass)
{
  if (!g_thread_supported ())
    g_thread_init (NULL);

  g_signal_newv ("resolved",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 NULL,                                   /* Class closure */
                 NULL, NULL,                             /* Accumulator, accu data */
                 g_cclosure_marshal_VOID__VOID,          /* Marshaller */
                 G_TYPE_NONE,                            /* Return type */
                 0, NULL);                               /* Number of params, param types */
}

static void
flow_ip_service_init (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv = ip_service->priv;

  priv->mutex = g_mutex_new ();
  priv->quality = FLOW_QUALITY_UNSPECIFIED;
}

static void
flow_ip_service_construct (FlowIPService *ip_service)
{
}

static void
flow_ip_service_dispose (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv = ip_service->priv;

  if (priv->resolve_id)
  {
    if (priv->resolve_id_is_idle_id)
      flow_source_remove_from_current_thread (priv->resolve_id);
    else
      flow_ip_resolver_cancel_resolution (flow_ip_resolver_get_default (), priv->resolve_id);

    priv->resolve_id = 0;
  }
}

static void
flow_ip_service_finalize (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv = ip_service->priv;
  GList                *l;

  g_free (priv->name);
  priv->name = NULL;

  for (l = priv->addresses; l; l = g_list_next (l))
    g_object_unref (l->data);

  g_list_free (priv->addresses);
  priv->addresses = NULL;

  g_mutex_free (priv->mutex);
}

static void
emit_resolved_signal (FlowIPService *ip_service)
{
  g_signal_emit_by_name (ip_service, "resolved");
}

static GList *
copy_object_list (GList *object_list)
{
  GList *new_list;
  GList *l;

  new_list = g_list_copy (object_list);

  for (l = new_list; l; l = g_list_next (l))
    g_object_ref (l->data);

  return new_list;
}

static gboolean
no_lookup_done (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv = ip_service->priv;

  g_mutex_lock (priv->mutex);

  priv->resolve_id = 0;
  g_object_ref (ip_service);

  g_mutex_unlock (priv->mutex);

  emit_resolved_signal (ip_service);
  g_object_unref (ip_service);
  return FALSE;
}

static void
lookup_done (GList *addr_list, GList *name_list, FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv = ip_service->priv;

  g_mutex_lock (priv->mutex);

  if (!priv->name)
  {
    if (name_list)
    {
      /* Successful address -> name */
      priv->name = g_strdup (name_list->data);
    }
    else
    {
      /* Failed address -> name */

      /* FIXME: We can try the next address in the list. Most
       * services will have a single address, so this isn't a big
       * problem for now. */
    }
  }
  else if (!priv->addresses)
  {
    if (addr_list)
    {
      /* Successful name -> addresses */
      priv->addresses = copy_object_list (addr_list);
    }
    else
    {
      /* Failed name -> addresses */
    }
  }
  else
  {
    /* Fields already filled in - don't touch anything */
  }

  priv->resolve_id = 0;
  g_object_ref (ip_service);

  g_mutex_unlock (priv->mutex);

  emit_resolved_signal (ip_service);
  g_object_unref (ip_service);
}

/* --- FlowIPService public API --- */

FlowIPService *
flow_ip_service_new (void)
{
  return g_object_new (FLOW_TYPE_IP_SERVICE, NULL);
}

gchar *
flow_ip_service_get_name (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv;
  gchar                *name = NULL;

  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), NULL);

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);

  if (priv->name)
    name = g_strdup (priv->name);

  g_mutex_unlock (priv->mutex);
  return NULL;
}

void
flow_ip_service_set_name (FlowIPService *ip_service, const gchar *name)
{
  FlowIPServicePrivate *priv;

  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);

  g_free (priv->name);
  priv->name = g_strdup (name);

  g_mutex_unlock (priv->mutex);
}

void
flow_ip_service_resolve (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv;
  FlowIPResolver       *default_resolver;

  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);

  if (priv->resolve_id)
  {
    g_mutex_unlock (priv->mutex);
    return;
  }

  if (priv->name)
  {
    /* Name -> addresses */

    default_resolver = flow_ip_resolver_get_default ();
    priv->resolve_id = flow_ip_resolver_resolve_name (default_resolver, priv->name,
                                                      (FlowIPLookupFunc *) lookup_done, ip_service);
    priv->resolve_id_is_idle_id = FALSE;
  }
  else if (priv->addresses)
  {
    /* Address (first found) -> name */

    default_resolver = flow_ip_resolver_get_default ();
    priv->resolve_id = flow_ip_resolver_resolve_ip_addr (default_resolver, priv->addresses->data,
                                                         (FlowIPLookupFunc *) lookup_done, ip_service);
    priv->resolve_id_is_idle_id = FALSE;
  }
  else
  {
    /* Nothing to look up - schedule a signal indicating we're done */

    priv->resolve_id = flow_idle_add_to_current_thread ((GSourceFunc) no_lookup_done, ip_service);
    priv->resolve_id_is_idle_id = TRUE;
  }

  g_mutex_unlock (priv->mutex);
}

gboolean
flow_ip_service_sync_resolve (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv;
  GMainLoop            *main_loop;
  guint                 signal_id;
  gboolean              result = FALSE;

  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), FALSE);

  priv = ip_service->priv;

  main_loop = g_main_loop_new (flow_get_main_context_for_current_thread (), FALSE);
  signal_id = g_signal_connect_swapped (ip_service, "resolved", (GCallback) g_main_loop_quit, main_loop);

  flow_ip_service_resolve (ip_service);
  g_main_loop_run (main_loop);

  g_signal_handler_disconnect (ip_service, signal_id);
  g_main_loop_unref (main_loop);

  g_mutex_lock (priv->mutex);

  if (priv->name && priv->addresses)
    result = TRUE;

  g_mutex_unlock (priv->mutex);

  return result;
}

void
flow_ip_service_add_address (FlowIPService *ip_service, FlowIPAddr *ip_addr)
{
  FlowIPServicePrivate *priv;

  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));
  g_return_if_fail (ip_addr != NULL);

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);
  g_object_ref (ip_addr);
  priv->addresses = g_list_append (priv->addresses, ip_addr);
  g_mutex_unlock (priv->mutex);
}

void
flow_ip_service_remove_address (FlowIPService *ip_service, FlowIPAddr *ip_addr)
{
  FlowIPServicePrivate *priv;

  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));
  g_return_if_fail (ip_addr != NULL);

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);
  priv->addresses = g_list_remove (priv->addresses, ip_addr);
  g_object_unref (ip_addr);
  g_mutex_unlock (priv->mutex);
}

FlowIPAddr *
flow_ip_service_get_nth_address (FlowIPService *ip_service, gint n)
{
  FlowIPServicePrivate *priv;
  FlowIPAddr           *ip_addr = NULL;

  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), NULL);
  g_return_val_if_fail (n >= 0, NULL);

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);

  ip_addr = g_list_nth_data (priv->addresses, n);
  if (ip_addr)
    g_object_ref (ip_addr);

  g_mutex_unlock (priv->mutex);

  return ip_addr;
}

FlowIPAddr *
flow_ip_service_find_address (FlowIPService *ip_service, FlowIPAddrFamily family)
{
  FlowIPServicePrivate *priv;
  GList                *l;
  FlowIPAddr           *found_addr = NULL;

  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), NULL);

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);

  for (l = priv->addresses; !found_addr && l; l = g_list_next (l))
  {
    FlowIPAddr *ip_addr = l->data;

    if ((family == FLOW_IP_ADDR_ANY_FAMILY || family == flow_ip_addr_get_family (ip_addr)) &&
        flow_ip_addr_is_valid (ip_addr))
      found_addr = ip_addr;
  }

  if (found_addr)
    g_object_ref (found_addr);

  g_mutex_unlock (priv->mutex);
  return found_addr;
}

GList *
flow_ip_service_list_addresses (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv;
  GList                *l;
  GList                *m = NULL;

  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), NULL);

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);

  for (l = priv->addresses; l; l = g_list_next (l))
  {
    g_object_ref (l->data);
    m = g_list_prepend (m, l->data);
  }

  m = g_list_reverse (m);

  g_mutex_unlock (priv->mutex);

  return m;
}

gint
flow_ip_service_get_port (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv;
  gint                  port;

  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), 0);

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);
  port = priv->port;
  g_mutex_unlock (priv->mutex);

  return port;
}

void
flow_ip_service_set_port (FlowIPService *ip_service, gint port)
{
  FlowIPServicePrivate *priv;

  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));
  g_return_if_fail (port >= 0 && port <= 65535);

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);
  priv->port = port;
  g_mutex_unlock (priv->mutex);
}

FlowQuality
flow_ip_service_get_quality (FlowIPService *ip_service)
{
  FlowIPServicePrivate *priv;
  FlowQuality           quality;

  g_return_val_if_fail (FLOW_IS_IP_SERVICE (ip_service), FLOW_QUALITY_UNSPECIFIED);

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);
  quality = priv->quality;
  g_mutex_unlock (priv->mutex);

  return quality;
}

void
flow_ip_service_set_quality (FlowIPService *ip_service, FlowQuality quality)
{
  FlowIPServicePrivate *priv;

  g_return_if_fail (FLOW_IS_IP_SERVICE (ip_service));

  priv = ip_service->priv;

  g_mutex_lock (priv->mutex);
  priv->quality = quality;
  g_mutex_unlock (priv->mutex);
}
