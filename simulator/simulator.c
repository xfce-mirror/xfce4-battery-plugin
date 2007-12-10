/* $Id$
 *
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
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

#include <stdlib.h>
#include <gtk/gtk.h>
#include <libxfcegui4/libxfcegui4.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <hal/libhal.h>

#define BORDER          (6)
#define DEFAULT_VOLTAGE (12)
#define MIN_CAPACITY    (2000)
#define MAX_CAPACITY    (6000)

typedef struct
{
  /* store */
  GtkListStore *store;

  /* widgets */
  GtkWidget    *widget_combo;
  GtkWidget    *widget_add;
  GtkWidget    *widget_remove;
  GtkWidget    *widget_udi;
  GtkWidget    *widget_capacity;
  GtkWidget    *widget_percentage;
  GtkWidget    *widget_present;
  GtkWidget    *widget_discharging;
  GtkWidget    *widget_automatic;
}
BatterySimulator;

enum
{
  COLUMN_LABEL,
  COLUMN_UDI,
  COLUMN_PERCENTAGE,
  COLUMN_CAPACITY,
  COLUMN_PRESENT,
  COLUMN_DISCHARGING,
  COLUMN_TIMEOUT_ID,
  N_COLUMNS
};



/* globals */
static LibHalContext  *context = NULL;
static DBusConnection *dbus_connection= NULL;



static gboolean
battery_simulator_device_remove (GtkTreeModel *model,
                                 GtkTreePath  *path,
                                 GtkTreeIter  *iter,
                                 gpointer      user_data)
{
  guint      timeout_id;
  gchar     *udi;
  DBusError  error;

  /* init error */
  dbus_error_init (&error);

  /* get timeout id and udi */
  gtk_tree_model_get (model, iter,
                      COLUMN_TIMEOUT_ID, &timeout_id,
                      COLUMN_UDI, &udi, -1);

  /* stop timeout */
  if (timeout_id > 0)
    g_source_remove (timeout_id);

  /* remove device */
  libhal_remove_device (context, udi, &error);

  /* cleanup */
  g_free (udi);

  /* cleanup */
  LIBHAL_FREE_DBUS_ERROR (&error);

  return FALSE;
}



static void
battery_simulator_device_update (GtkTreeModel *model,
                                 GtkTreeIter  *iter)
{
  DBusError  error;
  gint       capacity_current;
  gint       capacity, percentage;
  gboolean   discharging, present;
  gchar     *udi;

  /* initialize error */
  dbus_error_init (&error);

  /* read store data */
  gtk_tree_model_get (model, iter,
                      COLUMN_UDI, &udi,
                      COLUMN_CAPACITY, &capacity,
                      COLUMN_PERCENTAGE, &percentage,
                      COLUMN_PRESENT, &present,
                      COLUMN_DISCHARGING, &discharging, -1);

  /* calculate current capacity */
  capacity_current = capacity * percentage / 100;

  /* update properties */
  libhal_device_set_property_int (context, udi, "battery.reporting.current", capacity_current, &error);
  libhal_device_set_property_int (context, udi, "battery.charge_level.current", capacity_current * DEFAULT_VOLTAGE, &error);
  libhal_device_set_property_bool (context, udi, "battery.present", present, &error);
  libhal_device_set_property_bool (context, udi, "battery.rechargeable.is_charging", !discharging, &error);
  libhal_device_set_property_bool (context, udi, "battery.rechargeable.is_discharging", discharging, &error);

  /* cleanup */
  g_free (udi);

  /* cleanup */
  LIBHAL_FREE_DBUS_ERROR (&error);
}



