/*
 * * Copyright (C) 2008-2009 Ali <aliov@xfce.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <gtk/gtk.h>
#include <glib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

#ifdef HAVE_LIBXFCE4UI
#include <libxfce4ui/libxfce4ui.h>
#else
#include <libxfcegui4/libxfcegui4.h>
#endif

#include <devkit-power-gobject/devicekit-power.h>

#include "settings_ui.h"

#define BORDER 8
#define PLUGIN_WEBSITE  "http://goodies.xfce.org/projects/panel-plugins/xfce4-battery-plugin"

typedef struct
{
    /* Options */
    gboolean show_percentage;
    gboolean show_icon;
    gboolean show_time;
    gboolean hide_percentage_time_when_full;
    
} BattOptions;

typedef struct
{
    DkpClient       *client;
    GPtrArray       *array;
    
    XfcePanelPlugin *panel_plugin;
    
    GtkWidget	    *ebox;
    GtkWidget       *hvbox;
    
    BattOptions     *options;
    
} BattPlugin;

typedef struct
{
    const DkpDevice *device;
    
    GtkWidget	    *ebox;
    GtkWidget       *hvbox;
    GtkWidget 	    *progress;
    GtkWidget       *label_percentage;
    GtkWidget       *label_time;
    GtkWidget       *image;
    
    BattOptions     *options;
    
} BattDevice;

void		show_percentage_toggled_cb		  (GtkToggleButton *bt, gpointer data);

void		show_icon_toggled_cb			  (GtkToggleButton *bt, gpointer data);

void		show_time_toggled_cb			  (GtkToggleButton *bt, gpointer data);

void		hide_percentage_time_when_full_toggled_cb (GtkToggleButton *bt, gpointer data);

static void	battery_plugin_rescan_battery 		  (const DkpDevice *device, 
							   gpointer *obj, 
							   BattDevice *batt);

/**
 *
 * 
 **/
static void
battery_plugin_refresh_displayed_info (BattPlugin *plugin)
{
    guint i;
    
    for ( i = 0; i < plugin->array->len; i++)
    {
	BattDevice *batt = g_ptr_array_index (plugin->array, i);
	battery_plugin_rescan_battery (batt->device, NULL, batt);
    }
}

/**
 *
 * 
 **/
void show_percentage_toggled_cb	(GtkToggleButton *bt, gpointer data)
{
    BattPlugin *plugin = (BattPlugin *)data;
    
    plugin->options->show_percentage = gtk_toggle_button_get_active (bt);
    battery_plugin_refresh_displayed_info (plugin);
}

/**
 *
 * 
 **/
void show_icon_toggled_cb (GtkToggleButton *bt, gpointer data)
{
    guint i;
    BattPlugin *plugin = (BattPlugin *)data;
    
    plugin->options->show_icon = gtk_toggle_button_get_active (bt);
    
    for ( i = 0; i < plugin->array->len; i++)
    {
	BattDevice *batt = g_ptr_array_index (plugin->array, i);
	g_object_set (G_OBJECT (batt->image),
		      "visible", plugin->options->show_icon,
		      NULL);
	
    }
}

/**
 *
 * 
 **/
void show_time_toggled_cb (GtkToggleButton *bt, gpointer data)
{
    BattPlugin *plugin = (BattPlugin *)data;
    plugin->options->show_time = gtk_toggle_button_get_active (bt);
    battery_plugin_refresh_displayed_info (plugin);
}

/**
 *
 * 
 **/
void hide_percentage_time_when_full_toggled_cb (GtkToggleButton *bt, gpointer data)
{
    BattPlugin *plugin = (BattPlugin *)data;
    plugin->options->hide_percentage_time_when_full = gtk_toggle_button_get_active (bt);
    battery_plugin_refresh_displayed_info (plugin);
}

/**
 * load_pixbug_icon:
 * 
 **/
static GdkPixbuf *
load_pixbuf_icon (const gchar *icon_name, gint size)
{
    GdkPixbuf *pix = NULL;
    GError *error = NULL;
    
    pix = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), 
                                    icon_name, 
                                    size,
                                    GTK_ICON_LOOKUP_USE_BUILTIN,
                                    &error);
                                    
    if ( error )
    {
        g_warning ("Unable to load icon : %s : %s", icon_name, error->message);
        g_error_free (error);
    }
    
    return pix;
}

/**
 *
 **/
