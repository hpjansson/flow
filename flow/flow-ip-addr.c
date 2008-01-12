/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-addr.c - An Internet address.
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include "flow-gobject-util.h"
#include "flow-ip-addr.h"

/* --- Class properties --- */
FLOW_GOBJECT_PROPERTIES_BEGIN (flow_ip_addr)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- Class definition --- */
FLOW_GOBJECT_MAKE_IMPL_NO_PRIVATE (flow_ip_addr, FlowIPAddr, G_TYPE_OBJECT, 0)

static void
flow_ip_addr_type_init (GType type)
{
}

static void
flow_ip_addr_class_init (FlowIPAddrClass *klass)
{
}

static void
flow_ip_addr_init (FlowIPAddr *ip_addr)
{
  ip_addr->family = FLOW_IP_ADDR_INVALID;
}

static void
flow_ip_addr_construct (FlowIPAddr *ip_addr)
{
}

static void
flow_ip_addr_dispose (FlowIPAddr *ip_addr)
{
}

static void
flow_ip_addr_finalize (FlowIPAddr *ip_addr)
{
}

FlowIPAddr *
flow_ip_addr_new (void)
{
  return g_object_new (FLOW_TYPE_IP_ADDR, NULL);
}

gboolean
flow_ip_addr_is_valid (FlowIPAddr *ip_addr)
{
  return ip_addr->family == FLOW_IP_ADDR_INVALID ? FALSE : TRUE;
}

FlowIPAddrFamily
flow_ip_addr_get_family (FlowIPAddr *ip_addr)
{
  return ip_addr->family;
}

gchar *
flow_ip_addr_get_string (FlowIPAddr *ip_addr)
{
  switch (ip_addr->family)
  {
    case FLOW_IP_ADDR_IPV4:
      return g_strdup_printf ("%"  G_GUINT16_FORMAT ".%" G_GUINT16_FORMAT
                              ".%" G_GUINT16_FORMAT ".%" G_GUINT16_FORMAT,
                              ip_addr->addr [0], ip_addr->addr [1],
                              ip_addr->addr [2], ip_addr->addr [3]);

    case FLOW_IP_ADDR_IPV6:
      /* TODO: Output compressed form if possible */
      return g_strdup_printf ("%"   /* G_GINT16_MODIFIER */ "X:%" /* G_GINT16_MODIFIER */
                              "X:%" /* G_GINT16_MODIFIER */ "X:%" /* G_GINT16_MODIFIER */
                              "X:%" /* G_GINT16_MODIFIER */ "X:%" /* G_GINT16_MODIFIER */
                              "X:%" /* G_GINT16_MODIFIER */ "X:%" /* G_GINT16_MODIFIER */ "X",
                              (ip_addr->addr  [0] << 8) | ip_addr->addr  [1],
                              (ip_addr->addr  [2] << 8) | ip_addr->addr  [3],
                              (ip_addr->addr  [4] << 8) | ip_addr->addr  [5],
                              (ip_addr->addr  [6] << 8) | ip_addr->addr  [7],
                              (ip_addr->addr  [8] << 8) | ip_addr->addr  [9],
                              (ip_addr->addr [10] << 8) | ip_addr->addr [11],
                              (ip_addr->addr [12] << 8) | ip_addr->addr [13],
                              (ip_addr->addr [14] << 8) | ip_addr->addr [15]);
    default:
      break;
  }

  /* Invalid */
  return NULL;
}

static gint
parse_ipv4_field (const gchar *field, const gchar *punct)
{
  guint val = 0;

  for ( ; field < punct; field++)
  {
    gint d;

    val *= 10;
    d = *field - '0';

    if (d < 0 || d > 9)
      return -1;

    val += d;
  }

  if (val > 0xff)
    return -1;

  return val;
}

static gint
parse_ipv6_field (const gchar *field, const gchar *punct)
{
  guint val = 0;

  for ( ; field < punct; field++)
  {
    gint d;

    val *= 16;
    d = *field - '0';

    if (d < 0 || d > 9)
    {
      d = g_ascii_tolower (*field) - 'a' + 10;
      if (d < 10 || d > 15)
        return -1;
    }

    val += d;
  }

  if (val > 0xffff)
    return -1;

  return val;
}

