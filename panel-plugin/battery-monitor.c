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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <hal/libhal.h>

#include <panel-plugin/battery-monitor.h>
#include <libxfce4panel/xfce-panel-macros.h>

#define DEFAULT_LEVEL_LOW     (0.08)
#define DEFAULT_LEVEL_WARNING (0.20)


enum
{
  CHANGED,
  LAST_SIGNAL
};



static void         battery_monitor_class_init                         (BatteryMonitorClass *klass);
static void         battery_monitor_init                               (BatteryMonitor      *monior);
static void         battery_monitor_finalize                           (GObject             *object);
static BatteryInfo *battery_monitor_device_new                         (const gchar         *udi);
static void         battery_monitor_device_free                        (gpointer             user_data);
static void         battery_monitor_device_global_new                  (BatteryMonitor      *monitor);
static void         battery_monitor_device_global_free                 (BatteryMonitor      *monitor);
static void         battery_monitor_device_global_update               (BatteryMonitor      *monitor);
static void         battery_monitor_device_update_properties           (const gchar         *udi,
                                                                        BatteryInfo         *info,
                                                                        BatteryMonitor      *monitor);
static void         battery_monitor_device_property_modified           (LibHalContext       *context,
                                                                        const gchar         *udi,
                                                                        const gchar         *key,
                                                                        dbus_bool_t          is_removed,
                                                                        dbus_bool_t          is_added);
static void         battery_monitor_device_added                       (LibHalContext       *context,
                                                                        const gchar         *udi);
static void         battery_monitor_device_removed                     (LibHalContext       *context,
                                                                        const gchar         *udi);
static void         battery_monitor_devices_load                       (BatteryMonitor      *monitor);
static gboolean     battery_monitor_devices_update_idle                (gpointer             user_data);
static void         battery_monitor_devices_update_destroyed           (gpointer             user_data);



struct _BatteryMonitorClass
{
  GObjectClass __parent__;
};

struct _BatteryMonitor
{
  GObject __parent__;

  /* hash table with known devices */
  GHashTable     *devices;

  /* global battery information */
  BatteryInfo    *global;

  /*  dbus connection */
  DBusConnection *connection;

  /* hal context */
  LibHalContext  *context;

  /* idle update id */
  guint           idle_update_id;
};



static guint battery_monitor_signals[LAST_SIGNAL];



G_DEFINE_TYPE (BatteryMonitor, battery_monitor, G_TYPE_OBJECT);



static void
battery_monitor_class_init (BatteryMonitorClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = battery_monitor_finalize;

  /* setup signals for the monitor */
  battery_monitor_signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_NO_HOOKS,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
battery_monitor_init (BatteryMonitor *monitor)
{
  DBusError error;
  gboolean  status;

  /* initialize */
  monitor->context = NULL;
  monitor->connection = NULL;
  monitor->idle_update_id = 0;
  monitor->global = NULL;

  /* create hash table */
  monitor->devices = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, battery_monitor_device_free);

  /* initialize the error variable */
  dbus_error_init (&error);

  /* create new dbus connection */
  monitor->connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (G_UNLIKELY (monitor->connection == NULL))
    return;

  /* connect dbus to the main loop */
  dbus_connection_setup_with_g_main (monitor->connection, NULL);

  /* create new hal context */
  monitor->context = libhal_ctx_new ();
  if (G_UNLIKELY (monitor->context == NULL))
    return;

  /* add the monitor the the hal context */
  libhal_ctx_set_user_data (monitor->context, monitor);

  /* set the dbus connection on the context */
  status = libhal_ctx_set_dbus_connection (monitor->context, monitor->connection);
  if (G_UNLIKELY (status == FALSE))
    return;

  /* initialize the hal context */
  status = libhal_ctx_init (monitor->context, &error);
  if (G_UNLIKELY (status == FALSE))
    return;

  /* load battery devices */
  battery_monitor_devices_load (monitor);

  /* watch all device changes */
  status = libhal_device_property_watch_all (monitor->context, &error);
  if (G_UNLIKELY (status == FALSE))
    return;

  /* setup hal callbacks */
  libhal_ctx_set_device_property_modified (monitor->context, battery_monitor_device_property_modified);
  libhal_ctx_set_device_added (monitor->context, battery_monitor_device_added);
  libhal_ctx_set_device_removed (monitor->context, battery_monitor_device_removed);

  /* cleanup */
  LIBHAL_FREE_DBUS_ERROR (&error);
}



