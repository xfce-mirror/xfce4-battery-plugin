# $Id$

INCLUDES =								\
	-I$(top_builddir)						\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"xfce4-battery-plugin\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"					\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	$(PLATFORM_CPPFLAGS)

pluginsdir = 								\
	$(libexecdir)/xfce4/panel-plugins

plugins_PROGRAMS =							\
	xfce4-battery-plugin

xfce4_battery_plugin_SOURCES = 						\
	battery-monitor.h						\
	battery-monitor.c						\
	battery-plugin.h						\
	battery-plugin.c						\
	battery-preferences.h						\
	battery-preferences.c

xfce4_battery_plugin_CFLAGS = 						\
	$(DBUS_CFLAGS)							\
	$(HAL_CFLAGS)							\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(PLATFORM_CFLAGS)

xfce4_battery_plugin_LDADD = 						\
	$(DBUS_LIBS)							\
	$(HAL_LIBS)							\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCEGUI4_LIBS)						\
	$(LIBXFCE4PANEL_LIBS)						\
	$(PLATFORM_LDFLAGS)

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = battery-monitor.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_in_files)

CLEANFILES =								\
	$(desktop_in_files)						\
	$(desktop_DATA)
