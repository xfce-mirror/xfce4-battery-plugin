# $Id$

plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-battery-plugin

xfce4_battery_plugin_SOURCES =					\
	battery.h						\
	battery.c						\
	libacpi.h						\
	libacpi.c						\
	libapm.h						\
	libapm.c

xfce4_battery_plugin_CFLAGS =					\
	-I$(top_srcdir)						\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"			\
	$(LIBXFCE4PANEL_CFLAGS)					\
	$(LIBXFCEGUI4_CFLAGS)					\
	$(LIBXFCE4UTIL_CFLAGS)					\
	$(GTK_CFLAGS)

xfce4_battery_plugin_LDFLAGS =					\
	$(LIBXFCE4PANEL_LIBS)					\
	$(LIBXFCEGUI4_LIBS)					\
	$(LIBXFCE4UTIL_LIBS)					\
	$(GTK_LIBS)

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = battmon.desktop.in.in
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
