/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-pad.c - A pipeline element connector.
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
#include "flow-context-mgmt.h"
#include "flow-pad.h"

/* --- FlowPad private data --- */

typedef struct
{
}
FlowPadPrivate;

/* --- FlowPad properties --- */

static FlowElement *
flow_pad_get_owner_element_internal (FlowPad *pad)
{
  return pad->owner_element;
}

static void
flow_pad_set_owner_element_internal (FlowPad *pad, FlowElement *element)
{
  pad->owner_element = element;
  g_object_add_weak_pointer ((GObject *) element, (gpointer) &pad->owner_element);
}

static FlowPad *
flow_pad_get_connected_pad_internal (FlowPad *pad)
{
  return pad->connected_pad;
}

static void
flow_pad_set_connected_pad_internal (FlowPad *pad, FlowPad *other_pad)
{
  if (pad->connected_pad)
    g_object_remove_weak_pointer ((GObject *) pad->connected_pad, (gpointer) &pad->connected_pad);

  pad->connected_pad = other_pad;

  if (other_pad)
    g_object_add_weak_pointer ((GObject *) other_pad, (gpointer) &pad->connected_pad);
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_pad)
FLOW_GOBJECT_PROPERTY         (G_TYPE_OBJECT, "owner-element", "Owner element",
                               "Element this pad belongs to", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_pad_get_owner_element_internal, flow_pad_set_owner_element_internal,
                               flow_element_get_type)
FLOW_GOBJECT_PROPERTY         (G_TYPE_OBJECT, "connected-pad", "Connected pad",
                               "Pad connected to this pad", G_PARAM_READWRITE,
                               flow_pad_get_connected_pad_internal, flow_pad_set_connected_pad_internal,
                               flow_pad_get_type)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowPad definition --- */

FLOW_GOBJECT_MAKE_IMPL (flow_pad, FlowPad, G_TYPE_OBJECT, G_TYPE_FLAG_ABSTRACT)

/* --- FlowPad implementation --- */

static void
flow_pad_type_init (GType type)
{
}

static void
flow_pad_class_init (FlowPadClass *klass)
{
}

static void
flow_pad_init (FlowPad *pad)
{
  /* NOTE: We don't create the packet queue here. Subclasses will create it as needed.
   * This saves some memory, because some pads will never need to queue, and others
   * will only queue in rare circumstances. */
}

static void
flow_pad_construct (FlowPad *pad)
{
  if (!pad->owner_element)
    g_error ("Pad created with NULL owner element!");
}

static void
flow_pad_dispose (FlowPad *pad)
{
  /* The user may destroy us in one of our callbacks. When this happens, make
   * sure we don't generate any more output (clear queue and disconnect), but stay alive
   * until the callback dispatcher can exit. */

  if (pad->dispatch_depth)
  {
    if (pad->packet_queue)
      flow_packet_queue_clear (pad->packet_queue);

    if (pad->connected_pad)
    {
      g_object_remove_weak_pointer ((GObject *) pad->connected_pad, (gpointer) &pad->connected_pad);
      pad->connected_pad = NULL;
    }

    pad->was_disposed = TRUE;
    g_object_ref (pad);
    return;
  }

  if (pad->owner_element)
  {
    g_object_remove_weak_pointer ((GObject *) pad->owner_element, (gpointer) &pad->owner_element);
    pad->owner_element = NULL;
  }

  if (pad->connected_pad)
  {
    g_object_remove_weak_pointer ((GObject *) pad->connected_pad, (gpointer) &pad->connected_pad);
    pad->connected_pad = NULL;
  }

  if (pad->packet_queue)
  {
    g_object_unref (pad->packet_queue);
    pad->packet_queue = NULL;
  }
}

static void
flow_pad_finalize (FlowPad *pad)
{
}

/* - Functions to schedule a push when it can't be done immediately - */

typedef struct
{
  FlowPad *pad;
  guint    idle_id;
}
DelayedPushInfo;

static void
cancel_delayed_push (DelayedPushInfo *delayed_push_info, gpointer dead_pad)
{
  flow_source_remove_from_current_thread (delayed_push_info->idle_id);

  g_slice_free (DelayedPushInfo, delayed_push_info);
}

