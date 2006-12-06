/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-tls-protocol.c - TLS/SSL protocol element.
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
#include "flow-util.h"
#include "flow-gobject-util.h"
#include "flow-enum-types.h"
#include "flow-tls-protocol.h"

#include <gnutls/gnutls.h>
#include <gcrypt.h>

#include <errno.h>

#define UPSTREAM_INDEX   0
#define DOWNSTREAM_INDEX 1

#define MAX_READ         4096
#define DH_BITS_DEFAULT  1024

typedef enum
{
  /* Both directions */

  STATE_OPEN,
  STATE_CLOSED,
  STATE_CLOSING,

  /* From downstream only */

  STATE_OPENING
}
State;

static GStaticMutex       gnutls_mutex                 = G_STATIC_MUTEX_INIT;
static gboolean           gnutls_is_initialized        = FALSE;
static gboolean           gnutls_server_is_initialized = FALSE;
static gnutls_dh_params_t gnutls_dh_parameters;

/* --- FlowTlsProtocol private data --- */

typedef struct
{
  guint                            from_downstream_state   : 4;
  guint                            from_upstream_state     : 4;

  guint                            agent_role              : 2;
  guint                            session_initialized     : 1;

  gnutls_session_t                 tls_session;

  /* We assume that the gnutls_*_credentials_t types are just
   * aliases for pointers. This makes it a lot less complicated
   * for us. */

  gpointer                         creds;
}
FlowTlsProtocolPrivate;

/* --- FlowTlsProtocol properties --- */

static FlowAgentRole
flow_tls_protocol_get_agent_role_internal (FlowTlsProtocol *tls_protocol)
{
  FlowTlsProtocolPrivate *priv = tls_protocol->priv;

  return priv->agent_role;
}

static void
flow_tls_protocol_set_agent_role_internal (FlowTlsProtocol *tls_protocol, FlowAgentRole agent_role)
{
  FlowTlsProtocolPrivate *priv = tls_protocol->priv;

  priv->agent_role = agent_role;
}

FLOW_GOBJECT_PROPERTIES_BEGIN (flow_tls_protocol)
FLOW_GOBJECT_PROPERTY_ENUM    ("agent-role", "Agent role", "Agent role (server/client)",
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY,
                               flow_tls_protocol_get_agent_role_internal,
                               flow_tls_protocol_set_agent_role_internal,
                               FLOW_AGENT_ROLE_CLIENT,
                               flow_agent_role_get_type)
FLOW_GOBJECT_PROPERTIES_END   ()

/* --- FlowTlsProtocol definition --- */

FLOW_GOBJECT_MAKE_IMPL        (flow_tls_protocol, FlowTlsProtocol, FLOW_TYPE_DUPLEX_ELEMENT, 0)

/* --- FlowTlsProtocol implementation --- */

static void flow_tls_protocol_process_input (FlowElement *element, FlowPad *input_pad);

/* Thread safety functions for GCrypt */

static gint
mutex_init_gcrypt (void **mutex_ptr)
{
  *mutex_ptr = g_mutex_new ();
  return 0;
}

static gint
mutex_destroy_gcrypt (void **mutex_ptr)
{
  g_mutex_free (*mutex_ptr);
  return 0;
}

static gint
mutex_lock_gcrypt (void **mutex_ptr)
{
  g_mutex_lock (*mutex_ptr);
  return 0;
}

static gint
mutex_unlock_gcrypt (void **mutex_ptr)
{
  g_mutex_unlock (*mutex_ptr);
  return 0;
}

