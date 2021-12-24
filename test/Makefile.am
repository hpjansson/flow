noinst_PROGRAMS = \
	benchmark-propagation \
	test-file-io \
	test-ip-resolver \
	test-mux \
	test-mux-serializer \
	test-mux-deserializer \
	test-demux \
	test-packet \
	test-packet-queue \
	test-serializable \
	test-shunt-complex-file \
	test-shunt-process \
	test-shunt-simple-file \
	test-shunt-simple-tcp \
	test-shunt-simple-udp \
	test-tcp-io \
	test-tls-tcp-io

AM_LDFLAGS = $(top_builddir)/flow/libflow.la
AM_CFLAGS  = $(FLOW_CFLAGS) -I$(top_srcdir)/flow -I$(top_builddir)/flow -I$(top_srcdir)

EXTRA_DIST = \
	benchmark-common.c \
	test-mux-common.c \
	test-common.c \
	stress.sh \
	tests.list
