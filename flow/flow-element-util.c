/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-element-util.c - Utilities for element management.
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

#include "config.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "flow-element-util.h"

/**
 * flow_connect_simplex__simplex:
 * @output_simplex: First #FlowSimplexElement element or %NULL.
 * @input_simplex:  Second #FlowSimplexElement element or %NULL.
 *
 * Connects @output_simplex and @input_simplex so that packets flow from
 * the former to the latter.
 *
 * If %NULL is passed for one of the elements, any element present in
 * its "slot" will be disconnected.
 **/
void
flow_connect_simplex__simplex (FlowSimplexElement *output_simplex,
                               FlowSimplexElement *input_simplex)
{
  if (output_simplex)
  {
    if (input_simplex)
    {
      flow_pad_connect (FLOW_PAD (flow_simplex_element_get_output_pad (output_simplex)),
                        FLOW_PAD (flow_simplex_element_get_input_pad  (input_simplex)));
    }
    else
    {
      flow_pad_disconnect (FLOW_PAD (flow_simplex_element_get_output_pad (output_simplex)));
    }
  }
  else if (input_simplex)
  {
    flow_pad_disconnect (FLOW_PAD (flow_simplex_element_get_input_pad (input_simplex)));
  }
}

/**
 * flow_connect_duplex__duplex:
 * @downstream_duplex: First #FlowDuplexElement element or %NULL.
 * @upstream_duplex:   Second #FlowDuplexElement element or %NULL.
 *
 * Connects @downstream_duplex and @upstream_duplex. Packets will flow
 * both ways, with the former element termed "downstream" and the latter
 * "upstream". More specifically, @downstream_duplex's upstream pads will
 * be connected to @upstream_duplex's downstream pads.
 *
 * If %NULL is passed for one of the elements, any element present in
 * its "slot" will be disconnected.
 **/
void
flow_connect_duplex__duplex (FlowDuplexElement *downstream_duplex,
                             FlowDuplexElement *upstream_duplex)
{
  FlowPad *downstream_duplex_upstream_pads [2];
  FlowPad *upstream_duplex_downstream_pads [2] = { NULL, NULL };

  if (upstream_duplex)
  {
    upstream_duplex_downstream_pads [0] = FLOW_PAD (flow_duplex_element_get_upstream_input_pad (upstream_duplex));
    upstream_duplex_downstream_pads [1] = FLOW_PAD (flow_duplex_element_get_upstream_output_pad (upstream_duplex));
  }

  if (downstream_duplex)
  {
    downstream_duplex_upstream_pads [0] = FLOW_PAD (flow_duplex_element_get_upstream_input_pad (downstream_duplex));
    downstream_duplex_upstream_pads [1] = FLOW_PAD (flow_duplex_element_get_upstream_output_pad (downstream_duplex));

    if (upstream_duplex)
    {
      flow_pad_connect (downstream_duplex_upstream_pads [0],
                        upstream_duplex_downstream_pads [1]);
      flow_pad_connect (downstream_duplex_upstream_pads [1],
                        upstream_duplex_downstream_pads [0]);
    }
    else
    {
      flow_pad_disconnect (downstream_duplex_upstream_pads [0]);
      flow_pad_disconnect (downstream_duplex_upstream_pads [1]);
    }
  }
  else if (upstream_duplex)
  {
    flow_pad_disconnect (upstream_duplex_downstream_pads [0]);
    flow_pad_disconnect (upstream_duplex_downstream_pads [1]);
  }
}

/**
 * flow_connect_simplex_simplex__duplex:
 * @downstream_simplex_output: First #FlowSimplexElement element or %NULL.
 * @downstream_simplex_input:  Second #FlowSimplexElement element or %NULL.
 * @upstream_duplex:           A #FlowDuplexElement element or %NULL.
 *
 * This function is similar to #flow_connect_duplex__duplex, but instead of
 * a duplex element, two simplex elements are used for the downstream "slot".
 * Specifically, @downstream_simplex_output's output pad will be connected
 * to @upstream_duplex's downstream input pad, and @downstream_simplex_input's
 * input pad will be connected to @upstream_duplex's downstream output pad.
 *
 * If %NULL is passed for one of the elements, any element present in
 * its "slot" will be disconnected. You can pass %NULL for multiple elements.
 **/
