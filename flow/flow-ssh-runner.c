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

#include <unistd.h>
#include <glib/gstdio.h>
#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-messages.h"
#include "flow-process-result.h"
#include "flow-detailed-event.h"
#include "flow-ssh-connect-op.h"
#include "flow-shell-op.h"
#include "flow-ssh-master-registry.h"
#include "flow-ssh-runner.h"

#define DEBUG(x)

#define MAX_BUFFER_PACKETS 16
#define MAX_BUFFER_BYTES   4096

static void        shunt_read  (FlowShunt *shunt, FlowPacket *packet, FlowSshRunner *ssh_runner);
static FlowPacket *shunt_write (FlowShunt *shunt, FlowSshRunner *ssh_runner);

/* --- FlowSshRunner private data --- */

struct _FlowSshRunnerPrivate
{
  FlowSshConnectOp *connect_op;
  FlowSshConnectOp *next_connect_op;
  FlowShellOp      *shell_op;
  FlowShellOp      *next_shell_op;

  FlowSshMaster    *master;
  FlowShunt        *shunt;
};

/* --- FlowSshRunner properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ssh_runner)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowSshRunner definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_ssh_runner, FlowSshRunner, FLOW_TYPE_CONNECTOR, 0)

/* --- FlowSshRunner implementation --- */

static void
setup_shunt (FlowSshRunner *ssh_runner, FlowShunt *shunt)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;
  FlowPad              *input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));
  FlowPad              *output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));
  FlowPacketQueue      *packet_queue = flow_pad_get_packet_queue (input_pad);

  g_assert (priv->shunt == NULL);
  priv->shunt = shunt;

  flow_shunt_set_read_func (shunt, (FlowShuntReadFunc *) shunt_read, ssh_runner);
  flow_shunt_set_write_func (shunt, (FlowShuntWriteFunc *) shunt_write, ssh_runner);

  if (flow_pad_is_blocked (output_pad))
  {
    flow_shunt_block_reads (shunt);
  }

  if (!packet_queue || flow_packet_queue_get_length_packets (packet_queue) == 0)
  {
    flow_shunt_block_writes (shunt);
  }
}

static void
master_connect_finished (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;
  FlowPad *input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));
  FlowPad *output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));

  DEBUG (g_print ("Connect finished.\n"));

  if (flow_ssh_master_get_is_connected (priv->master))
  {
    FlowConnectivity old_connectivity;
    FlowShunt *shunt;

    DEBUG (g_print ("Running command.\n"));

    old_connectivity = flow_connector_get_state (FLOW_CONNECTOR (ssh_runner));
    flow_connector_set_state_internal (FLOW_CONNECTOR (ssh_runner), FLOW_CONNECTIVITY_CONNECTED);

    if (old_connectivity != FLOW_CONNECTIVITY_CONNECTED)
      flow_pad_push (output_pad, flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN));

    if (priv->shell_op)
    {
      shunt = flow_ssh_master_run_command (priv->master, flow_shell_op_get_cmd (priv->shell_op), NULL);
      if (!shunt)
      {
        /* TODO */
        g_warning ("Did not get a shunt from flow_ssh_master_run_command ().");
      }

      setup_shunt (ssh_runner, shunt);
    }
  }
  else
  {
    FlowDetailedEvent *detailed_event;
    GError *error = flow_ssh_master_get_last_error (priv->master);

    /* Emit STREAM_DENIED */

    detailed_event = flow_detailed_event_new (error ? error->message : flow_get_event_message (FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED));
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED);
    flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);

    if (error)
      g_clear_error (&error);

    flow_pad_push (output_pad, flow_packet_new_take_object (detailed_event, 0));
    flow_connector_set_state_internal (FLOW_CONNECTOR (ssh_runner), FLOW_CONNECTIVITY_DISCONNECTED);
  }

  flow_pad_unblock (input_pad);
}

static void
master_disconnected (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;
  FlowDetailedEvent *detailed_event;
  FlowPad *output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));
  GError *error = flow_ssh_master_get_last_error (priv->master);

  /* Emit STREAM_END */

  detailed_event = flow_detailed_event_new (error ? error->message : flow_get_event_message (FLOW_STREAM_DOMAIN, FLOW_STREAM_END));
  flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);

  if (error)
    g_clear_error (&error);

  flow_pad_push (output_pad, flow_packet_new_take_object (detailed_event, 0));
  flow_connector_set_state_internal (FLOW_CONNECTOR (ssh_runner), FLOW_CONNECTIVITY_DISCONNECTED);
}