static void
battery_monitor_finalize (GObject *object)
{
  BatteryMonitor *monitor = BATTERY_MONITOR (object);
  DBusError       error;

  /* stop pending idle update */
  if (monitor->idle_update_id)
    g_source_remove (monitor->idle_update_id);

  /* free the contect */
  if (monitor->context)
    {
      /* initialize the error variable */
      dbus_error_init (&error);

      /* shutdown and free context */
      libhal_ctx_shutdown (monitor->context, &error);
      libhal_ctx_free (monitor->context);

      /* cleanup error */
      LIBHAL_FREE_DBUS_ERROR (&error);
    }

  /* release the dbus connection */
  if (monitor->connection)
    dbus_connection_unref (monitor->connection);

  /* remove global device */
  if (monitor->global)
    battery_monitor_device_free (monitor->global);

  /* free hash table */
  g_hash_table_destroy (monitor->devices);

  (*G_OBJECT_CLASS (battery_monitor_parent_class)->finalize) (object);
}



static gint
battery_monitor_calculate_percentage (BatteryInfo *info)
{
  gint percentage = 0;

  /* calculate the percentage if last full is valid */
  if (G_LIKELY (info->charge_last_full > 0))
    percentage = info->charge_current * 100 / info->charge_last_full;

  return CLAMP (percentage, 0, 100);
}



static void
battery_monitor_calculate (BatteryInfo *info)
{
  /* update the percentage */
  info->percentage = battery_monitor_calculate_percentage (info);
}



static BatteryInfo *
battery_monitor_device_new (const gchar *udi)
{
  BatteryInfo *info;

  /* create new slice */
  info = panel_slice_new0 (BatteryInfo);

  /* set device identifier */
  info->udi = g_strdup (udi);
  info->model = NULL;

  /* set defaults */
  info->charge_last_full = info->charge_low = info->charge_warning = 0;
  info->charge_current = 0;
  info->percentage = info->remaining_time = 0;
  info->is_discharging = info->is_present = FALSE;

  return info;
}



static void
battery_monitor_device_free (gpointer user_data)
{
  BatteryInfo *info = (BatteryInfo *) user_data;

  /* free identifier */
  g_free (info->udi);

  /* free model */
  g_free (info->model);

  /* free structure */
  panel_slice_free (BatteryInfo, info);
}



static void
battery_monitor_device_global_new (BatteryMonitor *monitor)
{
  g_return_if_fail (monitor->global == NULL);

  /* create a new global device */
  monitor->global = battery_monitor_device_new (GLOBAL_DEVICE_UDI);

  /* initialize */
  battery_monitor_device_global_update (monitor);
}



static void
battery_monitor_device_global_free (BatteryMonitor *monitor)
{
  g_return_if_fail (monitor->global != NULL);

  /* remove global device */
  battery_monitor_device_free (monitor->global);

  /* unset */
  monitor->global = NULL;
}



static void
battery_monitor_device_global_update (BatteryMonitor *monitor)
{
  GList       *devices, *li;
  BatteryInfo *info;
  BatteryInfo *global = monitor->global;

  g_return_if_fail (global != NULL);

  /* reset variables */
  global->charge_last_full = 0;
  global->charge_low = 0;
  global->charge_warning = 0;
  global->charge_current = 0;
  global->is_discharging = TRUE;
  global->is_present = FALSE;

  /* get all devices */
  devices = g_hash_table_get_values (monitor->devices);

  /* walk */
  for (li = devices; li != NULL; li = li->next)
    {
      info = li->data;

      /* append values */
      global->charge_last_full += info->charge_last_full;
      global->charge_low += info->charge_low;
      global->charge_warning += info->charge_warning;
      global->charge_current += info->charge_current;

      /* global is charging, when atleast one battery is charging */
      if (!info->is_discharging)
        global->is_discharging = FALSE;

      /* global is present, when one battery is pressent */
      if (info->is_present)
        global->is_present = TRUE;
    }

  /* cleanup */
  g_list_free (devices);

  /* update calculations */
  battery_monitor_calculate (global);
}



