/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-pad.h - A pipeline element connector.
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

#ifndef _FLOW_PAD_H
#define _FLOW_PAD_H

/* FlowPad and FlowElement are co-dependent types */
typedef struct _FlowPad        FlowPad;
typedef struct _FlowPadPrivate FlowPadPrivate;
typedef struct _FlowPadClass   FlowPadClass;

#include <glib-object.h>
#include <flow/flow-element.h>
#include <flow/flow-packet-queue.h>

G_BEGIN_DECLS

#define FLOW_TYPE_PAD            (flow_pad_get_type ())
#define FLOW_PAD(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_PAD, FlowPad))
#define FLOW_PAD_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_PAD, FlowPadClass))
#define FLOW_IS_PAD(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_PAD))
#define FLOW_IS_PAD_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_PAD))
#define FLOW_PAD_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_PAD, FlowPadClass))
GType   flow_pad_get_type        (void) G_GNUC_CONST;

struct _FlowPad
{
  GObject          parent;

  /*< private >*/

  /* --- Protected --- */

  guint            is_blocked     : 1;
  guint            was_disposed   : 1;
  guint            dispatch_depth : 15;

  FlowPacketQueue *packet_queue;
  FlowElement     *owner_element;
  FlowPad         *connected_pad;

  /* --- Private --- */

  FlowPadPrivate *priv;
};

struct _FlowPadClass
{
  GObjectClass parent_class;

  /* Methods */

  void (*push)    (FlowPad *pad, FlowPacket *packet);
  void (*block)   (FlowPad *pad);
  void (*unblock) (FlowPad *pad);

  /*< private >*/

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

void             flow_pad_push                (FlowPad *pad, FlowPacket *packet);

void             flow_pad_connect             (FlowPad *pad, FlowPad *other_pad);
void             flow_pad_disconnect          (FlowPad *pad);

void             flow_pad_block               (FlowPad *pad);
void             flow_pad_unblock             (FlowPad *pad);
gboolean         flow_pad_is_blocked          (FlowPad *pad);

FlowElement     *flow_pad_get_owner_element   (FlowPad *pad);
FlowPad         *flow_pad_get_connected_pad   (FlowPad *pad);

FlowPacketQueue *flow_pad_get_packet_queue    (FlowPad *pad);
FlowPacketQueue *flow_pad_ensure_packet_queue (FlowPad *pad);

G_END_DECLS

#endif  /* _FLOW_PAD_H */
