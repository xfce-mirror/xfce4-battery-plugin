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

#define SPACING (6)

#include <libxfcegui4/libxfcegui4.h>

#include <panel-plugin/battery-preferences.h>
#include <panel-plugin/battery-plugin.h>
#include <panel-plugin/battery-monitor.h>



static void     battery_preferences_dialog_class_init      (BatteryPreferencesDialogClass *klass);
static void     battery_preferences_dialog_init            (BatteryPreferencesDialog      *dialog);
static void     battery_preferences_dialog_get_property    (GObject                       *object,
                                                            guint                          prop_id,
                                                            GValue                        *value,
                                                            GParamSpec                    *pspec);
static void     battery_preferences_dialog_set_property    (GObject                       *object,
                                                            guint                          prop_id,
                                                            const GValue                  *value,
                                                             GParamSpec                    *pspec);
static void     battery_preferences_dialog_finalize        (GObject                       *object);
static void     battery_preferences_dialog_response        (GtkDialog                     *dialog,
                                                             gint                           response_id);
static gboolean battery_preferences_separator_func         (GtkTreeModel                  *model,
                                                            GtkTreeIter                   *iter,
                                                            gpointer                       user_data);
static void     battery_preferences_dialog_toggled         (GtkWidget                     *button,
                                                            BatteryPreferencesDialog      *dialog);
static void     battery_preferences_visible_device_changed (GtkComboBox                   *combo_box,
                                                            BatteryPreferencesDialog      *dialog);



struct _BatteryPreferencesDialogClass
{
  XfceTitledDialogClass __parent__;
};

struct _BatteryPreferencesDialog
{
  XfceTitledDialog __parent__;

  /* the plugin */
  BatteryPlugin *plugin;

  /* widgets */
  GtkWidget *widget_combo;
  GtkWidget *widget_frame;
  GtkWidget *widget_status_icon;
  GtkWidget *widget_progressbar;
  GtkWidget *widget_label;
};

enum
{
  PROP_0,
  PROP_PLUGIN,
};

enum
{
  COLUMN_TEXT,
  COLUMN_UDI,
  COLUMN_SEP,
  N_COLUMNS
};



G_DEFINE_TYPE (BatteryPreferencesDialog, battery_preferences_dialog, XFCE_TYPE_TITLED_DIALOG);



static void
battery_preferences_dialog_class_init (BatteryPreferencesDialogClass *klass)
{
  GObjectClass   *gobject_class;
  GtkDialogClass *gtkdialog_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = battery_preferences_dialog_get_property;
  gobject_class->set_property = battery_preferences_dialog_set_property;
  gobject_class->finalize = battery_preferences_dialog_finalize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = battery_preferences_dialog_response;

  g_object_class_install_property (gobject_class,
                                   PROP_PLUGIN,
                                   g_param_spec_pointer ("plugin", "plugin", "plugin",
                                                         G_PARAM_READWRITE));
}



