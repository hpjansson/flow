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
#include "flow-tcp-connect-op.h"
#include "flow-ip-processor.h"

/* --- FlowIPProcessor private data --- */

typedef struct
{
  FlowPacket *current_packet;
  guint       resolve_to_addrs         : 1;
  guint       resolve_to_names         : 1;
  guint       require_addrs            : 1;
  guint       require_names            : 1;
  guint       drop_invalid_data        : 1;
  guint       drop_invalid_objs        : 1;
  guint       drop_invalid_ip_services : 1;
  guint       valid_state              : 1;
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

static gboolean
flow_ip_processor_get_require_addrs_internal (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  return priv->require_addrs ? TRUE : FALSE;
}

static void
flow_ip_processor_set_require_addrs_internal (FlowIPProcessor *ip_processor, gboolean require_addrs)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  priv->require_addrs = require_addrs;
}

static gboolean
flow_ip_processor_get_require_names_internal (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  return priv->require_names ? TRUE : FALSE;
}

static void
flow_ip_processor_set_require_names_internal (FlowIPProcessor *ip_processor, gboolean require_names)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  priv->require_names = require_names;
}

static gboolean
flow_ip_processor_get_drop_invalid_data_internal (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  return priv->drop_invalid_data ? TRUE : FALSE;
}

static void
flow_ip_processor_set_drop_invalid_data_internal (FlowIPProcessor *ip_processor, gboolean drop_invalid_data)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  priv->drop_invalid_data = drop_invalid_data;
}

static gboolean
flow_ip_processor_get_drop_invalid_objs_internal (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  return priv->drop_invalid_objs ? TRUE : FALSE;
}

static void
flow_ip_processor_set_drop_invalid_objs_internal (FlowIPProcessor *ip_processor, gboolean drop_invalid_objs)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  priv->drop_invalid_objs = drop_invalid_objs;
}

static gboolean
flow_ip_processor_get_drop_invalid_ip_services_internal (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  return priv->drop_invalid_ip_services ? TRUE : FALSE;
}

static void
flow_ip_processor_set_drop_invalid_ip_services_internal (FlowIPProcessor *ip_processor, gboolean drop_invalid_ip_services)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  priv->drop_invalid_ip_services = drop_invalid_ip_services;
}

static gboolean
flow_ip_processor_get_valid_state_internal (FlowIPProcessor *ip_processor)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  return priv->valid_state ? TRUE : FALSE;
}

static void
flow_ip_processor_set_valid_state_internal (FlowIPProcessor *ip_processor, gboolean valid_state)
{
  FlowIPProcessorPrivate *priv = ip_processor->priv;

  priv->valid_state = valid_state;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ip_processor)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("resolve-to-addrs", "Resolve to Addresses",
                               "If we should resolve to IP addresses",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_resolve_to_addrs_internal,
                               flow_ip_processor_set_resolve_to_addrs_internal,
                               TRUE)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("resolve-to-names", "Resolve to Names",
                               "If we should resolve to DNS names",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_resolve_to_names_internal,
                               flow_ip_processor_set_resolve_to_names_internal,
                               FALSE)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("require-addrs", "Require Addresses",
                               "If we should require resolved IP addresses to preface stream",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_require_addrs_internal,
                               flow_ip_processor_set_require_addrs_internal,
                               FALSE)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("require-names", "Require Names",
                               "If we should require resolved DNS names to preface stream",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_require_names_internal,
                               flow_ip_processor_set_require_names_internal,
                               FALSE)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("drop-invalid-data", "Drop Invalid Data",
                               "If we should drop data when in the invalid state",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_drop_invalid_data_internal,
                               flow_ip_processor_set_drop_invalid_data_internal,
                               FALSE)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("drop-invalid-objs", "Drop Invalid Objects",
                               "If we should drop objects when in the invalid state",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_drop_invalid_objs_internal,
                               flow_ip_processor_set_drop_invalid_objs_internal,
                               FALSE)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("drop-invalid-ip-services", "Drop Invalid IP Services",
                               "If we should drop IP service objects that do not match our requirements",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_drop_invalid_ip_services_internal,
                               flow_ip_processor_set_drop_invalid_ip_services_internal,
                               FALSE)
