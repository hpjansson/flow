/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* flow-messages.c - Default human-readable messages for events.
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

#include "flow-event-codes.h"
#include "flow-messages.h"

typedef struct
{
  const gint         code;
  const gchar const *text;
}
FlowMessage;

typedef struct
{
  const gchar       const *domain_name;  /* Will be replaced */
  const FlowMessage const *messages;
}
FlowMessageDomain;

static const FlowMessage stream_messages [] =
{
  { FLOW_STREAM_BEGIN,            "Stream opened"                            },
  { FLOW_STREAM_END,              "Stream closed"                            },
  { FLOW_STREAM_DENIED,           "Failed to acquire stream"                 },

  { FLOW_STREAM_SEGMENT_BEGIN,    "Stream segment began"                     },
  { FLOW_STREAM_SEGMENT_END,      "Stream segment ended"                     },
  { FLOW_STREAM_SEGMENT_DENIED,   "Stream segment unavailable"               },

  { FLOW_STREAM_FLUSH,            "Stream flushed"                           },

  { FLOW_STREAM_ERROR,            "An error occurred"                        },
  { FLOW_STREAM_APP_ERROR,        "An application error occurred"            },
  { FLOW_STREAM_PHYSICAL_ERROR,   "A physical error occurred"                },
  { FLOW_STREAM_RESOURCE_ERROR,   "Lack of resources"                        },
  { -1,                           NULL                                       }
};

static const FlowMessage file_messages [] =
{
  { FLOW_FILE_PERMISSION_DENIED, "Permission denied"                         },
  { FLOW_FILE_IS_NOT_A_FILE,     "Not a file"                                },
  { FLOW_FILE_TOO_MANY_LINKS,    "Unresolvable links"                        },
  { FLOW_FILE_OUT_OF_HANDLES,    "Ran out of file handles"                   },
  { FLOW_FILE_PATH_TOO_LONG,     "The path is too long"                      },
  { FLOW_FILE_NO_SPACE,          "No space left on device"                   },
  { FLOW_FILE_IS_READ_ONLY,      "The file cannot be written to"             },
  { FLOW_FILE_IS_LOCKED,         "The file is in use by another program"     },
  { FLOW_FILE_DOES_NOT_EXIST,    "The file does not exist"                   },
  { -1,                          NULL                                        }
};

static FlowMessageDomain builtin_domains [] =
{
  { FLOW_STREAM_DOMAIN,           stream_messages             },
  { FLOW_FILE_DOMAIN,             file_messages               },
  { NULL,                         NULL                        }
};

const gchar *
flow_get_event_message (const gchar *domain, gint code)
{
  static gboolean    interned_domains = FALSE;
  const FlowMessage *messages         = NULL;
  const gchar       *message          = NULL;
  gint               i;

  g_return_val_if_fail (domain != NULL, NULL);

  /* Make sure our domain names are interned for fast comparison */

  if G_UNLIKELY (!interned_domains)
  {
    for (i = 0; builtin_domains [i].messages != NULL; i++)
    {
      builtin_domains [i].domain_name = g_intern_static_string (builtin_domains [i].domain_name);
    }

    interned_domains = TRUE;
  }

  /* Find the domain */

  domain = g_intern_string (domain);

  for (i = 0; builtin_domains [i].messages != NULL; i++)
  {
    if (domain == builtin_domains [i].domain_name)
    {
      messages = builtin_domains [i].messages;
      break;
    }
  }

  if (!messages)
    return NULL;

  /* Find the message */

  for (i = 0; messages [i].text != NULL; i++)
  {
    if (messages [i].code == code)
    {
      message = messages [i].text;
      break;
    }
  }

  /* NOTE: We can call gettext here for i18n. */

  return message;
}