static gchar *
battery_get_time_string_label (guint seconds)
{
    gint  hours;
    gint  minutes;

    /* Add 0.5 to do rounding */
    minutes = (int) ( ( seconds / 60.0 ) + 0.5 );
    
    hours = minutes / 60;
    minutes = minutes % 60;
    
    return g_strdup_printf ("%02i:%02i", hours, minutes);
}

/**
 *
 **/
static gchar *
battery_get_time_string (guint seconds)
{
    char* timestring = NULL;
    gint  hours;
    gint  minutes;

    /* Add 0.5 to do rounding */
    minutes = (int) ( ( seconds / 60.0 ) + 0.5 );

    if (minutes == 0) 
    {
	timestring = g_strdup (_("Unknown time"));
	return timestring;
    }

    if (minutes < 60) 
    {
	timestring = g_strdup_printf (ngettext ("%i minute",
			              "%i minutes",
				      minutes), minutes);
	return timestring;
    }

    hours = minutes / 60;
    minutes = minutes % 60;

    if (minutes == 0)
	timestring = g_strdup_printf (ngettext (
			    "%i hour",
			    "%i hours",
			    hours), hours);
    else
	/* TRANSLATOR: "%i %s %i %s" are "%i hours %i minutes"
	 * Swap order with "%2$s %2$i %1$s %1$i if needed */
	timestring = g_strdup_printf (_("%i %s %i %s"),
			    hours, ngettext ("hour", "hours", hours),
			    minutes, ngettext ("minute", "minutes", minutes));
    return timestring;
}

/**
 * 
 **/
static void
battery_plugin_set_tooltip (BattDevice *batt, GtkTooltip *tooltip)
{
    gchar *tip = NULL;
    gchar *est_time_str = NULL;
    DkpDeviceState state;
    gint64 time_to_empty, time_to_full;
    gdouble percentage_double;
    guint percentage;
    
    g_object_get (G_OBJECT (batt->device),
		  "state", &state,
		  "time-to-empty", &time_to_empty,
		  "time-to-full", &time_to_full,
		  "percentage", &percentage_double,
		  NULL);
    
    percentage = (guint) percentage_double;
    
    if ( state == DKP_DEVICE_STATE_FULLY_CHARGED )
    {
	if ( time_to_empty > 0 )
	{
	    est_time_str = battery_get_time_string (time_to_empty);
	    tip = g_strdup_printf (_("Your battery is fully charged (%i%%).\nProvides %s runtime"), 
				   percentage,
				   est_time_str);
	    g_free (est_time_str);
	}
	else
	{
	    tip = g_strdup_printf (_("Your battery is fully charged (%i%%)."), 
				   percentage);
	}
    }
    else if ( state == DKP_DEVICE_STATE_CHARGING )
    {
	if ( time_to_full != 0 )
	{
	    est_time_str = battery_get_time_string (time_to_full);
	    tip = g_strdup_printf (_("Your battery is charging (%i%%)\n%s until is fully charged."), 
				   percentage, 
				   est_time_str);
	    g_free (est_time_str);
	}
	else
	{
	    tip = g_strdup_printf (_("Your battery is charging (%i%%)."),
				   percentage);
	}
    }
    else if ( state == DKP_DEVICE_STATE_DISCHARGING )
    {
	if ( time_to_empty != 0 )
	{
	    est_time_str = battery_get_time_string (time_to_empty);
	    tip = g_strdup_printf (_("Your battery is discharging (%i%%)\nestimated time left is %s."), 
				   percentage, 
				   est_time_str);
	    g_free (est_time_str);
	}
	else
	{
	    tip = g_strdup_printf (_("Your is discharging (%i%%)."),
				   percentage);
	}
    }
    else if ( state == DKP_DEVICE_STATE_PENDING_CHARGE )
    {
	tip = g_strdup_printf (_("Waiting to discharge (%i%%)."), percentage);
    }
    else if ( state == DKP_DEVICE_STATE_PENDING_DISCHARGE )
    {
	tip = g_strdup_printf (_("Waiting to charge (%i%%)."), percentage);
    }
    else if ( state == DKP_DEVICE_STATE_EMPTY )
    {
	tip = g_strdup (_("Your battery is empty"));
    }
    
    gtk_tooltip_set_text (tooltip, tip);
}

/**
 * 
 **/
static gboolean
battery_plugin_query_device_tooltip (GtkStatusIcon *icon, 
				     gint x,
				     gint y,
				     gboolean keyboard_mode,
				     GtkTooltip *tooltip,
				     BattDevice *batt)
{
    battery_plugin_set_tooltip (batt, tooltip);
    return TRUE;
}