gboolean
flow_ip_addr_set_string (FlowIPAddr *ip_addr, const gchar *string)
{
  static const gchar  valid_chars []     = "0123456789abcdefABCDEF.:";
  static const gchar  separator_chars [] = ".:";
  gboolean            well_formed        = TRUE;
  gint                compress_start     = -1;
  FlowIPAddrFamily    last_field_family  = FLOW_IP_ADDR_INVALID;
  FlowIPAddrFamily    addr_family        = FLOW_IP_ADDR_INVALID;
  gint                ipv4_fields        = 0;
  guint               string_len;
  const gchar        *field;
  const gchar        *punct;
  guint8              addr [16];
  gint                t, i;

  g_return_val_if_fail (string != NULL, FALSE);

  string_len = strlen (string);

  /* Bail if contains invalid characters */

  if (strspn (string, valid_chars) != string_len)
    return FALSE;

  /* Parse */

  for (i = 0, field = string; ; )
  {
    punct = field + strcspn (field, separator_chars);

    if (punct == field)
    {
      /* Two puncts in a row, or punct at beginning - must be "::". If first char is a '\0', or
       * if we have a single punctuation at the end of the string, we'll also end up here. */

      if (*punct != ':' || (i == 0 && *(punct + 1) != ':') || (i > 0 && *(punct - 1) != ':'))
      {
        well_formed = FALSE;
        break;
      }

      /* Can't have more than one :: in an address */
      if (compress_start >= 0)
      {
        well_formed = FALSE;
        break;
      }

      addr_family = FLOW_IP_ADDR_IPV6;
      last_field_family = FLOW_IP_ADDR_IPV6;
      compress_start = i;

      if (i == 0)
        field += 2;
      else
        field += 1;

      if (!*field)
        break;

      continue;
    }

    if (*punct == '.' || (*punct == '\0' && last_field_family == FLOW_IP_ADDR_IPV4))
    {
      /* IPv4 style - decimal */

      if (addr_family == FLOW_IP_ADDR_INVALID)
        addr_family = FLOW_IP_ADDR_IPV4;
      last_field_family = FLOW_IP_ADDR_IPV4;
      t = parse_ipv4_field (field, punct);

      if (t < 0 || i > 15 || ++ipv4_fields > 4)
      {
        /* Bad field contents, too long address, or too long IPv4 portion of IPv6 address */
        well_formed = FALSE;
        break;
      }

      addr [i++] = t;
    }
    else if (*punct == ':' || last_field_family == FLOW_IP_ADDR_IPV6)
    {
      /* IPv6 style - hexadecimal */

      /* Can have IPv4 syntax only at the end of an IPv6 string */
      if (last_field_family == FLOW_IP_ADDR_IPV4)
      {
        well_formed = FALSE;
        break;
      }

      addr_family = FLOW_IP_ADDR_IPV6;
      last_field_family = FLOW_IP_ADDR_IPV6;
      t = parse_ipv6_field (field, punct);

      if (t < 0 || i > 14)
      {
        /* Bad field contents, or too long address */
        well_formed = FALSE;
        break;
      }

      addr [i++] = t >> 8;
      addr [i++] = t & 0xff;
    }

    if (!*punct)
      break;

    field = punct + 1;
  }

  /* Post-process */

  if (well_formed)
  {
    if (compress_start >= 0)
    {
      /* Maybe decompress IPv6 shorthand */
      memmove (&addr [16 - (i - compress_start)], &addr [compress_start], i - compress_start);
      memset (&addr [compress_start], 0, 16 - i);
    }
    else if (addr_family == FLOW_IP_ADDR_IPV6)
    {
      /* IPv6 addresses with no compression in them must be specific */
      if (i != 16)
        well_formed = FALSE;
    }
    else if (addr_family == FLOW_IP_ADDR_IPV4)
    {
      /* IPv4 addresses must always be specific. If it is, clear the rest of the
       * field for cleanliness. */
      if (i != 4)
        well_formed = FALSE;
      else
        memset (&addr [4], 0, 16 - 4);
    }
    else
    {
      /* addr_family == FLOW_IP_ADDR_INVALID */
      well_formed = FALSE;
    }
  }

  /* If it's still well formed, set our address */

  if (well_formed)
  {
    ip_addr->family = addr_family;
    memcpy (ip_addr->addr, addr, 16);
  }

  return well_formed;
}

gboolean
flow_ip_addr_get_raw (FlowIPAddr *ip_addr, guint8 *dest, gint *len)
{
  g_return_val_if_fail (dest != NULL, FALSE);

  switch (ip_addr->family)
  {
    case FLOW_IP_ADDR_IPV4:
      memcpy (dest, ip_addr->addr, 4);
      *len = 4;
      break;

    case FLOW_IP_ADDR_IPV6:
      memcpy (dest, ip_addr->addr, 16);
      *len = 16;
      break;

    default:
      return FALSE;
  }

  return TRUE;
}

gboolean
flow_ip_addr_set_raw (FlowIPAddr *ip_addr, const guint8 *src, gint len)
{
  g_return_val_if_fail (src != NULL, FALSE);
  g_return_val_if_fail (len > 0, FALSE);

  switch (len)
  {
    case 4:
      ip_addr->family = FLOW_IP_ADDR_IPV4;
      memset (ip_addr->addr, 0, 16);
      memcpy (ip_addr->addr, src, 4);
      break;

    case 16:
      ip_addr->family = FLOW_IP_ADDR_IPV6;
      memcpy (ip_addr->addr, src, 16);
      break;

    default:
      return FALSE;
  }

  return TRUE;
}

