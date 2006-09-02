dnl configure.ac
dnl
dnl xfce4-battery-plugin - Battery monitor for the Xfce4 panel
dnl
dnl 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
dnl

m4_define([battmon_version], [0.4.0])

AC_INIT([xfce4-battery-plugin], [battmon_version()],
	[xfce-goodies-dev@lists.berlios.de])


BATTMON_VERSION=battmon_version()
AM_INIT_AUTOMAKE([xfce4-battery-plugin], [$BATTMON_VERSION])
AM_CONFIG_HEADER([config.h])

AM_MAINTAINER_MODE

dnl Check for UNIX variants
AC_AIX
AC_ISC_POSIX
AC_MINIX

dnl Check for basic programs
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_INTLTOOL

dnl Check for standard header files
AC_HEADER_STDC
AC_CHECK_FUNCS([sysctl])


dnl configure the panel plugin
XDT_CHECK_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-1.0], [4.3.20])
XDT_I18N([cs])

dnl Check for debugging support
XDT_FEATURE_DEBUG()

AC_OUTPUT([
Makefile
panel-plugin/Makefile
po/Makefile.in
])
