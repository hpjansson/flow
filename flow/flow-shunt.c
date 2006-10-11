/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-shunt.c - A connection to the outside world.
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

#include <unistd.h>
#include "flow-detailed-event.h"
#include "flow-position.h"
#include "flow-segment-request.h"
#include "flow-gobject-util.h"
#include "flow-util.h"
#include "flow-shunt.h"

/* Maximum internal buffers. May be allocated on stack. */

#define MAX_BUFFER 4096

/* Maximum number of packets to dispatch in one go, before returning
 * to main loop. Think of it as a time slice: Higher values reduce
 * overhead, but risk stalling the application's main thread. */

#define MAX_DISPATCH_PACKETS 32

struct _FlowShunt
{
  guint               shunt_type       : 4;

  guint               was_destroyed    : 1;  /* If user destroyed this shunt */

  guint               can_read         : 1;  /* If our channel is open for reads */
  guint               can_write        : 1;  /* If our channel is open for writes */

  guint               need_reads       : 1;  /* If we should watch for read events */
  guint               need_writes      : 1;  /* If we should watch for write events */

  guint               doing_reads      : 1;  /* If we are watching read events */
  guint               doing_writes     : 1;  /* If we are watching write events */

  guint               block_reads      : 1;  /* If user blocked read events */
  guint               block_writes     : 1;  /* If user blocked write events */

  guint               dispatched_begin : 1;  /* If we generated a beginning-of-stream packet */
  guint               dispatched_end   : 1;  /* If we generated an end-of-stream packet */
  guint               received_end     : 1;  /* If user queued an end-of-stream packet */

  guint               wait_dispatch    : 1;  /* Waiting for callback to dispatch to user */
  guint               in_dispatch      : 1;  /* In user dispatch callback; keep alive */
  guint               in_worker        : 1;  /* In worker thread; keep alive */

  guint               offset_changed   : 1;  /* Files only; wrote data since last position report */

  FlowPacketQueue    *read_queue;
  FlowPacketQueue    *write_queue;

  FlowShuntReadFunc  *read_func;
  gpointer            read_func_data;

  FlowShuntWriteFunc *write_func;
  gpointer            write_func_data;
};

static gboolean impl_is_initialized = FALSE;

/* ------------------------- *
 * Implementation Prototypes *
 * ------------------------- */

/* All of these must be implemented in flow-shunt-impl-*.c */

/* Init and finalization for the entire implementation. For instance, it
 * could load and unload shared objects (this is required for win32's
 * winsock DLL in particular). */

static void        flow_shunt_impl_init               (void);
static void        flow_shunt_impl_finalize           (void);

/* For a threaded implementation, these will operate the master lock,
 * controlling access to all shunts. */

static void        flow_shunt_impl_lock               (void);
static void        flow_shunt_impl_unlock             (void);

/* Indicates that the user is done with the shunt; proceed to
 * finalization ASAP. */

static void        flow_shunt_impl_destroy_shunt      (FlowShunt *shunt);

/* Frees any implementation-specific data. */

static void        flow_shunt_impl_finalize_shunt     (FlowShunt *shunt);

/* Shunt initialization functions. */

static FlowShunt  *flow_shunt_impl_open_file          (const gchar *path, FlowAccessMode access_mode);
static FlowShunt  *flow_shunt_impl_create_file        (const gchar *path, FlowAccessMode access_mode,
                                                      gboolean destructive,
                                                      FlowAccessMode creation_permissions_user,
                                                      FlowAccessMode creation_permissions_group,
                                                      FlowAccessMode creation_permissions_other);
static FlowShunt  *flow_shunt_impl_spawn_worker       (FlowWorkerFunc func, gpointer user_data);
static FlowShunt  *flow_shunt_impl_spawn_process      (FlowWorkerFunc func, gpointer user_data);
static FlowShunt  *flow_shunt_impl_spawn_command_line (const gchar *command_line);
static FlowShunt  *flow_shunt_impl_open_tcp_listener  (FlowIPService *local_service);
static FlowShunt  *flow_shunt_impl_connect_to_tcp     (FlowIPService *remote_service, gint local_port);
static FlowShunt  *flow_shunt_impl_open_udp_port      (FlowIPService *local_service);

/* Notifies the implementation that one of the need_* flags went from
 * FALSE to TRUE while its corresponding doing_* flag was FALSE. The
 * implementation should interrupt its wait or add the fd to epoll
 * or whatever.
 *
 * We don't notify when the flags go from TRUE to FALSE, because this
 * situation is best handled when the implementation actually receives
 * an event. It will then notice that the event is not desired, and
 * queue the data or do nothing. Doing it this way makes the
 * implementation wake up less frequently. */

static void        flow_shunt_impl_need_reads         (FlowShunt *shunt);
static void        flow_shunt_impl_need_writes        (FlowShunt *shunt);
static void        flow_shunt_impl_need_dispatch      (FlowShunt *shunt);

