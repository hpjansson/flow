/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-file-io.h - A prefab I/O class for local file connections.
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

#ifndef _FLOW_FILE_IO_H
#define _FLOW_FILE_IO_H

#include <flow/flow-detailed-event.h>
#include <flow/flow-io.h>
#include <flow/flow-file-connector.h>
#include <flow/flow-position.h>

G_BEGIN_DECLS

#define FLOW_TYPE_FILE_IO            (flow_file_io_get_type ())
#define FLOW_FILE_IO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), FLOW_TYPE_FILE_IO, FlowFileIO))
#define FLOW_FILE_IO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), FLOW_TYPE_FILE_IO, FlowFileIOClass))
#define FLOW_IS_FILE_IO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), FLOW_TYPE_FILE_IO))
#define FLOW_IS_FILE_IO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), FLOW_TYPE_FILE_IO))
#define FLOW_FILE_IO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), FLOW_TYPE_FILE_IO, FlowFileIOClass))
GType   flow_file_io_get_type        (void) G_GNUC_CONST;

typedef struct _FlowFileIO      FlowFileIO;
typedef struct _FlowFileIOClass FlowFileIOClass;

struct _FlowFileIO
{
  FlowIO            parent;

  /* --- Private --- */

  gpointer          priv;
};

struct _FlowFileIOClass
{
  FlowIOClass parent_class;

  /* Padding for future expansion */

  void (*_pad_1) (void);
  void (*_pad_2) (void);
  void (*_pad_3) (void);
  void (*_pad_4) (void);
};

FlowFileIO         *flow_file_io_new                   (void);

void                flow_file_io_open                  (FlowFileIO *file_io, const gchar *path, FlowAccessMode access_mode);
void                flow_file_io_create                (FlowFileIO *file_io, const gchar *path, FlowAccessMode access_mode,
                                                        gboolean replace_existing,
                                                        FlowAccessMode create_mode_user,
                                                        FlowAccessMode create_mode_group,
                                                        FlowAccessMode create_mode_others);
void                flow_file_io_close                 (FlowFileIO *file_io);
void                flow_file_io_seek                  (FlowFileIO *file_io, FlowOffsetAnchor anchor, goffset offset);
void                flow_file_io_seek_to               (FlowFileIO *file_io, goffset offset);

gboolean            flow_file_io_sync_open             (FlowFileIO *file_io, const gchar *path, FlowAccessMode access_mode);
gboolean            flow_file_io_sync_create           (FlowFileIO *file_io, const gchar *path, FlowAccessMode access_mode,
                                                        gboolean replace_existing,
                                                        FlowAccessMode create_mode_user,
                                                        FlowAccessMode create_mode_group,
                                                        FlowAccessMode create_mode_others);
void                flow_file_io_sync_close            (FlowFileIO *file_io);

gchar              *flow_file_io_get_path              (FlowFileIO *file_io);
FlowConnectivity    flow_file_io_get_connectivity      (FlowFileIO *file_io);
FlowConnectivity    flow_file_io_get_last_connectivity (FlowFileIO *file_io);

FlowFileConnector  *flow_file_io_get_file_connector    (FlowFileIO *io);
void                flow_file_io_set_file_connector    (FlowFileIO *io, FlowFileConnector *file_connector);

G_END_DECLS

#endif  /* _FLOW_FILE_IO_H */
