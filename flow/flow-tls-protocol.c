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

#define MAX_READ        4096
#define DH_BITS_DEFAULT 1024

typedef enum
{
  /* Both directions */

  STATE_OPEN,
  STATE_CLOSED,

  /* Downstream only */

  STATE_HANDSHAKING,
  STATE_QUITTING
}
State;

static GStaticMutex       gnutls_mutex                 = G_STATIC_MUTEX_INIT;
static gboolean           gnutls_is_initialized        = FALSE;
static gboolean           gnutls_server_is_initialized = FALSE;
static gnutls_dh_params_t gnutls_dh_parameters;

/* --- FlowTlsProtocol private data --- */

typedef struct
{
  guint8                           upstream_state;
  guint8                           downstream_state;
  guint16                          session_initialized : 1;
  FlowAgentRole                    agent_role;
  gnutls_session_t                 gnutls_session;
  gnutls_anon_client_credentials_t anon_creds;
  gnutls_anon_server_credentials_t server_anon_creds;
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
tls_log_func (gint level, const gchar *buf)
{
  g_print (buf);
  g_print ("\n");
}

static void
global_ref_gnutls (void)
{
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

#if 0
  gnutls_global_set_log_level (10);
  gnutls_global_set_log_function ((gnutls_log_func) tls_log_func);
#endif

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
     * gnutls_transport_set_errno (priv->gnutls_session, EAGAIN); */

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
do_downstream_tasks (FlowTlsProtocol *tls_protocol)
{
  FlowTlsProtocolPrivate *priv    = tls_protocol->priv;
  FlowElement            *element = (FlowElement *) tls_protocol;
  gint                    result;

  if (priv->downstream_state == STATE_HANDSHAKING)
  {
    result = gnutls_handshake (priv->gnutls_session);

    if (result == 0)
    {
      /* Handshake done */

      priv->downstream_state = STATE_OPEN;

      if (!flow_pad_is_blocked (g_ptr_array_index (element->output_pads, DOWNSTREAM_INDEX)))
        flow_pad_unblock (g_ptr_array_index (element->input_pads, UPSTREAM_INDEX));

      flow_tls_protocol_process_input ((FlowElement *) tls_protocol,
                                       g_ptr_array_index (element->input_pads, UPSTREAM_INDEX));
    }
#if 0
    else
    {
      gnutls_perror (result);
    }
#endif
  }
  else if (priv->downstream_state == STATE_QUITTING)
  {
    result = gnutls_bye (priv->gnutls_session, GNUTLS_SHUT_RDWR);

    if (result == 0)
    {
      /* Quit done */

      priv->downstream_state = STATE_CLOSED;

      if (!flow_pad_is_blocked (g_ptr_array_index (element->output_pads, DOWNSTREAM_INDEX)))
        flow_pad_unblock (g_ptr_array_index (element->input_pads, UPSTREAM_INDEX));

      flow_tls_protocol_process_input ((FlowElement *) tls_protocol,
                                       g_ptr_array_index (element->input_pads, UPSTREAM_INDEX));
    }
  }
}

static void
initialize_server_params (void)
{
  g_static_mutex_lock (&gnutls_mutex);

  if (gnutls_server_is_initialized)
  {
    g_static_mutex_unlock (&gnutls_mutex);
    return;
  }

  gnutls_dh_params_init (&gnutls_dh_parameters);
  gnutls_dh_params_generate2 (gnutls_dh_parameters, DH_BITS_DEFAULT);

  gnutls_server_is_initialized = TRUE;

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

    gnutls_anon_allocate_server_credentials (&priv->server_anon_creds);
    gnutls_anon_set_server_dh_params (priv->server_anon_creds, gnutls_dh_parameters);

    gnutls_init (&priv->gnutls_session, GNUTLS_SERVER);

  gnutls_set_default_priority (priv->gnutls_session);
  gnutls_kx_set_priority (priv->gnutls_session, kx_prio);

    gnutls_credentials_set (priv->gnutls_session, GNUTLS_CRD_ANON, priv->server_anon_creds);

    gnutls_dh_set_prime_bits (priv->gnutls_session, DH_BITS_DEFAULT);
  }
  else
  {
    gnutls_init (&priv->gnutls_session, GNUTLS_CLIENT);

  gnutls_set_default_priority (priv->gnutls_session);
  gnutls_kx_set_priority (priv->gnutls_session, kx_prio);

    gnutls_anon_allocate_client_credentials (&priv->anon_creds);
    gnutls_credentials_set (priv->gnutls_session, GNUTLS_CRD_ANON, priv->anon_creds);
  }

  gnutls_transport_set_lowat (priv->gnutls_session, 0);
  gnutls_transport_set_push_function (priv->gnutls_session,
                                      (gnutls_push_func) send_for_gnutls);
  gnutls_transport_set_pull_function (priv->gnutls_session,
                                      (gnutls_pull_func) recv_for_gnutls);
  gnutls_transport_set_ptr (priv->gnutls_session, tls_protocol);

  priv->session_initialized = TRUE;
}

static void
finalize_session (FlowTlsProtocol *tls_protocol)
{
  FlowTlsProtocolPrivate *priv = tls_protocol->priv;

  if (!priv->session_initialized)
    return;

  gnutls_deinit (priv->gnutls_session);
  global_unref_gnutls ();

  priv->session_initialized = FALSE;
}

static gboolean
begin_session (FlowTlsProtocol *tls_protocol, gint input_index)
{
  FlowTlsProtocolPrivate *priv    = tls_protocol->priv;
  FlowElement            *element = (FlowElement *) tls_protocol;

  initialize_session (tls_protocol);

  /* Upstream */

  if (input_index == UPSTREAM_INDEX)
  {
    if (priv->upstream_state != STATE_CLOSED)
    {
      /* Bogus FLOW_STREAM_BEGIN? */
      return FALSE;
    }

    priv->upstream_state   = STATE_OPEN;
    priv->downstream_state = STATE_HANDSHAKING;

    flow_pad_unblock (g_ptr_array_index (element->input_pads, DOWNSTREAM_INDEX));
    return FALSE;
  }

  /* Downstream */

  if (priv->downstream_state == STATE_CLOSED)
  {
    /* We have to block further input until the handshake is done, but the
     * FLOW_STREAM_BEGIN packed must be allowed through, so the physical connection
     * will be opened. */

    priv->downstream_state = STATE_HANDSHAKING;

    flow_pad_block (g_ptr_array_index (element->input_pads, UPSTREAM_INDEX));
  }

  return FALSE;
}

static gboolean
end_session (FlowTlsProtocol *tls_protocol, gint input_index)
{
  FlowTlsProtocolPrivate *priv = tls_protocol->priv;

  /* Upstream */

  if (input_index == UPSTREAM_INDEX)
  {
    if (priv->upstream_state == STATE_OPEN)
    {
      priv->upstream_state = STATE_CLOSED;

      if (priv->downstream_state == STATE_CLOSED)
      {
        finalize_session (tls_protocol);
        return FALSE;
      }

      priv->downstream_state = STATE_QUITTING;
      do_downstream_tasks (tls_protocol);
      return TRUE;
    }

    /* Bogus FLOW_STREAM_END? */
    return FALSE;
  }

  /* Downstream */

  if (priv->upstream_state == STATE_CLOSED)
    finalize_session (tls_protocol);

  priv->downstream_state = STATE_CLOSED;
  return FALSE;
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
     * or quit sequence. */

    if (priv->downstream_state == STATE_HANDSHAKING ||
        priv->downstream_state == STATE_QUITTING)
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
    input_index = DOWNSTREAM_INDEX;
  }
  else
  {
    /* We have to finish the handshake sequence before we can let upstream
     * send more data.
     *
     * We have to finish the quit sequence before we can let the FLOW_STREAM_END
     * through and close the connection. */

    if (priv->downstream_state == STATE_HANDSHAKING ||
        priv->downstream_state == STATE_QUITTING)
      return;

    input_index = UPSTREAM_INDEX;
  }

  flow_pad_unblock (g_ptr_array_index (element->input_pads, input_index));
}