/**
 * 
 * 
 **/
static gboolean
battery_plugin_device_size_changed_cb (XfcePanelPlugin *panel_plugin, gint size, BattDevice *batt)
{
    GdkPixbuf *icon;

    if (xfce_panel_plugin_get_orientation (panel_plugin) == GTK_ORIENTATION_HORIZONTAL)
    {
        gtk_widget_set_size_request (GTK_WIDGET(batt->progress),
				     BORDER, size - BORDER);
				     
	gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (batt->progress), GTK_PROGRESS_BOTTOM_TO_TOP);
    }
    else
    {
        gtk_widget_set_size_request (GTK_WIDGET (batt->progress),
				     size - BORDER, BORDER);
	gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (batt->progress), GTK_PROGRESS_LEFT_TO_RIGHT);
    }
    
    icon = load_pixbuf_icon ("battery", size - BORDER) ;
    
    gtk_image_set_from_pixbuf (GTK_IMAGE (batt->image), icon);
    
    g_object_unref (icon);
    
    return TRUE;
}

/**
 * 
 * 
 **/
static void
battery_plugin_rescan_battery (const DkpDevice *device, gpointer *obj, BattDevice *batt)
{
    GtkAdjustment *adj;
    gdouble percentage;
    DkpDeviceState state;
    gchar percentage_str[32];
    
    g_object_get (G_OBJECT (device),
		  "percentage", &percentage,
		  "state", &state,
		  NULL);
    
    g_snprintf (percentage_str, 32, "%i%%", (gint) percentage);
    gtk_label_set_text (GTK_LABEL (batt->label_percentage), NULL);
    gtk_label_set_text (GTK_LABEL (batt->label_time), NULL);
    
    if ( batt->options->show_percentage && ( state != DKP_DEVICE_STATE_FULLY_CHARGED || !batt->options->hide_percentage_time_when_full ))
    {
	gtk_label_set_text (GTK_LABEL (batt->label_percentage), percentage_str);
	gtk_widget_show (batt->label_percentage);
    }
    else
	gtk_widget_show (batt->label_percentage);
    
    if ( batt->options->show_time )
    {
	gint64 time_to_full, time_to_empty;
	gchar *time_str = NULL;
	
	g_object_get (G_OBJECT (batt->device),
		      "time-to-empty", &time_to_empty,
		      "time-to-full", &time_to_full,
		      NULL);
	
	gtk_widget_hide (batt->label_time);
	
	if ( state == DKP_DEVICE_STATE_FULLY_CHARGED )
	{
	    if (!batt->options->hide_percentage_time_when_full)
	    {
		time_str = battery_get_time_string_label (time_to_empty);
		gtk_label_set_text (GTK_LABEL (batt->label_time), time_str);
		g_free (time_str);
		gtk_widget_show (batt->label_time);
	    }
	}
	else if ( state == DKP_DEVICE_STATE_DISCHARGING )
	{
	    time_str = battery_get_time_string_label (time_to_empty);
	    gtk_label_set_text (GTK_LABEL (batt->label_time), time_str);
	    g_free (time_str);
	    gtk_widget_show (batt->label_time);
	}
	else if ( state == DKP_DEVICE_STATE_CHARGING )
	{
	    time_str = battery_get_time_string_label (time_to_full);
	    gtk_label_set_text (GTK_LABEL (batt->label_time), time_str);
	    g_free (time_str);
	    gtk_widget_show (batt->label_time);
	}
    }
    else
	gtk_widget_hide (batt->label_time);
	
    g_object_get (G_OBJECT (batt->progress),
		  "adjustment", &adj,
		  NULL);
		  
    gtk_adjustment_set_value (adj, percentage);
}

/**
 * 
 * 
 **/