static void
battery_monitor_device_update_properties (const gchar    *udi,
                                          BatteryInfo    *info,
                                          BatteryMonitor *monitor)
{
  gint       current;
  gint       remaining_time;
  gchar     *model;
  DBusError  error;

  g_return_if_fail (g_str_equal (info->udi, udi));

  /* initialize error */
  dbus_error_init (&error);

  /* make sure the device exists */
  if (libhal_device_exists (monitor->context, udi, NULL))
    {
      /* update static battery information if needed */
      if (G_UNLIKELY (info->charge_last_full == 0))
        {
          /* get the last full capacity, leave when there is none, since it makes the device useless */
          if (libhal_device_property_exists (monitor->context, udi, "battery.charge_level.last_full", &error))
            info->charge_last_full = libhal_device_get_property_int (monitor->context, udi, "battery.charge_level.last_full", &error);
          else
            return;

          /* get the low warning capacity */
          if (libhal_device_property_exists (monitor->context, udi, "battery.charge_level.low", &error))
            info->charge_low = libhal_device_get_property_int (monitor->context, udi, "battery.charge_level.low", &error);
          else
            info->charge_low = info->charge_last_full * DEFAULT_LEVEL_LOW;

          /* get the warning capacity */
          if (libhal_device_property_exists (monitor->context, udi, "battery.charge_level.warning", &error))
            info->charge_warning = libhal_device_get_property_int (monitor->context, udi, "battery.charge_level.warning", &error);
          else
            info->charge_warning = info->charge_last_full * DEFAULT_LEVEL_WARNING;

          /* get device model */
          if (libhal_device_property_exists (monitor->context, udi, "battery.model", &error))
            {
              model = libhal_device_get_property_string (monitor->context, udi, "battery.model", &error);
              
              if (G_LIKELY (model))
                {
                  if (g_utf8_strlen (model, -1) > 0)
                    info->model = model;
                  else
                    g_free (model);
                }
            }
        }

      /* whether the battery is discharging */
      if (libhal_device_property_exists (monitor->context, udi, "battery.rechargeable.is_discharging", &error))
        info->is_discharging = libhal_device_get_property_bool (monitor->context, udi, "battery.rechargeable.is_discharging", &error);

      /* whether the battery is present */
      if (libhal_device_property_exists (monitor->context, udi, "battery.present", &error))
        info->is_present = libhal_device_get_property_bool (monitor->context, udi, "battery.present", &error);
      else
        info->is_present = FALSE;
        
      /* current remaining time */
      if (libhal_device_property_exists (monitor->context, udi, "battery.remaining_time", &error))
        {
          remaining_time = libhal_device_get_property_int (monitor->context, udi, "battery.remaining_time", &error);

          /* only set when valid */
          if (G_LIKELY (remaining_time > 0))
            info->remaining_time = remaining_time;
        }
      
      /* current change level */
      if (libhal_device_property_exists (monitor->context, udi, "battery.charge_level.current", &error))
        {
          current = libhal_device_get_property_int (monitor->context, udi, "battery.charge_level.current", &error);

          /* only set when valid */
          if (G_LIKELY (current > 0))
            info->charge_current = current;
            
          /* fix some weirdness for unplugged batteries but not properly removed inside hal */
          if (info->charge_current == 0)
            {
              info->remaining_time = 0;
              info->is_present = FALSE;
            }
        }

      /* update the global battery */
      if (G_UNLIKELY (monitor->global != NULL))
        {
          /* append to global current charge */
          monitor->global->charge_current += info->charge_current;

          /* when atleast one battery is pressent, global is present */
          if (info->is_present)
            monitor->global->is_present = TRUE;

          /* when one battery is charging, global is charging */
          if (!info->is_discharging && info->is_present)
            monitor->global->is_discharging = FALSE;
        }

      /* recalculate the battery status */
      battery_monitor_calculate (info);
    }

  /* free error */
  LIBHAL_FREE_DBUS_ERROR (&error);
}



