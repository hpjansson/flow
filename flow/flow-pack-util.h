/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-pack-util.h - Utilities for packing basic types to buffers.
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

#ifndef _FLOW_PACK_UTIL_H
#define _FLOW_PACK_UTIL_H

#include <glib.h>
#include <flow/flow-packet-queue.h>

G_BEGIN_DECLS

guchar         *flow_pack_uint32 (guint32 n_in, guchar *buf_out);
gboolean        flow_unpack_uint32 (const guchar **buf_ptr_inout, const guchar *buf_end, guint32 *n_out);
guchar         *flow_pack_uint64 (guint64 n_in, guchar *buf_out);
gboolean        flow_unpack_uint64 (const guchar **buf_ptr_inout, const guchar *buf_end, guint64 *n_out);
guchar         *flow_pack_string (const gchar *string_in, guchar *buf_out);
gboolean        flow_unpack_string (const guchar **buf_ptr_inout, const guchar *buf_end, gchar **string_out);

gboolean        flow_unpack_uint32_from_iter (FlowPacketByteIter *iter, guint32 *n_out);
gboolean        flow_unpack_uint64_from_iter (FlowPacketByteIter *iter, guint64 *n_out);
gboolean        flow_unpack_string_from_iter (FlowPacketByteIter *iter, gchar **string_out);

G_END_DECLS

#endif  /* _FLOW_PACK_UTIL_H */