static void
battery_plugin_add_battery (BattPlugin *plugin, const DkpDevice *device)
{
    BattDevice *batt;
    GtkWidget *vbox;
    GdkPixbuf *pix;
    GtkObject *adj;
    gint size;
    
    batt = g_new0 (BattDevice, 1);
    
    batt->device = device;
    batt->options = plugin->options;
    
    batt->ebox = gtk_event_box_new ();
    gtk_event_box_set_above_child (GTK_EVENT_BOX (batt->ebox), TRUE);
    gtk_widget_set_has_tooltip (batt->ebox, TRUE);
    
    batt->hvbox = xfce_hvbox_new (xfce_panel_plugin_get_orientation (plugin->panel_plugin),
				  FALSE, 2);
    gtk_container_set_border_width (GTK_CONTAINER (batt->hvbox), BORDER / 2);
    size = xfce_panel_plugin_get_size (plugin->panel_plugin);
    pix = load_pixbuf_icon ("battery", size - BORDER) ;
    
    batt->image = gtk_image_new_from_pixbuf (pix);
    
    g_object_unref (pix);
    
    batt->label_percentage = gtk_label_new (NULL);
    batt->label_time = gtk_label_new (NULL);
    
    adj = gtk_adjustment_new (0., 0., 100., 1., 0., 0.);

    batt->progress = gtk_progress_bar_new_with_adjustment (GTK_ADJUSTMENT (adj));

    gtk_box_pack_start (GTK_BOX (batt->hvbox), batt->image, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (batt->hvbox), batt->progress, FALSE, FALSE, 0);
    
    vbox = gtk_vbox_new (TRUE, 0);
    
    gtk_box_pack_start (GTK_BOX (vbox), batt->label_percentage, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), batt->label_time, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (batt->hvbox), vbox, FALSE, FALSE, 0);
    
    gtk_container_add (GTK_CONTAINER (batt->ebox), batt->hvbox);
    gtk_box_pack_start (GTK_BOX (plugin->hvbox), batt->ebox, FALSE, FALSE, 0);
    
    g_signal_connect (plugin->panel_plugin, "size-changed", 
		      G_CALLBACK (battery_plugin_device_size_changed_cb), batt);
	
    g_signal_connect (batt->ebox, "query-tooltip",
		      G_CALLBACK (battery_plugin_query_device_tooltip), batt);
    
    battery_plugin_device_size_changed_cb (plugin->panel_plugin, size, batt);
    gtk_widget_show_all (batt->ebox);
    
    battery_plugin_rescan_battery (device, NULL, batt);
    g_ptr_array_add (plugin->array, batt);
    g_signal_connect (G_OBJECT (batt->device), "changed",
		      G_CALLBACK (battery_plugin_rescan_battery), batt);
}

/**
 * 
 * 
 **/
static void
battery_plugin_get_batteries (BattPlugin *plugin)
{
    GError *error = NULL;
    GPtrArray *array;
    guint i;
    
    array = dkp_client_enumerate_devices (plugin->client, &error);
    
    if ( error )
    {
        g_critical ("Couldn't enumerate power devices : %s", error->message);
        g_error_free (error);
        return; //FIXME: Display error image.
    }
    
    for ( i = 0; i < array->len; i++)
    {
        DkpDevice *device;
        DkpDeviceType type;
        
        device = g_ptr_array_index (array, i);
        
        g_object_get (G_OBJECT (device), 
                      "type", &type,
                      NULL);
	/*
	 * Support for other battery device could be added later.
	 */
        if ( type == DKP_DEVICE_TYPE_BATTERY )
        {
            battery_plugin_add_battery (plugin, device);
        }
    }
}

/**
 * 
 * 
 **/
static gboolean
battery_plugin_orientation_changed_cb (XfcePanelPlugin *panel_plugin, GtkOrientation orientation, BattPlugin *plugin)
{
    guint i;
    
    for ( i = 0; i < plugin->array->len; i++)
    {
	BattDevice *batt;
	
	batt = g_ptr_array_index (plugin->array, i);
	
	xfce_hvbox_set_orientation (XFCE_HVBOX (batt->hvbox), orientation);
	battery_plugin_device_size_changed_cb (panel_plugin, 
					       xfce_panel_plugin_get_size (panel_plugin),
					       batt);
    }
    
    xfce_hvbox_set_orientation (XFCE_HVBOX (plugin->hvbox), orientation);
    
    return TRUE;
}

/**
 * 
 * 
 **/
static gboolean
battery_plugin_size_changed_cb (XfcePanelPlugin *panel_plugin, gint size, BattPlugin *plugin)
{
    return TRUE;
}

/**
 * 
 * 
 **/