void
flow_connect_simplex_simplex__duplex (FlowSimplexElement *downstream_simplex_output,
                                      FlowSimplexElement *downstream_simplex_input,
                                      FlowDuplexElement  *upstream_duplex)
{
  FlowPad *duplex_downstream_pads [2];
  FlowPad *simplex_pad;

  if (upstream_duplex)
  {
    duplex_downstream_pads [0] = FLOW_PAD (flow_duplex_element_get_downstream_input_pad (upstream_duplex));
    duplex_downstream_pads [1] = FLOW_PAD (flow_duplex_element_get_downstream_output_pad (upstream_duplex));

    if (downstream_simplex_output)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_output_pad (downstream_simplex_output));
      flow_pad_connect (simplex_pad, duplex_downstream_pads [0]);
    }
    else
    {
      flow_pad_disconnect (duplex_downstream_pads [0]);
    }

    if (downstream_simplex_input)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_input_pad (downstream_simplex_input));
      flow_pad_connect (simplex_pad, duplex_downstream_pads [1]);
    }
    else
    {
      flow_pad_disconnect (duplex_downstream_pads [1]);
    }
  }
  else
  {
    if (downstream_simplex_output)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_output_pad (downstream_simplex_output));
      flow_pad_disconnect (simplex_pad);
    }

    if (downstream_simplex_input)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_input_pad (downstream_simplex_input));
      flow_pad_disconnect (simplex_pad);
    }
  }
}

/**
 * flow_connect_duplex__simplex_simplex:
 * @downstream_duplex:       A #FlowDuplexElement element or %NULL.
 * @upstream_simplex_output: First #FlowSimplexElement element or %NULL.
 * @upstream_simplex_input:  Second #FlowSimplexElement element or %NULL.
 *
 * This function is similar to #flow_connect_duplex__duplex, but instead of
 * a duplex element, two simplex elements are used for the upstream "slot".
 * Specifically, @downstream_duplex' upstream output pad will be connected to
 * @upstream_simplex_input's input pad, and @downstream_duplex' upstream
 * input pad will be connected to @upstream_simplex_output's output pad.
 *
 * If %NULL is passed for one of the elements, any element present in
 * its "slot" will be disconnected. You can pass %NULL for multiple elements.
 **/
void
flow_connect_duplex__simplex_simplex (FlowDuplexElement  *downstream_duplex,
                                      FlowSimplexElement *upstream_simplex_output,
                                      FlowSimplexElement *upstream_simplex_input)
{
  FlowPad *duplex_upstream_pads [2];
  FlowPad *simplex_pad;

  if (downstream_duplex)
  {
    duplex_upstream_pads [0] = FLOW_PAD (flow_duplex_element_get_upstream_input_pad (downstream_duplex));
    duplex_upstream_pads [1] = FLOW_PAD (flow_duplex_element_get_upstream_output_pad (downstream_duplex));

    if (upstream_simplex_output)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_output_pad (upstream_simplex_output));
      flow_pad_connect (duplex_upstream_pads [0], simplex_pad);
    }
    else
    {
      flow_pad_disconnect (duplex_upstream_pads [0]);
    }

    if (upstream_simplex_input)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_input_pad (upstream_simplex_input));
      flow_pad_connect (duplex_upstream_pads [1], simplex_pad);
    }
    else
    {
      flow_pad_disconnect (duplex_upstream_pads [1]);
    }
  }
  else
  {
    if (upstream_simplex_output)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_output_pad (upstream_simplex_output));
      flow_pad_disconnect (simplex_pad);
    }

    if (upstream_simplex_input)
    {
      simplex_pad = FLOW_PAD (flow_simplex_element_get_input_pad (upstream_simplex_input));
      flow_pad_disconnect (simplex_pad);
    }
  }
}

