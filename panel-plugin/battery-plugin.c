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

#include <gtk/gtk.h>
#include <libxfce4panel/xfce-hvbox.h>

#include <panel-plugin/battery-plugin.h>
#include <panel-plugin/battery-preferences.h>

#define PROGRESS_BAR_THINKNESS (8)
#define PANEL_PADDING          (1)
#define FRAME_PADDING          (1)
#define SPACING                (2)



static gchar *
battery_plugin_get_icon_name (BatteryInfo *info)
{
  gchar       *icon_name;
  const gchar *icon_number;

  if (info->is_present == FALSE)
    {
      /* no icon present */
      icon_name = g_strdup ("ac-adapter");
    }
  else if (BATTERY_FULLY_CHARGED (info))
    {
      /* battery is fully charged */
      icon_name = g_strdup ("battery-charged");
    }
  else
    {
      /* get battery icon number */
      if (info->percentage <= 10)
        icon_number = "000";
      else if (info->percentage <= 30)
        icon_number = "020";
      else if (info->percentage <= 50)
        icon_number = "040";
      else if (info->percentage <= 70)
        icon_number = "060";
      else if (info->percentage <= 90)
        icon_number = "080";
      else
        icon_number = "100";

      /* set name */
      if (info->is_discharging)
        icon_name = g_strdup_printf ("battery-discharging-%s", icon_number);
      else
        icon_name = g_strdup_printf ("battery-charging-%s", icon_number);
    }

  return icon_name;
}



static void
battery_plugin_update_status_icon (BatteryPlugin *plugin,
                                   BatteryInfo   *info)
{
  gchar     *icon_name;
  gint       icon_size;
  gint       panel_size;
  GdkPixbuf *pixbuf;

  /* get icon name */
  icon_name = battery_plugin_get_icon_name (info);

  /* check if the icon has changed */
  if (plugin->status_icon_name == NULL ||
      strcmp (plugin->status_icon_name, icon_name) != 0)
    {
      /* calculate icon size */
      panel_size = xfce_panel_plugin_get_size (plugin->panel_plugin);
      icon_size = panel_size - (2 * FRAME_PADDING) -
                  - (2 * (panel_size > 26 ? PANEL_PADDING : 0))
                  - (2 * MAX (plugin->frame->style->xthickness, plugin->frame->style->ythickness));

      /* load the icon (we use a convenience function for sharp icons) */
      pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), icon_name, icon_size , 0, NULL);

      if (G_LIKELY (pixbuf))
        {
          /* set pixbuf */
          gtk_image_set_from_pixbuf (GTK_IMAGE (plugin->icon), pixbuf);

          /* release pixbuf */
          g_object_unref (G_OBJECT (pixbuf));
        }
      else
        {
          /* meh no icon, tell the user */
          g_warning ("%s \"%s\"", _("Failed to load the battery icon"), icon_name);
        }
    }

  /* remove old icon name */
  g_free (plugin->status_icon_name);

  /* set new icon */
  plugin->status_icon_name = icon_name;
}



static void
battery_plugin_update_progressbar (BatteryPlugin *plugin,
                                   BatteryInfo   *info)
{
  /* set statusbar fraction */
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (plugin->progressbar), info->percentage / 100.0);
}



static void
battery_plugin_update_label (BatteryPlugin *plugin,
                             BatteryInfo   *info)
{
  GString        *string;
  gint            panel_size;
  GtkOrientation  orientation;

  /* create new string with some size */
  string = g_string_sized_new (100);

  /* start string */
  string = g_string_append (string, "<span size=\"");

  /* label size (xx-small,x-small,small,medium,large) */
  panel_size = xfce_panel_plugin_get_size (plugin->panel_plugin);
  if (panel_size <= 30)
    string = g_string_append (string, "small\">");
  else
    string = g_string_append (string, "medium\">");

  /* create the label */
  if (info->is_present == FALSE)
    {
      /* battery not present */
      string = g_string_append (string, _("Not Present"));
    }
  else if (BATTERY_FULLY_CHARGED (info))
    {
      /* append "Changed" */
      string = g_string_append (string, _("Charged"));
    }
  else if (info->remaining_time > 0)
    {
      /* get panel orientation */
      orientation = xfce_panel_plugin_get_orientation (plugin->panel_plugin);

      /* append both time and percentage */
      g_string_append_printf (string, "%d:%02d%s(%d%%)",
                              info->remaining_time / 3600,
                              info->remaining_time / 60 % 60,
                              orientation == GTK_ORIENTATION_HORIZONTAL ? " " : "\n",
                              info->percentage);
    }
  else
    {
      /* append percentage */
      g_string_append_printf (string, "%d%%", info->percentage);
    }

  /* finish string */
  string = g_string_append (string, "</span>");

  /* set new label text */
  gtk_label_set_markup (GTK_LABEL (plugin->label), string->str);

  /* cleanup */
  g_string_free (string, TRUE);
}



