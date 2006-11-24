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
#include "flow-context-mgmt.h"
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

typedef struct
{
  GSource    source;

  GPtrArray *waiting_shunts;
  GPtrArray *dispatching_shunts;
}
ShuntSource;

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

  ShuntSource        *shunt_source;

  FlowPacketQueue    *read_queue;
  FlowPacketQueue    *write_queue;

  FlowShuntReadFunc  *read_func;
  gpointer            read_func_data;

  FlowShuntWriteFunc *write_func;
  gpointer            write_func_data;
};

static gboolean   impl_is_initialized = FALSE;
static GPtrArray *shunt_sources       = NULL;

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
static FlowShunt  *flow_shunt_impl_open_udp_port      (FlowIPService *local_service);
static FlowShunt  *flow_shunt_impl_open_tcp_listener  (FlowIPService *local_service);
static FlowShunt  *flow_shunt_impl_connect_to_tcp     (FlowIPService *remote_service, gint local_port);

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

/* Synchronous shunt functions */

static FlowPacket *flow_sync_shunt_impl_read          (FlowSyncShunt *sync_shunt);
static FlowPacket *flow_sync_shunt_impl_try_read      (FlowSyncShunt *sync_shunt);
static void        flow_sync_shunt_impl_write         (FlowSyncShunt *sync_shunt, FlowPacket *packet);

/* ----------------- *
 * Helper Prototypes *
 * ----------------- */

/* These are used in the implementations. */

static void        flow_shunt_init_common             (FlowShunt *shunt, ShuntSource *shunt_source);
static void        flow_shunt_finalize_common         (FlowShunt *shunt);

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
 * Dispatch to User *
 * ---------------- */