static FlowPacket *
try_recv_from_gnutls (FlowTlsProtocol *tls_protocol)
{
  FlowTlsProtocolPrivate *priv = tls_protocol->priv;
  FlowPacket             *packet;
  guint8                  buf [MAX_READ];
  ssize_t                 result;

  result = gnutls_record_recv (priv->gnutls_session, buf, MAX_READ);
  if (result < 0)
  {
#if 0
    gnutls_perror (result);
#endif
    return NULL;
  }
  else if (result == 0)
  {
    if (priv->downstream_state != STATE_CLOSED)
    {
      gnutls_bye (priv->gnutls_session, SHUT_WR);
      priv->downstream_state = STATE_CLOSED;
    }

    return NULL;
  }

  packet = flow_packet_new (FLOW_PACKET_FORMAT_BUFFER, buf, result);
  return packet;
}

static gboolean
process_data (FlowTlsProtocol *tls_protocol)
{
  FlowTlsProtocolPrivate *priv                = tls_protocol->priv;
  FlowElement            *element             = (FlowElement *) tls_protocol;
  gboolean                processed_some_data = FALSE;
  FlowPacketQueue        *packet_queue;
  FlowPad                *input_pad;
  FlowPad                *output_pad;
  FlowPacket             *packet;
  gint                    packet_offset;
  ssize_t                 result;

  /* Push data going downstream */

  input_pad = g_ptr_array_index (element->input_pads, UPSTREAM_INDEX);

  packet_queue = flow_pad_get_packet_queue (input_pad);

  if (packet_queue)
  {
    for ( ; flow_packet_queue_peek_packet (packet_queue, &packet, &packet_offset); )
    {
      guint8 *data;
      gint    len;

      if (flow_packet_get_format (packet) != FLOW_PACKET_FORMAT_BUFFER)
        break;

      len  = flow_packet_get_size (packet) - packet_offset;
      data = flow_packet_get_data (packet) + packet_offset;

      result = gnutls_record_send (priv->gnutls_session, data, len);

      if (result < 1)
        break;

      processed_some_data = TRUE;

      if (result == len)
        flow_packet_queue_drop_packet (packet_queue);
      else
        flow_packet_queue_pop_bytes_exact (packet_queue, NULL, result);
    }
  }

  /* Pull data going upstream */

  output_pad = g_ptr_array_index (element->output_pads, UPSTREAM_INDEX);

  for ( ; (packet = try_recv_from_gnutls (tls_protocol)); )
  {
    processed_some_data = TRUE;
    flow_pad_push (output_pad, packet);
  }

  return processed_some_data;
}