FLOW_GOBJECT_PROPERTY_BOOLEAN ("valid-state", "Valid State",
                               "If we are currently in the valid state",
                               G_PARAM_READWRITE,
                               flow_ip_processor_get_valid_state_internal,
                               flow_ip_processor_set_valid_state_internal,
                               TRUE)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowIPProcessor definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_ip_processor, FlowIPProcessor, FLOW_TYPE_SIMPLEX_ELEMENT, 0)

/* --- FlowIPProcessor implementation --- */

static void flow_ip_processor_process_input (FlowIPProcessor *ip_processor, FlowPad *input_pad);

static void
validate_ip_service (FlowIPProcessor *ip_processor, FlowIPService *ip_service)
{
  FlowIPProcessorPrivate *priv           = ip_processor->priv;
  gboolean                new_valid_state = FALSE;

  if ((!(priv->require_addrs && flow_ip_service_get_n_addresses (ip_service) == 0) &&
       !(priv->require_names && !flow_ip_service_have_name (ip_service))))
  {
    new_valid_state = TRUE;
  }

  if (priv->valid_state != new_valid_state)
  {
    /* Must use property setter, so property change listeners get called */
    flow_ip_processor_set_valid_state (ip_processor, new_valid_state);
  }
}

static void
current_ip_resolved (FlowIPProcessor *ip_processor, FlowIPService *ip_service)
{
  FlowIPProcessorPrivate *priv       = ip_processor->priv;
  FlowElement            *element    = (FlowElement *) ip_processor;
  FlowPad                *input_pad  = g_ptr_array_index (element->input_pads, 0);
  FlowPad                *output_pad = g_ptr_array_index (element->output_pads, 0);
  FlowPacket             *packet;

  packet = priv->current_packet;
  priv->current_packet = NULL;

  g_assert (packet != NULL);

  g_signal_handlers_disconnect_by_func (ip_service, current_ip_resolved, ip_processor);

  validate_ip_service (ip_processor, ip_service);

  if (priv->valid_state || !priv->drop_invalid_ip_services)
    flow_pad_push (output_pad, packet);
  else
    flow_packet_free (packet);

  if (!flow_pad_is_blocked (output_pad))
    flow_pad_unblock (input_pad);

  flow_ip_processor_process_input (ip_processor, input_pad);
}