static void
shutdown_master (FlowSshRunner *ssh_runner, FlowSshMaster *ssh_master)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;

  g_signal_handlers_disconnect_by_func (ssh_master, (GCallback) master_connect_finished, ssh_runner);
  g_signal_handlers_disconnect_by_func (ssh_master, (GCallback) master_disconnected, ssh_runner);
  g_object_unref (ssh_master);
}

static void
disconnect_from_remote_service (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;

  if (priv->shunt)
  {
    flow_shunt_destroy (priv->shunt);
    priv->shunt = NULL;
  }

  if (priv->master)
  {
    shutdown_master (ssh_runner, priv->master);
    priv->master = NULL;
  }
}

static void
install_next_remote_service (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;
  FlowSshMasterRegistry *registry;

  if (!priv->next_connect_op)
    return;

  if (priv->connect_op)
    g_object_unref (priv->connect_op);
  priv->connect_op = priv->next_connect_op;
  priv->next_connect_op = NULL;
}

static void
run_next_shell_op (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;
  FlowSshMasterRegistry *registry = flow_ssh_master_registry_get_default ();
  FlowSshMaster *ssh_master;

  DEBUG (g_print ("run_next_shell_op\n"));

  if (!priv->connect_op)
    return;

  if (priv->shell_op)
    flow_gobject_unref_clear (priv->shell_op);
  priv->shell_op = priv->next_shell_op;
  priv->next_shell_op = NULL;

  ssh_master = flow_ssh_master_registry_get_master (registry,
                                                    flow_ssh_connect_op_get_remote_service (priv->connect_op),
                                                    NULL /* flow_ssh_connect_op_get_remote_user (priv->connect_op) */);

  if (priv->master)
    shutdown_master (ssh_runner, priv->master);

  priv->master = g_object_ref (ssh_master);
  g_signal_connect_swapped (ssh_master, "connect-finished", (GCallback) master_connect_finished, ssh_runner);
  g_signal_connect_swapped (ssh_master, "disconnected", (GCallback) master_disconnected, ssh_runner);

  if (priv->next_connect_op)
    install_next_remote_service (ssh_runner);

  if (!flow_ssh_master_get_is_connected (ssh_master))
    flow_connector_set_state_internal (FLOW_CONNECTOR (ssh_runner), FLOW_CONNECTIVITY_CONNECTING);

  flow_ssh_master_connect (ssh_master);
}

static void
set_next_shell_op (FlowSshRunner *ssh_runner, FlowShellOp *shell_op)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;
  FlowPad *input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));

  DEBUG (g_print ("set_next_shell_op\n"));

  g_assert (priv->next_shell_op == NULL);
  priv->next_shell_op = g_object_ref (shell_op);

  if (!priv->shell_op)
    run_next_shell_op (ssh_runner);

  flow_pad_block (input_pad);
  if (priv->shunt)
    flow_shunt_block_writes (priv->shunt);
}

static void
end_shell_op (FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;

  disconnect_from_remote_service (ssh_runner);

  g_assert (priv->shell_op != NULL);
  g_object_unref (priv->shell_op);
  priv->shell_op = NULL;

  disconnect_from_remote_service (ssh_runner);

  if (priv->next_connect_op)
    install_next_remote_service (ssh_runner);

  if (priv->next_shell_op)
    run_next_shell_op (ssh_runner);
}

