/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-shunt.h - A connection to the outside world.
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

#ifndef _FLOW_SHUNT_H
#define _FLOW_SHUNT_H

#include <flow/flow-event-codes.h>
#include <flow/flow-packet-queue.h>
#include <flow/flow-ip-service.h>
#include <flow/flow-detailed-event.h>

G_BEGIN_DECLS

typedef enum
{
  FLOW_NO_ACCESS       = 0,
  FLOW_READ_ACCESS     = (1 << 0),
  FLOW_WRITE_ACCESS    = (1 << 1),
  FLOW_EXECUTE_ACCESS  = (1 << 2)
}
FlowAccessMode;

typedef struct _FlowShunt     FlowShunt;
typedef struct _FlowSyncShunt FlowSyncShunt;

typedef void        (FlowShuntReadFunc)   (FlowShunt *shunt, FlowPacket *packet, gpointer user_data);
typedef FlowPacket *(FlowShuntWriteFunc)  (FlowShunt *shunt, gpointer user_data);
typedef void        (FlowWorkerFunc)      (FlowSyncShunt *sync_shunt, gpointer user_data);

FlowShunt  *flow_open_file              (const gchar *path, FlowAccessMode access_mode);
FlowShunt  *flow_create_file            (const gchar *path, FlowAccessMode access_mode, gboolean destructive,
                                         FlowAccessMode creation_permissions_user,
                                         FlowAccessMode creation_permissions_group,
                                         FlowAccessMode creation_permissions_other);
FlowShunt  *flow_spawn_worker           (FlowWorkerFunc func, gpointer user_data);
FlowShunt  *flow_spawn_process          (FlowWorkerFunc func, gpointer user_data);
FlowShunt  *flow_spawn_command_line     (const gchar *command_line);
FlowShunt  *flow_open_udp_port          (FlowIPService *local_service);
FlowShunt  *flow_open_tcp_listener      (FlowIPService *local_service);
FlowShunt  *flow_connect_to_tcp         (FlowIPService *remote_service, gint local_port);

void        flow_shunt_destroy          (FlowShunt *shunt);

void        flow_shunt_dispatch_now     (FlowShunt *shunt, gint *n_reads_done, gint *n_writes_done);

void        flow_shunt_get_read_func    (FlowShunt *shunt, FlowShuntReadFunc **read_func, gpointer *user_data);
void        flow_shunt_set_read_func    (FlowShunt *shunt, FlowShuntReadFunc *read_func, gpointer user_data);

void        flow_shunt_get_write_func   (FlowShunt *shunt, FlowShuntWriteFunc **write_func, gpointer *user_data);
void        flow_shunt_set_write_func   (FlowShunt *shunt, FlowShuntWriteFunc *write_func, gpointer user_data);

void        flow_shunt_block_reads      (FlowShunt *shunt);
void        flow_shunt_unblock_reads    (FlowShunt *shunt);

void        flow_shunt_block_writes     (FlowShunt *shunt);
void        flow_shunt_unblock_writes   (FlowShunt *shunt);

/* --- Release global resources --- */

void        flow_shutdown_shunts        (void);

/* --- For FlowWorkerFunc implementations only --- */

gboolean    flow_sync_shunt_read        (FlowSyncShunt *sync_shunt, FlowPacket **packet_dest);
gboolean    flow_sync_shunt_try_read    (FlowSyncShunt *sync_shunt, FlowPacket **packet_dest);
void        flow_sync_shunt_write       (FlowSyncShunt *sync_shunt, FlowPacket *packet);

G_END_DECLS

#endif  /* _FLOW_SHUNT_H */
