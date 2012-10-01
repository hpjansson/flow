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

const guchar *
flow_unpack_uint64 (const guchar *buf_in, guint64 *n_out)
{
  guint64 n = 0;
  guchar c;
  guint shift = 0;

  do
  {
    c = *(buf_in++);
    n |= (c & 0x7f) << shift;
    shift += 7;
  }
  while (c & 0x80);

  *n_out = n;
  return buf_in;
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

const guchar *
flow_unpack_uint32 (const guchar *buf_in, guint32 *n_out)
{
  guint64 n = 0;
  guchar c;
  guint shift = 0;

  do
  {
    c = *(buf_in++);
    n |= (c & 0x7f) << shift;
    shift += 7;
  }
  while (c & 0x80);

  *n_out = n;
  return buf_in;
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

const guchar *
flow_unpack_string (const guchar *buf_in, gchar *string_out)
{
  guint32 len;

  buf_in = flow_unpack_uint32 (buf_in, &len);
  memcpy (string_out, buf_in, len);
  string_out [len] = '\0';

  return buf_in;
}
