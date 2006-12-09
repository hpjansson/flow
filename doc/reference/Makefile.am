## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=flow

# The top-level SGML file.
DOC_MAIN_SGML_FILE=flow-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../flow

HFILE_GLOB=$(top_srcdir)/flow/*.h
CFILE_GLOB=$(top_srcdir)/flow/*.c

# Headers to ignore
IGNORE_HFILES=

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.

INCLUDES = \
	-I$(srcdir) 			\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/flow 		\
	-I$(top_builddir) 		\
	-I$(top_builddir)/flow 		\
	$(FLOW_CFLAGS)

GTKDOC_LIBS = \
	$(top_builddir)/flow/libflow.la	\
	$(FLOW_LIBS)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = version.xml

# Images to copy into HTML directory
HTML_IMAGES =

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS =

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += \
	version.xml.in
