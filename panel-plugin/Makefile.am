plugindir = @XFCE4_PANEL_PLUGINSDIR@

plugin_LTLIBRARIES =							\
	libbattmon.la

libbattmon_la_LDFLAGS =							\
	-avoid-version							\
	-export-dynamic							\
	-module

libbattmon_la_SOURCES =							\
	apmlib.c							\
	apmlib.h							\
	acpi-linux.c							\
	acpi-linux.h							\
	battmon.c

libbattmon_la_CFLAGS =							\
	@XFCE4_PANEL_CFLAGS@

libbattmon_la_LIBADD = 							\
	@XFCE4_PANEL_LIBS@
