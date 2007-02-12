SUBDIRS = flow test doc

EXTRA_DIST =                \
        flow.spec           \
        flow.spec.in

BUILT_EXTRA_DIST =          \
        gtk-doc.make

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

check-local:
	@echo
	@echo '*** '
	@echo '*** Running tests'
	@echo '*** '
	@echo
	@for i in `cat $(top_srcdir)/test/tests.list`; \
  do \
    if ! $(top_builddir)/test/$$i; then \
      exit 1; \
    fi; \
  done
	@echo 