static void
battery_plugin_tooltip_append (GString     *string,
                               BatteryInfo *info)
{
  gchar *time_string;

  if (info->is_present == FALSE)
    {
      /* Not Present */
      string = g_string_append (string, _("Not Present"));
    }
  else if (BATTERY_FULLY_CHARGED (info))
    {
      /* Fully Charged (100%) */
      g_string_append_printf (string, "%s (100%%)", _("Fully Charged"));
    }
  else
    {
      /* create time string */
      if (info->remaining_time > 3600)
        {
          /* 1 hr 5 min */
          time_string = g_strdup_printf ("%d %s %d %s",
                                         info->remaining_time / 3600, _("hr"),
                                         info->remaining_time / 60 % 60, _("min"));
        }
      else if (info->remaining_time > 0)
        {
          /* 9 min */
          time_string = g_strdup_printf ("%d %s",
                                         info->remaining_time / 60 % 60, _("min"));
        }
      else
        {
          /* no time */
          time_string = NULL;
        }

      if (info->is_discharging)
        {
          if (time_string)
            {
              /* 1hr 20min (73%) remaining */
              g_string_append_printf (string, "%s (%d%%) %s",
                                      time_string, info->percentage, _("remaining"));
            }
          else
            {
              /* 73% remaining */
              g_string_append_printf (string, "%d%% %s", info->percentage, _("remaining"));
            }
        }
      else /* charging */
        {
          /* Charging (50% */
          g_string_append_printf (string, "%s (%d%%", _("Charging"), info->percentage);

          /* append time */
          if (time_string)
            {
              /* ) <ln> 1 hr 3 min remaining */
              g_string_append_printf (string, ")\n%s %s", time_string, _("remaining"));
            }
          else
            {
              /* completed) */
              g_string_append_printf (string, " %s)", _("completed"));
            }
        }

      /* cleanup */
      g_free (time_string);
    }
}



#if GTK_CHECK_VERSION(2,12,0)
static gboolean
battery_plugin_query_tooltip (GtkWidget     *widget,
                              gint           x,
                              gint           y,
                              gboolean       keyboard_mode,
                              GtkTooltip    *tooltip,
                              BatteryPlugin *plugin)
{
  GList       *li, *devices;
  BatteryInfo *info;
  gchar       *icon_name;
  GString     *string;
  GdkPixbuf   *pixbuf;
  gint         n;

  /* create string */
  string = g_string_sized_new (200);

  /* get device for the tooltip icon */
  if (G_UNLIKELY (battery_monitor_has_global (plugin->monitor)))
    {
      /* get global device */
      info = battery_monitor_get_device (plugin->monitor, GLOBAL_DEVICE_UDI);

      /* bold baby */
      string = g_string_append (string, "<b>");
    }
  else
    {
      /* get normal device */
      info = battery_monitor_get_device (plugin->monitor, plugin->visible_device);
    }

  /* get pixbuf */
  icon_name = battery_plugin_get_icon_name (info);
  pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), icon_name, 32 , 0, NULL);
  g_free (icon_name);

  if (G_LIKELY (pixbuf))
    {
      /* set tooltip icon */
      gtk_tooltip_set_icon (tooltip, pixbuf);

      /* release pixbuf */
      g_object_unref (G_OBJECT (pixbuf));
    }

  /* append the status of the first widget */
  battery_plugin_tooltip_append (string, info);

  if (G_UNLIKELY (battery_monitor_has_global (plugin->monitor)))
    {
      /* get list of devices */
      devices = battery_monitor_get_devices (plugin->monitor);

      /* insert 2 new lines */
      string = g_string_append (string, "</b>\n\n");

      for (li = devices, n = 1; li != NULL; li = li->next, n++)
        {
          info = li->data;

          /* append the device name */
          if (G_UNLIKELY (info->model))
            g_string_append_printf (string, "%s: %s\n", _("Battery ID"), info->model);
          else
            g_string_append_printf (string, "%s: #%d\n", _("Battery ID"), n);

          /* make status grey */
          string = g_string_append (string, "<i>");

          /* append the status */
          battery_plugin_tooltip_append (string, info);

          /* close status */
          string = g_string_append (string, "</i>");

          /* append a new line */
          if (li->next != NULL)
            string = g_string_append (string, "\n\n");
        }

      /* cleanup */
      g_list_free (devices);
    }

  /* set this string as tooltip */
  gtk_tooltip_set_markup (tooltip, string->str);

  /* cleanup */
  g_string_free (string, TRUE);

  return TRUE;
}



