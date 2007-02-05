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

void
flow_disconnect_element (FlowElement *element)
{
  GPtrArray *pads;
  guint      i;

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

void
flow_replace_element (FlowElement *original, FlowElement *replacement)
{
  GPtrArray *original_input_pads;
  GPtrArray *original_output_pads;
  GPtrArray *replacement_input_pads;
  GPtrArray *replacement_output_pads;
  guint      i;

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
