/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-io.h - A prefab I/O class based on a bin of processing elements.
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

#ifndef _FLOW_IO_H
#define _FLOW_IO_H

#include <flow/flow-bin.h>
#include <flow/flow-user-adapter.h>

G_BEGIN_DECLS

#define FLOW_TYPE_IO            (flow_io_get_type ())
#define FLOW_IO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_IO, FlowIO))
#define FLOW_IO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_IO, FlowIOClass))
#define FLOW_IS_IO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_IO))
#define FLOW_IS_IO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_IO))
#define FLOW_IO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_IO, FlowIOClass))
GType   flow_io_get_type        (void) G_GNUC_CONST;

typedef struct _FlowIO      FlowIO;
typedef struct _FlowIOClass FlowIOClass;

struct _FlowIO
{
  FlowBin          parent;

  /* --- Protected --- */

  guint            read_stream_is_open     : 1;
  guint            write_stream_is_open    : 1;
  guint            need_to_check_bin       : 1;
  guint            drop_read_data          : 1;

  GError          *error;

  /* --- Private --- */

  gpointer         priv;
};

struct _FlowIOClass
{
  FlowBinClass parent_class;

  /* Methods */

  void     (*check_bin)           (FlowIO *io);
  gboolean (*handle_input_object) (FlowIO *io, gpointer object);
  void     (*prepare_read)        (FlowIO *io, gint request_len);
  void     (*successful_read)     (FlowIO *io, gint len);
  void     (*prepare_write)       (FlowIO *io, gint request_len);

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowIO          *flow_io_new               (void);

gint             flow_io_read              (FlowIO *io, gpointer dest_buffer, gint max_len);
gboolean         flow_io_read_exact        (FlowIO *io, gpointer dest_buffer, gint exact_len);
gpointer         flow_io_read_object       (FlowIO *io);
void             flow_io_write             (FlowIO *io, gpointer src_buffer, gint exact_len);
void             flow_io_write_object      (FlowIO *io, gpointer object);
void             flow_io_flush             (FlowIO *io);

void             flow_io_set_read_notify   (FlowIO *io, FlowNotifyFunc func, gpointer user_data);
void             flow_io_set_write_notify  (FlowIO *io, FlowNotifyFunc func, gpointer user_data);

void             flow_io_block_reads       (FlowIO *io);
void             flow_io_unblock_reads     (FlowIO *io);

void             flow_io_block_writes      (FlowIO *io);
void             flow_io_unblock_writes    (FlowIO *io);

gint             flow_io_sync_read         (FlowIO *io, gpointer dest_buffer, gint max_len,
                                            GError **error);
gboolean         flow_io_sync_read_exact   (FlowIO *io, gpointer dest_buffer, gint exact_len,
                                            GError **error);
gpointer         flow_io_sync_read_object  (FlowIO *io, GError **error);
gboolean         flow_io_sync_write        (FlowIO *io, gpointer src_buffer, gint exact_len,
                                            GError **error);
gboolean         flow_io_sync_write_object (FlowIO *io, gpointer object, GError **error);
gboolean         flow_io_sync_flush        (FlowIO *io, GError **error);

FlowUserAdapter *flow_io_get_user_adapter  (FlowIO *io);
void             flow_io_set_user_adapter  (FlowIO *io, FlowUserAdapter *user_adapter);

void             flow_io_check_bin         (FlowIO *io);
void             flow_io_check_events      (FlowIO *io);

G_END_DECLS

#endif  /* _FLOW_IO_H */