static void
battery_simulator_device_init (gint          id,
                               const gchar  *udi,
                               GtkListStore *store,
                               GtkTreeIter  *iter)
{
  DBusError  error;
  gint       capacity, percentage;
  gboolean   discharging, present;
  gchar     *label;

  /* initialize error */
  dbus_error_init (&error);

  /* create random values */
  capacity = g_random_int_range (MIN_CAPACITY, MAX_CAPACITY);
  percentage = g_random_int_range (0, 100);
  discharging = g_random_boolean ();
  present = g_random_boolean ();

  /* set default properties for this battery */
  libhal_device_set_property_int (context, udi, "battery.voltage.design", DEFAULT_VOLTAGE, &error);
  libhal_device_set_property_int (context, udi, "battery.voltage.current", DEFAULT_VOLTAGE, &error);
  libhal_device_set_property_int (context, udi, "battery.reporting.design", capacity, &error);
  libhal_device_set_property_int (context, udi, "battery.reporting.last_full", capacity, &error);
  libhal_device_set_property_int (context, udi, "battery.charge_level.design", capacity * DEFAULT_VOLTAGE, &error);
  libhal_device_set_property_int (context, udi, "battery.charge_level.last_full", capacity * DEFAULT_VOLTAGE, &error);

  /* create label */
  label = g_strdup_printf ("Battery %d", id);

  /* set values */
  gtk_list_store_set (store, iter,
                      COLUMN_LABEL, label,
                      COLUMN_UDI, udi,
                      COLUMN_CAPACITY, capacity,
                      COLUMN_PERCENTAGE, percentage,
                      COLUMN_PRESENT, present,
                      COLUMN_DISCHARGING, discharging,
                      COLUMN_TIMEOUT_ID, 0, -1);

  /* cleanup */
  g_free (label);

  /* cleanup */
  LIBHAL_FREE_DBUS_ERROR (&error);
}



static gchar *
battery_simulator_device_new (gint id)
{
  gchar     *tmp, *udi;
  DBusError  error;

  /* initialize error */
  dbus_error_init (&error);

  /* create new hal device */
  tmp = libhal_new_device (context, &error);
  if (G_UNLIKELY (tmp == NULL))
    {
      /* print warning */
      g_message ("Unable to create new hal devices.");

      /* cleanup */
      LIBHAL_FREE_DBUS_ERROR (&error);

      return NULL;
    }

  /* create real udi */
  udi = g_strdup_printf ("/org/freedesktop/Hal/devices/acpi_BAT%d", id);

  /* set device properties */
  libhal_device_set_property_string (context, tmp, "info.udi", udi, &error);
  libhal_device_set_property_string (context, tmp, "info.parent", "/org/freedesktop/Hal/devices/computer", &error);

  /* set needed battery properties */
  libhal_device_add_capability (context, tmp, "battery", &error);
  libhal_device_property_strlist_append (context, tmp, "info.category", "battery", &error);
  libhal_device_set_property_string (context, tmp, "battery.type", "primary", &error);
  libhal_device_set_property_bool (context, tmp, "battery.is_rechargeable", TRUE, &error);

  /* battery units */
  libhal_device_set_property_string (context, tmp, "battery.charge_level.unit", "mWh", &error);
  libhal_device_set_property_string (context, tmp, "battery.voltage.unit", "mV", &error);
  libhal_device_set_property_string (context, tmp, "battery.reporting.unit", "mAh", &error);

  /* commit the device when there are no errors */
  if (dbus_error_is_set (&error) ||
      !libhal_device_commit_to_gdl (context, tmp, udi, &error))
    {
      /* cleanup udi */
      g_free (udi);
      udi = NULL;
    }

  /* cleanup */
  g_free (tmp);

  /* cleanup */
  LIBHAL_FREE_DBUS_ERROR (&error);

  return udi;
}



static gboolean
battery_simulator_timeout (gpointer user_data)
{
  GtkTreeRowReference *reference = (GtkTreeRowReference *) user_data;
  GtkTreeModel        *model;
  GtkTreeIter          iter;
  GtkTreePath         *path;
  gint                 percentage;
  gboolean             discharging;

  if (gtk_tree_row_reference_valid (reference))
    {
      /* get model and path */
      model = gtk_tree_row_reference_get_model (reference);
      path = gtk_tree_row_reference_get_path (reference);

      /* get iter */
      if (gtk_tree_model_get_iter (model, &iter, path))
        {
          /* get the udi, capacity and whether its discharging */
          gtk_tree_model_get (model, &iter,
                              COLUMN_PERCENTAGE, &percentage,
                              COLUMN_DISCHARGING, &discharging, -1);

          /* change percentage */
          if (discharging)
            percentage--;
          else
            percentage++;

          /* store */
          gtk_list_store_set (GTK_LIST_STORE (model), &iter, COLUMN_PERCENTAGE, CLAMP (percentage, 0, 100));

          /* update hal */
          battery_simulator_device_update (model, &iter);
        }

      /* cleanup */
      gtk_tree_path_free (path);

      return TRUE;
    }
  else
    {
      return FALSE;
    }
}



