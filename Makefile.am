ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =								\
	panel-plugin						\
	icons								\
	po

distclean-local:
	rm -rf *.cache *~

distuninstallcheck_listfiles =											\
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

EXTRA_DIST = 								\
	README.md								\
	meson.build								\
	po/meson.build							\
	xfce-revision.h.in 						\
	$(NULL)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:

