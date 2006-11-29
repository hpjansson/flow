/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tls-protocol.h - TLS/SSL protocol element.
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

#ifndef _FLOW_TLS_PROTOCOL_H
#define _FLOW_TLS_PROTOCOL_H

#include <flow/flow-duplex-element.h>

G_BEGIN_DECLS

#define FLOW_TYPE_TLS_PROTOCOL            (flow_tls_protocol_get_type ())
#define FLOW_TLS_PROTOCOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_TLS_PROTOCOL, FlowTlsProtocol))
#define FLOW_TLS_PROTOCOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_TLS_PROTOCOL, FlowTlsProtocolClass))
#define FLOW_IS_TLS_PROTOCOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_TLS_PROTOCOL))
#define FLOW_IS_TLS_PROTOCOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_TLS_PROTOCOL))
#define FLOW_TLS_PROTOCOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_TLS_PROTOCOL, FlowTlsProtocolClass))
GType   flow_tls_protocol_get_type        (void) G_GNUC_CONST;

typedef enum
{
  FLOW_AGENT_ROLE_SERVER,
  FLOW_AGENT_ROLE_CLIENT
}
FlowAgentRole;

typedef struct _FlowTlsProtocol      FlowTlsProtocol;
typedef struct _FlowTlsProtocolClass FlowTlsProtocolClass;

struct _FlowTlsProtocol
{
  FlowDuplexElement    parent;

  gpointer             priv;
};

struct _FlowTlsProtocolClass
{
  FlowDuplexElementClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

G_END_DECLS

FlowTlsProtocol *flow_tls_protocol_new (FlowAgentRole agent_role);

#endif  /* _FLOW_TLS_PROTOCOL_H */
