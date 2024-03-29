/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tcp-listener.c - Connection listener that spawns FlowTcpConnectors.
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

#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-tcp-listener.h"
#include "flow-anonymous-event.h"
#include "flow-detailed-event.h"
#include "flow-context-mgmt.h"

/* Implemented in flow-tcp-connector.c */
void _flow_tcp_connector_install_connected_shunt (FlowTcpConnector *tcp_connector, FlowShunt *connected_shunt);

static void shunt_read (FlowShunt *shunt, FlowPacket *packet, FlowTcpListener *tcp_listener);

/* --- FlowTcpListener private data --- */

struct _FlowTcpListenerPrivate
{
  FlowIPService   *local_ip_service;
  FlowShunt       *shunt;

  GQueue          *connected_shunts;

  guint            waiting_for_pop;
  GMainLoop       *pop_loop;
};

/* --- FlowTcpListener properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_tcp_listener)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowTcpListener definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_tcp_listener, FlowTcpListener, G_TYPE_OBJECT, 0)

/* --- FlowTcpListener implementation --- */

static void
shunt_read (FlowShunt *shunt, FlowPacket *packet, FlowTcpListener *tcp_listener)
{
  FlowTcpListenerPrivate *priv          = tcp_listener->priv;
  FlowPacketFormat        packet_format = flow_packet_get_format (packet);
  gpointer                packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_ANONYMOUS_EVENT (packet_data))
    {
      FlowAnonymousEvent *anonymous_event = (FlowAnonymousEvent *) packet_data;
      FlowShunt          *connected_shunt;

      /* Take ownership of non-refcounted FlowShunt */

      flow_anonymous_event_set_destroy_notify (anonymous_event, NULL);
      connected_shunt = flow_anonymous_event_get_data (anonymous_event);

      g_assert (connected_shunt != NULL);

      g_queue_push_tail (priv->connected_shunts, connected_shunt);

      if (priv->waiting_for_pop)
      {
        g_assert (priv->pop_loop != NULL);

        g_main_loop_quit (priv->pop_loop);
      }
      else
      {
        g_signal_emit_by_name (tcp_listener, "new-connection");
      }
    }

    /* We may get FlowDetailedEvents if resource exhaustion is preventing us from
     * accepting connections. However, there isn't much we can do about that. */
  }

  flow_packet_unref (packet);
}

static void
flow_tcp_listener_type_init (GType type)
{
}

static void
flow_tcp_listener_class_init (FlowTcpListenerClass *klass)
{
  g_signal_newv ("new-connection",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 NULL,                                   /* Class closure */
                 NULL, NULL,                             /* Accumulator, accu data */
                 g_cclosure_marshal_VOID__VOID,          /* Marshaller */
                 G_TYPE_NONE,                            /* Return type */
                 0, NULL);                               /* Number of params, param types */
}

static void
flow_tcp_listener_init (FlowTcpListener *tcp_listener)
{
  FlowTcpListenerPrivate *priv = tcp_listener->priv;

  priv->connected_shunts = g_queue_new ();
}

static void
flow_tcp_listener_construct (FlowTcpListener *tcp_listener)
{
}

static void
flow_tcp_listener_dispose (FlowTcpListener *tcp_listener)
{
  FlowTcpListenerPrivate *priv = tcp_listener->priv;

  flow_gobject_unref_clear (priv->local_ip_service);

  if (priv->shunt)
  {
    flow_shunt_destroy (priv->shunt);
    priv->shunt = NULL;
  }
}

static void
flow_tcp_listener_finalize (FlowTcpListener *tcp_listener)
{
  FlowTcpListenerPrivate *priv = tcp_listener->priv;
  FlowShunt              *connected_shunt;

  if (priv->pop_loop)
    g_main_loop_unref (priv->pop_loop);

  while ((connected_shunt = g_queue_pop_head (priv->connected_shunts)))
  {
    flow_shunt_destroy (connected_shunt);
  }

  g_queue_free (priv->connected_shunts);
  priv->connected_shunts = NULL;
}