static void
battery_monitor_device_property_modified (LibHalContext *context,
                                          const gchar   *udi,
                                          const gchar   *key,
                                          dbus_bool_t    is_removed,
                                          dbus_bool_t    is_added)
{
  BatteryMonitor *monitor = libhal_ctx_get_user_data (context);

  g_return_if_fail (BATTERY_IS_MONITOR (monitor));
  g_return_if_fail (monitor->context == context);

  /* check if we need to update */
  if (monitor->idle_update_id == 0 &&
      g_hash_table_lookup (monitor->devices, udi))
    {
      /* schedule an idle update */
      battery_monitor_devices_update (monitor);
    }
}



static void
battery_monitor_device_added (LibHalContext *context,
                              const gchar   *udi)
{
  BatteryMonitor *monitor;
  BatteryInfo    *info;
  DBusError       error;

  /* get the monitor */
  monitor = libhal_ctx_get_user_data (context);

  /* check context */
  g_return_if_fail (monitor->context == context);

  /* leave when this is a known device */
  if (G_UNLIKELY (g_hash_table_lookup (monitor->devices, udi)))
    return;

  /* initialize error */
  dbus_error_init (&error);

  /* check if the device has the battery capability */
  if (libhal_device_query_capability (context, udi, "battery", &error))
    {
      /* create new info for this device */
      info = battery_monitor_device_new (udi);

      /* load properties */
      battery_monitor_device_update_properties (udi, info, monitor);

      /* add device to the hash table */
      g_hash_table_insert (monitor->devices, info->udi, info);

      /* handle global battery */
      if (g_hash_table_size (monitor->devices) > 1)
        {
          if (monitor->global == NULL)
            {
              /* create a global battery */
              battery_monitor_device_global_new (monitor);
            }
          else
            {
              /* update the global battery */
              battery_monitor_device_global_update (monitor);
            }
        }

      /* emit signal */
      g_signal_emit (G_OBJECT (monitor), battery_monitor_signals[CHANGED], 0);
    }

  /* free error */
  LIBHAL_FREE_DBUS_ERROR (&error);
}



static void
battery_monitor_device_removed (LibHalContext *context,
                                const gchar   *udi)
{
  BatteryMonitor *monitor;
  BatteryInfo    *info;

  /* get the monitor */
  monitor = libhal_ctx_get_user_data (context);

  /* check context */
  g_return_if_fail (monitor->context == context);

  /* lookup the battery in the hash table */
  info = g_hash_table_lookup (monitor->devices, udi);

  /* battery was found */
  if (info)
    {
      /* remove the device from the table */
      g_hash_table_remove (monitor->devices, udi);

      /* handle the global battery */
      if (monitor->global != NULL)
        {
          if (g_hash_table_size (monitor->devices) < 2)
            {
              /* one or less batteries, remove the global battery */
              battery_monitor_device_global_free (monitor);
            }
          else
            {
              /* total update of the global battery */
              battery_monitor_device_global_update (monitor);
            }
        }

      /* emit signal */
      g_signal_emit (G_OBJECT (monitor), battery_monitor_signals[CHANGED], 0);
    }
}



static void
battery_monitor_devices_load (BatteryMonitor *monitor)
{
  gchar       **devices;
  gint          n, n_devices;
  BatteryInfo  *info;
  DBusError     error;

  /* initialize error */
  dbus_error_init (&error);

  /* get device names with battery capability */
  devices = libhal_find_device_by_capability (monitor->context, "battery", &n_devices, &error);

  /* return when no devices were found */
  if (devices != NULL)
    {
      /* add all the devices */
      for (n = 0; n < n_devices; n++)
        {
          /* create battery information */
          info = battery_monitor_device_new (devices[n]);

          /* load information */
          battery_monitor_device_update_properties (devices[n], info, monitor);

          /* add device to the hash table */
          g_hash_table_insert (monitor->devices, info->udi, info);
        }

      /* free names */
      libhal_free_string_array (devices);
    }

  /* setup an global battery when there are more then 2 batteries */
  if (n_devices > 1)
    battery_monitor_device_global_new (monitor);

  /* free error */
  LIBHAL_FREE_DBUS_ERROR (&error);
}



