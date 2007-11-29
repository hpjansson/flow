/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-mux-serializer.h - Serialize a multiplexed stream into plain buffers
 *
 * Copyright (C) 2007 Andreas Rottmann
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
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors: Andreas Rottmann <a.rottmann@gmx.at>
 */
#if !defined(_FLOW_MUX_SERIALIZER_H)
#define _FLOW_MUX_SERIALIZER_H

#include <flow/flow-simplex-element.h>

G_BEGIN_DECLS

/* FlowMuxSerializer */
#define FLOW_TYPE_MUX_SERIALIZER	   (flow_mux_serializer_get_type ())
#define FLOW_MUX_SERIALIZER(obj)	   (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_MUX_SERIALIZER, FlowMuxSerializer))
#define FLOW_MUX_SERIALIZER_CLASS(vtable)  (G_TYPE_CHECK_CLASS_CAST ((vtable), FLOW_TYPE_MUX_SERIALIZER, FlowMuxSerializerClass))
#define FLOW_IS_MUX_SERIALIZER(obj)        (G_TYPE_CHECK_TYPE ((obj), FLOW_TYPE_MUX_SERIALIZER))
#define FLOW_MUX_SERIALIZER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_MUX_SERIALIZER, FlowMuxSerializerClass))

typedef struct _FlowMuxSerializer FlowMuxSerializer;
typedef struct _FlowMuxSerializerClass FlowMuxSerializerClass;
typedef struct _FlowMuxHeaderOps FlowMuxHeaderOps;

struct _FlowMuxSerializer
{
  FlowSimplexElement base;

  gpointer priv;
};

struct _FlowMuxSerializerClass
{
  FlowSimplexElementClass base_class;
};

struct _FlowMuxHeaderOps
{
  guint (*get_size) (gpointer user_data);
  void  (*parse)    (const guint8 *buffer, guint *channel_id, guint32 *size, gpointer user_data);
  void  (*unparse)  (guint8 *buffer, guint channel_id, guint32 size, gpointer user_data);
};

GType flow_mux_serializer_get_type (void) G_GNUC_CONST;

FlowMuxSerializer        *flow_mux_serializer_new (void);

guint flow_mux_serializer_get_header_size (FlowMuxSerializer *serializer);
void  flow_mux_serializer_parse_header (FlowMuxSerializer *serializer,
                                        const guint8 *hdr,
                                        guint *channel_id,
                                        guint32 *size);

extern FlowMuxHeaderOps flow_mux_serializer_default_ops;

G_END_DECLS

#endif
