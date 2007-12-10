/* $Id$ */
/*
 * Copyright (c) 2006-2007 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __BATTERY_PLUGIN_H__
#define __BATTERY_PLUGIN_H__

#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4util/libxfce4util.h>
#include <panel-plugin/battery-monitor.h>

G_BEGIN_DECLS

typedef struct _BatteryPlugin BatteryPlugin;

struct _BatteryPlugin
{
  /* the panel plugin */
  XfcePanelPlugin  *panel_plugin;

  /* the monitor */
  BatteryMonitor   *monitor;

#if !GTK_CHECK_VERSION(2,12,0)
  /* tooltip */
  GtkTooltips      *tooltips;
#endif

  /* panel widgets */
  GtkWidget        *ebox;
  GtkWidget        *frame;
  GtkWidget        *box;
  GtkWidget        *icon;
  GtkWidget        *progressbar;
  GtkWidget        *label;

  /* current icon name */
  gchar            *status_icon_name;

  /* visible battery udi */
  gchar            *visible_device;

  /* settings */
  guint             show_frame : 1;
  guint             show_status_icon : 1;
  guint             show_progressbar : 1;
  guint             show_label : 1;
};



void battery_plugin_update (BatteryPlugin *plugin);

G_END_DECLS

#endif /* !__BATTERY_PLUGIN_H__ */