static void
battery_simulator_timeout_destroyed (gpointer user_data)
{
  /* free the row reference */
  gtk_tree_row_reference_free (user_data);
}



static void
battery_simulator_percentage_changed (GtkRange         *range,
                                      BatterySimulator *simulator)
{
  gint        percentage;
  GtkTreeIter iter;

  /* get active iter */
  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (simulator->widget_combo), &iter))
    return;

  /* get value */
  percentage = gtk_range_get_value (GTK_RANGE (range));

  /* store value */
  gtk_list_store_set (simulator->store, &iter, COLUMN_PERCENTAGE, percentage, -1);

  /* update status */
  battery_simulator_device_update (GTK_TREE_MODEL (simulator->store), &iter);
}



static void
battery_simulator_button_toggled (GtkWidget        *button,
                                  BatterySimulator *simulator)
{
  gboolean             active;
  GtkTreeIter          iter;
  guint                timeout_id;
  GtkTreePath         *path;
  GtkTreeRowReference *reference;

  /* get active iter */
  if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (simulator->widget_combo), &iter))
    return;

  /* active state */
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  if (button == simulator->widget_present)
    {
      /* store value */
      gtk_list_store_set (simulator->store, &iter, COLUMN_PRESENT, active, -1);
    }
  else if (button == simulator->widget_discharging)
    {
      /* store value */
      gtk_list_store_set (simulator->store, &iter, COLUMN_DISCHARGING, active, -1);
    }
  else if (button == simulator->widget_automatic)
    {
      /* get timeout id */
      gtk_tree_model_get (GTK_TREE_MODEL (simulator->store), &iter, COLUMN_TIMEOUT_ID, &timeout_id, -1);

      if (active && timeout_id == 0)
        {
          /* get the row reference */
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (simulator->store), &iter);
          reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (simulator->store), path);
          gtk_tree_path_free (path);

          /* create new timeout */
          timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 10*1000, battery_simulator_timeout,
                                           reference, battery_simulator_timeout_destroyed);

          /* set value */
          gtk_list_store_set (simulator->store, &iter, COLUMN_TIMEOUT_ID, timeout_id, -1);
        }
      else if (!active && timeout_id != 0)
        {
          /* stop timeout */
          g_source_remove (timeout_id);

          /* set value */
          gtk_list_store_set (simulator->store, &iter, COLUMN_TIMEOUT_ID, 0, -1);
        }
    }

  /* update status */
  battery_simulator_device_update (GTK_TREE_MODEL (simulator->store), &iter);
}



static void
battery_simulator_button_clicked (GtkWidget        *button,
                                  BatterySimulator *simulator)
{
  GtkTreeIter  iter;
  gchar       *udi;
  static gint  id = 10;

  if (button == simulator->widget_add)
    {
      /* try to create a new device */
      udi = battery_simulator_device_new (id);

      if (G_LIKELY (udi))
        {
          /* create new item */
          gtk_list_store_append (simulator->store, &iter);

          /* initialize device */
          battery_simulator_device_init (id, udi, simulator->store, &iter);

          /* set active combo iter */
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (simulator->widget_combo), &iter);

          /* increase id */
          id++;

          /* cleanup */
          g_free (udi);
        }
    }
  else if (button == simulator->widget_remove)
    {

    }
}