static void
set_next_connect_op (FlowSshRunner *ssh_runner, FlowSshConnectOp *op)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;

  g_object_ref (op);

  if (priv->next_connect_op)
    g_object_unref (priv->next_connect_op);

  priv->next_connect_op = op;

  if (!priv->connect_op)
    install_next_remote_service (ssh_runner);
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
      set_next_connect_op (ssh_runner, packet_data);
      flow_packet_unref (packet);
      packet = NULL;
    }
    else if (FLOW_IS_SHELL_OP (packet_data))
    {
      set_next_shell_op (ssh_runner, FLOW_SHELL_OP (packet_data));
      flow_packet_unref (packet);
      packet = NULL;
    }
    else if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      if (flow_detailed_event_matches (packet_data, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
      {
        run_next_shell_op (ssh_runner);
      }
      else if (flow_detailed_event_matches (packet_data, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
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

static void
shunt_read (FlowShunt *shunt, FlowPacket *packet, FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv          = ssh_runner->priv;
  FlowPacketFormat      packet_format = flow_packet_get_format (packet);
  gpointer              packet_data   = flow_packet_get_data (packet);
  gboolean              end_stream    = FALSE;

  if (packet_format == FLOW_PACKET_FORMAT_OBJECT)
  {
    if (FLOW_IS_DETAILED_EVENT (packet_data))
    {
      FlowDetailedEvent *detailed_event = (FlowDetailedEvent *) packet_data;

      /* Throw away STREAM_BEGIN and STREAM_END; we use those for the master connection. The
       * shunt will also generate STREAM_SEGMENT_BEGIN and STREAM_SEGMENT_END, which we
       * pass on. */

      if (flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN) ||
          flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
          flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
      {
        flow_packet_unref (packet);
        packet = NULL;
      }
    }
    else if (FLOW_IS_PROCESS_RESULT (packet_data))
    {
      end_shell_op (ssh_runner);

      if (flow_connector_get_state (FLOW_CONNECTOR (ssh_runner)) == FLOW_CONNECTIVITY_DISCONNECTING &&
          priv->shell_op == NULL)
      {
        flow_connector_set_state_internal (FLOW_CONNECTOR (ssh_runner), FLOW_CONNECTIVITY_DISCONNECTED);
        end_stream = TRUE;
      }
    }
    else
    {
      flow_handle_universal_events (FLOW_ELEMENT (ssh_runner), packet);
    }
  }

  if (packet)
  {
    FlowPad *output_pad;

    output_pad = FLOW_PAD (flow_simplex_element_get_output_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));
    flow_pad_push (output_pad, packet);

    if (end_stream)
      flow_pad_push (output_pad, flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END));
  }
}

static FlowPacket *
shunt_write (FlowShunt *shunt, FlowSshRunner *ssh_runner)
{
  FlowSshRunnerPrivate *priv = ssh_runner->priv;
  FlowPad         *input_pad;
  FlowPacketQueue *packet_queue;
  FlowPacket      *packet;

  input_pad = FLOW_PAD (flow_simplex_element_get_input_pad (FLOW_SIMPLEX_ELEMENT (ssh_runner)));
  packet_queue = flow_pad_get_packet_queue (input_pad);

  if (!priv->next_shell_op &&
      (!packet_queue ||
       (flow_packet_queue_get_length_packets (packet_queue) < MAX_BUFFER_PACKETS &&
        flow_packet_queue_get_length_bytes (packet_queue) < MAX_BUFFER_BYTES)))
  {
    flow_pad_unblock (input_pad);
    packet_queue = flow_pad_get_packet_queue (input_pad);
  }

  if (!packet_queue || flow_packet_queue_get_length_packets (packet_queue) == 0)
  {
    flow_shunt_block_writes (shunt);
    return NULL;
  }

  while (!flow_pad_is_blocked (input_pad))
  {
    packet = flow_packet_queue_pop_packet (packet_queue);
    if (!packet)
      break;

    packet = handle_outbound_packet (ssh_runner, packet);
    if (packet)
      break;
  }

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

  while (!priv->next_shell_op)
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
      flow_packet_queue_get_length_packets (packet_queue) >= MAX_BUFFER_PACKETS ||
      priv->next_shell_op)
  {
    flow_pad_block (input_pad);
  }

  if (priv->shunt)
  {
    /* FIXME: The shunt's locking might be a performance liability. We could cache the state. */

    if (priv->next_shell_op)
      flow_shunt_block_writes (priv->shunt);
    else
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

  disconnect_from_remote_service (ssh_runner);

  flow_gobject_unref_clear (priv->connect_op);
  flow_gobject_unref_clear (priv->next_connect_op);
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

  if (priv->connect_op)
    remote_service = flow_ssh_connect_op_get_remote_service (priv->connect_op);

  return remote_service;
}
