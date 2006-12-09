/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-packet.h - A data packet.
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

#ifndef _FLOW_PACKET_H
#define _FLOW_PACKET_H

#include <glib-object.h>

G_BEGIN_DECLS

#define FLOW_PACKET_MAX_SIZE (1 << 29)

typedef enum
{
  FLOW_PACKET_FORMAT_BUFFER,
  FLOW_PACKET_FORMAT_OBJECT
}
FlowPacketFormat;

typedef struct _FlowPacket FlowPacket;

struct _FlowPacket
{
  guint format          :  2;
  guint size            : 30;
};

FlowPacket       *flow_packet_new             (FlowPacketFormat format, gpointer data, guint size);
FlowPacket       *flow_packet_new_take_object (gpointer object, guint size);
FlowPacket       *flow_packet_copy            (FlowPacket *packet);
void              flow_packet_free            (FlowPacket *packet);

FlowPacketFormat  flow_packet_get_format      (FlowPacket *packet);
guint             flow_packet_get_size        (FlowPacket *packet);
gpointer          flow_packet_get_data        (FlowPacket *packet);

G_END_DECLS

#endif  /* _FLOW_PACKET_H */