gboolean
flow_ip_addr_is_loopback (FlowIPAddr *ip_addr)
{
  if (ip_addr->family == FLOW_IP_ADDR_IPV4)
  {
    guint32 addr;

    addr = GUINT32_FROM_BE (*((guint32 *) ip_addr->addr));

    if ((addr & 0xff000000) == 127 << 24)
      return TRUE;
  }
  else
  {
    const guint8 loopback_addr [16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

    /* ::1 (loopback) */

    if (!memcmp (ip_addr->addr, loopback_addr, 16))
      return TRUE;
  }

  return FALSE;
}

gboolean
flow_ip_addr_is_multicast (FlowIPAddr *ip_addr)
{
  if (ip_addr->family == FLOW_IP_ADDR_IPV4)
  {
    guint32 addr;

    addr = GUINT32_FROM_BE (*((guint32 *) ip_addr->addr));

    if ((addr & 0xf0000000) == 0xe0000000)
      return TRUE;
  }
  else
  {
    if (ip_addr->addr [0] == 0xff)
      return TRUE;
  }

  return FALSE;
}

gboolean
flow_ip_addr_is_broadcast (FlowIPAddr *ip_addr)
{
  if (ip_addr->family == FLOW_IP_ADDR_IPV4)
  {
    guint32 addr;

    addr = GUINT32_FROM_BE (*((guint32 *) ip_addr->addr));

    if (addr == 0xffffffff)
      return TRUE;
  }

  /* Broadcast is not defined in IPv6; instead, multicast is used */

  return FALSE;
}

gboolean
flow_ip_addr_is_reserved (FlowIPAddr *ip_addr)
{
  if (ip_addr->family == FLOW_IP_ADDR_IPV4)
  {
    guint32 addr;

    addr = GUINT32_FROM_BE (*((guint32 *) ip_addr->addr));

    if ((addr & 0xffff0000) == 0)
      return TRUE;

    if ((addr & 0xf8000000) == 0xf0000000)
      return TRUE;
  }
  else
  {
    guint8 b, t;

    /* See http://www.iana.org/assignments/ipv6-address-space */

    b = ip_addr->addr [0];

    /* Exception for the loopback address, ::1 */
    if (b == 0x00 && ip_addr->addr [15] != 0x01)
      return TRUE;

    if (b == 0x01)
      return TRUE;

    if ((b & 0xfe) == 0x02)
      return TRUE;

    if ((b & 0xfc) == 0x04)
      return TRUE;

    if ((b & 0xf8) == 0x08)
      return TRUE;

    if ((b & 0xf0) == 0x10)
      return TRUE;

    /* 2000::/3 is the global unicast block (the Internets) */

    t = b & 0xe0;

    if (t == 0x40 || t == 0x60 || t == 0x80 || t == 0xa0 || t == 0xc0)
      return TRUE;

    if ((b & 0xf0) == 0xe0)
      return TRUE;

    if ((b & 0xf8) == 0xf0)
      return TRUE;

    if ((b & 0xfc) == 0xf8)
      return TRUE;

    /* fc00::/7 is the unique local unicast block (site-local prefix) */

    if (b == 0xfe && (ip_addr->addr [1] & 0x80) == 0x00)
      return TRUE;

    /* fe80::/10 is the link local unicast block (link-local prefix) */

    if (b == 0xfe && (ip_addr->addr [1] & 0xc0) == 0xc0)
      return TRUE;

    /* ff00::/8 is the multicast block */
  }

  return FALSE;
}

gboolean
flow_ip_addr_is_private (FlowIPAddr *ip_addr)
{
  if (ip_addr->family == FLOW_IP_ADDR_IPV4)
  {
    guint32 addr;

    addr = GUINT32_FROM_BE (*((guint32 *) ip_addr->addr));

    if ((addr & 0xff000000) == (10 << 24))  /* 10.0.0.0/8 */
      return TRUE;

    if ((addr & 0xfff00000) == 0xac100000)  /* 172.16.0.0/12 */
      return TRUE;

    if ((addr & 0xffff0000) == 0xc0a80000)  /* 192.168.0.0/16 */
      return TRUE;
  }
  else
  {
    /* Site-local prefix */

    if ((ip_addr->addr [0] & 0xfe) == 0xfc)
      return TRUE;

    /* Link-local prefix */

    if (ip_addr->addr [0] == 0xfe && (ip_addr->addr [1] & 0xc0) == 0x80)
      return TRUE;
  }

  return FALSE;
}

gboolean
flow_ip_addr_is_internet (FlowIPAddr *ip_addr)
{
  if (!flow_ip_addr_is_private   (ip_addr) &&
      !flow_ip_addr_is_reserved  (ip_addr) &&
      !flow_ip_addr_is_loopback  (ip_addr) &&
      !flow_ip_addr_is_multicast (ip_addr) &&
      !flow_ip_addr_is_broadcast (ip_addr))
  {
    return TRUE;
  }

  return FALSE;
}