static void
global_ref_gnutls (void)
{
  guchar buf [1];
  static struct gcry_thread_cbs gcrypt_thread_callbacks =
  {
    GCRY_THREAD_OPTION_USER,
    NULL,                     /* Global init */
    mutex_init_gcrypt,
    mutex_destroy_gcrypt,
    mutex_lock_gcrypt,
    mutex_unlock_gcrypt,
    NULL,                     /* read */
    NULL,                     /* write */
    NULL,                     /* select */
    NULL,                     /* waitpid */
    NULL,                     /* accept */
    NULL,                     /* connect */
    NULL,                     /* sendmsg */
    NULL                      /* recvmsg */
  };

  g_static_mutex_lock (&gnutls_mutex);

  if (!gnutls_is_initialized)
  {
    gnutls_global_set_mem_functions ((gnutls_alloc_function)     g_malloc,
                                     (gnutls_alloc_function)     g_malloc,
                                     (gnutls_is_secure_function) NULL,
                                     (gnutls_realloc_function)   g_realloc,
                                     (gnutls_free_function)      g_free);

    gcry_control (GCRYCTL_SET_THREAD_CBS, &gcrypt_thread_callbacks);

    gnutls_is_initialized = TRUE;
  }

  /* Increase global init count by 1 */

  if (gnutls_global_init ())
    g_error (G_STRLOC ": Failed to initialize GNU TLS.");

  /* Run the GCrypt randomizer so it'll do its non-threadsafe initialization
   * while we hold a lock. This is a workaround for a bug in GCrypt 1.2.3. If
   * someone initializes GCrypt in another part of the program that can
   * run concurrently with us, we're fucked */

  gcry_randomize (buf, 1, 0);

  g_static_mutex_unlock (&gnutls_mutex);
}

static void
global_unref_gnutls (void)
{
  g_static_mutex_lock (&gnutls_mutex);

  /* Decrease global init count by 1, possibly freeing GNU TLS global overhead */

  gnutls_global_deinit ();

  g_static_mutex_unlock (&gnutls_mutex);
}

/* Similar to send(2): ssize_t (*gnutls_push_func) (gnutls_transport_ptr_t, const void *, size_t) */
static ssize_t
send_for_gnutls (FlowTlsProtocol *tls_protocol, gconstpointer src, size_t len)
{
  FlowElement *element = (FlowElement *) tls_protocol;
  FlowPad     *output_pad;
  FlowPacket  *packet;

  /* TODO: Check how big the packets can get, and optionally split them up so
   * memory can be re-used with higher granularity. */

  output_pad = g_ptr_array_index (element->output_pads, DOWNSTREAM_INDEX);
  packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, (gpointer) src, len);
  flow_pad_push (output_pad, packet);

  return len;
}

/* Similar to recv(2): ssize_t (*gnutls_pull_func) (gnutls_transport_ptr_t, void *, size_t) */
static ssize_t
recv_for_gnutls (FlowTlsProtocol *tls_protocol, gpointer dest, size_t len)
{
  FlowElement            *element = (FlowElement *) tls_protocol;
  FlowPad                *input_pad;
  FlowPacketQueue        *packet_queue;

  input_pad = g_ptr_array_index (element->input_pads, DOWNSTREAM_INDEX);

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
  {
    /* FIXME: We should use the following function, but it's only available
     * in GnuTLS 1.6 onwards, which just came out (November 2006). Wait until
     * it's available in major distros before fixing.
     *
     * gnutls_transport_set_errno (priv->tls_session, EAGAIN); */

    errno = EAGAIN;
    return -1;
  }

  len = flow_packet_queue_pop_bytes (packet_queue, dest, len);
  if (len == 0)
  {
    len = -1;
    errno = EAGAIN;
  }

  return len;
}

static void
initialize_server_params (void)
{
  g_static_mutex_lock (&gnutls_mutex);

  if (!gnutls_server_is_initialized)
  {
    gnutls_dh_params_init (&gnutls_dh_parameters);
    gnutls_dh_params_generate2 (gnutls_dh_parameters, DH_BITS_DEFAULT);

    gnutls_server_is_initialized = TRUE;
  }

  g_static_mutex_unlock (&gnutls_mutex);
}

