# $Id$

SUBDIRS =								\
	panel-plugin							\
	icons								\
	po

AUTOMAKE_OPTIONS =							\
	dist-bzip2

distclean-local:
	rm -rf *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz

EXTRA_DIST =								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in						\
	$(desktop_in_in_files)

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update