static void
battery_preferences_dialog_init (BatteryPreferencesDialog *dialog)
{
  GtkWidget       *dialog_vbox;
  GtkWidget       *vbox;
  GtkWidget       *hbox;
  GtkWidget       *frame;
  GtkWidget       *label;
  GtkWidget       *alignment;
  GtkListStore    *store;
  GtkCellRenderer *cell;

  /* setup dialog */
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "xfce4-settings");
  gtk_window_set_title (GTK_WINDOW (dialog), _("Battery Monitor"));
  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  dialog_vbox = gtk_vbox_new (FALSE, SPACING);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), dialog_vbox, TRUE, TRUE, 0);

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), frame, FALSE, TRUE, 0);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

  label = gtk_label_new (_("<b>General</b>"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), SPACING, SPACING, SPACING * 3, SPACING);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  hbox = gtk_hbox_new (FALSE, SPACING * 2);
  gtk_container_add (GTK_CONTAINER (alignment), hbox);

  label = gtk_label_new_with_mnemonic (_("_Visible battery:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  store = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

  dialog->widget_combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
  gtk_box_pack_start (GTK_BOX (hbox), dialog->widget_combo, TRUE, TRUE, 0);
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (dialog->widget_combo), battery_preferences_separator_func, NULL, NULL);
  g_signal_connect (G_OBJECT (dialog->widget_combo), "changed", G_CALLBACK (battery_preferences_visible_device_changed), dialog);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), dialog->widget_combo);

  cell = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dialog->widget_combo), cell, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (dialog->widget_combo), cell, "text", COLUMN_TEXT, NULL);

  g_object_unref (G_OBJECT (store));

  frame = gtk_frame_new (NULL);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), frame, FALSE, TRUE, 0);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);

  label = gtk_label_new (_("<b>Appearance</b>"));
  gtk_frame_set_label_widget (GTK_FRAME (frame), label);
  gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

  alignment = gtk_alignment_new (0.5, 0.5, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), SPACING, SPACING, SPACING * 3, SPACING);
  gtk_container_add (GTK_CONTAINER (frame), alignment);

  vbox = gtk_vbox_new (FALSE, SPACING);
  gtk_container_add (GTK_CONTAINER (alignment), vbox);

  dialog->widget_frame = gtk_check_button_new_with_mnemonic (_("Show _frame border"));
  g_signal_connect (G_OBJECT (dialog->widget_frame), "toggled", G_CALLBACK (battery_preferences_dialog_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->widget_frame, FALSE, FALSE, 0);

  dialog->widget_status_icon = gtk_check_button_new_with_mnemonic (_("Show status _icon"));
  g_signal_connect (G_OBJECT (dialog->widget_status_icon), "toggled", G_CALLBACK (battery_preferences_dialog_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->widget_status_icon, FALSE, FALSE, 0);

  dialog->widget_progressbar = gtk_check_button_new_with_mnemonic (_("Show progress _bar"));
  g_signal_connect (G_OBJECT (dialog->widget_progressbar), "toggled", G_CALLBACK (battery_preferences_dialog_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->widget_progressbar, FALSE, FALSE, 0);

  dialog->widget_label = gtk_check_button_new_with_mnemonic (_("Show _label"));
  g_signal_connect (G_OBJECT (dialog->widget_label ), "toggled", G_CALLBACK (battery_preferences_dialog_toggled), dialog);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->widget_label, FALSE, FALSE, 0);
}



static void
battery_preferences_dialog_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  BatteryPreferencesDialog *dialog = BATTERY_PREFERENCES_DIALOG (object);

  switch (prop_id)
    {
      case PROP_PLUGIN:
        g_value_set_pointer (value, battery_preferences_dialog_get_plugin (dialog));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
battery_preferences_dialog_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  BatteryPreferencesDialog *dialog = BATTERY_PREFERENCES_DIALOG (object);

  switch (prop_id)
    {
      case PROP_PLUGIN:
        battery_preferences_dialog_set_plugin (dialog, g_value_get_pointer (value));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}



static void
battery_preferences_dialog_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (battery_preferences_dialog_parent_class)->finalize) (object);
}



static void
battery_preferences_dialog_response (GtkDialog *widget,
                                     gint       response_id)
{
  BatteryPreferencesDialog *dialog = BATTERY_PREFERENCES_DIALOG (widget);

  /* unblock menu */
  xfce_panel_plugin_unblock_menu (dialog->plugin->panel_plugin);

  /* destroy the dialog */
  gtk_widget_destroy (GTK_WIDGET (widget));
}



static gboolean
battery_preferences_separator_func (GtkTreeModel *model,
                                    GtkTreeIter  *iter,
                                    gpointer      user_data)
{
  gboolean separator = FALSE;

  /* whether the separator bool is true */
  gtk_tree_model_get (model, iter, COLUMN_SEP, &separator, -1);

  return separator;
}



static void
battery_preferences_visible_device_changed (GtkComboBox              *combo_box,
                                            BatteryPreferencesDialog *dialog)
{
  gchar        *udi;
  GtkTreeModel *model;
  GtkTreeIter   iter;

  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      /* get model */
      model = gtk_combo_box_get_model (combo_box);

      /* get selected udi from model */
      gtk_tree_model_get (model, &iter, COLUMN_UDI, &udi, -1);

      /* free the old udi */
      g_free (dialog->plugin->visible_device);

      /* set new device */
      dialog->plugin->visible_device = udi;

      /* update plugin */
      battery_plugin_update (dialog->plugin);
    }
}



