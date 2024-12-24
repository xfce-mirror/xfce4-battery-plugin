dnl xfce4-battery-plugin - Battery monitor for the Xfce4 panel
dnl
dnl 2003      Benedikt Meurer <benny@xfce.org>
dnl 2006-2007 Nick Schermer <nick@xfce.org>
dnl 2010      Florian Rivoal <frivoal@xfce.org>
dnl 2012      Landry Breuil <landry@xfce.org>
dnl 2016-2019 Andre Miranda <andreldm@xfce.org>
dnl

dnl ***************************
dnl *** Version information ***
dnl ***************************
XDT_VERSION_INIT([1.1.6])

dnl ***************************
dnl *** Initialize autoconf ***
dnl ***************************
AC_COPYRIGHT([Copyright (c) 2003-2024 The Xfce development team. All rights reserved.])
AC_INIT([xfce4-battery-plugin], [xdt_version], [https://gitlab.xfce.org/panel-plugins/xfce4-battery-plugin], [xfce4-battery-plugin])
AC_REVISION([xdt_version_build])
AC_PREREQ([2.69])
AC_CANONICAL_TARGET()
AC_CONFIG_MACRO_DIRS([m4])

dnl ***************************
dnl *** Initialize automake ***
dnl ***************************
AM_INIT_AUTOMAKE([1.8 dist-bzip2 tar-ustar no-dist-gzip foreign])
AC_CONFIG_HEADERS([config.h])
AM_MAINTAINER_MODE()
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl ********************************
dnl *** Check for basic programs ***
dnl ********************************
AC_PROG_CC()
LT_PATH_LD()
AC_PROG_INSTALL()

dnl Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl **********************************
dnl *** Check for standard headers ***
dnl **********************************
AC_CHECK_HEADERS([dirent.h stdio.h stdlib.h unistd.h string.h fcntl.h \
                  ctype.h sys/stat.h sys/time.h sys/ioctl.h sys/types.h])

dnl ************************************
dnl *** Check for standard functions ***
dnl ************************************
AC_CHECK_FUNCS([sysctl])

dnl *******************************
dnl *** Platform specific flags ***
dnl *******************************
AC_CANONICAL_HOST
case $host_os in
    *netbsd*) netbsd=true;;
    *)  netbsd=false;;
esac
AM_CONDITIONAL([NETBSD], [test x$netbsd = xtrue])

dnl ******************************
dnl *** Check for i18n support ***
dnl ******************************
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GIO_UNIX], [gio-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [3.22.0])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.16.0])
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [4.16.0])
case "$target_os" in
    darwin*)
    LDFLAGS="-Wl,-dead_strip_dylibs -Wl,-framework,CoreFoundation -Wl,-framework,IOKit $LDFLAGS"
    ;;
esac

dnl ***********************************
dnl *** Check for debugging support ***
dnl ***********************************
XDT_FEATURE_DEBUG([xdt_debug_default])

AC_CONFIG_FILES([
Makefile
panel-plugin/Makefile
icons/Makefile
icons/16x16/Makefile
icons/22x22/Makefile
icons/24x24/Makefile
icons/32x32/Makefile
icons/scalable/Makefile
po/Makefile.in
])
AC_OUTPUT

dnl ***************************
dnl *** Print configuration ***
dnl ***************************
echo
echo "Build Configuration:"
echo
echo "* Debug Support:    $enable_debug"
echo