static void
flow_ip_processor_process_input (FlowIPProcessor *ip_processor, FlowPad *input_pad)
{
  FlowIPProcessorPrivate *priv    = ip_processor->priv;
  FlowElement            *element = (FlowElement *) ip_processor;
  FlowPacketQueue        *packet_queue;
  FlowPacket             *packet;
  FlowPad                *output_pad;

  /* If we're doing a lookup, wait for it to complete before processing further
   * packets. This preserves stream order. */
  if (priv->current_packet)
    return;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  output_pad = g_ptr_array_index (element->output_pads, 0);

  for ( ; (packet = flow_packet_queue_pop_packet (packet_queue)); )
  {
    gboolean should_push_packet = FALSE;

    flow_handle_universal_events (element, packet);

    if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
    {
      gpointer       packet_data     = flow_packet_get_data (packet);
      FlowIPService *ip_service      = NULL;

      if (FLOW_IS_IP_SERVICE (packet_data))
        ip_service = packet_data;
      else if (FLOW_IS_TCP_CONNECT_OP (packet_data))
        ip_service = flow_tcp_connect_op_get_remote_service (packet_data);

      if (ip_service)
      {
        if (((priv->resolve_to_addrs && flow_ip_service_get_n_addresses (ip_service) == 0) ||
             (priv->resolve_to_names && !flow_ip_service_have_name (ip_service))))
        {
          /* Need to do a lookup */
          priv->current_packet = packet;

          g_signal_connect_swapped (ip_service, "resolved", (GCallback) current_ip_resolved, ip_processor);
          flow_ip_service_resolve (ip_service);
          break;
        }

        /* No lookup needed; see if the IP service is valid according to our criteria */
        validate_ip_service (ip_processor, ip_service);

        if (priv->valid_state || !priv->drop_invalid_ip_services)
          should_push_packet = TRUE;  /* IP service, don't drop */
      }
      else if (priv->valid_state || !priv->drop_invalid_objs)
        should_push_packet = TRUE;  /* Object packet, don't drop */
    }
    else if (priv->valid_state || !priv->drop_invalid_data)
      should_push_packet = TRUE;  /* Data packet, don't drop */

    if (should_push_packet)
      flow_pad_push (output_pad, packet);
    else
      flow_packet_free (packet);
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

  priv->resolve_to_addrs         = TRUE;
  priv->resolve_to_names         = FALSE;
  priv->require_addrs            = FALSE;
  priv->require_names            = FALSE;
  priv->drop_invalid_data        = FALSE;
  priv->drop_invalid_objs        = FALSE;
  priv->drop_invalid_ip_services = FALSE;
  priv->valid_state              = TRUE;
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
    g_signal_handlers_disconnect_by_func (ip_service, current_ip_resolved, ip_processor);
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
  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  return flow_ip_processor_get_resolve_to_addrs_internal (ip_processor);
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
  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  return flow_ip_processor_get_resolve_to_names_internal (ip_processor);
}

void
flow_ip_processor_set_resolve_to_names (FlowIPProcessor *ip_processor, gboolean resolve_to_names)
{
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  g_object_set (ip_processor, "resolve-to-names", resolve_to_names, NULL);
}

gboolean
flow_ip_processor_get_require_addrs (FlowIPProcessor *ip_processor)
{
  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  return flow_ip_processor_get_require_addrs_internal (ip_processor);
}

void
flow_ip_processor_set_require_addrs (FlowIPProcessor *ip_processor, gboolean require_addrs)
{
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  g_object_set (ip_processor, "require-addrs", require_addrs, NULL);
}

gboolean
flow_ip_processor_get_require_names (FlowIPProcessor *ip_processor)
{
  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  return flow_ip_processor_get_require_names_internal (ip_processor);
}

void
flow_ip_processor_set_require_names (FlowIPProcessor *ip_processor, gboolean require_names)
{
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  g_object_set (ip_processor, "require-names", require_names, NULL);
}

gboolean
flow_ip_processor_get_drop_invalid_data (FlowIPProcessor *ip_processor)
{
  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  return flow_ip_processor_get_drop_invalid_data_internal (ip_processor);
}

void
flow_ip_processor_set_drop_invalid_data (FlowIPProcessor *ip_processor, gboolean drop_invalid_data)
{
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  g_object_set (ip_processor, "drop-invalid-data", drop_invalid_data, NULL);
}

gboolean
flow_ip_processor_get_drop_invalid_objs (FlowIPProcessor *ip_processor)
{
  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  return flow_ip_processor_get_drop_invalid_objs_internal (ip_processor);
}

void
flow_ip_processor_set_drop_invalid_objs (FlowIPProcessor *ip_processor, gboolean drop_invalid_objs)
{
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  g_object_set (ip_processor, "drop-invalid-objs", drop_invalid_objs, NULL);
}

gboolean
flow_ip_processor_get_drop_invalid_ip_services (FlowIPProcessor *ip_processor)
{
  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  return flow_ip_processor_get_drop_invalid_ip_services_internal (ip_processor);
}

void
flow_ip_processor_set_drop_invalid_ip_services (FlowIPProcessor *ip_processor, gboolean drop_invalid_ip_services)
{
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  g_object_set (ip_processor, "drop-invalid-ip-services", drop_invalid_ip_services, NULL);
}

gboolean
flow_ip_processor_get_valid_state (FlowIPProcessor *ip_processor)
{
  g_return_val_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor), FALSE);

  return flow_ip_processor_get_valid_state_internal (ip_processor);
}

void
flow_ip_processor_set_valid_state (FlowIPProcessor *ip_processor, gboolean valid_state)
{
  g_return_if_fail (FLOW_IS_IP_PROCESSOR (ip_processor));

  g_object_set (ip_processor, "valid-state", valid_state, NULL);
}