static gboolean
delayed_push (DelayedPushInfo *delayed_push_info)
{
  FlowPad *pad = delayed_push_info->pad;

  FLOW_PAD_GET_CLASS (pad)->push (pad, NULL);

  g_object_weak_unref ((GObject *) pad, (GWeakNotify) cancel_delayed_push, delayed_push_info);
  g_slice_free (DelayedPushInfo, delayed_push_info);
  return FALSE;
}

static void
prepare_delayed_push (FlowPad *pad)
{
  DelayedPushInfo *delayed_push_info;

  delayed_push_info = g_slice_new (DelayedPushInfo);

  delayed_push_info->pad = pad;
  delayed_push_info->idle_id = flow_idle_add_to_current_thread ((GSourceFunc) delayed_push, delayed_push_info);
  g_object_weak_ref ((GObject *) pad, (GWeakNotify) cancel_delayed_push, delayed_push_info);
}

/* --- FlowPad public API --- */

void
flow_pad_push (FlowPad *pad, FlowPacket *packet)
{
  g_return_if_fail (FLOW_IS_PAD (pad));

  FLOW_PAD_GET_CLASS (pad)->push (pad, packet);

  /* At this point, pad may be gone */
}

void
flow_pad_connect (FlowPad *pad, FlowPad *other_pad)
{
  gpointer pad_ref       = pad;
  gpointer other_pad_ref = other_pad;

  g_return_if_fail (FLOW_IS_PAD (pad));
  g_return_if_fail (FLOW_IS_PAD (other_pad));

  g_object_add_weak_pointer ((GObject *) pad, &pad_ref);
  g_object_add_weak_pointer ((GObject *) other_pad, &other_pad_ref);

  g_object_set (pad, "connected-pad", other_pad, NULL);

  /* At this point, either pad may be gone */

  if (pad_ref && other_pad_ref)
    g_object_set (other_pad, "connected-pad", pad, NULL);

  /* Stimulate data flow across new connection */

  if (pad_ref)
    prepare_delayed_push (pad);
  if (other_pad_ref)
    prepare_delayed_push (other_pad);

  /* Clean up */

  if (pad_ref)
    g_object_remove_weak_pointer ((GObject *) pad, &pad_ref);
  if (other_pad_ref)
    g_object_remove_weak_pointer ((GObject *) other_pad, &other_pad_ref);
}

void
flow_pad_disconnect (FlowPad *pad)
{
  g_return_if_fail (FLOW_IS_PAD (pad));

  g_object_set (pad, "connected-pad", NULL, NULL);
}

void
flow_pad_block (FlowPad *pad)
{
  FlowPadClass *pad_klass;

  g_return_if_fail (FLOW_IS_PAD (pad));

  if (pad->is_blocked)
    return;

  pad->is_blocked = TRUE;

  pad_klass = FLOW_PAD_GET_CLASS (pad);
  pad_klass->block (pad);

  /* At this point, pad may be gone */
}

void
flow_pad_unblock (FlowPad *pad)
{
  FlowPadClass *pad_klass;

  g_return_if_fail (FLOW_IS_PAD (pad));

  if (!pad->is_blocked)
    return;

  pad->is_blocked = FALSE;

  pad_klass = FLOW_PAD_GET_CLASS (pad);
  pad_klass->unblock (pad);

  /* At this point, pad may be gone */
}

gboolean
flow_pad_is_blocked (FlowPad *pad)
{
  g_return_val_if_fail (FLOW_IS_PAD (pad), FALSE);

  return pad->is_blocked;
}

FlowElement *
flow_pad_get_owner_element (FlowPad *pad)
{
  g_return_val_if_fail (FLOW_IS_PAD (pad), NULL);

  return flow_pad_get_owner_element_internal (pad);
}

FlowPad *
flow_pad_get_connected_pad (FlowPad *pad)
{
  g_return_val_if_fail (FLOW_IS_PAD (pad), NULL);

  return flow_pad_get_connected_pad_internal (pad);
}

FlowPacketQueue *
flow_pad_get_packet_queue (FlowPad *pad)
{
  g_return_val_if_fail (FLOW_IS_PAD (pad), NULL);

  return pad->packet_queue;
}
