/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-mux-deserializer.h - Deserialize a multiplexed stream from a bytestream
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
#if !defined(_FLOW_MUX_DESERIALIZER_H)
#define _FLOW_MUX_DESERIALIZER_H

#include <flow/flow-simplex-element.h>

G_BEGIN_DECLS

/* FlowMuxDeserializer */
#define FLOW_TYPE_MUX_DESERIALIZER	   (flow_mux_deserializer_get_type ())
#define FLOW_MUX_DESERIALIZER(obj)	   (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_MUX_DESERIALIZER, FlowMuxDeserializer))
#define FLOW_MUX_DESERIALIZER_CLASS(vtable)  (G_TYPE_CHECK_CLASS_CAST ((vtable), FLOW_TYPE_MUX_DESERIALIZER, FlowMuxDeserializerClass))
#define FLOW_IS_MUX_DESERIALIZER(obj)        (G_TYPE_CHECK_TYPE ((obj), FLOW_TYPE_MUX_DESERIALIZER))
#define FLOW_MUX_DESERIALIZER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_MUX_DESERIALIZER, FlowMuxDeserializerClass))

typedef struct _FlowMuxDeserializer FlowMuxDeserializer;
typedef struct _FlowMuxDeserializerClass FlowMuxDeserializerClass;

struct _FlowMuxDeserializer
{
  FlowSimplexElement base;

  gpointer priv;
};

struct _FlowMuxDeserializerClass
{
  FlowSimplexElementClass base_class;
};

GType flow_mux_deserializer_get_type (void) G_GNUC_CONST;

FlowMuxDeserializer        *flow_mux_deserializer_new (void);

gint flow_mux_deserializer_get_header_size (FlowMuxDeserializer *deserializer);
void flow_mux_deserializer_unparse_header (FlowMuxDeserializer *deserializer,
                                           guint8 *hdr, guint channel_id, guint32 size);

G_END_DECLS

#endif
