AM_CFLAGS          = $(FLOW_CFLAGS)
LIBS               = $(FLOW_LIBS)

flowdatadir        = $(datadir)/flow

pkgconfigdir       = $(libdir)/pkgconfig
pkgconfig_DATA     = flow.pc

flow_headers_to_scan_for_enums = \
	flow-position.h \
	flow-shunt.h \
	flow-tls-protocol.h

EXTRA_DIST         = flow.pc.in
CLEANFILES         = flow.pc

lib_LTLIBRARIES    = libflow.la
libflow_la_LDFLAGS = -version-info 0:0:0

AM_CFLAGS += \
	-DG_LOG_DOMAIN=\"Flow\" \
	-D_REENTRANT \
	-I$(top_srcdir) \
	-I$(top_srcdir)/flow \
	-I$(top_builddir)/flow

flow_built_sources = \
	flow-marshallers.c \
	flow-marshallers.h \
	flow-enum-types.c \
	flow-enum-types.h

flow_built_headers = \
	flow-enum-types.h

libflow_la_SOURCES = \
	flow-anonymous-event.c \
	flow-bin.c \
	flow-duplex-element.c \
	flow-collector.c \
	flow-connector.c \
	flow-context-mgmt.c \
	flow-controller.c \
	flow-common.c \
	flow-demux.c \
	flow-destructor.c \
	flow-detailed-event.c \
	flow-element.c \
	flow-element-util.c \
	flow-emitter.c \
	flow-event.c \
	flow-event-codes.c \
	flow-file-connect-op.c \
	flow-file-connector.c \
	flow-file-io.c \
	flow-gerror-util.c \
	flow-gobject-util.c \
	flow-input-pad.c \
	flow-io.c \
	flow-ip-addr.c \
	flow-ip-processor.c \
	flow-ip-resolver.c \
	flow-ip-service.c \
	flow-joiner.c \
	flow-messages.c \
	flow-mux.c \
	flow-mux-event.c \
	flow-mux-serializer.c \
	flow-mux-deserializer.c \
	flow-network-util.c \
	flow-output-pad.c \
	flow-pack-util.c \
	flow-packet.c \
	flow-packet-queue.c \
	flow-pad.c \
	flow-position.c \
	flow-process-result.c \
	flow-property-event.c \
	flow-segment-request.c \
	flow-serializable.c \
	flow-shell-op.c \
	flow-shunt.c \
	flow-simplex-element.c \
	flow-splitter.c \
	flow-ssh-connect-op.c \
	flow-ssh-master.c \
	flow-ssh-master-registry.c \
	flow-ssh-runner.c \
	flow-stdio-connector.c \
	flow-tcp-connect-op.c \
	flow-tcp-connector.c \
	flow-tcp-io.c \
	flow-tcp-io-listener.c \
	flow-tcp-listener.c \
	flow-tls-protocol.c \
	flow-tls-tcp-io.c \
	flow-tls-tcp-io-listener.c \
	flow-udp-connect-op.c \
	flow-udp-connector.c \
	flow-udp-io.c \
	flow-user-adapter.c \
	flow-util.c \
	$(flow_built_sources)

