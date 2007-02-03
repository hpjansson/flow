/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-processor.c - IP service processing element.
 *
 * Copyright (C) 2007 Hans Petter Jansson
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
#include "flow-ip-processor.h"
#include "flow-ip-service.h"

/* --- FlowIPProcessor private data --- */

typedef struct
{
  FlowPacket *current_packet;
  guint       resolve_to_addrs : 1;
  guint       resolve_to_names : 1;
}
FlowIPProcessorPrivate;

/* --- FlowIPProcessor properties --- */

static gboolean
flow_ip_processor_get_resolve_to_addrs_internal (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  return priv->resolve_to_addrs ? TRUE : FALSE;
}

static void
flow_ip_processor_set_resolve_to_addrs_internal (FlowIPProcessor *ip_processor, gboolean resolve_to_addrs)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  priv->resolve_to_addrs = resolve_to_addrs;
}

static gboolean
flow_ip_processor_get_resolve_to_names_internal (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  return priv->resolve_to_names ? TRUE : FALSE;
}

static void
flow_ip_processor_set_resolve_to_names_internal (FlowIPProcessor *ip_processor, gboolean resolve_to_names)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  priv->resolve_to_names = resolve_to_names;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ip_processor)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("resolve-to-addresses", "Resolve to Addresses", "If we should resolve to IP addresses",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_resolve_to_addrs_internal,
                               flow_ip_processor_set_resolve_to_addrs_internal,
                               TRUE)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("resolve-to-names", "Resolve to Names", "If we should resolve to DNS names",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_resolve_to_names_internal,
                               flow_ip_processor_set_resolve_to_names_internal,
                               FALSE)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowIPProcessor definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_ip_processor, FlowIPProcessor, FLOW_TYPE_SIMPLEX_ELEMENT, 0)

/* --- FlowIPProcessor implementation --- */

static void
current_ip_resolved (FlowIPProcessor *ip_processor, FlowIPService *ip_service)
{
  FlowIPProcessorPrivate *priv    = ip_processor->priv;
  FlowElement            *element = (FlowElement *) ip_processor;
  FlowPacket             *packet;
  FlowPad                *output_pad;

  packet = priv->current_packet;
  priv->current_packet = NULL;

  g_assert (packet != NULL);
  g_assert (flow_packet_get_data (packet) == ip_service);

  g_signal_handlers_disconnect_by_func (ip_service, current_ip_resolved, ip_service);

  output_pad = g_ptr_array_index (element->output_pads, 0);
  flow_pad_push (output_pad, packet);
}

static void
flow_ip_processor_process_input (FlowIPProcessor *ip_processor, FlowPad *input_pad)
{
  FlowIPProcessorPrivate *priv    = ip_processor->priv;
  FlowElement            *element = (FlowElement *) ip_processor;
  FlowPacketQueue        *packet_queue;
  FlowPacket             *packet;
  FlowPad                *output_pad;

  if (priv->current_packet)
    return;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  output_pad = g_ptr_array_index (element->output_pads, 0);

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    flow_handle_universal_events (element, packet);

    if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
    {
      FlowIPService *ip_service = flow_packet_get_data (packet);

      if (FLOW_IS_IP_SERVICE (ip_service) &&
          ((priv->resolve_to_addrs && !flow_ip_service_get_nth_address (ip_service, 0)) ||
           (priv->resolve_to_names && !flow_ip_service_get_name (ip_service))))
      {
        /* Need to do a lookup */
        priv->current_packet = packet;

        g_signal_connect_swapped (ip_service, "resolved", (GCallback) current_ip_resolved, ip_processor);
        flow_ip_service_resolve (ip_service);
        break;
      }
    }

    /* No lookup necessary; just pass it on */
    flow_pad_push (output_pad, packet);
  }
}

static void
flow_ip_processor_output_pad_unblocked (FlowIPProcessor *ip_processor, FlowPad *output_pad)
{
  FlowIPProcessorPrivate *priv      = ip_processor->priv;
  FlowElement            *element   = (FlowElement *) ip_processor;
  FlowPad                *input_pad = g_ptr_array_index (element->input_pads, 0);

  flow_ip_processor_process_input (ip_processor, input_pad);

  if (!priv->current_packet)
    flow_pad_unblock (input_pad);
}

static void
flow_ip_processor_type_init (GType type)
{
}

static void
flow_ip_processor_class_init (FlowIPProcessorClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->output_pad_unblocked = (void (*) (FlowElement *, FlowPad *)) flow_ip_processor_output_pad_unblocked;
  element_klass->process_input        = (void (*) (FlowElement *, FlowPad *)) flow_ip_processor_process_input;
}

static void
flow_ip_processor_init (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  priv->resolve_to_addrs = TRUE;
  priv->resolve_to_names = FALSE;
}

static void
flow_ip_processor_construct (FlowIPProcessor *ip_processor)
{
}

static void
flow_ip_processor_dispose (FlowIPProcessor *ip_processor)
{
}

static void
flow_ip_processor_finalize (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  if (priv->current_packet)
  {
    FlowIPService *ip_service = flow_packet_get_data (priv->current_packet);

    g_assert (FLOW_IS_IP_SERVICE (ip_service));
    g_signal_handlers_disconnect_by_func (ip_service, current_ip_resolved, ip_service);
    flow_packet_free (priv->current_packet);
    priv->current_packet = NULL;
  }
}

/* --- FlowIPProcessor public API --- */

FlowIPProcessor *
flow_ip_processor_new (void)
{
  return g_object_new (FLOW_TYPE_IP_PROCESSOR, NULL);
}

gboolean
flow_ip_processor_get_resolve_to_addrs (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv;

  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  priv = ip_processor->priv;
  return priv->resolve_to_addrs;
}

void
flow_ip_processor_set_resolve_to_addrs (FlowIPProcessor *ip_processor, gboolean resolve_to_addrs)
{
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  g_object_set (ip_processor, "resolve-to-addrs", resolve_to_addrs, NULL);
}

gboolean
flow_ip_processor_get_resolve_to_names (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv;

  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  priv = ip_processor->priv;
  return priv->resolve_to_names;
}

void
flow_ip_processor_set_resolve_to_names (FlowIPProcessor *ip_processor, gboolean resolve_to_names)
{
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  g_object_set (ip_processor, "resolve-to-names", resolve_to_names, NULL);
}
