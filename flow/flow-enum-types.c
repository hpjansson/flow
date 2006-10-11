
/* Generated data (by glib-mkenums) */

#include "config.h"
#include <glib-object.h>
#include "flow-enum-headers.h"
#include "flow-enum-types.h"

/* Enumerations from "flow-position.h" */
GType
flow_offset_anchor_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { FLOW_OFFSET_ANCHOR_CURRENT, "FLOW_OFFSET_ANCHOR_CURRENT", "current" },
      { FLOW_OFFSET_ANCHOR_BEGIN, "FLOW_OFFSET_ANCHOR_BEGIN", "begin" },
      { FLOW_OFFSET_ANCHOR_END, "FLOW_OFFSET_ANCHOR_END", "end" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("FlowOffsetAnchor", values);
  }
  return etype;
}

/* Generated data ends here */

