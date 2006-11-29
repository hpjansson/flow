noinst_PROGRAMS = \
	benchmark-propagation \
	test-ip-resolver \
	test-packet \
	test-packet-queue \
	test-shunt-complex-file \
	test-shunt-process \
	test-shunt-simple-file \
	test-shunt-simple-tcp \
	test-shunt-simple-udp \
	test-tcp-io \
	test-tls-tcp-io

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/flow \
	-I$(top_builddir)/flow

AM_LDFLAGS = $(top_builddir)/flow/libflow.la
AM_CFLAGS  = $(FLOW_CFLAGS) -I$(top_srcdir)/flow

EXTRA_DIST = \
	benchmark-common.c \
	test-common.c \
	stress.sh \
	tests.list
