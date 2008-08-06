/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-gerror-util.h - GError utilities.
 *
 * Copyright (C) 2008 Hans Petter Jansson
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
#include <stdarg.h>
#include "flow-gerror-util.h"

GError *
flow_gerror_from_detailed_event (FlowDetailedEvent *detailed_event, const gchar *domain, ...)
{
  GError  *error = NULL;
  va_list  var_args;
  gint     code;

  g_return_val_if_fail (FLOW_IS_DETAILED_EVENT (detailed_event), NULL);
  g_return_val_if_fail (domain != NULL, NULL);

  va_start (var_args, domain);

  for (code = va_arg (var_args, gint); code >= 0; code = va_arg (var_args, gint))
  {
    if (flow_detailed_event_matches (detailed_event, domain, code))
    {
      GQuark quark = g_quark_from_static_string (domain);

      error = g_error_new_literal (quark, code, flow_event_get_description (FLOW_EVENT (detailed_event)));
      break;
    }
  }

  va_end (var_args);
  return error;
}
