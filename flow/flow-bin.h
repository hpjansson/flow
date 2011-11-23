/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-bin.h - A container for stream elements.
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

#ifndef _FLOW_BIN_H
#define _FLOW_BIN_H

#include <glib-object.h>
#include <flow/flow-element.h>

G_BEGIN_DECLS

#define FLOW_TYPE_BIN            (flow_bin_get_type ())
#define FLOW_BIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_BIN, FlowBin))
#define FLOW_BIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_BIN, FlowBinClass))
#define FLOW_IS_BIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_BIN))
#define FLOW_IS_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_BIN))
#define FLOW_BIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_BIN, FlowBinClass))
GType   flow_bin_get_type        (void) G_GNUC_CONST;

typedef struct _FlowBin        FlowBin;
typedef struct _FlowBinPrivate FlowBinPrivate;
typedef struct _FlowBinClass   FlowBinClass;

struct _FlowBin
{
  GObject         parent;

  /*< private >*/

  FlowBinPrivate *priv;
};

struct _FlowBinClass
{
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowBin     *flow_bin_new                          (void);

void         flow_bin_add_element                  (FlowBin *bin, FlowElement *element, const gchar *element_name);
void         flow_bin_remove_element               (FlowBin *bin, FlowElement *element);

gboolean     flow_bin_have_element                 (FlowBin *bin, FlowElement *element);
FlowElement *flow_bin_get_element                  (FlowBin *bin, const gchar *element_name);
const gchar *flow_bin_get_element_name             (FlowBin *bin, FlowElement *element);

GList       *flow_bin_list_elements                (FlowBin *bin);

GList       *flow_bin_list_unconnected_input_pads  (FlowBin *bin);
GList       *flow_bin_list_unconnected_output_pads (FlowBin *bin);

G_END_DECLS

#endif  /* _FLOW_BIN_H */
