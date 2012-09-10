/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ssh-runner.c - Origin/endpoint for connected SSH session.
 *
 * Copyright (C) 2012 Hans Petter Jansson
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
#include "flow-detailed-event.h"
#include "flow-ssh-connect-op.h"
#include "flow-ssh-runner.h"

#define MAX_BUFFER_PACKETS 16
#define MAX_BUFFER_BYTES   4096

static void        shunt_read  (FlowShunt *shunt, FlowPacket *packet, FlowSshRunner *ssh_runner);
static FlowPacket *shunt_write (FlowShunt *shunt, FlowSshRunner *ssh_runner);

/* --- FlowSshRunner private data --- */

struct _FlowSshRunnerPrivate
{
  FlowSshConnectOp *op;
  FlowSshConnectOp *next_op;

  FlowShunt        *shunt;
};

/* --- FlowSshRunner properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ssh_runner)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowSshRunner definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_ssh_runner, FlowSshRunner, FLOW_TYPE_CONNECTOR, 0)

/* --- FlowSshRunner implementation --- */

static void
setup_shunt (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;
  FlowPad                 *output_pad;

  flow_shunt_set_read_func (priv->shunt, (FlowShuntReadFunc *) shunt_read, ssh_runner);
  flow_shunt_set_write_func (priv->shunt, (FlowShuntWriteFunc *) shunt_write, ssh_runner);

  output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));

  if (flow_pad_is_blocked (output_pad))
  {
    flow_shunt_block_reads (priv->shunt);
  }
}

static void
connect_to_remote_service (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;

  if (priv->shunt)
  {
    /* We already have an active shunt. This can happen when a shunt has
     * been installed using _flow_ssh_runner_install_connected_shunt (). */

    return;
  }

  if (priv->next_op)
  {
    if (priv->op)
      g_object_unref (priv->op);

    priv->op = priv->next_op;
    priv->next_op = NULL;
  }

  if (!priv->op)
  {
    g_warning ("FlowSshRunner got FLOW_STREAM_BEGIN before connect op.");
    return;
  }

  priv->shunt = flow_connect_to_tcp (flow_ssh_connect_op_get_remote_service (priv->op),
                                     -1);
  setup_shunt (ssh_runner);
  flow_connector_set_state_internal (FLOW_CONNECTOR (ssh_runner), FLOW_CONNECTIVITY_CONNECTING);
}

static void
set_op (FlowSshRunner *ssh_runner, FlowSshConnectOp *op)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;

  g_object_ref (op);

  if (priv->next_op)
    g_object_unref (priv->next_op);

  priv->next_op = op;
}

static FlowPacket *
handle_outbound_packet (FlowSshRunner *ssh_runner, FlowPacket *packet)
{
  FlowPacketFormat packet_format = flow_packet_get_format (packet);
  gpointer         packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_SSH_CONNECT_OP (packet_data))
    {
      set_op (ssh_runner, packet_data);
      flow_packet_unref (packet);
      packet = NULL;
    }
    else if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        connect_to_remote_service (ssh_runner);
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (ssh_runner), FLOW_CONNECTIVITY_DISCONNECTING);
      }
    }
    else
    {
      flow_handle_universal_events (FLOW_ELEMENT (ssh_runner), packet);
    }
  }

  return packet;
}

static FlowPacket *
handle_inbound_packet (FlowSshRunner *ssh_runner, FlowPacket *packet)
{
  FlowSshRunnerPrivate *priv          = ssh_runner->priv;
  FlowPacketFormat         packet_format = flow_packet_get_format (packet);
  gpointer                 packet_data   = flow_packet_get_data (packet);

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (ssh_runner), FLOW_CONNECTIVITY_CONNECTED);
      }
      else if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
               flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
      {
        if (priv->shunt)
        {
          flow_shunt_destroy (priv->shunt);
          priv->shunt = NULL;
        }

        flow_connector_set_state_internal (FLOW_CONNECTOR (ssh_runner), FLOW_CONNECTIVITY_DISCONNECTED);
      }
    }
    else
    {
      flow_handle_universal_events (FLOW_ELEMENT (ssh_runner), packet);
    }
  }

  return packet;
}

static void
shunt_read (FlowShunt *shunt, FlowPacket *packet, FlowSshRunner *ssh_runner)
{
  packet = handle_inbound_packet (ssh_runner, packet);

  if (packet)
  {
    FlowPad *output_pad;

    output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));
    flow_pad_push (output_pad, packet);
  }
}