static void
battery_simulator_combo_changed (GtkComboBox      *combobox,
                                 BatterySimulator *simulator)
{
  GtkTreeIter  iter;
  gboolean     present, discharging;
  gint         percentage, capacity;
  guint        timeout_id;
  gchar       *udi, *capacity_str;

  /* get active iter */
  if (!gtk_combo_box_get_active_iter (combobox, &iter))
    return;

  /* get data from model */
  gtk_tree_model_get (GTK_TREE_MODEL (simulator->store), &iter,
                      COLUMN_UDI, &udi,
                      COLUMN_PERCENTAGE, &percentage,
                      COLUMN_CAPACITY, &capacity,
                      COLUMN_PRESENT, &present,
                      COLUMN_DISCHARGING, &discharging,
                      COLUMN_TIMEOUT_ID, &timeout_id, -1);

  /* capacity string */
  capacity_str = g_strdup_printf ("%d mAh", capacity);

  /* update dialog */
  gtk_label_set_text (GTK_LABEL (simulator->widget_udi), udi);
  gtk_label_set_text (GTK_LABEL (simulator->widget_capacity), capacity_str);
  gtk_range_set_value (GTK_RANGE (simulator->widget_percentage), percentage);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (simulator->widget_present), present);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (simulator->widget_discharging), discharging);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (simulator->widget_automatic), timeout_id != 0);

  /* cleanup */
  g_free (capacity_str);
  g_free (udi);
}



static GtkWidget *
battery_simulator_dialog_new (BatterySimulator *simulator)
{
  GtkWidget       *dialog;
  GtkWidget       *vbox_main;
  GtkWidget       *frame;
  GtkWidget       *table;
  GtkWidget       *label;
  GtkWidget       *scale;
  GtkWidget       *hbox;
  GtkWidget       *button;
  GtkWidget       *combo;
  GtkCellRenderer *cell;
  GtkWidget       *image;

  /* create dialg */
  dialog = xfce_titled_dialog_new_with_buttons ("HAL Battery Simulator",
                                                NULL, GTK_DIALOG_NO_SEPARATOR,
                                                GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                                NULL);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog), "Created for the Xfce battery monitor plugin.");
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "utilities-system-monitor");

  vbox_main = GTK_DIALOG (dialog)->vbox;
  gtk_container_set_border_width (GTK_CONTAINER (vbox_main), BORDER * 2);

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (vbox_main), frame, FALSE, TRUE, 0);

  table = gtk_table_new (6, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), BORDER * 2);
  gtk_table_set_row_spacings (GTK_TABLE (table), BORDER);
  gtk_container_add (GTK_CONTAINER (frame), table);
  gtk_container_set_border_width (GTK_CONTAINER (table), BORDER);

  /* udi */
  label = gtk_label_new ("<i>UDI:</i>");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, (GTK_FILL), (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  label = simulator->widget_udi = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, (GTK_EXPAND | GTK_FILL), (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* capacity */
  label = gtk_label_new ("<i>Capacity:</i>");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, (GTK_FILL), (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  label = simulator->widget_capacity = gtk_label_new ("");
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 1, 2, (GTK_EXPAND | GTK_FILL), (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);

  /* percentage */
  label = gtk_label_new ("<i>Percentage:</i>");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, (GTK_FILL), (GTK_FILL), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  scale = simulator->widget_percentage = gtk_hscale_new_with_range (0, 100, 1);
  gtk_scale_set_value_pos (GTK_SCALE (scale), GTK_POS_RIGHT);
  gtk_scale_set_digits (GTK_SCALE (scale), 0);
  gtk_table_attach (GTK_TABLE (table), scale, 1, 2, 2, 3, (GTK_EXPAND | GTK_FILL), (GTK_FILL), 0, 0);
  g_signal_connect (G_OBJECT (scale), "value-changed", G_CALLBACK (battery_simulator_percentage_changed), simulator);

  button = simulator->widget_present = gtk_check_button_new_with_mnemonic ("Present");
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (battery_simulator_button_toggled), simulator);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 3, 4, (GTK_EXPAND | GTK_FILL), (GTK_FILL), 0, 0);

  button = simulator->widget_discharging = gtk_check_button_new_with_mnemonic ("Discharging");
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (battery_simulator_button_toggled), simulator);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 4, 5, (GTK_EXPAND | GTK_FILL), (GTK_FILL), 0, 0);

  button = simulator->widget_automatic = gtk_check_button_new_with_mnemonic ("Automatic (dis)charging");
  g_signal_connect (G_OBJECT (button), "toggled", G_CALLBACK (battery_simulator_button_toggled), simulator);
  gtk_table_attach (GTK_TABLE (table), button, 1, 2, 5, 6, (GTK_EXPAND | GTK_FILL), (GTK_FILL), 0, 0);

  /* frame label */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_frame_set_label_widget (GTK_FRAME (frame), hbox);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), BORDER);

  /* combo box */
  combo = simulator->widget_combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (simulator->store));
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (combo), "changed", G_CALLBACK (battery_simulator_combo_changed), simulator);

  /* text renderer for the combo box */
  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell, "text", COLUMN_LABEL, NULL);

  /* add button */
  button = simulator->widget_add = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (battery_simulator_button_clicked), simulator);

  image = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  /* remove button */
  button = simulator->widget_remove = gtk_button_new ();
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (battery_simulator_button_clicked), simulator);

  image = gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);

  /* show all widgets inside dialog */
  gtk_widget_show_all (vbox_main);

  return dialog;
}



