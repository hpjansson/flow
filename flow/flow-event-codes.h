/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-event-codes.h - Event codes and domains for builtin events.
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

#ifndef _FLOW_EVENT_CODES_H
#define _FLOW_EVENT_CODES_H

#include <glib.h>

G_BEGIN_DECLS

#define FLOW_STREAM_DOMAIN "flow-stream"
#define FLOW_FILE_DOMAIN   "flow-file"
#define FLOW_EXEC_DOMAIN   "flow-exec"
#define FLOW_SOCKET_DOMAIN "flow-socket"
#define FLOW_LOOKUP_DOMAIN "flow-lookup"
#define FLOW_SSH_DOMAIN    "flow-ssh"

#define FLOW_STREAM_DOMAIN_QUARK flow_stream_domain_quark ()
#define FLOW_FILE_DOMAIN_QUARK   flow_file_domain_quark ()
#define FLOW_EXEC_DOMAIN_QUARK   flow_exec_domain_quark ()
#define FLOW_SOCKET_DOMAIN_QUARK flow_socket_domain_quark ()
#define FLOW_LOOKUP_DOMAIN_QUARK flow_lookup_domain_quark ()
#define FLOW_SSH_DOMAIN_QUARK    flow_ssh_domain_quark ()

typedef enum
{
  FLOW_STREAM_BEGIN,
  FLOW_STREAM_END,
  FLOW_STREAM_END_CONVERSE,
  FLOW_STREAM_DENIED,
  FLOW_STREAM_SEGMENT_BEGIN,
  FLOW_STREAM_SEGMENT_END,
  FLOW_STREAM_SEGMENT_DENIED,

  FLOW_STREAM_ERROR,
  FLOW_STREAM_APP_ERROR,
  FLOW_STREAM_PHYSICAL_ERROR,
  FLOW_STREAM_RESOURCE_ERROR,

  FLOW_STREAM_FLUSH,
}
FlowStreamEventCode;

typedef enum
{
  FLOW_FILE_REACHED_END,

  FLOW_FILE_NEED_RESTART,
  FLOW_FILE_RESTART,

  FLOW_FILE_PERMISSION_DENIED,
  FLOW_FILE_IS_NOT_A_FILE,
  FLOW_FILE_TOO_MANY_LINKS,
  FLOW_FILE_OUT_OF_HANDLES,
  FLOW_FILE_PATH_TOO_LONG,
  FLOW_FILE_NO_SPACE,
  FLOW_FILE_IS_READ_ONLY,
  FLOW_FILE_IS_LOCKED,
  FLOW_FILE_DOES_NOT_EXIST
}
FlowFileEventCode;

typedef enum
{
  FLOW_EXEC_PARSE_ERROR,
  FLOW_EXEC_RUN_ERROR
}
FlowExecEventCode;

typedef enum
{
  FLOW_SOCKET_ADDRESS_PROTECTED,
  FLOW_SOCKET_ADDRESS_IN_USE,
  FLOW_SOCKET_ADDRESS_DOES_NOT_EXIST,
  FLOW_SOCKET_CONNECTION_REFUSED,
  FLOW_SOCKET_CONNECTION_RESET,
  FLOW_SOCKET_NETWORK_UNREACHABLE,
  FLOW_SOCKET_ACCEPT_ERROR,
  FLOW_SOCKET_OVERSIZED_PACKET
}
FlowSocketEventCode;

typedef enum
{
  FLOW_LOOKUP_TEMPORARY_SERVER_FAILURE,
  FLOW_LOOKUP_PERMANENT_SERVER_FAILURE,
  FLOW_LOOKUP_NO_RECORDS
}
FlowLookupEventCode;

typedef enum
{
  FLOW_SSH_MASTER_FAILED,
  FLOW_SSH_MASTER_NOT_CONNECTED
}
FlowSshEventCode;

GQuark flow_stream_domain_quark (void);
GQuark flow_file_domain_quark   (void);
GQuark flow_exec_domain_quark   (void);
GQuark flow_socket_domain_quark (void);
GQuark flow_lookup_domain_quark (void);
GQuark flow_ssh_domain_quark    (void);

G_END_DECLS

#endif  /* _FLOW_EVENT_CODES_H */
