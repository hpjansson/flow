/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-util.h - General utilities.
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

#ifndef _FLOW_UTIL_H
#define _FLOW_UTIL_H

#include <glib.h>
#include <flow-shunt.h>
#include <flow-simplex-element.h>
#include <flow-duplex-element.h>
#include <flow-packet.h>

G_BEGIN_DECLS

typedef void (*FlowNotifyFunc) (gpointer user_data);

void         flow_connect_simplex__simplex         (FlowSimplexElement *output_simplex,
                                                    FlowSimplexElement *input_simplex);
void         flow_connect_duplex__duplex           (FlowDuplexElement *downstream_duplex,
                                                    FlowDuplexElement *upstream_duplex);
void         flow_connect_simplex_simplex__duplex  (FlowSimplexElement *downstream_simplex_output,
                                                    FlowSimplexElement *downstream_simplex_input,
                                                    FlowDuplexElement  *upstream_duplex);
void         flow_connect_duplex__simplex_simplex  (FlowDuplexElement  *downstream_duplex,
                                                    FlowSimplexElement *upstream_simplex_output,
                                                    FlowSimplexElement *upstream_simplex_input);

void         flow_insert_Isimplex__simplex         (FlowSimplexElement *output_simplex_inserted,
                                                    FlowSimplexElement *input_simplex);
void         flow_insert_simplex__Isimplex         (FlowSimplexElement *output_simplex,
                                                    FlowSimplexElement *input_simplex_inserted);
void         flow_insert_Iduplex__duplex           (FlowDuplexElement *downstream_duplex_inserted,
                                                    FlowDuplexElement *upstream_duplex);
void         flow_insert_duplex__Iduplex           (FlowDuplexElement *downstream_duplex,
                                                    FlowDuplexElement *upstream_duplex_inserted);
void         flow_insert_Isimplex_Isimplex__duplex (FlowSimplexElement *downstream_simplex_output_inserted,
                                                    FlowSimplexElement *downstream_simplex_input_inserted,
                                                    FlowDuplexElement *upstream_duplex);
void         flow_insert_duplex__Isimplex_Isimplex (FlowDuplexElement *downstream_duplex,
                                                    FlowSimplexElement *upstream_simplex_output_inserted,
                                                    FlowSimplexElement *upstream_simplex_input_inserted);
void         flow_insert_Iduplex__simplex_simplex  (FlowDuplexElement *downstream_duplex_inserted,
                                                    FlowSimplexElement *upstream_simplex_output,
                                                    FlowSimplexElement *upstream_simplex_input);
void         flow_insert_simplex_simplex__Iduplex  (FlowSimplexElement *downstream_simplex_output,
                                                    FlowSimplexElement *downstream_simplex_input,
                                                    FlowDuplexElement *upstream_duplex_inserted);

void         flow_disconnect_element               (FlowElement *element);

void         flow_replace_element                  (FlowElement *element, FlowElement *replacement);

FlowPacket  *flow_create_simple_event_packet       (const gchar *domain, gint code);
gboolean     flow_handle_universal_events          (FlowElement *element, FlowPacket *packet);
gpointer     flow_read_object_from_shunt           (FlowShunt *shunt);

gint         flow_g_ptr_array_find                 (GPtrArray *array, gpointer data);
gboolean     flow_g_ptr_array_remove_sparse        (GPtrArray *array, gpointer data);
void         flow_g_ptr_array_add_sparse           (GPtrArray *array, gpointer data);
guint        flow_g_ptr_array_compress             (GPtrArray *array);

gchar       *flow_strerror                         (gint errnum);

G_END_DECLS

#endif  /* _FLOW_UTIL_H */