static void
initialize_session (FlowTlsProtocol *tls_protocol)
{
  FlowTlsProtocolPrivate *priv = tls_protocol->priv;
  const int kx_prio []         = { GNUTLS_KX_ANON_DH, 0 };

  if (priv->session_initialized)
    return;

  global_ref_gnutls ();

  if (priv->agent_role == FLOW_AGENT_ROLE_SERVER)
  {
    initialize_server_params ();

    gnutls_anon_allocate_server_credentials ((gpointer) &priv->creds);
    gnutls_anon_set_server_dh_params (priv->creds, gnutls_dh_parameters);

    gnutls_init (&priv->tls_session, GNUTLS_SERVER);

  gnutls_set_default_priority (priv->tls_session);
  gnutls_kx_set_priority (priv->tls_session, kx_prio);

    gnutls_credentials_set (priv->tls_session, GNUTLS_CRD_ANON, priv->creds);

    gnutls_dh_set_prime_bits (priv->tls_session, DH_BITS_DEFAULT);
  }
  else
  {
    gnutls_init (&priv->tls_session, GNUTLS_CLIENT);

  gnutls_set_default_priority (priv->tls_session);
  gnutls_kx_set_priority (priv->tls_session, kx_prio);

    gnutls_anon_allocate_client_credentials ((gpointer) &priv->creds);
    gnutls_credentials_set (priv->tls_session, GNUTLS_CRD_ANON, priv->creds);
  }

  gnutls_transport_set_lowat (priv->tls_session, 0);
  gnutls_transport_set_push_function (priv->tls_session,
                                      (gnutls_push_func) send_for_gnutls);
  gnutls_transport_set_pull_function (priv->tls_session,
                                      (gnutls_pull_func) recv_for_gnutls);
  gnutls_transport_set_ptr (priv->tls_session, tls_protocol);

  priv->session_initialized = TRUE;
}

static void
finalize_session (FlowTlsProtocol *tls_protocol)
{
  FlowTlsProtocolPrivate *priv = tls_protocol->priv;

  if (!priv->session_initialized)
    return;

  gnutls_deinit (priv->tls_session);
  global_unref_gnutls ();

  priv->session_initialized = FALSE;
}

static void
flow_tls_protocol_output_pad_blocked (FlowElement *element, FlowPad *output_pad)
{
  FlowTlsProtocol        *tls_protocol = (FlowTlsProtocol *) element;
  FlowTlsProtocolPrivate *priv         = tls_protocol->priv;
  gint                    input_index;

  if (output_pad == g_ptr_array_index (element->output_pads, UPSTREAM_INDEX))
  {
    /* We can't block data coming from downstream while we're in a handshake
     * sequence. */

    if (priv->from_downstream_state == STATE_OPENING)
      return;

    input_index = DOWNSTREAM_INDEX;
  }
  else
  {
    input_index = UPSTREAM_INDEX;
  }

  flow_pad_block (g_ptr_array_index (element->input_pads, input_index));
}

static void
flow_tls_protocol_output_pad_unblocked (FlowElement *element, FlowPad *output_pad)
{
  FlowTlsProtocol        *tls_protocol = (FlowTlsProtocol *) element;
  FlowTlsProtocolPrivate *priv         = tls_protocol->priv;
  gint                    input_index;

  if (output_pad == g_ptr_array_index (element->output_pads, UPSTREAM_INDEX))
  {
    /* If downstream was closed, we have to wait for upstream to close
     * too before we can accept more data from downstream */

    if (priv->from_downstream_state == STATE_CLOSING)
      return;

    input_index = DOWNSTREAM_INDEX;
  }
  else
  {
    /* If upstream was closed, we have to wait for downstream to close
     * too before we can accept more data from upstream */

    if (priv->from_upstream_state == STATE_CLOSING)
      return;

    /* We have to finish the handshake sequence before we can let upstream
     * send more data */

    if (priv->from_downstream_state == STATE_OPENING)
      return;

    input_index = UPSTREAM_INDEX;
  }

  flow_pad_unblock (g_ptr_array_index (element->input_pads, input_index));
}

static void
block_from_upstream (FlowTlsProtocol *tls_protocol)
{
  FlowElement *element = (FlowElement *) tls_protocol;

  flow_pad_block (g_ptr_array_index (element->input_pads, UPSTREAM_INDEX));
}

