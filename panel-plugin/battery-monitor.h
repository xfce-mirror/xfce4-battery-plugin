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

#ifndef __BATTERY_MONITOR_H__
#define __BATTERY_MONITOR_H__

G_BEGIN_DECLS

typedef struct _BatteryMonitorClass BatteryMonitorClass;
typedef struct _BatteryMonitor      BatteryMonitor;
typedef struct _BatteryInfo         BatteryInfo;



#define GLOBAL_DEVICE_UDI ("/org/xfce/Global")

#define BATTERY_TYPE_MONITOR            (battery_monitor_get_type ())
#define BATTERY_MONITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), BATTERY_TYPE_MONITOR, BatteryMonitor))
#define BATTERY_MONITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), BATTERY_TYPE_MONITOR, BatteryMonitorClass))
#define BATTERY_IS_MONITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BATTERY_TYPE_MONITOR))
#define BATTERY_IS_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), BATTERY_TYPE_MONITOR))
#define BATTERY_MONITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), BATTERY_TYPE_MONITOR, BatteryMonitorClass))

#define BATTERY_FULLY_CHARGED(info)     ((info)->is_discharging == FALSE && (info)->charge_current == (info)->charge_last_full)



struct _BatteryInfo
{
  /* device identifier */
  gchar    *udi;

  /* device model */
  gchar    *model;

  /* last update time */
  GTimeVal  time;

  /* charge levels */
  gint      charge_last_full;
  gint      charge_low;
  gint      charge_warning;
  gint      charge_current;
  gint      charge_current_prev;

  /* discharge rate */
  gint      rate_discharging;
  gint      rate_charging;

  /* status */
  guint     is_discharging : 1;
  guint     is_present : 1;

  /* calculations */
  gint      percentage;
  gint      remaining_time;
};



GType           battery_monitor_get_type         (void) G_GNUC_CONST;

gfloat          battery_monitor_polyval          (gfloat          x,
                                                  gdouble        *coef);

BatteryMonitor *battery_monitor_get_default      (void) G_GNUC_MALLOC;

BatteryInfo    *battery_monitor_get_device       (BatteryMonitor *monitor,
                                                  const gchar    *udi);

void            battery_monitor_devices_update   (BatteryMonitor *monitor);

gboolean        battery_monitor_has_global       (BatteryMonitor *monitor);

GList          *battery_monitor_get_devices      (BatteryMonitor *monitor) G_GNUC_MALLOC;

gchar          *battery_monitor_get_first_device (BatteryMonitor *monitor) G_GNUC_MALLOC;

G_END_DECLS

#endif /* !__BATTERY__PREFERENCES_H__ */
