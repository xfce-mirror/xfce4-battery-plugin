AM_CPPFLAGS =									\
	-I$(top_srcdir)							\
	-DG_LOG_DOMAIN=\"xfce4-battery-plugin\"	\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"	\
	$(PLATFORM_CPPFLAGS)

#
# Battery plugin
#
plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = libbattery.la

libbattery_la_SOURCES =						\
	battery.c								\
	libacpi.h								\
	libacpi.c						\
	libsmc.c

libbattery_la_CFLAGS =						\
	$(LIBXFCE4PANEL_CFLAGS)					\
	$(LIBXFCE4UI_CFLAGS)					\
	$(LIBXFCE4UTIL_CFLAGS)					\
	$(GTK_CFLAGS)							\
	$(PLATFORM_CFLAGS)

libbattery_la_LIBADD =						\
	$(LIBXFCE4PANEL_LIBS)					\
	$(LIBXFCE4UI_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)					\
	$(GTK_LIBS)

libbattery_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS)

if NETBSD
libbattery_la_LDFLAGS += -lprop -Wl,-rpath=/usr/X11R7/lib
endif

#
# Desktop file
#
desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = battery.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
$(desktop_DATA): $(desktop_in_files) Makefile
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = \
	$(desktop_in_files) \
	meson.build \
	$(NULL)

CLEANFILES = $(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