/* Synchronous shunt functions */

static FlowPacket *flow_sync_shunt_impl_read          (FlowSyncShunt *sync_shunt);
static FlowPacket *flow_sync_shunt_impl_try_read      (FlowSyncShunt *sync_shunt);
static void        flow_sync_shunt_impl_write         (FlowSyncShunt *sync_shunt, FlowPacket *packet);

/* ----------------- *
 * Helper Prototypes *
 * ----------------- */

/* These are used in the implementations. */

static void        flow_shunt_init                    (FlowShunt *shunt);
static void        flow_shunt_finalize                (FlowShunt *shunt);

static void        flow_shunt_read_state_changed      (FlowShunt *shunt);
static void        flow_shunt_write_state_changed     (FlowShunt *shunt);

/* ------------------------ *
 * Select an Implementation *
 * ------------------------ */

/* For the time being, we only have one implementation - the
 * "generic Unix-like" one. When we get more implementations,
 * we need ifdefs here. */

#include "flow-shunt-impl-unix.c"

/* ---------------- *
 * Helper Functions *
 * ---------------- */

static void
flow_shunt_init (FlowShunt *shunt)
{
  if (!impl_is_initialized)
  {
    flow_shunt_impl_init ();
    impl_is_initialized = TRUE;
  }

  shunt->read_queue  = flow_packet_queue_new ();
  shunt->write_queue = flow_packet_queue_new ();
}

static void
flow_shunt_finalize (FlowShunt *shunt)
{
  flow_shunt_impl_lock ();

  g_object_unref (shunt->read_queue);
  g_object_unref (shunt->write_queue);

  shunt->read_queue  = NULL;
  shunt->write_queue = NULL;

  flow_shunt_impl_finalize_shunt (shunt);

  flow_shunt_impl_unlock ();
}

/* Assumes that caller is holding the impl lock */
static void
flow_shunt_read_state_changed (FlowShunt *shunt)
{
  gboolean new_need_reads;

  /* We watch for reads when these conditions are met:
   * - Either of the following:
   *   - Reading is not blocked AND we have a read function
   *   - We haven't dispatched a "stream begins" event. */

  new_need_reads = 
    (shunt->can_read && flow_packet_queue_get_length_bytes (shunt->read_queue) <= MAX_BUFFER &&
     ((!shunt->block_reads && shunt->read_func) ||
     !shunt->dispatched_begin)) ? TRUE : FALSE;

  if (shunt->need_reads != new_need_reads)
  {
    shunt->need_reads = new_need_reads;

    if (!shunt->doing_reads && new_need_reads)
    {
      flow_shunt_impl_need_reads (shunt);
    }
  }

  if (flow_packet_queue_get_length_packets (shunt->read_queue) > 0 &&
      !shunt->block_reads && shunt->read_func)
    flow_shunt_impl_need_dispatch (shunt);
}

/* Assumes that caller is holding the impl lock */
static void
flow_shunt_write_state_changed (FlowShunt *shunt)
{
  gboolean new_need_writes;

  /* We watch for writes when these conditions are met:
   * - There are packets in the outgoing queue. */

  new_need_writes =
    (shunt->can_write && (flow_packet_queue_get_length_packets (shunt->write_queue) > 0 ||
      !shunt->dispatched_begin)) ? TRUE : FALSE;

  if (shunt->need_writes != new_need_writes)
  {
    shunt->need_writes = new_need_writes;

    if (!shunt->doing_writes && new_need_writes)
    {
      flow_shunt_impl_need_writes (shunt);
    }
  }

  if (!shunt->received_end &&
      flow_packet_queue_get_length_bytes (shunt->write_queue) <= MAX_BUFFER &&
      !shunt->block_writes && shunt->write_func)
    flow_shunt_impl_need_dispatch (shunt);
}

/* -------------------- *
 * FlowShunt public API *
 * -------------------- */

FlowShunt *
flow_open_file (const gchar *path, FlowAccessMode access_mode)
{
  g_return_val_if_fail (path != NULL, NULL);

  return flow_shunt_impl_open_file (path, access_mode);
}

FlowShunt *
flow_create_file (const gchar *path, FlowAccessMode access_mode, gboolean destructive,
                  FlowAccessMode creation_permissions_user,
                  FlowAccessMode creation_permissions_group,
                  FlowAccessMode creation_permissions_other)
{
  g_return_val_if_fail (path != NULL, NULL);

  return flow_shunt_impl_create_file (path, access_mode, destructive,
                                      creation_permissions_user,
                                      creation_permissions_group,
                                      creation_permissions_other);
}

FlowShunt *
flow_spawn_worker (FlowWorkerFunc func, gpointer user_data)
{
  g_return_val_if_fail (func != NULL, NULL);

  return flow_shunt_impl_spawn_worker (func, user_data);
}