static void
unblock_from_upstream (FlowTlsProtocol *tls_protocol)
{
  FlowElement *element = (FlowElement *) tls_protocol;

  if (!flow_pad_is_blocked (g_ptr_array_index (element->output_pads, DOWNSTREAM_INDEX)))
    flow_pad_unblock (g_ptr_array_index (element->input_pads, UPSTREAM_INDEX));
}

static void
block_from_downstream (FlowTlsProtocol *tls_protocol)
{
  FlowElement *element = (FlowElement *) tls_protocol;

  flow_pad_block (g_ptr_array_index (element->input_pads, DOWNSTREAM_INDEX));
}

static void
unblock_from_downstream (FlowTlsProtocol *tls_protocol)
{
  FlowElement *element = (FlowElement *) tls_protocol;

  if (!flow_pad_is_blocked (g_ptr_array_index (element->output_pads, UPSTREAM_INDEX)))
    flow_pad_unblock (g_ptr_array_index (element->input_pads, DOWNSTREAM_INDEX));
}

static void
close_from_upstream (FlowTlsProtocol *tls_protocol, gint tls_error, gboolean generate_stream_end)
{
  FlowTlsProtocolPrivate *priv    = tls_protocol->priv;
  FlowElement            *element = (FlowElement *) tls_protocol;
  FlowDetailedEvent      *detailed_event;
  FlowPacket             *packet;

  if (priv->from_upstream_state == STATE_OPEN)
  {
    /* First thing, pass along a FLOW_STREAM_END and block further input */

    priv->from_upstream_state = STATE_CLOSING;
    block_from_upstream (tls_protocol);

    if (generate_stream_end)
    {
      if (tls_error < 0)
      {
        detailed_event = flow_detailed_event_new_literal (gnutls_strerror (tls_error));
        flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
        flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);

        packet = flow_packet_new_take_object (detailed_event, 0);
      }
      else
      {
        packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
      }

      flow_pad_push (g_ptr_array_index (element->output_pads, DOWNSTREAM_INDEX), packet);
    }
  }

  if (priv->from_upstream_state == STATE_CLOSING)
  {
    /* If one direction is waiting for the other to close, and it was closed,
     * also close the first direction */

    if (priv->from_downstream_state == STATE_CLOSING)
      priv->from_downstream_state = STATE_CLOSED;

    if (priv->from_downstream_state == STATE_CLOSED)
      priv->from_upstream_state = STATE_CLOSED;
  }

  /* If we managed to close both ways, finalize the session and unblock input */

  if (priv->from_upstream_state   == STATE_CLOSED &&
      priv->from_downstream_state == STATE_CLOSED)
  {
    finalize_session (tls_protocol);
    unblock_from_upstream (tls_protocol);
    unblock_from_downstream (tls_protocol);
  }
}

static void
close_from_downstream (FlowTlsProtocol *tls_protocol, gint tls_error, gboolean generate_stream_end)
{
  FlowTlsProtocolPrivate *priv    = tls_protocol->priv;
  FlowElement            *element = (FlowElement *) tls_protocol;
  FlowDetailedEvent      *detailed_event;
  FlowPacket             *packet;

  if (priv->from_downstream_state == STATE_OPEN ||
      priv->from_downstream_state == STATE_OPENING)
  {
    /* First thing, pass along a FLOW_STREAM_END and block further input */

    priv->from_downstream_state = STATE_CLOSING;
    block_from_downstream (tls_protocol);

    if (generate_stream_end)
    {
      if (tls_error < 0)
      {
        detailed_event = flow_detailed_event_new_literal (gnutls_strerror (tls_error));
        flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_ERROR);
        flow_detailed_event_add_code (detailed_event, FLOW_STREAM_DOMAIN, FLOW_STREAM_END);

        packet = flow_packet_new_take_object (detailed_event, 0);
      }
      else
      {
        packet = flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_END);
      }

      flow_pad_push (g_ptr_array_index (element->output_pads, UPSTREAM_INDEX), packet);
    }
  }

  if (priv->from_downstream_state == STATE_CLOSING)
  {
    /* If one direction is waiting for the other to close, and it was closed,
     * also close the first direction */

    if (priv->from_upstream_state == STATE_CLOSING)
      priv->from_upstream_state = STATE_CLOSED;

    if (priv->from_upstream_state == STATE_CLOSED)
      priv->from_downstream_state = STATE_CLOSED;
  }

  /* If we managed to close both ways, finalize the session and unblock input */

  if (priv->from_upstream_state   == STATE_CLOSED &&
      priv->from_downstream_state == STATE_CLOSED)
  {
    finalize_session (tls_protocol);
    unblock_from_upstream (tls_protocol);
    unblock_from_downstream (tls_protocol);
  }
}

