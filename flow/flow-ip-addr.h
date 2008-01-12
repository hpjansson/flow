/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-addr.h - An Internet address.
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

#ifndef _FLOW_IP_ADDR_H
#define _FLOW_IP_ADDR_H

#include <glib-object.h>

#ifndef G_PLATFORM_WIN32
# include <arpa/inet.h>
#else
# include <winsock2.h>
#endif

G_BEGIN_DECLS

#define FLOW_TYPE_IP_ADDR            (flow_ip_addr_get_type ())
#define FLOW_IP_ADDR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_IP_ADDR, FlowIPAddr))
#define FLOW_IP_ADDR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_IP_ADDR, FlowIPAddrClass))
#define FLOW_IS_IP_ADDR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_IP_ADDR))
#define FLOW_IS_IP_ADDR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_IP_ADDR))
#define FLOW_IP_ADDR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_IP_ADDR, FlowIPAddrClass))
GType   flow_ip_addr_get_type        (void) G_GNUC_CONST;

typedef enum
{
  FLOW_IP_ADDR_INVALID,
  FLOW_IP_ADDR_ANY_FAMILY = FLOW_IP_ADDR_INVALID,
  FLOW_IP_ADDR_IPV4,
  FLOW_IP_ADDR_IPV6
}
FlowIPAddrFamily;

typedef struct _FlowIPAddr      FlowIPAddr;
typedef struct _FlowIPAddrClass FlowIPAddrClass;

struct _FlowIPAddr
{
  GObject          object;

  /* --- Protected --- */

  FlowIPAddrFamily family;
  guint8           addr [16];
};

struct _FlowIPAddrClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowIPAddr       *flow_ip_addr_new                     (void);

FlowIPAddrFamily  flow_ip_addr_get_family              (FlowIPAddr *ip_addr);
gboolean          flow_ip_addr_is_valid                (FlowIPAddr *ip_addr);

gchar            *flow_ip_addr_get_string              (FlowIPAddr *ip_addr);
gboolean          flow_ip_addr_set_string              (FlowIPAddr *ip_addr, const gchar *string);

gboolean          flow_ip_addr_get_raw                 (FlowIPAddr *ip_addr, guint8 *dest, gint *len);
gboolean          flow_ip_addr_set_raw                 (FlowIPAddr *ip_addr, const guint8 *src, gint len);

gboolean          flow_ip_addr_is_loopback             (FlowIPAddr *ip_addr);
gboolean          flow_ip_addr_is_multicast            (FlowIPAddr *ip_addr);
gboolean          flow_ip_addr_is_broadcast            (FlowIPAddr *ip_addr);
gboolean          flow_ip_addr_is_reserved             (FlowIPAddr *ip_addr);
gboolean          flow_ip_addr_is_private              (FlowIPAddr *ip_addr);
gboolean          flow_ip_addr_is_internet             (FlowIPAddr *ip_addr);

G_END_DECLS

#endif /* _FLOW_IP_ADDR_H */
