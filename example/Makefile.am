noinst_PROGRAMS = \
	list-interfaces \
	multi-target-cp \
	ssh-runner

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/flow \
	-I$(top_builddir)/flow

AM_LDFLAGS = $(top_builddir)/flow/libflow.la
AM_CFLAGS  = $(FLOW_CFLAGS) -I$(top_srcdir)/flow
