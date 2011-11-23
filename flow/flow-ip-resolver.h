/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-resolver.h - IP address resolver.
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

#ifndef _FLOW_IP_RESOLVER_H
#define _FLOW_IP_RESOLVER_H

#include <flow/flow-ip-addr.h>

typedef void (FlowIPLookupFunc) (GList *addr_list, GList *name_list, GError *error, gpointer data);

#define FLOW_TYPE_IP_RESOLVER            (flow_ip_resolver_get_type ())
#define FLOW_IP_RESOLVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_IP_RESOLVER, FlowIPResolver))
#define FLOW_IP_RESOLVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_IP_RESOLVER, FlowIPResolverClass))
#define FLOW_IS_IP_RESOLVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_IP_RESOLVER))
#define FLOW_IS_IP_RESOLVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_IP_RESOLVER))
#define FLOW_IP_RESOLVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_IP_RESOLVER, FlowIPResolverClass))
GType   flow_ip_resolver_get_type        (void) G_GNUC_CONST;

typedef struct _FlowIPResolver        FlowIPResolver;
typedef struct _FlowIPResolverPrivate FlowIPResolverPrivate;
typedef struct _FlowIPResolverClass   FlowIPResolverClass;

struct _FlowIPResolver
{
  GObject      object;

  /*< private >*/

  FlowIPResolverPrivate *priv;
};

struct _FlowIPResolverClass
{
  GObjectClass parent_class;

  /*< private >*/

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowIPResolver *flow_ip_resolver_new                (void);

FlowIPResolver *flow_ip_resolver_get_default        (void);

guint           flow_ip_resolver_resolve_name       (FlowIPResolver *resolver, const gchar *name,
                                                     FlowIPLookupFunc *func, gpointer data);
guint           flow_ip_resolver_resolve_ip_addr    (FlowIPResolver *resolver, FlowIPAddr *addr,
                                                     FlowIPLookupFunc *func, gpointer data);
void            flow_ip_resolver_cancel_resolution  (FlowIPResolver *resolver, guint lookup_id);

#endif /* _FLOW_IP_RESOLVER_H */
