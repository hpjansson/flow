/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-bin.c - A unidirectional multi-element bin.
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
#include "flow-gobject-util.h"
#include "flow-bin.h"

/* --- FlowBin private data --- */

typedef struct
{
  GHashTable *string_to_element;
  GHashTable *element_to_string;
}
FlowBinPrivate;

/* --- FlowBin properties --- */

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_bin)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowBin definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_bin, FlowBin, G_TYPE_OBJECT, 0)

static void
flow_bin_type_init (GType type)
{
}

static void
flow_bin_class_init (FlowBinClass *klass)
{
  static const GType param_types [1] = { G_TYPE_OBJECT };

  /* Signals */

  g_signal_newv ("element-added",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 NULL,                                   /* Class closure */
                 NULL, NULL,                             /* Accumulator, accu data */
                 g_cclosure_marshal_VOID__OBJECT,        /* Marshaller */
                 G_TYPE_NONE,                            /* Return type */
                 1, (GType *) param_types);              /* Number of params, param types */

  g_signal_newv ("element-removed",
                 G_TYPE_FROM_CLASS (klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 NULL,                                   /* Class closure */
                 NULL, NULL,                             /* Accumulator, accu data */
                 g_cclosure_marshal_VOID__OBJECT,        /* Marshaller */
                 G_TYPE_NONE,                            /* Return type */
                 1, (GType *) param_types);              /* Number of params, param types */
}

static void
flow_bin_init (FlowBin *bin)
{
  FlowBinPrivate *priv = bin->priv;

  /* We can't g_object_unref() on removal, because we want to emit a signal
   * after removal, but before unref. */
  priv->element_to_string = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, NULL);
  priv->string_to_element = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
flow_bin_construct (FlowBin *bin)
{
}

static void
flow_bin_dispose (FlowBin *bin)
{
}

static void
flow_bin_finalize (FlowBin *bin)
{
  FlowBinPrivate *priv = bin->priv;

  g_hash_table_foreach (priv->element_to_string, (GHFunc) g_object_unref, NULL);

  g_hash_table_destroy (priv->element_to_string);
  g_hash_table_destroy (priv->string_to_element);
}

static gboolean
find_element_by_ptr (FlowBin *bin, FlowElement *element, const gchar **name_out)
{
  FlowBinPrivate *priv = bin->priv;

  return g_hash_table_lookup_extended (priv->element_to_string, element, NULL, (gpointer *) name_out);
}

static FlowElement *
find_element_by_name (FlowBin *bin, const gchar *name)
{
  FlowBinPrivate *priv = bin->priv;

  return g_hash_table_lookup (priv->string_to_element, name);
}

static void
add_element_to_list (gpointer key, gpointer value, gpointer user_data)
{
  FlowElement  *element  = key;
  GList       **list_ptr = user_data;

  *list_ptr = g_list_prepend (*list_ptr, element);
}

static void
add_unconnected_input_pads_to_list (gpointer key, gpointer value, gpointer user_data)
{
  FlowElement  *element  = key;
  GList       **list_ptr = user_data;
  GPtrArray    *input_pads;
  guint         i;

  input_pads = flow_element_get_input_pads (element);

  for (i = 0; i < input_pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (input_pads, i);

    if (!flow_pad_get_connected_pad (pad))
      *list_ptr = g_list_prepend (*list_ptr, pad);
  }
}

static void
add_unconnected_output_pads_to_list (gpointer key, gpointer value, gpointer user_data)
{
  FlowElement  *element  = key;
  GList       **list_ptr = user_data;
  GPtrArray    *output_pads;
  guint         i;

  output_pads = flow_element_get_output_pads (element);

  for (i = 0; i < output_pads->len; i++)
  {
    FlowPad *pad = g_ptr_array_index (output_pads, i);

    if (!flow_pad_get_connected_pad (pad))
      *list_ptr = g_list_prepend (*list_ptr, pad);
  }
}

/* --- FlowBin public API --- */

/**
 * flow_bin_new:
 * 
 * Creates a new #FlowBin.
 * 
 * Return value: A new #FlowBin.
 **/
FlowBin *
flow_bin_new (void)
{
  return g_object_new (FLOW_TYPE_BIN, NULL);
}

/**
 * flow_bin_add_element:
 * @bin: A #FlowBin.
 * @element: A #FlowElement to add to @bin.
 * @element_name: A unique string to identify @element in the context of @bin.
 * 
 * Adds @element to @bin.
 **/
void
flow_bin_add_element (FlowBin *bin, FlowElement *element, const gchar *element_name)
{
  FlowBinPrivate *priv;
  gchar          *element_name_copy = NULL;

  g_return_if_fail (FLOW_IS_BIN (bin));
  g_return_if_fail (FLOW_IS_ELEMENT (element));

  priv = bin->priv;

  if (find_element_by_ptr (bin, element, NULL))
  {
    g_warning ("Attempted to add element to bin twice.");
    return;
  }

  if (element_name && find_element_by_name (bin, element_name))
  {
    g_warning ("Attempted to add to elements with same name to bin.");
    return;
  }

  g_object_ref (element);

  if (element_name)
  {
    element_name_copy = g_strdup (element_name);
    g_hash_table_insert (priv->string_to_element, element_name_copy, element);
  }

  g_hash_table_insert (priv->element_to_string, element, element_name_copy);

  g_signal_emit_by_name (bin, "element-added", element);
}

/**
 * flow_bin_remove_element:
 * @bin: A #FlowBin.
 * @element: A #FlowElement to remove from @bin.
 * 
 * Removes @element from @bin.
 **/
void
flow_bin_remove_element (FlowBin *bin, FlowElement *element)
{
  FlowBinPrivate *priv;
  const gchar    *element_name;

  g_return_if_fail (FLOW_IS_BIN (bin));
  g_return_if_fail (FLOW_IS_ELEMENT (element));

  priv = bin->priv;

  if (!find_element_by_ptr (bin, element, &element_name))
  {
    g_warning ("Attempted removal of element not in bin.");
    return;
  }

  if (element_name)
    g_hash_table_remove (priv->string_to_element, element_name);

  g_hash_table_remove (priv->element_to_string, element);

  g_signal_emit_by_name (bin, "element-removed", element);
  g_object_unref (element);
}

/**
 * flow_bin_have_element:
 * @bin: A #FlowBin.
 * @element: A #FlowElement.
 * 
 * Checks if @element belongs to @bin.
 * 
 * Return value: %TRUE if @element belongs to @bin, %FALSE otherwise.
 **/
gboolean
flow_bin_have_element (FlowBin *bin, FlowElement *element)
{
  g_return_val_if_fail (FLOW_IS_BIN (bin), FALSE);
  g_return_val_if_fail (FLOW_IS_ELEMENT (element), FALSE);

  return find_element_by_ptr (bin, element, NULL);
}

/**
 * flow_bin_get_element:
 * @bin: A #FlowBin.
 * @element_name: A unique string identifier.
 * 
 * Finds a #FlowElement belonging to @bin by its unique identifier.
 * 
 * Return value: A #FlowElement, or %NULL if there was no match.
 **/
FlowElement *
flow_bin_get_element (FlowBin *bin, const gchar *element_name)
{
  g_return_val_if_fail (FLOW_IS_BIN (bin), NULL);
  g_return_val_if_fail (element_name != NULL, NULL);

  return find_element_by_name (bin, element_name);
}

/**
 * flow_bin_get_element_name:
 * @bin: A #FlowBin.
 * @element: A #FlowElement belonging to @bin.
 * 
 * If @element belongs to @bin, gets its unique identifier. If you
 * plan to use the string later, you should make a copy of it.
 * 
 * Return value: @element's identifier, or %NULL if it did not belong to @bin.
 **/
const gchar *
flow_bin_get_element_name (FlowBin *bin, FlowElement *element)
{
  const gchar *element_name;

  g_return_val_if_fail (FLOW_IS_BIN (bin), NULL);
  g_return_val_if_fail (FLOW_IS_ELEMENT (element), NULL);

  if (!find_element_by_ptr (bin, element, &element_name))
    return NULL;

  return element_name;
}

/**
 * flow_bin_list_elements:
 * @bin: A #FlowBin.
 * 
 * Generates a list of pointers to the elements that belong to @bin. You
 * must free the list when you're done with it, but you don't need to
 * unref the elements.
 * 
 * Return value: A #GList of pointers to #FlowElement.
 **/
GList *
flow_bin_list_elements (FlowBin *bin)
{
  FlowBinPrivate *priv;
  GList          *element_list = NULL;

  g_return_val_if_fail (FLOW_IS_BIN (bin), NULL);

  priv = bin->priv;

  g_hash_table_foreach (priv->element_to_string, add_element_to_list, &element_list);
  return element_list;
}

/**
 * flow_bin_list_unconnected_input_pads:
 * @bin: A #FlowBin.
 * 
 * Generates a list of pointers to any unconnected input pads belonging
 * to members of @bin. You must free the list when you're done with it, but you
 * don't need to unref the elements.
 * 
 * Return value: A #GList of pointers to #FlowInputPad.
 **/
GList *
flow_bin_list_unconnected_input_pads (FlowBin *bin)
{
  FlowBinPrivate *priv;
  GList          *input_pad_list = NULL;

  g_return_val_if_fail (FLOW_IS_BIN (bin), NULL);

  priv = bin->priv;

  g_hash_table_foreach (priv->element_to_string, add_unconnected_input_pads_to_list, &input_pad_list);
  return input_pad_list;
}

/**
 * flow_bin_list_unconnected_output_pads:
 * @bin: A #FlowBin.
 * 
 * Generates a list of pointers to any unconnected output pads belonging
 * to members of @bin. You must free the list when you're done with it, but you
 * don't need to unref the elements.
 * 
 * Return value: A #GList of pointers to #FlowOutputPad.
 **/
GList *
flow_bin_list_unconnected_output_pads (FlowBin *bin)
{
  FlowBinPrivate *priv;
  GList          *output_pad_list = NULL;

  g_return_val_if_fail (FLOW_IS_BIN (bin), NULL);

  priv = bin->priv;

  g_hash_table_foreach (priv->element_to_string, add_unconnected_output_pads_to_list, &output_pad_list);
  return output_pad_list;
}