static FlowTcpConnector *
pop_connection (FlowTcpListener *tcp_listener)
{
  FlowTcpListenerPrivate *priv = tcp_listener->priv;
  FlowTcpConnector       *tcp_connector;
  FlowShunt              *connected_shunt;

  connected_shunt = g_queue_pop_head (priv->connected_shunts);
  if (!connected_shunt)
    return NULL;

  tcp_connector = flow_tcp_connector_new ();
  _flow_tcp_connector_install_connected_shunt (tcp_connector, connected_shunt);

  return tcp_connector;
}

/* --- FlowTcpListener public API --- */

FlowTcpListener *
flow_tcp_listener_new (void)
{
  return g_object_new (FLOW_TYPE_TCP_LISTENER, NULL);
}

FlowIPService *
flow_tcp_listener_get_local_service (FlowTcpListener *tcp_listener)
{
  FlowTcpListenerPrivate *priv;

  g_return_val_if_fail (FLOW_IS_TCP_LISTENER (tcp_listener), FALSE);

  priv = tcp_listener->priv;

  return priv->local_ip_service;
}

gboolean
flow_tcp_listener_set_local_service (FlowTcpListener *tcp_listener, FlowIPService *ip_service, FlowDetailedEvent **result_event)
{
  FlowTcpListenerPrivate *priv;
  gboolean                result = TRUE;

  g_return_val_if_fail (FLOW_IS_TCP_LISTENER (tcp_listener), FALSE);
  g_return_val_if_fail (ip_service == NULL || FLOW_IS_IP_SERVICE (ip_service), FALSE);

  priv = tcp_listener->priv;

  flow_gobject_unref_clear (priv->local_ip_service);

  if (priv->shunt)
  {
    flow_shunt_destroy (priv->shunt);
    priv->shunt = NULL;
  }

  /* If service is NULL, don't listen to anything */

  if (ip_service)
  {
    gpointer  object;

    if (!flow_ip_service_find_address (ip_service, FLOW_IP_ADDR_ANY_FAMILY) &&
        flow_ip_service_have_name (ip_service))
    {
      /* If the caller didn't resolve the IP service, we try to look it up
       * for him. This blocks, using the current thread's Flow main context. Note
       * that for the main thread, this is not the default GLib main context, so
       * e.g. GUI callbacks won't fire. */

      flow_ip_service_sync_resolve (ip_service, NULL);
    }

    priv->shunt = flow_open_tcp_listener (ip_service);

    while ((object = flow_read_object_from_shunt (priv->shunt)))
    {
      if (FLOW_IS_DETAILED_EVENT (object))
        break;

      g_object_unref (object);
    }

    /* There must be an event describing the result of the bind */
    g_assert (object != NULL);

    if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
    {
      priv->local_ip_service = g_object_ref (ip_service);
      flow_shunt_set_read_func (priv->shunt, (FlowShuntReadFunc *) shunt_read, tcp_listener);
      g_object_unref (object);
    }
    else
    {
      flow_shunt_destroy (priv->shunt);
      priv->shunt = NULL;
      result = FALSE;

      if (result_event)
        *result_event = object;
      else
        g_object_unref (object);
    }
  }

  return result;
}

FlowTcpConnector *
flow_tcp_listener_pop_connection (FlowTcpListener *tcp_listener)
{
  g_return_val_if_fail (FLOW_IS_TCP_LISTENER (tcp_listener), NULL);

  return pop_connection (tcp_listener);
}

FlowTcpConnector *
flow_tcp_listener_sync_pop_connection (FlowTcpListener *tcp_listener)
{
  FlowTcpListenerPrivate *priv;
  FlowTcpConnector       *tcp_connector;

  g_return_val_if_fail (FLOW_IS_TCP_LISTENER (tcp_listener), NULL);

  priv = tcp_listener->priv;

  priv->waiting_for_pop++;

  while (!(tcp_connector = pop_connection (tcp_listener)))
  {
    if G_UNLIKELY (!priv->pop_loop)
    {
      GMainContext *main_context;

      main_context = flow_get_main_context_for_current_thread ();
      priv->pop_loop = g_main_loop_new (main_context, FALSE);
    }

    g_main_loop_run (priv->pop_loop);
  }

  priv->waiting_for_pop--;

  return tcp_connector;
}