#if 0
static void process_input_from_upstream (FlowTlsProtocol *tls_protocol, FlowPad *input_pad);
static void process_input_from_downstream (FlowTlsProtocol *tls_protocol, FlowPad *input_pad);
#endif

static void
do_handshake (FlowTlsProtocol *tls_protocol)
{
  FlowTlsProtocolPrivate *priv    = tls_protocol->priv;
  FlowElement            *element = (FlowElement *) tls_protocol;
  ssize_t                 result;

  result = gnutls_handshake (priv->tls_session);

  if (result == 0)
  {
    /* Handshake done */

    priv->from_downstream_state = STATE_OPEN;
    unblock_from_upstream (tls_protocol);

#if 0
    process_input_from_upstream (tls_protocol, g_ptr_array_index (element->input_pads, UPSTREAM_INDEX));
    process_input_from_downstream (tls_protocol, g_ptr_array_index (element->input_pads, DOWNSTREAM_INDEX));
#endif
  }
  else if (gnutls_error_is_fatal (result))
  {
    /* Protocol error */

    g_print ("[%p] Handshake error\n", tls_protocol);

    close_from_upstream (tls_protocol, result, TRUE);
    close_from_downstream (tls_protocol, result, TRUE);
  }
}

static void
process_object_from_upstream (FlowTlsProtocol *tls_protocol, FlowPacket *packet)
{
  FlowTlsProtocolPrivate *priv    = tls_protocol->priv;
  FlowElement            *element = (FlowElement *) tls_protocol;
  gpointer                object  = flow_packet_get_data (packet);

  flow_handle_universal_events (element, packet);

  if (FLOW_IS_DETAILED_EVENT (object))
  {
    if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
    {
      if (priv->from_upstream_state == STATE_CLOSED)
      {
        priv->from_upstream_state = STATE_OPEN;

        flow_pad_push (g_ptr_array_index (element->output_pads, DOWNSTREAM_INDEX), packet);
        packet = NULL;

        if (priv->from_downstream_state == STATE_CLOSED)
        {
          /* New session */
          initialize_session (tls_protocol);
          block_from_upstream (tls_protocol);
        }
      }
      else if (priv->from_upstream_state == STATE_OPEN)
      {
        /* Downstream has already seen FLOW_STREAM_BEGIN */
        flow_packet_free (packet);
        packet = NULL;
      }
      else if (priv->from_upstream_state == STATE_CLOSING)
      {
        /* Can't happen - input should be blocked */
        g_assert_not_reached ();
      }
      else
      {
        g_assert_not_reached ();
      }
    }
    else if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
    {
      if (priv->from_upstream_state == STATE_OPEN)
      {
        /* This can't fail because our "send" helper always accepts and queues outbound data */
        gnutls_bye (priv->tls_session, GNUTLS_SHUT_WR);
        close_from_upstream (tls_protocol, 0, FALSE);
      }
      else
      {
        /* Downstream has already seen FLOW_STREAM_END */
        flow_packet_free (packet);
        packet = NULL;
      }
    }
    else if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_FLUSH))
    {
      /* GNU TLS always syncs */
    }
  }

  if (packet)
    flow_pad_push (g_ptr_array_index (element->output_pads, DOWNSTREAM_INDEX), packet);
}

