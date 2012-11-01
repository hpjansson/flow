/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-pack-util.c - Utilities for packing basic types to buffers.
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

#include "config.h"

#include <string.h>
#include "flow-pack-util.h"

guchar *
flow_pack_uint64 (guint64 n_in, guchar *buf_out)
{
  while (n_in > 0x7f)
  {
    *(buf_out++) = 0x80 | (n_in & 0x7f);
    n_in >>= 7;
  }

  *(buf_out++) = n_in;
  return buf_out;
}

gboolean
flow_unpack_uint64 (const guchar **buf_ptr_inout, const guchar *buf_end, guint64 *n_out)
{
  guint64 n = 0;
  guchar c;
  guint shift = 0;
  const guchar *buf_in = *buf_ptr_inout;

  do
  {
    if (buf_in == buf_end)
      return FALSE;

    c = *(buf_in++);
    n |= ((guint64) c & 0x7f) << shift;
    shift += 7;
  }
  while (c & 0x80);

  *buf_ptr_inout = buf_in;
  *n_out = n;
  return TRUE;
}

guchar *
flow_pack_uint32 (guint32 n_in, guchar *buf_out)
{
  while (n_in > 0x7f)
  {
    *(buf_out++) = 0x80 | (n_in & 0x7f);
    n_in >>= 7;
  }

  *(buf_out++) = n_in;
  return buf_out;
}

gboolean
flow_unpack_uint32 (const guchar **buf_ptr_inout, const guchar *buf_end, guint32 *n_out)
{
  guint32 n = 0;
  guchar c;
  guint shift = 0;
  const guchar *buf_in = *buf_ptr_inout;

  do
  {
    if (buf_in == buf_end)
      return FALSE;

    c = *(buf_in++);
    n |= (c & 0x7f) << shift;
    shift += 7;
  }
  while (c & 0x80);

  *buf_ptr_inout = buf_in;
  *n_out = n;
  return TRUE;
}

guchar *
flow_pack_string (const gchar *string_in, guchar *buf_out)
{
  guint32 len;

  len = strlen (string_in);
  buf_out = flow_pack_uint32 (len, buf_out);
  memcpy (buf_out, string_in, len);

  return buf_out + len;
}

gboolean
flow_unpack_string (const guchar **buf_ptr_inout, const guchar *buf_end, gchar **string_out)
{
  guint32 len;
  const guchar *buf_in = *buf_ptr_inout;

  if (!flow_unpack_uint32 (&buf_in, buf_end, &len))
    return FALSE;

  if (buf_in + len > buf_end)
    return FALSE;

  *string_out = g_malloc (len + 1);

  memcpy (*string_out, buf_in, len);
  (*string_out) [len] = '\0';

  *buf_ptr_inout = buf_in + len;
  return TRUE;
}

gboolean
flow_unpack_uint32_from_iter (FlowPacketByteIter *iter, guint32 *n_out)
{
  guchar buf [5];
  const guchar *p = buf;
  gint len;

  len = flow_packet_byte_iter_peek (iter, buf, 5);

  if (flow_unpack_uint32 (&p, buf + len, n_out))
  {
    flow_packet_byte_iter_advance (iter, p - buf);
    return TRUE;
  }

  return FALSE;
}

gboolean
flow_unpack_uint64_from_iter (FlowPacketByteIter *iter, guint64 *n_out)
{
  guchar buf [9];
  const guchar *p = buf;
  gint len;

  len = flow_packet_byte_iter_peek (iter, buf, 9);

  if (flow_unpack_uint64 (&p, buf + len, n_out))
  {
    flow_packet_byte_iter_advance (iter, p - buf);
    return TRUE;
  }

  return FALSE;
}

gboolean
flow_unpack_string_from_iter (FlowPacketByteIter *iter, gchar **string_out)
{
  guchar buf [5];
  const guchar *p = buf;
  gint len;
  guint32 n;

  len = flow_packet_byte_iter_peek (iter, buf, 5);

  if (!flow_unpack_uint32 (&p, buf + len, &n))
    return FALSE;

  flow_packet_byte_iter_advance (iter, p - buf);

  if (flow_packet_byte_iter_get_remaining_bytes (iter) < n)
    return FALSE;

  *string_out = g_malloc (n + 1);
  len = flow_packet_byte_iter_pop (iter, *string_out, n);
  g_assert (len == n);
  *(*string_out + n) = '\0';

  return TRUE;
}
