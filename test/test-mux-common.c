/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* test-mux-common.c - Utilities for Mux/Demux testing.
 *
 * Copyright (C) 2007 Andreas Rottmann
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
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * Authors: Andreas Rottmann <a.rottmann@gmx.at>
 */

static void
check_user_adapter_packets (FlowUserAdapter *adapter,
                            GList *expected_packets)
{
  GList *node;
  FlowPacketQueue *input_packet_queue;
  
  input_packet_queue = flow_user_adapter_get_input_queue (adapter);
  for (node = expected_packets; node; node = node->next)
  {
    FlowPacket *qpacket, *epacket;
    
    epacket = (FlowPacket *) node->data;
    qpacket = flow_packet_queue_pop_packet (input_packet_queue);
    while (qpacket == NULL)
    {
      flow_user_adapter_wait_for_input (adapter);
      qpacket = flow_packet_queue_pop_packet (input_packet_queue);
    }
    if (qpacket->format != epacket->format)
      test_end (TEST_RESULT_FAILED, "packet got wrong format");
    
    if (qpacket->format == FLOW_PACKET_FORMAT_BUFFER)
    {
      if (qpacket->size != epacket->size)
        test_end (TEST_RESULT_FAILED, "packet got wrong size");
      if (memcmp (flow_packet_get_data (qpacket),
                  flow_packet_get_data (epacket),
                  qpacket->size) != 0)
        test_end (TEST_RESULT_FAILED, "packet contents are bad");
    }
    else
    {
      GObject *qobj = G_OBJECT (flow_packet_get_data (qpacket));
      GObject *eobj = G_OBJECT (flow_packet_get_data (epacket));
      
      if (G_OBJECT_GET_CLASS (qobj) != G_OBJECT_GET_CLASS (eobj))
        test_end (TEST_RESULT_FAILED, "class mismatch for object packet");

      /* TODO: Test "eobj == qobj" */
    }

    flow_packet_free (epacket);
    flow_packet_free (qpacket);
  }
}