pkginclude_HEADERS = \
	flow.h \
	flow-anonymous-event.h \
	flow-bin.h \
	flow-duplex-element.h \
	flow-collector.h \
	flow-connector.h \
	flow-context-mgmt.h \
	flow-controller.h \
	flow-demux.h \
	flow-destructor.h \
	flow-detailed-event.h \
	flow-element.h \
	flow-element-util.h \
	flow-emitter.h \
	flow-event.h \
	flow-event-codes.h \
	flow-file-connect-op.h \
	flow-file-connector.h \
	flow-file-io.h \
	flow-gerror-util.h \
	flow-input-pad.h \
	flow-io.h \
	flow-ip-addr.h \
	flow-ip-processor.h \
	flow-ip-resolver.h \
	flow-ip-service.h \
	flow-joiner.h \
	flow-messages.h \
	flow-mux.h \
	flow-mux-event.h \
	flow-mux-deserializer.h \
	flow-mux-serializer.h \
	flow-network-util.h \
	flow-output-pad.h \
	flow-pack-util.h \
	flow-packet.h \
	flow-packet-queue.h \
	flow-pad.h \
	flow-position.h \
	flow-process-result.h \
	flow-property-event.h \
	flow-segment-request.h \
	flow-serializable.h \
	flow-shell-op.h \
	flow-shunt.h \
	flow-simplex-element.h \
	flow-splitter.h \
	flow-ssh-connect-op.h \
	flow-ssh-master.h \
	flow-ssh-master-registry.h \
	flow-ssh-runner.h \
	flow-stdio-connector.h \
	flow-tcp-connect-op.h \
	flow-tcp-connector.h \
	flow-tcp-io.h \
	flow-tcp-io-listener.h \
	flow-tcp-listener.h \
	flow-tls-protocol.h \
	flow-tls-tcp-io.h \
	flow-tls-tcp-io-listener.h \
	flow-udp-connect-op.h \
	flow-udp-connector.h \
	flow-udp-io.h \
	flow-user-adapter.h \
	flow-util.h

noinst_HEADERS = \
	flow-gobject-util.h \
	flow-common-impl-unix.h

stamp_files = stamp-flow-marshallers.h
gen_sources = xgen-fmlh xgen-fmlc

CLEANFILES += $(gen_sources)
MAINTAINERCLEANFILES = $(flow_built_sources) $(stamp_files)
BUILT_SOURCES = $(flow_built_sources)
EXTRA_DIST += \
	$(flow_built_sources) \
	flow-marshallers.list \
	flow-enum-headers.h \
	flow-common-impl-unix.c \
	flow-ip-resolver-impl-unix.c \
	flow-network-util-impl-unix.c \
	flow-shunt-impl-unix.c

# Don't build the library until we have built the header that it needs:
$(OBJECTS) $(libflow_la_OBJECTS): $(flow_built_headers)

flow-marshallers.h: stamp-flow-marshallers.h
	@true
stamp-flow-marshallers.h: @REBUILD@ flow-marshallers.list
	$(GLIB_GENMARSHAL) --prefix=flow_marshal $(srcdir)/flow-marshallers.list --header >> xgen-fmlh \
	&& (cmp -s xgen-fmlh flow-marshallers.h || cp xgen-fmlh flow-marshallers.h) \
	&& rm -f xgen-fmlh \
	&& echo timestamp > $(@F)
flow-marshallers.c: @REBUILD@ flow-marshallers.list
	echo '#include "flow-marshallers.h"' >xgen-fmlc && \
	$(GLIB_GENMARSHAL) --prefix=flow_marshal $(srcdir)/flow-marshallers.list --body >> xgen-fmlc \
	&& cp xgen-fmlc flow-marshallers.c \
	&& rm -f xgen-fmlc

flow-enum-types.h: @REBUILD@ $(flow_headers_to_scan_for_enums) Makefile
	(cd $(srcdir) && glib-mkenums \
			--fhead "#ifndef _FLOW_ENUM_TYPES_H\n#define _FLOW_ENUM_TYPES_H\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* Enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define FLOW_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" 	\
			--ftail "G_END_DECLS\n\n#endif /* _FLOW_ENUM_TYPES_H */" \
		$(flow_headers_to_scan_for_enums)) > $@

flow-enum-types.c: @REBUILD@ $(flow_headers_to_scan_for_enums) Makefile
	(cd $(srcdir) && glib-mkenums \
			--fhead "#include \"config.h\"\n" \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"flow-enum-headers.h\"\n" \
			--fhead "#include \"flow-enum-types.h\"\n" \
		      	--fprod "\n/* Enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(flow_headers_to_scan_for_enums)) > $@

# If srcdir!=builddir, clean out maintainer-clean files from builddir.
# This allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi
