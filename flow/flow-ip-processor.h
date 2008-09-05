/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-ip-processor.h - IP service processing element.
 *
 * Copyright (C) 2007 Hans Petter Jansson
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

#ifndef _FLOW_IP_PROCESSOR_H
#define _FLOW_IP_PROCESSOR_H

#include <glib-object.h>
#include <flow-simplex-element.h>

G_BEGIN_DECLS

#define FLOW_TYPE_IP_PROCESSOR            (flow_ip_processor_get_type ())
#define FLOW_IP_PROCESSOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_IP_PROCESSOR, FlowIPProcessor))
#define FLOW_IP_PROCESSOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_IP_PROCESSOR, FlowIPProcessorClass))
#define FLOW_IS_IP_PROCESSOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_IP_PROCESSOR))
#define FLOW_IS_IP_PROCESSOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_IP_PROCESSOR))
#define FLOW_IP_PROCESSOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_IP_PROCESSOR, FlowIPProcessorClass))
GType   flow_ip_processor_get_type        (void) G_GNUC_CONST;

typedef struct _FlowIPProcessor      FlowIPProcessor;
typedef struct _FlowIPProcessorClass FlowIPProcessorClass;

struct _FlowIPProcessor
{
  FlowSimplexElement parent;

  gpointer           priv;
};

struct _FlowIPProcessorClass
{
  FlowSimplexElementClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowIPProcessor *flow_ip_processor_new                   (void);

gboolean         flow_ip_processor_get_resolve_to_addrs  (FlowIPProcessor *ip_processor);
void             flow_ip_processor_set_resolve_to_addrs  (FlowIPProcessor *ip_processor, gboolean resolve_to_addrs);

gboolean         flow_ip_processor_get_resolve_to_names  (FlowIPProcessor *ip_processor);
void             flow_ip_processor_set_resolve_to_names  (FlowIPProcessor *ip_processor, gboolean resolve_to_names);

gboolean         flow_ip_processor_get_require_addrs     (FlowIPProcessor *ip_processor);
void             flow_ip_processor_set_require_addrs     (FlowIPProcessor *ip_processor, gboolean require_addrs);

gboolean         flow_ip_processor_get_require_names     (FlowIPProcessor *ip_processor);
void             flow_ip_processor_set_require_names     (FlowIPProcessor *ip_processor, gboolean require_names);

gboolean         flow_ip_processor_get_drop_invalid_data (FlowIPProcessor *ip_processor);
void             flow_ip_processor_set_drop_invalid_data (FlowIPProcessor *ip_processor, gboolean drop_invalid_data);

gboolean         flow_ip_processor_get_drop_invalid_objs (FlowIPProcessor *ip_processor);
void             flow_ip_processor_set_drop_invalid_objs (FlowIPProcessor *ip_processor, gboolean drop_invalid_objs);

gboolean         flow_ip_processor_get_valid_state       (FlowIPProcessor *ip_processor);
void             flow_ip_processor_set_valid_state       (FlowIPProcessor *ip_processor, gboolean valid_state);

G_END_DECLS

#endif  /* _FLOW_IP_PROCESSOR_H */