/**
 * flow_insert_Isimplex__simplex:
 * @output_simplex_inserted: A #FlowSimplexElement to insert.
 * @input_simplex:           A #FlowSimplexElement.
 *
 * Inserts @output_simplex_inserted before @input_simplex in an
 * established pipeline, so that packets will flow from @output_simplex_inserted
 * to @input_simplex. If a pad was previously connected to @input_simplex'
 * input pad, it will be connected to @output_simplex_inserted's input pad
 * after the operation. Thus, @output_simplex_inserted is "inserted" between
 * @input_simplex and any preceding element.
 **/
void
flow_insert_Isimplex__simplex (FlowSimplexElement *output_simplex_inserted,
                               FlowSimplexElement *input_simplex)
{
  FlowPad *pad_1;
  FlowPad *pad_2;

  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (output_simplex_inserted));
  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (input_simplex));

  pad_1 = FLOW_PAD (flow_simplex_element_get_input_pad (input_simplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_simplex_element_get_input_pad (output_simplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  flow_connect_simplex__simplex (output_simplex_inserted, input_simplex);
}

/**
 * flow_insert_simplex__Isimplex:
 * @output_simplex:         A #FlowSimplexElement.
 * @input_simplex_inserted: A #FlowSimplexElement to insert.
 *
 * Inserts @input_simplex_inserted after @output_simplex in an
 * established pipeline, so that packets will flow from @output_simplex
 * to @input_simplex_inserted. If a pad was previously connected to @output_simplex'
 * output pad, it will be connected to @input_simplex_inserted's output pad
 * after the operation. Thus, @input_simplex_inserted is "inserted" between
 * @output_simplex and any subsequent element.
 **/
void
flow_insert_simplex__Isimplex (FlowSimplexElement *output_simplex,
                               FlowSimplexElement *input_simplex_inserted)
{
  FlowPad *pad_1;
  FlowPad *pad_2;

  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (output_simplex));
  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (input_simplex_inserted));

  pad_1 = FLOW_PAD (flow_simplex_element_get_output_pad (output_simplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_simplex_element_get_output_pad (input_simplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  flow_connect_simplex__simplex (output_simplex, input_simplex_inserted);
}

/**
 * flow_insert_Iduplex__duplex:
 * @downstream_duplex_inserted: A #FlowDuplexElement to insert.
 * @upstream_duplex:            A #FlowDuplexElement.
 *
 * Inserts @downstream_duplex_inserted before @upstream_duplex in an
 * established pipeline, so that @downstream_duplex_inserted will be
 * downstream from @upstream_duplex. Pads previously connected
 * to @upstream_duplex' downstream pads will be connected to
 * @downstream_duplex_inserted's downstream pads after the operation.
 * Thus, @downstream_duplex_inserted is "inserted" between
 * @upstream_duplex and any preceding element(s).
 **/
void
flow_insert_Iduplex__duplex (FlowDuplexElement *downstream_duplex_inserted,
                             FlowDuplexElement *upstream_duplex)
{
  FlowPad *pad_1;
  FlowPad *pad_2;

  g_return_if_fail (FLOW_IS_DUPLEX_ELEMENT (downstream_duplex_inserted));
  g_return_if_fail (FLOW_IS_DUPLEX_ELEMENT (upstream_duplex));

  /* */

  pad_1 = FLOW_PAD (flow_duplex_element_get_downstream_output_pad (upstream_duplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_duplex_element_get_downstream_output_pad (downstream_duplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* */

  pad_1 = FLOW_PAD (flow_duplex_element_get_downstream_input_pad (upstream_duplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_duplex_element_get_downstream_input_pad (downstream_duplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* Connect */

  flow_connect_duplex__duplex (downstream_duplex_inserted, upstream_duplex);
}

/**
 * flow_insert_duplex__Iduplex:
 * @downstream_duplex:        A #FlowDuplexElement.
 * @upstream_duplex_inserted: A #FlowDuplexElement to insert.
 *
 * Inserts @upstream_duplex_inserted after @downstream_duplex in an
 * established pipeline, so that @upstream_duplex_inserted will be
 * upstream from @downstream_duplex. Pads previously connected
 * to @downstream_duplex' upstream pads will be connected to
 * @upstream_duplex_inserted's upstream pads after the operation.
 * Thus, @upstream_duplex_inserted is "inserted" between
 * @downstream_duplex and any subsequent element(s).
 **/
void
flow_insert_duplex__Iduplex (FlowDuplexElement *downstream_duplex,
                             FlowDuplexElement *upstream_duplex_inserted)
{
  FlowPad *pad_1;
  FlowPad *pad_2;

  g_return_if_fail (FLOW_IS_DUPLEX_ELEMENT (downstream_duplex));
  g_return_if_fail (FLOW_IS_DUPLEX_ELEMENT (upstream_duplex_inserted));

  /* */

  pad_1 = FLOW_PAD (flow_duplex_element_get_upstream_output_pad (downstream_duplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_duplex_element_get_upstream_output_pad (upstream_duplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* */

  pad_1 = FLOW_PAD (flow_duplex_element_get_upstream_input_pad (downstream_duplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_duplex_element_get_upstream_input_pad (upstream_duplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* Connect */

  flow_connect_duplex__duplex (downstream_duplex, upstream_duplex_inserted);
}

/**
 * flow_insert_Isimplex_Isimplex__duplex:
 * @downstream_simplex_output_inserted: A #FlowSimplexElement to insert.
 * @downstream_simplex_input_inserted:  A #FlowSimplexElement to insert.
 * @upstream_duplex:                    A #FlowDuplexElement.
 *
 * Like #flow_insert_Iduplex__duplex, except a pair of #FlowSimplexElement
 * elements is inserted instead of a #FlowDuplexElement.
 *
 * The simplex elements will be connected to the downstream pads of
 * @upstream_duplex. Packets will flow from @downstream_simplex_output_inserted
 * to @upstream_duplex, and from @upstream_duplex to
 * @downstream_simplex_input_inserted. 
 **/
void
flow_insert_Isimplex_Isimplex__duplex (FlowSimplexElement *downstream_simplex_output_inserted,
                                       FlowSimplexElement *downstream_simplex_input_inserted,
                                       FlowDuplexElement *upstream_duplex)
{
  FlowPad *pad_1;
  FlowPad *pad_2;

  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (downstream_simplex_output_inserted));
  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (downstream_simplex_input_inserted));
  g_return_if_fail (FLOW_IS_DUPLEX_ELEMENT (upstream_duplex));

  /* */

  pad_1 = FLOW_PAD (flow_duplex_element_get_downstream_output_pad (upstream_duplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_simplex_element_get_output_pad (downstream_simplex_input_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* */

  pad_1 = FLOW_PAD (flow_duplex_element_get_downstream_input_pad (upstream_duplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_simplex_element_get_input_pad (downstream_simplex_output_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* Connect */

  flow_connect_simplex_simplex__duplex (downstream_simplex_output_inserted,
                                        downstream_simplex_input_inserted,
                                        upstream_duplex);
}

/**
 * flow_insert_duplex__Isimplex_Isimplex:
 * @downstream_duplex:                A #FlowDuplexElement.
 * @upstream_simplex_output_inserted: A #FlowSimplexElement to insert.
 * @upstream_simplex_input_inserted:  A #FlowSimplexElement to insert.
 *
 * Like #flow_insert_duplex__Iduplex, except a pair of #FlowSimplexElement
 * elements is inserted instead of a #FlowDuplexElement.
 *
 * The simplex elements will be connected to the upstream pads of
 * @downstream_duplex. Packets will flow from @downstream_duplex to
 * @upstream_simplex_input_inserted, and from @upstream_simplex_output_inserted
 * to @downstream_duplex.
 **/
void
flow_insert_duplex__Isimplex_Isimplex (FlowDuplexElement *downstream_duplex,
                                       FlowSimplexElement *upstream_simplex_output_inserted,
                                       FlowSimplexElement *upstream_simplex_input_inserted)
{
  FlowPad *pad_1;
  FlowPad *pad_2;

  g_return_if_fail (FLOW_IS_DUPLEX_ELEMENT (downstream_duplex));
  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (upstream_simplex_output_inserted));
  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (upstream_simplex_input_inserted));

  /* */

  pad_1 = FLOW_PAD (flow_duplex_element_get_upstream_output_pad (downstream_duplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_simplex_element_get_output_pad (upstream_simplex_input_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* */

  pad_1 = FLOW_PAD (flow_duplex_element_get_upstream_input_pad (downstream_duplex));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_simplex_element_get_input_pad (upstream_simplex_output_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* Connect */

  flow_connect_duplex__simplex_simplex (downstream_duplex,
                                        upstream_simplex_output_inserted,
                                        upstream_simplex_input_inserted);
}

/**
 * flow_insert_Iduplex__simplex_simplex:
 * @downstream_duplex_inserted: A #FlowDuplexElement to insert.
 * @upstream_simplex_output:    A #FlowSimplexElement.
 * @upstream_simplex_input:     A #FlowSimplexElement.
 *
 * Like #flow_insert_Iduplex__duplex, except the #FlowDuplexElement is
 * inserted before a pair of #FlowSimplexElement elements instead of
 * another #FlowDuplexElement.
 *
 * The simplex elements will be connected to the upstream pads of
 * @downstream_duplex_inserted. Packets will flow from @downstream_duplex_inserted
 * to @upstream_simplex_input, and from @upstream_simplex_output to
 * @downstream_duplex_inserted.
 **/
void
flow_insert_Iduplex__simplex_simplex (FlowDuplexElement *downstream_duplex_inserted,
                                      FlowSimplexElement *upstream_simplex_output,
                                      FlowSimplexElement *upstream_simplex_input)
{
  FlowPad *pad_1;
  FlowPad *pad_2;

  g_return_if_fail (FLOW_IS_DUPLEX_ELEMENT (downstream_duplex_inserted));
  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (upstream_simplex_output));
  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (upstream_simplex_input));

  /* */

  pad_1 = FLOW_PAD (flow_simplex_element_get_output_pad (upstream_simplex_output));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_duplex_element_get_downstream_output_pad (downstream_duplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* */

  pad_1 = FLOW_PAD (flow_simplex_element_get_input_pad (upstream_simplex_input));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_duplex_element_get_downstream_input_pad (downstream_duplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* Connect */

  flow_connect_duplex__simplex_simplex (downstream_duplex_inserted,
                                        upstream_simplex_output,
                                        upstream_simplex_input);
}

/**
 * flow_insert_simplex_simplex__Iduplex:
 * @downstream_simplex_output: A #FlowSimplexElement.
 * @downstream_simplex_input:  A #FlowSimplexElement.
 * @upstream_duplex_inserted:  A #FlowDuplexElement to insert.
 *
 * Like #flow_insert_duplex__Iduplex, except the #FlowDuplexElement is
 * inserted after a pair of #FlowSimplexElement elements instead of
 * another #FlowDuplexElement.
 *
 * The simplex elements will be connected to the downstream pads of
 * @upstream_duplex_inserted. Packets will flow from
 * @downstream_simplex_output to @upstream_duplex_inserted, and from
 * @upstream_duplex_inserted to @downstream_simplex_input. 
 **/
void
flow_insert_simplex_simplex__Iduplex (FlowSimplexElement *downstream_simplex_output,
                                      FlowSimplexElement *downstream_simplex_input,
                                      FlowDuplexElement *upstream_duplex_inserted)
{
  FlowPad *pad_1;
  FlowPad *pad_2;

  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (downstream_simplex_output));
  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (downstream_simplex_input));
  g_return_if_fail (FLOW_IS_DUPLEX_ELEMENT (upstream_duplex_inserted));

  /* */

  pad_1 = FLOW_PAD (flow_simplex_element_get_output_pad (downstream_simplex_input));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_duplex_element_get_upstream_output_pad (upstream_duplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* */

  pad_1 = FLOW_PAD (flow_simplex_element_get_input_pad (downstream_simplex_output));
  pad_1 = flow_pad_get_connected_pad (pad_1);

  pad_2 = FLOW_PAD (flow_duplex_element_get_upstream_input_pad (upstream_duplex_inserted));

  if (pad_1)
    flow_pad_connect (pad_1, pad_2);
  else
    flow_pad_disconnect (pad_2);

  /* Connect */

  flow_connect_simplex_simplex__duplex (downstream_simplex_output,
                                        downstream_simplex_input,
                                        upstream_duplex_inserted);
}

/**
 * flow_disconnect_element:
 * @element: A #FlowElement.
 *
 * Disconnects all pads belonging to @element. If @element belonged
 * to a pipeline, this will leave a "gap" in it where neighbors were
 * previously connected to @element.
 **/
void
flow_disconnect_element (FlowElement *element)
{
  GPtrArray *pads;
  guint      i;

  g_return_if_fail (FLOW_IS_ELEMENT (element));

  pads = flow_element_get_input_pads (element);

  for (i = 0; i < pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (pads, i);
    flow_pad_disconnect (pad);
  }

  pads = flow_element_get_output_pads (element);

  for (i = 0; i < pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (pads, i);
    flow_pad_disconnect (pad);
  }
}

/**
 * flow_extract_simplex_element:
 * @simplex_element: A #FlowSimplexElement.
 *
 * Like #flow_disconnect_element, disconnects all pads belonging to
 * @simplex_element. If @simplex_element belonged to a pipeline, the resulting "gap"
 * is closed by connecting @simplex_element's neighbors to each other.
 **/
void
flow_extract_simplex_element (FlowSimplexElement *simplex_element)
{
  FlowPad *pads [2];
  gint     i;

  g_return_if_fail (FLOW_IS_SIMPLEX_ELEMENT (simplex_element));

  pads [0] = FLOW_PAD (flow_simplex_element_get_input_pad (simplex_element));
  pads [1] = FLOW_PAD (flow_simplex_element_get_output_pad (simplex_element));

  for (i = 0; i < 2; i++)
    pads [i] = flow_pad_get_connected_pad (pads [i]);

  flow_disconnect_element (FLOW_ELEMENT (simplex_element));

  if (pads [0] && pads [1])
    flow_pad_connect (pads [0], pads [1]);
}

/**
 * flow_extract_duplex_element:
 * @duplex_element: A #FlowDuplexElement.
 *
 * Like #flow_disconnect_element, disconnects all pads belonging to
 * @duplex_element. If @duplex_element belonged to a pipeline, the resulting "gap"
 * is closed by connecting @duplex_element's neighbors to each other.
 **/
void
flow_extract_duplex_element (FlowDuplexElement *duplex_element)
{
  FlowPad *pads [4];
  gint     i;

  g_return_if_fail (FLOW_IS_DUPLEX_ELEMENT (duplex_element));

  pads [0] = FLOW_PAD (flow_duplex_element_get_upstream_input_pad (duplex_element));
  pads [1] = FLOW_PAD (flow_duplex_element_get_upstream_output_pad (duplex_element));
  pads [2] = FLOW_PAD (flow_duplex_element_get_downstream_input_pad (duplex_element));
  pads [3] = FLOW_PAD (flow_duplex_element_get_downstream_output_pad (duplex_element));

  for (i = 0; i < 4; i++)
    pads [i] = flow_pad_get_connected_pad (pads [i]);

  flow_disconnect_element (FLOW_ELEMENT (duplex_element));

  if (pads [0] && pads [3])
    flow_pad_connect (pads [0], pads [3]);

  if (pads [1] && pads [2])
    flow_pad_connect (pads [1], pads [2]);
}

/**
 * flow_replace_element:
 * @original:    A #FlowElement to be replaced.
 * @replacement: A #FlowElement to replace the original with.
 *
 * If @original belongs to a pipeline of connected elements, disconnects
 * it from its neighbors and substitutes @replacement, which takes over its
 * place in the pipeline.
 *
 * @replacement must have the same number and type of pads as @original.
 **/
void
flow_replace_element (FlowElement *original, FlowElement *replacement)
{
  GPtrArray *original_input_pads;
  GPtrArray *original_output_pads;
  GPtrArray *replacement_input_pads;
  GPtrArray *replacement_output_pads;
  guint      i;

  g_return_if_fail (FLOW_IS_ELEMENT (original));
  g_return_if_fail (FLOW_IS_ELEMENT (replacement));

  if (original == replacement)
    return;

  original_input_pads     = flow_element_get_input_pads (original);
  original_output_pads    = flow_element_get_output_pads (original);
  replacement_input_pads  = flow_element_get_input_pads (replacement);
  replacement_output_pads = flow_element_get_output_pads (replacement);

  /* Check that the number of input/output pads match up */

  if (original_input_pads->len != replacement_input_pads->len)
  {
    g_warning ("Replacement element has different number of input pads!");
    return;
  }

  if (original_output_pads->len != replacement_output_pads->len)
  {
    g_warning ("Replacement element has different number of output pads!");
    return;
  }

  /* Substitute replacement connections for original */

  for (i = 0; i < original_output_pads->len; i++)
  {
    FlowPad *original_pad    = g_ptr_array_index (original_output_pads, i);
    FlowPad *replacement_pad = g_ptr_array_index (replacement_output_pads, i);
    FlowPad *connected_pad;

    connected_pad = flow_pad_get_connected_pad (original_pad);

    if (connected_pad)
      flow_pad_connect (replacement_pad, connected_pad);
    else
      flow_pad_disconnect (replacement_pad);
  }

  for (i = 0; i < original_input_pads->len; i++)
  {
    FlowPad *original_pad    = g_ptr_array_index (original_input_pads, i);
    FlowPad *replacement_pad = g_ptr_array_index (replacement_input_pads, i);
    FlowPad *connected_pad;

    connected_pad = flow_pad_get_connected_pad (original_pad);

    if (connected_pad)
      flow_pad_connect (replacement_pad, connected_pad);
    else
      flow_pad_disconnect (replacement_pad);
  }
}

static void collect_pipeline_elements_recurse (FlowElement *element, GHashTable *visited);

static void
collect_pipeline_elements_from_pad_array (GPtrArray *pads, GHashTable *visited)
{
  gint i;

  for (i = 0; i < pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (pads, i);
    FlowPad *connected_pad = flow_pad_get_connected_pad (pad);
    FlowElement *connected_element = flow_pad_get_owner_element (connected_pad);

    if (!g_hash_table_lookup (visited, connected_element))
      collect_pipeline_elements_recurse (connected_element, visited);
  }
}

static void
collect_pipeline_elements_recurse (FlowElement *element, GHashTable *visited)
{
  g_hash_table_insert (visited, element, element);

  collect_pipeline_elements_from_pad_array (flow_element_get_input_pads (element), visited);
  collect_pipeline_elements_from_pad_array (flow_element_get_output_pads (element), visited);
}

typedef struct
{
  GFunc func;
  gpointer data;
}
ForeachPipelineElementData;

static void
foreach_pipeline_element (FlowElement *element, gpointer value, ForeachPipelineElementData *data)
{
  data->func (element, data->data);
}

void
flow_pipeline_foreach_element (FlowElement *element, GFunc func, gpointer data)
{
  GHashTable *visited;
  ForeachPipelineElementData foreach_data;

  visited = g_hash_table_new (g_direct_hash, g_direct_equal);

  collect_pipeline_elements_recurse (element, visited);

  foreach_data.func = func;
  foreach_data.data = data;
  g_hash_table_foreach (visited, (GHFunc) foreach_pipeline_element, &foreach_data);

  g_hash_table_destroy (visited);
}