static gboolean
battery_simulator_connect (void)
{
  DBusError error;

  /* init error */
  dbus_error_init (&error);

  /* create new dbus connection */
  dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (G_UNLIKELY (dbus_connection == NULL))
    return FALSE;

  /* connect dbus to the main loop */
  dbus_connection_setup_with_g_main (dbus_connection, NULL);

  /* create new hal context */
  context = libhal_ctx_new ();
  if (G_UNLIKELY (context == NULL))
    return FALSE;

  /* connect the dbus connection to the hal context */
  if (G_UNLIKELY (!libhal_ctx_set_dbus_connection (context, dbus_connection)))
    return FALSE;

  /* initialize the hal context */
  if (G_UNLIKELY (!libhal_ctx_init (context, &error)))
    return FALSE;

  /* cleanup */
  LIBHAL_FREE_DBUS_ERROR (&error);

  return TRUE;
}



static void
battery_simulator_disconnect (void)
{
  DBusError error;

  /* init error */
  dbus_error_init (&error);

  /* free the contect */
  if (G_LIKELY (context))
    {
      libhal_ctx_shutdown (context, &error);
      libhal_ctx_free (context);
    }

  /* free the dbus connection */
  if (G_LIKELY (dbus_connection))
    dbus_connection_unref (dbus_connection);

  /* cleanup */
  LIBHAL_FREE_DBUS_ERROR (&error);
}



gint
main (gint argc, gchar **argv)
{
  GtkWidget        *dialog;
  BatterySimulator *simulator;

  gtk_init (&argc, &argv);

  /* connect to hal */
  if (battery_simulator_connect () == FALSE)
    return 1;

  /* create structure */
  simulator = g_new0 (BatterySimulator, 1);

  /* create store */
  simulator->store = gtk_list_store_new (N_COLUMNS,
                                         G_TYPE_STRING,   /* COLUMN_LABEL */
                                         G_TYPE_STRING,   /* COLUMN_UDI */
                                         G_TYPE_INT,      /* COLUMN_PERCENTAGE */
                                         G_TYPE_INT,      /* COLUMN_LAST_FULL */
                                         G_TYPE_BOOLEAN,  /* COLUMN_PRESENT */
                                         G_TYPE_BOOLEAN,  /* COLUMN_DISCHARGING */
                                         G_TYPE_UINT);    /* COLUMN_TIMEOUT_ID */

  /* create window */
  dialog = battery_simulator_dialog_new (simulator);

  /* add battery */
  battery_simulator_button_clicked (simulator->widget_add, simulator);

  /* show the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* destroy the dialog */
  gtk_widget_destroy (dialog);

  /* stop all running timeouts */
  gtk_tree_model_foreach (GTK_TREE_MODEL (simulator->store), battery_simulator_device_remove, simulator);

  /* clear and release store */
  gtk_list_store_clear (simulator->store);
  g_object_unref (G_OBJECT (simulator->store));

  /* free structure */
  g_free (simulator);

  /* disconnect */
  battery_simulator_disconnect ();

  return 0;
}