#else



static void
battery_plugin_update_tooltip (BatteryPlugin *plugin)
{
  GList       *li, *devices;
  GString     *string;
  gint         n;
  gint         n_present_devices = 0;
  BatteryInfo *info;

  /* create string with some size */
  string = g_string_sized_new (200);

  /* get the devices from the monitor */
  devices = battery_monitor_get_devices (plugin->monitor);

  /* walk the devices */
  for (li = devices, n = 1; li != NULL; li = li->next, n++)
    {
      info = li->data;

      if (info->is_present)
        {
          /* append the battery status */
          battery_plugin_update_tooltip_append (string, info);

          /* append new line if there is another battery */
          if (g_list_next (li) != NULL)
            string = g_string_append (string, "\n");

          /* increase */
          n_present_devices++;
        }
    }

  /* free list */
  g_list_free (devices);

  /* set the tooltip */
  gtk_tooltips_set_tip (plugin->tooltips, plugin->ebox, string->str, NULL);

  /* cleanup tooltip string */
  g_string_free (string, TRUE);
}
#endif



static void
battery_plugin_widget_visibility (GtkWidget *widget,
                                  gboolean   show)
{
  if (show && !GTK_WIDGET_VISIBLE (widget))
    gtk_widget_show (widget);
  else if (!show && GTK_WIDGET_VISIBLE (widget))
    gtk_widget_hide (widget);
}



void
battery_plugin_update (BatteryPlugin *plugin)
{
  BatteryInfo *info;

  /* get the battery info of the visible battery */
  info = battery_monitor_get_device (plugin->monitor, plugin->visible_device);

  /* fix widget visibility */
  battery_plugin_widget_visibility (plugin->icon, plugin->show_status_icon);
  battery_plugin_widget_visibility (plugin->progressbar, plugin->show_progressbar);
  battery_plugin_widget_visibility (plugin->label, plugin->show_label);

  /* update panel widgets */
  if (G_LIKELY (info))
    {
      /* hide some widgets when the battery is not present */
      if (!info->is_present)
        {
          /* hide the label when the icon is visible */
          if (plugin->show_status_icon)
            gtk_widget_hide (plugin->label);

          /* hide the progressbar when the icon or label is visible */
          if (plugin->show_status_icon || plugin->show_label)
            gtk_widget_hide (plugin->progressbar);
        }

      /* update status of the widgets */
      if (plugin->show_status_icon)
        battery_plugin_update_status_icon (plugin, info);

      if (plugin->show_progressbar && GTK_WIDGET_VISIBLE (plugin->progressbar))
        battery_plugin_update_progressbar (plugin, info);

      if (plugin->show_label && GTK_WIDGET_VISIBLE (plugin->label))
        battery_plugin_update_label (plugin, info);
    }
  else
    {
      /* TODO: on ac, no batteries */
    }

#if !GTK_CHECK_VERSION (2,12,0)
  /* update the tooltip */
  battery_plugin_update_tooltip (plugin);
#endif
}



