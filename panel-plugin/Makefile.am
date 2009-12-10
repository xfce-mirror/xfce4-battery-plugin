# $Id$

INCLUDES =								\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"xfce4-battery-plugin\"				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DI_KNOW_THE_DEVICEKIT_POWER_API_IS_SUBJECT_TO_CHANGE		\
	$(PLATFORM_CPPFLAGS)

#
# Battery plugin
#
plugindir =								\
	$(libexecdir)/xfce4/panel-plugins

plugin_PROGRAMS =							\
	xfce4-battery-plugin

xfce4_battery_plugin_SOURCES =						\
	batt-plugin.c							\
	interface_ui.h

xfce4_battery_plugin_CFLAGS =						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(GMODULE_CFLAGS)						\
	$(DKP_CFLAGS)							\
	$(PLATFORM_CFLAGS)

xfce4_battery_plugin_LDADD =						\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCEGUI4_LIBS)						\
	$(GMODULE_LIBS)							\
	$(DKP_LIBS)

interface = 								\
	settings.ui

interface_h =								\
	$(interface:.ui=_ui.h)

if MAINTAINER_MODE
BUILT_SOURCES = $(interface_h)

settings_ui.h: settings.ui
	exo-csource --static --strip-comments --strip-content --name=settings_ui $< > $@

endif

#
# Desktop file
#
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = xfce4-battery-plugin.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =                                    \
	$(interface)                           	\
	$(interface_h)				\
	$(desktop_in_in_files)

CLEANFILES =					\
	$(interface_h)				\
	$(desktop_in_files)			\
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