static void
process_object_from_downstream (FlowTlsProtocol *tls_protocol, FlowPacket *packet)
{
  FlowTlsProtocolPrivate *priv    = tls_protocol->priv;
  FlowElement            *element = (FlowElement *) tls_protocol;
  gpointer                object  = flow_packet_get_data (packet);

  flow_handle_universal_events (element, packet);

  if (FLOW_IS_DETAILED_EVENT (object))
  {
    if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
    {
      if (priv->from_downstream_state == STATE_CLOSED)
      {
        if (priv->from_upstream_state == STATE_CLOSED)
          initialize_session (tls_protocol);

        priv->from_downstream_state = STATE_OPENING;
        priv->from_upstream_state   = STATE_OPEN;

        /* Since we're handshaking, we can't send data. Block data coming from upstream. */
        block_from_upstream (tls_protocol);

        flow_pad_push (g_ptr_array_index (element->output_pads, DOWNSTREAM_INDEX),
                       flow_create_simple_event_packet (FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN));

        do_handshake (tls_protocol);
      }
      else if (priv->from_downstream_state == STATE_CLOSING)
      {
        /* Can't happen - input should be blocked */
        g_assert_not_reached ();
      }
      else
      {
        /* Upstream has already seen FLOW_STREAM_BEGIN */
        flow_packet_free (packet);
        packet = NULL;
      }
    }
    else if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_END))
    {
      /* If we didn't receive a "bye" first, this is irregular. However, there are
       * legit TLS implementations that omit it. The upshot is that we can't tell if
       * the peer terminated the session or if the EOF was forged by a third party. */

      close_from_downstream (tls_protocol, 0, FALSE);
    }
  }

  if (packet)
    flow_pad_push (g_ptr_array_index (element->output_pads, UPSTREAM_INDEX), packet);
}

static void
process_input_from_upstream (FlowTlsProtocol *tls_protocol, FlowPad *input_pad)
{
  FlowTlsProtocolPrivate *priv = tls_protocol->priv;
  FlowPacketQueue        *packet_queue;
  FlowPacket             *packet;
  gint                    packet_offset;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  while (flow_packet_queue_peek_packet (packet_queue, &packet, &packet_offset))
  {
    ssize_t result;

    if G_UNLIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
    {
      flow_packet_queue_pop_packet (packet_queue);
      process_object_from_upstream (tls_protocol, packet);
    }
    else if (priv->from_downstream_state == STATE_OPENING)
    {
      /* We're handshaking, so can't send data. This shouldn't happen, because we
       * block data coming from upstream while we're handshaking. */

      g_assert_not_reached ();
    }
    else if G_LIKELY (priv->from_upstream_state == STATE_OPEN)
    {
      guint8 *data;
      gint    len;

      len  = flow_packet_get_size (packet) - packet_offset;
      data = flow_packet_get_data (packet) + packet_offset;

      result = gnutls_record_send (priv->tls_session, data, len);

      if (result == len)
      {
        flow_packet_queue_drop_packet (packet_queue);
      }
      else if (result > 0)
      {
        flow_packet_queue_pop_bytes_exact (packet_queue, NULL, result);
      }
      else
      {
        /* Send can't fail because our helper always accepts and queues outbound data. */
        g_assert_not_reached ();
      }
    }
    else if (priv->from_upstream_state == STATE_CLOSED)
    {
      g_print ("[%p] Ate %d bytes (from upstream)\n", tls_protocol, flow_packet_get_size (packet));

      /* While the stream is closed, eat data packets. */
      flow_packet_queue_pop_packet (packet_queue);
      flow_packet_free (packet);
    }
    else if (priv->from_upstream_state == STATE_CLOSING)
    {
      /* Can't happen - input should be blocked */
      g_assert_not_reached ();
    }
    else
    {
      g_assert_not_reached ();
    }
  }
}

static void
process_input_from_downstream (FlowTlsProtocol *tls_protocol, FlowPad *input_pad)
{
  FlowTlsProtocolPrivate *priv    = tls_protocol->priv;
  FlowElement            *element = (FlowElement *) tls_protocol;
  FlowPacketQueue        *packet_queue;
  FlowPacket             *packet;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  while (flow_packet_queue_peek_packet (packet_queue, &packet, NULL))
  {
    ssize_t result;

    if G_UNLIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
    {
      flow_packet_queue_pop_packet (packet_queue);
      process_object_from_downstream (tls_protocol, packet);
    }
    else if G_LIKELY (priv->from_downstream_state == STATE_OPEN)
    {
      guint8 buf [MAX_READ];

      while ((result = gnutls_record_recv (priv->tls_session, buf, MAX_READ)) > 0)
      {
        packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buf, result);
        flow_pad_push (g_ptr_array_index (element->output_pads, UPSTREAM_INDEX), packet);
      }

      if (result == 0)
      {
        /* Got bye from remote end */

        close_from_downstream (tls_protocol, 0, TRUE);
      }
      else if (gnutls_error_is_fatal (result))
      {
        /* Crypto error */

        g_print ("[%p] Crypto error (from downstream)\n", tls_protocol);

        close_from_downstream (tls_protocol, result, TRUE);
      }
    }
    else if (priv->from_downstream_state == STATE_OPENING)
    {
      do_handshake (tls_protocol);
    }
    else if (priv->from_downstream_state == STATE_CLOSED)
    {
      g_print ("[%p] Ate %d bytes (from downstream)\n", tls_protocol, flow_packet_get_size (packet));

      /* When the stream is closed (TLS bye or physically), eat data packets. */
      flow_packet_queue_pop_packet (packet_queue);
      flow_packet_free (packet);
    }
    else if (priv->from_downstream_state == STATE_CLOSING)
    {
      /* Can't happen - input should be blocked */
      g_assert_not_reached ();
    }
    else
    {
      g_assert_not_reached ();
    }
  }
}

static void
flow_tls_protocol_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowTlsProtocol        *tls_protocol = FLOW_TLS_PROTOCOL (element);
  FlowPacketQueue        *packet_queue;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  if (input_pad == g_ptr_array_index (element->input_pads, UPSTREAM_INDEX))
  {
    process_input_from_upstream (tls_protocol, input_pad);
  }
  else
  {
    process_input_from_downstream (tls_protocol, input_pad);
  }
}

static void
flow_tls_protocol_type_init (GType type)
{
}

static void
flow_tls_protocol_class_init (FlowTlsProtocolClass *klass)
{
  FlowElementClass *element_klass = FLOW_ELEMENT_CLASS (klass);

  element_klass->output_pad_blocked   = flow_tls_protocol_output_pad_blocked;
  element_klass->output_pad_unblocked = flow_tls_protocol_output_pad_unblocked;
  element_klass->process_input        = flow_tls_protocol_process_input;
}

static void
flow_tls_protocol_init (FlowTlsProtocol *tls_protocol)
{
  FlowTlsProtocolPrivate *priv = tls_protocol->priv;

  if (!g_thread_supported ())
    g_thread_init (NULL);

  priv->agent_role = FLOW_AGENT_ROLE_CLIENT;

  priv->from_upstream_state   = STATE_CLOSED;
  priv->from_downstream_state = STATE_CLOSED;
}

static void
flow_tls_protocol_construct (FlowTlsProtocol *tls_protocol)
{
}

static void
flow_tls_protocol_dispose (FlowTlsProtocol *tls_protocol)
{
}

static void
flow_tls_protocol_finalize (FlowTlsProtocol *tls_protocol)
{
  finalize_session (tls_protocol);
}

/* --- FlowTlsProtocol public API --- */

FlowTlsProtocol *
flow_tls_protocol_new (FlowAgentRole agent_role)
{
  return g_object_new (FLOW_TYPE_TLS_PROTOCOL, "agent-role", agent_role, NULL);
}