static FlowPacket *
shunt_write (FlowShunt *shunt, FlowSshRunner *ssh_runner)
{
  FlowPad         *input_pad;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));
  packet_queue = flow_pad_get_packet_queue (input_pad);

  if (!packet_queue ||
      (flow_packet_queue_get_length_packets (packet_queue) < MAX_BUFFER_PACKETS &&
       flow_packet_queue_get_length_bytes (packet_queue) < MAX_BUFFER_BYTES))
  {
    flow_pad_unblock (input_pad);
    packet_queue = flow_pad_get_packet_queue (input_pad);
  }

  if (!packet_queue || flow_packet_queue_get_length_packets (packet_queue) == 0)
  {
    flow_shunt_block_writes (shunt);
    return NULL;
  }

  do
  {
    packet = flow_packet_queue_pop_packet (packet_queue);
    if (!packet)
      break;

    packet = handle_outbound_packet (ssh_runner, packet);
  }
  while (!packet);

  return packet;
}

static void
flow_ssh_runner_process_input (FlowSshRunner *ssh_runner, FlowPad *input_pad)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;
  FlowPacketQueue         *packet_queue;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  while (!priv->shunt)
  {
    FlowPacket *packet;
 
    /* Not connected or connecting to anything; process input packets immediately. These
     * packets may change the desired peer address or request beginning-of-stream, or they
     * may be bogus data to be discarded. */

    packet = flow_packet_queue_pop_packet (packet_queue);
    if (!packet)
      break;

    packet = handle_outbound_packet (ssh_runner, packet);
    if (packet)
      flow_packet_unref (packet);
  }

  if (flow_packet_queue_get_length_bytes (packet_queue) >= MAX_BUFFER_BYTES ||
      flow_packet_queue_get_length_packets (packet_queue) >= MAX_BUFFER_PACKETS)
  {
    flow_pad_block (input_pad);
  }

  if (priv->shunt)
  {
    /* FIXME: The shunt's locking might be a performance liability. We could cache the state. */
    flow_shunt_unblock_writes (priv->shunt);
  }
}

static void
flow_ssh_runner_output_pad_blocked (FlowSshRunner *ssh_runner, FlowPad *output_pad)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;

  if (priv->shunt)
    flow_shunt_block_reads (priv->shunt);
}

static void
flow_ssh_runner_output_pad_unblocked (FlowSshRunner *ssh_runner, FlowPad *output_pad)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;

  if (priv->shunt)
    flow_shunt_unblock_reads (priv->shunt);
}

static void
flow_ssh_runner_type_init (GType type)
{
}

static void
flow_ssh_runner_class_init (FlowSshRunnerClass *klass)
{
  FlowElementClass *element_klass = (FlowElementClass *) klass;

  element_klass->process_input        = (void (*) (FlowElement *, FlowPad *)) flow_ssh_runner_process_input;
  element_klass->output_pad_blocked   = (void (*) (FlowElement *, FlowPad *)) flow_ssh_runner_output_pad_blocked;
  element_klass->output_pad_unblocked = (void (*) (FlowElement *, FlowPad *)) flow_ssh_runner_output_pad_unblocked;
}

static void
flow_ssh_runner_init (FlowSshRunner *ssh_runner)
{
}

static void
flow_ssh_runner_construct (FlowSshRunner *ssh_runner)
{
}

static void
flow_ssh_runner_dispose (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;

  flow_gobject_unref_clear (priv->op);
  flow_gobject_unref_clear (priv->next_op);

  if (priv->shunt)
  {
    flow_shunt_destroy (priv->shunt);
    priv->shunt = NULL;
  }
}

static void
flow_ssh_runner_finalize (FlowSshRunner *ssh_runner)
{
}

/* --- FlowSshRunner public API --- */

FlowSshRunner *
flow_ssh_runner_new (void)
{
  return g_object_new (FLOW_TYPE_SSH_RUNNER, NULL);
}

FlowIPService *
flow_ssh_runner_get_remote_service (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate    *priv;
  FlowIPService           *remote_service = NULL;

  g_return_val_if_fail (FLOW_IS_SSH_RUNNER (ssh_runner), NULL);

  priv = ssh_runner->priv;

  if (priv->op)
    remote_service = flow_ssh_connect_op_get_remote_service (priv->op);

  return remote_service;
}