static void
dispatch_for_shunt (FlowShunt *shunt, gint *n_reads_done, gint *n_writes_done)
{
  FlowPacket  *packet;
  FlowPacket  *packets [MAX_DISPATCH_PACKETS];
  gint         written_packets;
  gint         written_bytes;
  gint         read_packets;
  gint         read_bytes;
  gint         read_data_bytes;
  gint         j;

  written_bytes = flow_packet_queue_get_length_bytes (shunt->write_queue);

  /* Peek packets from read queue. We stage them in an array so we don't
   * have to lock/unlock around each peek. */

  read_packets    = MAX_DISPATCH_PACKETS;
  read_bytes      = 0;
  read_data_bytes = 0;
  flow_packet_queue_peek_packets (shunt->read_queue, packets, &read_packets);

  /* It's important that wait_dispatch be cleared before we unlock, as the shunt
   * may have to be re-queued for dispatches while we're running unlocked. */

  shunt->in_dispatch   = TRUE;
  shunt->wait_dispatch = FALSE;
  flow_shunt_impl_unlock ();

  /* --- UNLOCKED CODE BEGINS --- */

  /* Dispatch reads */

  for (j = 0; shunt->read_func && !shunt->block_reads && !shunt->was_destroyed && j < read_packets; j++)
  {
    guint packet_size;

    packet = packets [j];
    packet_size = flow_packet_get_size (packet);

    read_bytes += packet_size;
    if (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_BUFFER)
      read_data_bytes += packet_size;

    shunt->read_func (shunt, packet, shunt->read_func_data);
  }

  read_packets = j;

  /* Dispatch writes */

  for (written_packets = 0; shunt->write_func && !shunt->block_writes && !shunt->was_destroyed &&
                            written_packets < MAX_DISPATCH_PACKETS && written_bytes <= MAX_BUFFER &&
                            !shunt->received_end; written_packets++)
  {
    packet = shunt->write_func (shunt, shunt->write_func_data);
    if G_UNLIKELY (!packet)
      break;

    /* Written packets are staged in an array so we only have to lock once
     * when we queue them */
    packets [written_packets] = packet;
    written_bytes += flow_packet_get_size (packet);

    /* If we get end-of-stream, don't request any more writes */

    if G_UNLIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
    {
      FlowDetailedEvent *detailed_event = flow_packet_get_data (packet);

      if (FLOW_IS_DETAILED_EVENT (detailed_event) &&
          flow_detailed_event_matches (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
      {
        shunt->received_end = TRUE;
      }
    }
  }

  /* --- UNLOCKED CODE ENDS --- */

  flow_shunt_impl_lock ();
  shunt->in_dispatch = FALSE;

  /* Steal (not drop!) read packets. NOTE: The packets may already have been
   * freed by the user. */

  flow_packet_queue_steal (shunt->read_queue, read_packets, read_bytes, read_data_bytes);

  /* Queue written packets */

  for (j = 0; j < written_packets; j++)
    flow_packet_queue_push_packet (shunt->write_queue, packets [j]);

  if G_UNLIKELY (shunt->was_destroyed)
  {
    flow_shunt_impl_finalize_shunt (shunt);
  }
  else
  {
    if (read_packets > 0)
      flow_shunt_read_state_changed (shunt);
    if (written_packets > 0)
      flow_shunt_write_state_changed (shunt);
  }

  if G_UNLIKELY (n_reads_done)
    *n_reads_done = read_packets;
  if G_UNLIKELY (n_writes_done)
    *n_writes_done = written_packets;
}

static gboolean
dispatch_for_source (ShuntSource *shunt_source)
{
  GPtrArray *current_dispatch_shunts;
  guint      i;

  flow_shunt_impl_lock ();

  /* Swap dispatch_shunts so new dispatch requests aren't added to current
   * dispatch. If this were to happen, and one or more of the consumers are
   * slow, we might never leave this function. */

  current_dispatch_shunts          = shunt_source->waiting_shunts;
  shunt_source->waiting_shunts     = shunt_source->dispatching_shunts;
  shunt_source->dispatching_shunts = current_dispatch_shunts;

  /* Do dispatch */

  for (i = 0; i < current_dispatch_shunts->len; i++)
  {
    FlowShunt *shunt = g_ptr_array_index (current_dispatch_shunts, i);

    if (!shunt)
      continue;  /* Shunt was disposed while queued */

    dispatch_for_shunt (shunt, NULL, NULL);
  }

  /* Clear dispatch array */
  g_ptr_array_set_size (current_dispatch_shunts, 0);

  flow_shunt_impl_unlock ();
  return FALSE;
}

static gboolean
shunt_source_prepare (GSource *source, gint *timeout)
{
  ShuntSource *shunt_source = (ShuntSource *) source;

  if (shunt_source->waiting_shunts->len > 0)
  {
    *timeout = 0;
    return TRUE;
  }

  return FALSE;
}

static gboolean
shunt_source_check (GSource *source)
{
  ShuntSource *shunt_source = (ShuntSource *) source;

  if (shunt_source->waiting_shunts->len > 0)
    return TRUE;

  return FALSE;
}

static gboolean
shunt_source_dispatch (GSource *source, GSourceFunc user_func, gpointer user_data)
{
  if G_LIKELY (user_func)
    user_func (user_data);

  return TRUE;  /* FIXME: What does this mean? */
}

/* FIXME: Is this called in the g_source_unref() thread or in the context's thread? */
static void
shunt_source_finalize (GSource *source)
{
  ShuntSource *shunt_source = (ShuntSource *) source;

  g_ptr_array_remove_fast (shunt_sources, shunt_source);

  g_ptr_array_free (shunt_source->waiting_shunts, TRUE);
  g_ptr_array_free (shunt_source->dispatching_shunts, TRUE);
}

static GSourceFuncs shunt_source_funcs =
{
  shunt_source_prepare,
  shunt_source_check,
  shunt_source_dispatch,
  shunt_source_finalize,

  NULL,
  NULL
};

/* Assumes that caller is holding the impl lock */
/* Must run in user's thread */
static void
add_shunt_to_shunt_source (FlowShunt *shunt, ShuntSource *shunt_source)
{
  GMainContext    *main_context;
  GSource         *source;
  guint            i;

  if (shunt_source)
  {
    shunt->shunt_source = shunt_source;
    g_source_ref ((GSource *) shunt_source);
    return;
  }

  main_context = flow_get_main_context_for_current_thread ();

  if G_UNLIKELY (!shunt_sources)
    shunt_sources = g_ptr_array_new ();

  for (i = 0; i < shunt_sources->len; i++)
  {
    source = g_ptr_array_index (shunt_sources, i);

    if (g_source_get_context (source) == main_context)
    {
      shunt->shunt_source = (ShuntSource *) source;
      g_source_ref (source);
      return;
    }
  }

  source = g_source_new (&shunt_source_funcs, sizeof (ShuntSource));
  shunt_source = (ShuntSource *) source;

  shunt->shunt_source = shunt_source;

  g_source_set_priority    (source, G_PRIORITY_DEFAULT_IDLE);
  g_source_set_can_recurse (source, FALSE);
  g_source_set_callback    (source, (GSourceFunc) dispatch_for_source, source, NULL);
  g_source_attach          (source, main_context);

  shunt_source->waiting_shunts     = g_ptr_array_new ();
  shunt_source->dispatching_shunts = g_ptr_array_new ();

  g_ptr_array_add (shunt_sources, source);
}

/* Assumes that caller is holding the impl lock */
static void
remove_shunt_from_shunt_source (FlowShunt *shunt)
{
  ShuntSource *shunt_source = shunt->shunt_source;
  GSource     *source       = (GSource *) shunt_source;

  shunt->shunt_source = NULL;
  g_source_unref (source);
}

/* ---------------- *
 * Helper Functions *
 * ---------------- */

/* Invoked from implementation */
static void
flow_shunt_init_common (FlowShunt *shunt, ShuntSource *shunt_source)
{
  if (!impl_is_initialized)
  {
    flow_shunt_impl_init ();
    impl_is_initialized = TRUE;
  }

  shunt->read_queue  = flow_packet_queue_new ();
  shunt->write_queue = flow_packet_queue_new ();

  add_shunt_to_shunt_source (shunt, shunt_source);
}

/* Invoked from implementation */
static void
flow_shunt_finalize_common (FlowShunt *shunt)
{
  g_assert (shunt->was_destroyed == TRUE);

  remove_shunt_from_shunt_source (shunt);

  flow_gobject_unref_clear (shunt->read_queue);
  flow_gobject_unref_clear (shunt->write_queue);
}

/* Assumes that caller is holding the impl lock */
static void
flow_shunt_need_dispatch (FlowShunt *shunt)
{
  GSource      *source;
  ShuntSource  *shunt_source;
  GMainContext *main_context;

  if (shunt->wait_dispatch)
    return;

  shunt_source = shunt->shunt_source;
  source       = (GSource *) shunt_source;

  shunt->wait_dispatch = TRUE;
  g_ptr_array_add (shunt_source->waiting_shunts, shunt);

  /* If we're the first shunt to be added to this source since the
   * last dispatch, the main context may be sleeping. Wake it up. */

  if (shunt_source->waiting_shunts->len == 1)
  {
    main_context = g_source_get_context (source);
    g_main_context_wakeup (main_context);
  }
}

/* Assumes that caller is holding the impl lock */
static void
flow_shunt_read_state_changed (FlowShunt *shunt)
{
  gboolean new_need_reads;

  g_assert (shunt->was_destroyed == FALSE);

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
    flow_shunt_need_dispatch (shunt);
}

/* Assumes that caller is holding the impl lock */
static void
flow_shunt_write_state_changed (FlowShunt *shunt)
{
  gboolean new_need_writes;

  g_assert (shunt->was_destroyed == FALSE);

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
    flow_shunt_need_dispatch (shunt);
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
  g_return_if_fail (shunt->was_destroyed == FALSE);

  flow_shunt_impl_lock ();

  shunt->was_destroyed = TRUE;

  if (shunt->wait_dispatch)
  {
    ShuntSource *shunt_source = shunt->shunt_source;

    shunt->wait_dispatch = FALSE;
    flow_g_ptr_array_remove_sparse (shunt_source->waiting_shunts, shunt);
    flow_g_ptr_array_remove_sparse (shunt_source->dispatching_shunts, shunt);
  }

  flow_shunt_impl_destroy_shunt (shunt);

  flow_shunt_impl_unlock ();
}

void
flow_shunt_dispatch_now (FlowShunt *shunt, gint *n_reads_done, gint *n_writes_done)
{
  g_return_if_fail (shunt != NULL);
  g_return_if_fail (shunt->was_destroyed == FALSE);

  flow_shunt_impl_lock ();

  dispatch_for_shunt (shunt, n_reads_done, n_writes_done);

  flow_shunt_impl_unlock ();
}

void
flow_shunt_get_read_func (FlowShunt *shunt, FlowShuntReadFunc **read_func, gpointer *user_data)
{
  g_return_if_fail (shunt != NULL);
  g_return_if_fail (shunt->was_destroyed == FALSE);

  flow_shunt_impl_lock ();

  if (read_func)
    *read_func = shunt->read_func;

  if (user_data)
    *user_data = shunt->read_func_data;

  flow_shunt_impl_unlock ();
}

void
flow_shunt_set_read_func (FlowShunt *shunt, FlowShuntReadFunc *read_func, gpointer user_data)
{
  g_return_if_fail (shunt != NULL);
  g_return_if_fail (shunt->was_destroyed == FALSE);

  flow_shunt_impl_lock ();

  shunt->read_func      = read_func;
  shunt->read_func_data = user_data;

  flow_shunt_read_state_changed (shunt);

  flow_shunt_impl_unlock ();
}

void
flow_shunt_get_write_func (FlowShunt *shunt, FlowShuntWriteFunc **write_func, gpointer *user_data)
{
  g_return_if_fail (shunt != NULL);
  g_return_if_fail (shunt->was_destroyed == FALSE);

  flow_shunt_impl_lock ();

  if (write_func)
    *write_func = shunt->write_func;

  if (user_data)
    *user_data = shunt->write_func_data;

  flow_shunt_impl_unlock ();
}

void
flow_shunt_set_write_func (FlowShunt *shunt, FlowShuntWriteFunc *write_func, gpointer user_data)
{
  g_return_if_fail (shunt != NULL);
  g_return_if_fail (shunt->was_destroyed == FALSE);

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
  g_return_if_fail (shunt->was_destroyed == FALSE);

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
  g_return_if_fail (shunt->was_destroyed == FALSE);

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
  g_return_if_fail (shunt->was_destroyed == FALSE);

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
  g_return_if_fail (shunt->was_destroyed == FALSE);

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