FlowShunt *
flow_spawn_process (FlowWorkerFunc func, gpointer user_data)
{
  g_return_val_if_fail (func != NULL, NULL);

  return flow_shunt_impl_spawn_process (func, user_data);
}

FlowShunt *
flow_spawn_command_line (const gchar *command_line)
{
  g_return_val_if_fail (command_line != NULL, NULL);

  return flow_shunt_impl_spawn_command_line (command_line);
}

FlowShunt *
flow_open_udp_port (FlowIPService *local_service)
{
  g_return_val_if_fail (local_service == NULL || FLOW_IS_IP_SERVICE (local_service), NULL);

  return flow_shunt_impl_open_udp_port (local_service);
}

FlowShunt *
flow_open_tcp_listener (FlowIPService *local_service)
{
  g_return_val_if_fail (local_service == NULL || FLOW_IS_IP_SERVICE (local_service), NULL);

  return flow_shunt_impl_open_tcp_listener (local_service);
}

FlowShunt *
flow_connect_to_tcp (FlowIPService *remote_service, gint local_port)
{
  g_return_val_if_fail (FLOW_IS_IP_SERVICE (remote_service), NULL);
  g_return_val_if_fail (flow_ip_service_get_port (remote_service) > 0, NULL);

  return flow_shunt_impl_connect_to_tcp (remote_service, local_port);
}

void
flow_shunt_destroy (FlowShunt *shunt)
{
  g_return_if_fail (shunt != NULL);

  flow_shunt_impl_lock ();

  shunt->was_destroyed = TRUE;
  flow_shunt_impl_destroy_shunt (shunt);

  flow_shunt_impl_unlock ();
}

void
flow_shunt_set_read_func (FlowShunt *shunt, FlowShuntReadFunc *read_func, gpointer user_data)
{
  g_return_if_fail (shunt != NULL);

  flow_shunt_impl_lock ();

  shunt->read_func      = read_func;
  shunt->read_func_data = user_data;

  flow_shunt_read_state_changed (shunt);

  flow_shunt_impl_unlock ();
}

void
flow_shunt_set_write_func (FlowShunt *shunt, FlowShuntWriteFunc *write_func, gpointer user_data)
{
  g_return_if_fail (shunt != NULL);

  flow_shunt_impl_lock ();

  shunt->write_func      = write_func;
  shunt->write_func_data = user_data;

  flow_shunt_write_state_changed (shunt);

  flow_shunt_impl_unlock ();
}

void
flow_shunt_block_reads (FlowShunt *shunt)
{
  g_return_if_fail (shunt != NULL);

  flow_shunt_impl_lock ();

  if (!shunt->block_reads)
  {
    shunt->block_reads = TRUE;
    flow_shunt_read_state_changed (shunt);
  }

  flow_shunt_impl_unlock ();
}

void
flow_shunt_unblock_reads (FlowShunt *shunt)
{
  g_return_if_fail (shunt != NULL);

  flow_shunt_impl_lock ();

  if (shunt->block_reads)
  {
    shunt->block_reads = FALSE;
    flow_shunt_read_state_changed (shunt);
  }

  flow_shunt_impl_unlock ();
}

void
flow_shunt_block_writes (FlowShunt *shunt)
{
  g_return_if_fail (shunt != NULL);

  flow_shunt_impl_lock ();

  if (!shunt->block_writes)
  {
    shunt->block_writes = TRUE;
    flow_shunt_write_state_changed (shunt);
  }

  flow_shunt_impl_unlock ();
}

void
flow_shunt_unblock_writes (FlowShunt *shunt)
{
  g_return_if_fail (shunt != NULL);

  flow_shunt_impl_lock ();

  if (shunt->block_writes)
  {
    shunt->block_writes = FALSE;
    flow_shunt_write_state_changed (shunt);
  }

  flow_shunt_impl_unlock ();
}

void
flow_shutdown_shunts (void)
{
  flow_shunt_impl_lock ();

  flow_shunt_impl_finalize ();
  impl_is_initialized = FALSE;

  flow_shunt_impl_unlock ();
}

FlowPacket *
flow_sync_shunt_read (FlowSyncShunt *sync_shunt)
{
  g_return_val_if_fail (sync_shunt != NULL, NULL);

  return flow_sync_shunt_impl_read (sync_shunt);
}

FlowPacket *
flow_sync_shunt_try_read (FlowSyncShunt *sync_shunt)
{
  g_return_val_if_fail (sync_shunt != NULL, NULL);

  return flow_sync_shunt_impl_try_read (sync_shunt);
}

void
flow_sync_shunt_write (FlowSyncShunt *sync_shunt, FlowPacket *packet)
{
  g_return_if_fail (sync_shunt != NULL);
  g_return_if_fail (packet != NULL);

  flow_sync_shunt_impl_write (sync_shunt, packet);
}
