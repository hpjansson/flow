
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

/* Enumerations from "flow-tls-protocol.h" */
GType
flow_agent_role_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { FLOW_AGENT_ROLE_SERVER, "FLOW_AGENT_ROLE_SERVER", "server" },
      { FLOW_AGENT_ROLE_CLIENT, "FLOW_AGENT_ROLE_CLIENT", "client" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("FlowAgentRole", values);
  }
  return etype;
}

/* Generated data ends here */