static void
flow_tls_protocol_process_input (FlowElement *element, FlowPad *input_pad)
{
  FlowTlsProtocol        *tls_protocol = FLOW_TLS_PROTOCOL (element);
  FlowTlsProtocolPrivate *priv         = tls_protocol->priv;
  FlowPacketQueue        *packet_queue;
  FlowPacket             *packet;
  gint                    input_index;
  gint                    output_index;

  packet_queue = flow_pad_get_packet_queue (input_pad);
  if (!packet_queue)
    return;

  if (input_pad == g_ptr_array_index (element->input_pads, UPSTREAM_INDEX))
  {
    input_index  = UPSTREAM_INDEX;
    output_index = DOWNSTREAM_INDEX;
  }
  else
  {
    input_index  = DOWNSTREAM_INDEX;
    output_index = UPSTREAM_INDEX;
  }

  for ( ; flow_packet_queue_peek_packet (packet_queue, &packet, NULL); )
  {
    gboolean retain_packet = FALSE;

    if G_UNLIKELY (flow_packet_get_format (packet) == FLOW_PACKET_FORMAT_OBJECT)
    {
      gpointer object = flow_packet_get_data (packet);

      flow_handle_universal_events (element, packet);

      if (FLOW_IS_DETAILED_EVENT (object))
      {
        if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_BEGIN))
        {
          retain_packet = begin_session (tls_protocol, input_index);
        }
        else if (flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_END) ||
                 flow_detailed_event_matches (object, FLOW_STREAM_DOMAIN, FLOW_STREAM_DENIED))
        {
          retain_packet = end_session (tls_protocol, input_index);
        }
      }

      if (retain_packet)
      {
        flow_pad_block (input_pad);
        break;
      }
      else
      {
        flow_packet_queue_pop_packet (packet_queue);
        flow_pad_push (g_ptr_array_index (element->output_pads, output_index), packet);

        do_downstream_tasks (tls_protocol); /* ew */
      }
    }
    else
    {
      /* We have data */

      /* If TLS subsystem is not processing data packages (maybe because it's blocking
       * on data going in the other direction), break out of loop. However, we want to
       * process events if there are any at the head of the queue. */

      if (priv->downstream_state == STATE_OPEN)
      {
        if (!process_data (tls_protocol))
          break;
      }
      else if (input_index == DOWNSTREAM_INDEX)
      {
        do_downstream_tasks (tls_protocol);
      }
      else
      {
        break;
      }
    }
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

  priv->upstream_state   = STATE_CLOSED;
  priv->downstream_state = STATE_CLOSED;
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