static void
battery_plugin_dialog_response_cb (GtkWidget *dlg, int response, BattPlugin *plugin)
{
    gboolean result;

    if (response == GTK_RESPONSE_HELP)
    {
        result = g_spawn_command_line_async ("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);

        if (G_UNLIKELY (result == FALSE))
            g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
    }
    else
    {
        gtk_widget_destroy (dlg);
        xfce_panel_plugin_unblock_menu (plugin->panel_plugin);
    }
}

/**
 * 
 **/
static void
battery_plugin_init_options (BattPlugin *plugin)
{
    plugin->options = g_new0 (BattOptions, 1);
    
    plugin->options->show_percentage   = TRUE;
    plugin->options->show_time         = FALSE;
    plugin->options->show_icon         = TRUE;
    plugin->options->hide_percentage_time_when_full = FALSE;
}

/**
 * 
 **/
static void
battery_plugin_configure_cb (BattPlugin *plugin)
{
    GtkBuilder *builder;
    GError *error = NULL;
    GtkWidget *dialog;
    GtkWidget *vbox;
    
    dialog = xfce_titled_dialog_new_with_buttons (_("Battery Monitor"),
                                                  GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin->panel_plugin))),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                                  GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                                  GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                                  NULL);
    
    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name  (GTK_WINDOW (dialog), "battery");
    
    builder = gtk_builder_new ();
    gtk_builder_add_from_string (builder, settings_ui, settings_ui_length, &error);
    
    if ( G_UNLIKELY (error) )
    {
	g_critical ("Unable to open dialog :%s", error->message);
	g_error_free (error);
	g_object_unref (builder);
	return;
    }
    
    vbox = GTK_WIDGET (gtk_builder_get_object (builder, "vbox"));
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER - 2);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox,
                        TRUE, TRUE, 0);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "show-icon")),
				  plugin->options->show_icon);

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "show-time")),
				  plugin->options->show_time);
				  
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "show-percentage")),
				  plugin->options->show_percentage);
    
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "hide-percentage-time-when-full")),
				  plugin->options->hide_percentage_time_when_full);

    gtk_builder_connect_signals (builder, plugin);
    g_signal_connect (dialog, "response", 
		      G_CALLBACK (battery_plugin_dialog_response_cb), plugin);
    gtk_widget_show (dialog);
    g_object_unref (builder);
}

/**
 * 
 * 
 **/
static void
battery_plugin_destroy_batt_data (BattDevice *batt)
{
    gtk_widget_destroy (batt->ebox);
    g_free (batt);
}

/**
 * 
 * 
 **/
static void
battery_plugin_free_data_cb (BattPlugin *plugin)
{
    g_ptr_array_foreach (plugin->array, (GFunc)battery_plugin_destroy_batt_data, NULL);
    g_ptr_array_free (plugin->array, TRUE);
    g_object_unref (plugin->client);
    
    g_free (plugin->options);
    g_free (plugin);
}

/**
 * 
 * 
 **/
static void
battery_plugin_construct (XfcePanelPlugin *panel_plugin)
{
    BattPlugin *plugin;
    
    plugin = g_new0 (BattPlugin, 1);
    
    battery_plugin_init_options (plugin);
    
    plugin->client  = dkp_client_new ();
    plugin->array   = g_ptr_array_new ();
    
    plugin->panel_plugin = panel_plugin;
    plugin->ebox = gtk_event_box_new ();
    
    gtk_container_add (GTK_CONTAINER (panel_plugin), GTK_WIDGET (plugin->ebox));
		     
    plugin->hvbox = xfce_hvbox_new (xfce_panel_plugin_get_orientation (plugin->panel_plugin),
				    FALSE, 0);
				    
    gtk_container_add (GTK_CONTAINER (plugin->ebox), plugin->hvbox);
    
    xfce_panel_plugin_add_action_widget (panel_plugin, GTK_WIDGET (plugin->ebox));
    
    g_signal_connect_swapped (panel_plugin, "free-data",
			      G_CALLBACK (battery_plugin_free_data_cb), plugin);
			      
    g_signal_connect (panel_plugin, "size-changed", 
		      G_CALLBACK (battery_plugin_size_changed_cb), plugin);
    
    g_signal_connect (panel_plugin, "orientation-changed", 
		      G_CALLBACK (battery_plugin_orientation_changed_cb), plugin);
		      
    xfce_panel_plugin_menu_show_configure (panel_plugin);
    g_signal_connect_swapped (panel_plugin, "configure-plugin", 
			      G_CALLBACK (battery_plugin_configure_cb), plugin);
    
    gtk_widget_show_all (GTK_WIDGET (panel_plugin));
    
    battery_plugin_get_batteries (plugin);
    gtk_widget_set_size_request(plugin->ebox, -1, -1);
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (battery_plugin_construct);