static void
battery_preferences_dialog_toggled (GtkWidget                *button,
                                    BatteryPreferencesDialog *dialog)
{
  gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  g_return_if_fail (BATTERY_IS_PREFERENCES_DIALOG (dialog));

  if (button == dialog->widget_frame)
    {
      dialog->plugin->show_frame = active;
      gtk_frame_set_shadow_type (GTK_FRAME (dialog->plugin->frame), active ? GTK_SHADOW_IN : GTK_SHADOW_NONE);
    }
  else if (button == dialog->widget_status_icon)
    {
      dialog->plugin->show_status_icon = active;
    }
  else if (button == dialog->widget_progressbar)
    {
      dialog->plugin->show_progressbar = active;
    }
  else if (button == dialog->widget_label)
    {
      dialog->plugin->show_label = active;
    }

  /* update plugin */
  battery_plugin_update (dialog->plugin);
}



void
battery_preferences_dialog_new (BatteryPlugin *plugin)
{
  GtkWidget *dialog;

  /* create dialog */
  dialog = g_object_new (BATTERY_TYPE_PREFERENCES_DIALOG, "plugin", plugin, NULL);

  /* show the dialog */
  gtk_widget_show_all (dialog);
}



void
battery_preferences_dialog_set_plugin (BatteryPreferencesDialog *dialog,
                                       BatteryPlugin            *plugin)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GList        *devices, *li;
  BatteryInfo  *info;
  gchar        *name;
  gint          n;

  g_return_if_fail (BATTERY_IS_PREFERENCES_DIALOG (dialog));

  /* unblock menu of previous plugin */
  if (dialog->plugin)
      xfce_panel_plugin_unblock_menu (dialog->plugin->panel_plugin);

  /* set plugin */
  dialog->plugin = plugin;

  /* block menu */
  xfce_panel_plugin_block_menu (plugin->panel_plugin);

  /* update toggle buttons */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->widget_frame), plugin->show_frame);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->widget_status_icon), plugin->show_status_icon);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->widget_progressbar), plugin->show_progressbar);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->widget_label), plugin->show_label);

  /* get combo model */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->widget_combo));

  /* clear store */
  gtk_list_store_clear (GTK_LIST_STORE (model));

  /* add the global battery */
  if (battery_monitor_has_global (plugin->monitor))
    {
      /* add global item */
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COLUMN_TEXT, _("Global"),
                          COLUMN_UDI, GLOBAL_DEVICE_UDI,
                          COLUMN_SEP, FALSE, -1);

      /* set as active devices */
      if (plugin->visible_device && strcmp (GLOBAL_DEVICE_UDI, plugin->visible_device) == 0)
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (dialog->widget_combo), &iter);

      /* separator */
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COLUMN_SEP, TRUE, -1);
    }

  /* get devices */
  devices = battery_monitor_get_devices (plugin->monitor);

  for (li = devices, n = 1; li != NULL; li = li->next, n++)
    {
      info = li->data;

      /* create device name */
      if (info->model)
        name = g_strdup_printf (_("Battery %s"), info->model);
      else
        name = g_strdup_printf (_("Battery #%d"), n);

      /* append device */
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COLUMN_TEXT, name,
                          COLUMN_UDI, info->udi,
                          COLUMN_SEP, FALSE, -1);

      /* cleanup */
      g_free (name);

      /* check if this is the active item */
      if (info->udi && plugin->visible_device
          && strcmp (info->udi, plugin->visible_device) == 0)
        gtk_combo_box_set_active_iter (GTK_COMBO_BOX (dialog->widget_combo), &iter);
    }

  /* cleanup */
  g_list_free (devices);
}



BatteryPlugin *
battery_preferences_dialog_get_plugin (BatteryPreferencesDialog *dialog)
{
  g_return_val_if_fail (BATTERY_IS_PREFERENCES_DIALOG (dialog), NULL);

  return dialog->plugin;
}