static void
battery_plugin_load (BatteryPlugin *plugin)
{
  gchar       *file;
  XfceRc      *rc;

  /* set defaults */
  plugin->show_frame = TRUE;
  plugin->show_status_icon = TRUE;
  plugin->show_progressbar = FALSE;
  plugin->show_label = TRUE;

  /* get rc file, create one if needed */
  file = xfce_panel_plugin_lookup_rc_file (plugin->panel_plugin);
  if (G_LIKELY (file))
    {
      /* open rc, read-only */
      rc = xfce_rc_simple_open (file, TRUE);

      /* cleanup */
      g_free (file);

      if (G_LIKELY (rc))
        {
          /* set group */
          xfce_rc_set_group (rc, "Preferences");

          /* read settings */
          plugin->visible_device = g_strdup (xfce_rc_read_entry (rc, "VisibleDevice", NULL));
          plugin->show_frame = xfce_rc_read_bool_entry (rc, "ShowFrame", TRUE);
          plugin->show_status_icon = xfce_rc_read_bool_entry (rc, "ShowStatusIcon", TRUE);
          plugin->show_progressbar = xfce_rc_read_bool_entry (rc, "ShowProgressBar", FALSE);
          plugin->show_label = xfce_rc_read_bool_entry (rc, "ShowLabel",TRUE);

          /* close the rc file */
          xfce_rc_close (rc);
        }
    }
}



static void
battery_plugin_save (BatteryPlugin *plugin)
{
  gchar       *file;
  XfceRc      *rc;

  /* get rc file, create one if needed */
  file = xfce_panel_plugin_save_location (plugin->panel_plugin, TRUE);
  if (G_LIKELY (file))
    {
      /* open rc, read-write */
      rc = xfce_rc_simple_open (file, FALSE);

      /* cleanup */
      g_free (file);

      if (G_LIKELY (rc))
        {
          /* read appearance settings */
          xfce_rc_set_group (rc, "Preferences");

          /* write settings */
          xfce_rc_write_entry (rc, "VisibleDevice", plugin->visible_device);
          xfce_rc_write_bool_entry (rc, "ShowFrame", plugin->show_frame);
          xfce_rc_write_bool_entry (rc, "ShowStatusIcon", plugin->show_status_icon);
          xfce_rc_write_bool_entry (rc, "ShowProgressBar", plugin->show_progressbar);
          xfce_rc_write_bool_entry (rc, "ShowLabel", plugin->show_label);

          /* close the rc file */
          xfce_rc_close (rc);
        }
    }
}



static void
battery_plugin_orientation_changed (BatteryPlugin  *plugin,
                                    GtkOrientation  orientation)
{
  gint size;
  
  /* set the orientation of the hvbox */
  xfce_hvbox_set_orientation (XFCE_HVBOX (plugin->box), orientation);
  
  /* get the panel size */
  size = xfce_panel_plugin_get_size (plugin->panel_plugin);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      /* progressbar size and orienation */
      gtk_widget_set_size_request (plugin->progressbar, PROGRESS_BAR_THINKNESS, size);
      gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (plugin->progressbar), GTK_PROGRESS_BOTTOM_TO_TOP);

      /* label justification */
      gtk_label_set_justify (GTK_LABEL (plugin->label), GTK_JUSTIFY_LEFT);
    }
  else
    {
      /* progressbar size and orienation */
      gtk_widget_set_size_request (plugin->progressbar, size, PROGRESS_BAR_THINKNESS);
      gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (plugin->progressbar), GTK_PROGRESS_LEFT_TO_RIGHT);

      /* label justification */
      gtk_label_set_justify (GTK_LABEL (plugin->label), GTK_JUSTIFY_CENTER);
    }
}



static gboolean
battery_plugin_set_size (BatteryPlugin *plugin,
                         gint           size)
{
  /* set the border width of the frame */
  gtk_container_set_border_width (GTK_CONTAINER (plugin->frame), size > 26 ? PANEL_PADDING : 0);
  
  /* poke function above to set proper progressbar sizings */
  battery_plugin_orientation_changed (plugin, xfce_panel_plugin_get_orientation (plugin->panel_plugin));
  
  /* free last icon name */
  g_free (plugin->status_icon_name);
  plugin->status_icon_name = NULL;

  /* update the plugin */
  battery_plugin_update (plugin);

  return TRUE;
}



static void
battery_plugin_free (BatteryPlugin *plugin)
{
  /* release the monitor */
  g_object_unref (G_OBJECT (plugin->monitor));

  /* destroy widgets */
  gtk_widget_destroy (plugin->ebox);

#if !GTK_CHECK_VERSION(2,12,0)
  /* release tooltips */
  g_object_unref (G_OBJECT (plugin->tooltips));
#endif

  /* cleanup */
  g_free (plugin->status_icon_name);
}