static gboolean
battery_monitor_devices_update_idle (gpointer user_data)
{
  BatteryMonitor *monitor = BATTERY_MONITOR (user_data);

  /* reset the global battery if needed */
  if (G_UNLIKELY (monitor->global))
    {
      monitor->global->charge_current = 0;
      monitor->global->is_discharging = TRUE;
      monitor->global->is_present = FALSE;
    }

  /* update all devices */
  g_hash_table_foreach (monitor->devices, (GHFunc) battery_monitor_device_update_properties, monitor);

  /* update the global battery */
  if (G_UNLIKELY (monitor->global))
    {
      /* update global calculations */
      battery_monitor_calculate (monitor->global);
    }

  /* emit signal */
  g_signal_emit (G_OBJECT (monitor), battery_monitor_signals[CHANGED], 0);

  return FALSE;
}



static void
battery_monitor_devices_update_destroyed (gpointer user_data)
{
  BATTERY_MONITOR (user_data)->idle_update_id = 0;
}



static gint
battery_monitor_sort_devices_func (gconstpointer a,
                                   gconstpointer b)
{
  const BatteryInfo *info_a = a;
  const BatteryInfo *info_b = b;

  return strcmp (info_a->udi, info_b->udi);
}



BatteryMonitor *
battery_monitor_get_default (void)
{
  static BatteryMonitor *monitor = NULL;

  if (monitor == NULL)
    {
      monitor = g_object_new (BATTERY_TYPE_MONITOR, NULL);
      g_object_add_weak_pointer (G_OBJECT (monitor), (gpointer) &monitor);
    }
  else
    {
      g_object_ref (G_OBJECT (monitor));
    }

  return monitor;
}



void
battery_monitor_devices_update (BatteryMonitor *monitor)
{
  g_return_if_fail (BATTERY_IS_MONITOR (monitor));

  if (monitor->idle_update_id == 0)
    {
      /* queue an idle update */
      monitor->idle_update_id = g_idle_add_full (G_PRIORITY_LOW, battery_monitor_devices_update_idle,
                                                 monitor, battery_monitor_devices_update_destroyed);
    }
}



gboolean
battery_monitor_has_global (BatteryMonitor *monitor)
{
  g_return_val_if_fail (BATTERY_IS_MONITOR (monitor), FALSE);

  return (monitor->global != NULL);
}



BatteryInfo *
battery_monitor_get_device (BatteryMonitor *monitor,
                            const gchar    *udi)
{
  g_return_val_if_fail (BATTERY_IS_MONITOR (monitor), NULL);
  g_return_val_if_fail (udi != NULL, NULL);

  if (G_UNLIKELY (monitor->global != NULL && strcmp (udi, GLOBAL_DEVICE_UDI) == 0))
    return monitor->global;
  else
    return g_hash_table_lookup (monitor->devices, udi);
}



GList *
battery_monitor_get_devices (BatteryMonitor *monitor)
{
  GList *devices;

  g_return_val_if_fail (BATTERY_IS_MONITOR (monitor), NULL);

  /* get the list of devices */
  devices = g_hash_table_get_values (monitor->devices);

  /* sort the list of devices */
  devices = g_list_sort (devices, battery_monitor_sort_devices_func);

  return devices;
}



gchar *
battery_monitor_get_first_device (BatteryMonitor *monitor)
{
  GList *devices;
  gchar *udi = NULL;

  g_return_val_if_fail (BATTERY_IS_MONITOR (monitor), NULL);

  /* return the global device if there is one */
  if (monitor->global != NULL)
    return g_strdup (GLOBAL_DEVICE_UDI);

  /* get devices */
  devices = battery_monitor_get_devices (monitor);

  /* pick the first device */
  if (g_list_length (devices) > 0)
    udi = g_strdup (((BatteryInfo *) (devices->data))->udi);

  /* cleanup */
  g_list_free (devices);

  return udi;
}