static BatteryPlugin *
battery_plugin_new (XfcePanelPlugin *panel_plugin)
{
  BatteryPlugin  *plugin;
  GtkOrientation  orientation;

  /* create a new panel structure */
  plugin = panel_slice_new0 (BatteryPlugin);

  /* add the panel plugin to the battery */
  plugin->panel_plugin = panel_plugin;
  plugin->status_icon_name = NULL;

#if !GTK_CHECK_VERSION(2,12,0)
  /* create tooltips */
  plugin->tooltips = gtk_tooltips_new ();
#if GTK_CHECK_VERSION(2,9,0)
  g_object_ref_sink (G_OBJECT (plugin->tooltips));
#else
  g_object_ref (G_OBJECT (plugin->tooltips));
  gtk_object_sink (GTK_OBJECT (plugin->tooltips));
#endif
#endif

  /* create the battery monitor */
  plugin->monitor = battery_monitor_get_default ();

  /* load settings */
  battery_plugin_load (plugin);

  /* make sure a battery is added */
  if (!plugin->visible_device || !battery_monitor_get_device (plugin->monitor, plugin->visible_device))
    plugin->visible_device = battery_monitor_get_first_device (plugin->monitor);

  /* get plugin orientation */
  orientation = xfce_panel_plugin_get_orientation (panel_plugin);

  /* new event box widget */
  plugin->ebox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (plugin->panel_plugin), plugin->ebox);
#if GTK_CHECK_VERSION(2,12,0)
  g_signal_connect (G_OBJECT (plugin->ebox), "query-tooltip", G_CALLBACK (battery_plugin_query_tooltip), plugin);
  g_object_set (G_OBJECT (plugin->ebox), "has-tooltip", TRUE, NULL);
#endif
  gtk_widget_show (plugin->ebox);

  /* new frame widget */
  plugin->frame = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER (plugin->ebox), plugin->frame);
  gtk_frame_set_shadow_type (GTK_FRAME (plugin->frame), plugin->show_frame ? GTK_SHADOW_IN : GTK_SHADOW_NONE);
  gtk_widget_show (plugin->frame);

  /* new box widget */
  plugin->box = xfce_hvbox_new (orientation, FALSE, SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (plugin->box), FRAME_PADDING);
  gtk_container_add (GTK_CONTAINER (plugin->frame), plugin->box);
  gtk_widget_show (plugin->box);

  /* new icon widget */
  plugin->icon = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (plugin->box), plugin->icon, FALSE, FALSE, 0);

  /* new progressbar widget */
  plugin->progressbar = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (plugin->box), plugin->progressbar, FALSE, FALSE, 0);

  /* new label widget */
  plugin->label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (plugin->box), plugin->label, FALSE, FALSE, 0);

  /* poke orienation function */
  battery_plugin_orientation_changed (plugin, orientation);

  /* connect signal to the monitor */
  g_signal_connect_swapped (G_OBJECT (plugin->monitor), "changed", G_CALLBACK (battery_plugin_update), plugin);

  /* force update */
  battery_monitor_devices_update (plugin->monitor);

  return plugin;
}



static void
battery_plugin_construct (XfcePanelPlugin *panel_plugin)
{
  BatteryPlugin *plugin;

  /* setup translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* create a new battery plugin */
  plugin = battery_plugin_new (panel_plugin);

  /* set plugin stuff */
  xfce_panel_plugin_add_action_widget (panel_plugin, plugin->ebox);
  xfce_panel_plugin_menu_show_configure (panel_plugin);

  /* connect plugin signals */
  g_signal_connect_swapped (G_OBJECT (panel_plugin), "free-data", G_CALLBACK (battery_plugin_free), plugin);
  g_signal_connect_swapped (G_OBJECT (panel_plugin), "save", G_CALLBACK (battery_plugin_save), plugin);
  g_signal_connect_swapped (G_OBJECT (panel_plugin), "size-changed", G_CALLBACK (battery_plugin_set_size), plugin);
  g_signal_connect_swapped (G_OBJECT (panel_plugin), "orientation-changed", G_CALLBACK (battery_plugin_orientation_changed), plugin);
  g_signal_connect_swapped (G_OBJECT (panel_plugin), "configure-plugin", G_CALLBACK (battery_preferences_dialog_new), plugin);
}



/* register the panel plugin */
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (battery_plugin_construct);
