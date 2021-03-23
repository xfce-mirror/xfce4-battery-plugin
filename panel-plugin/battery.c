/* Copyright (c) 2003 Nicholas Penwarden <toth64@yahoo.com>
 * Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 * Copyright (c) 2003 Edscott Wilson Garcia <edscott@users.sourceforge.net>
 * Copyright (c) 2005 Eduard Roccatello <eduard@xfce.org>
 * Copyright (c) 2006 Nick Schermer <nick@xfce.org>
 * Copyright (c) 2010 Florian Rivoal <frivoal@xfce.org>
 * Copyright (c) 2012 Landry Breuil <landry@xfce.org>
 * Copyright (c) 2016 Andre Miranda <andreldm@xfce.org>
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

#if (defined(__OpenBSD__) || defined(__NetBSD__))
#include <sys/param.h>
#include <sys/ioctl.h>
#include <machine/apmvar.h>
#define APMDEVICE "/dev/apm"
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

#include "libacpi.h"

#include <sys/time.h>
#include <time.h>

#define AC_COLOR        "#8888FF"
#define HIGH_COLOR      "#00ff00"
#define LOW_COLOR       "#ffff00"
#define CRITICAL_COLOR  "#ff0000"
#define AVERAGING_CYCLE 5
#define PLUGIN_WEBSITE  "https://docs.xfce.org/panel-plugins/xfce4-battery-plugin"

typedef struct
{
    gboolean    display_label;
    gboolean    display_icon;
    gboolean    display_power;
    gboolean    display_percentage;
    gboolean    display_bar;
    gboolean    display_time;
    gboolean    hide_when_full;
    gboolean    tooltip_display_percentage;
    gboolean    tooltip_display_time;
    int         low_percentage;
    int         critical_percentage;
    int         action_on_low;
    int         action_on_critical;
    char       *command_on_low;
    char       *command_on_critical;
    GdkRGBA     colorA;
    GdkRGBA     colorH;
    GdkRGBA     colorL;
    GdkRGBA     colorC;
} t_battmon_options;

typedef struct
{
    XfcePanelPlugin *plugin;

    GtkWidget         *ebox, *timechargebox, *actempbox;
    GtkWidget         *battstatus;
    int                timeoutid; /* To update apm status */
    gboolean           low;
    gboolean           critical;
    t_battmon_options  options;
    GtkLabel          *label;
    GtkLabel          *charge;
    GtkLabel          *rtime;
    GtkLabel          *acfan;
    GtkLabel          *temp;
    GtkWidget         *image;
    GtkCssProvider    *css_provider;
} t_battmon;

typedef struct
{
    GtkWidget        *cb_disp_power;
    GtkWidget        *cb_disp_label;
    GtkWidget        *cb_disp_percentage;
    GtkWidget        *cb_disp_bar;
    GtkWidget        *cb_disp_time;
    GtkWidget        *cb_hide_when_full;
    GtkWidget        *cb_disp_tooltip_percentage;
    GtkWidget        *cb_disp_tooltip_time;
    GtkWidget        *cb_disp_icon;
    GtkWidget        *sb_low_percentage;
    GtkWidget        *sb_critical_percentage;
    GtkWidget        *co_action_low;
    GtkWidget        *co_action_critical;
    GtkWidget        *en_command_low;
    GtkWidget        *en_command_critical;
    GtkWidget        *ac_color_button;
    GtkWidget        *high_color_button;
    GtkWidget        *low_color_button;
    GtkWidget        *critical_color_button;
    t_battmon        *battmon;
} t_battmon_dialog;

enum {BM_DO_NOTHING, BM_MESSAGE, BM_COMMAND, BM_COMMAND_TERM};
enum {BM_BROKEN, BM_USE_ACPI, BM_USE_APM};
enum {BM_MISSING, BM_CRITICAL, BM_CRITICAL_CHARGING, BM_LOW, BM_LOW_CHARGING, BM_OK, BM_OK_CHARGING, BM_FULL, BM_FULL_CHARGING};

static gboolean battmon_set_size(XfcePanelPlugin *plugin, int size, t_battmon *battmon);

static void
init_options(t_battmon_options *options)
{
    options->display_icon = FALSE;
    options->display_label = FALSE;
    options->display_power = FALSE;
    options->display_percentage = TRUE;
    options->display_bar = TRUE;
    options->display_time = FALSE;
    options->tooltip_display_percentage = FALSE;
    options->tooltip_display_time = FALSE;
    options->low_percentage = 10;
    options->critical_percentage = 5;
    options->action_on_low = 1;
    options->action_on_critical = 1;
    options->command_on_low = NULL;
    options->command_on_critical = NULL;
    gdk_rgba_parse(&(options->colorA), AC_COLOR);
    gdk_rgba_parse(&(options->colorH), HIGH_COLOR);
    gdk_rgba_parse(&(options->colorL), LOW_COLOR);
    gdk_rgba_parse(&(options->colorC), CRITICAL_COLOR);
}

static void
update_apm_status(t_battmon *battmon)
{
    int method = BM_BROKEN;
    int present = 0, charge = 0, rate = 0;
    int lcapacity = 0, ccapacity = 0;
    gboolean fan = FALSE;
    const char *temp;
    static int old_state = -1, new_state = BM_MISSING;
    gchar *icon_name = NULL;
    int time_remaining = 0;
    gboolean acline = FALSE;
    gchar buffer[128];
    gchar *css, *color_str;
    GtkWidget *dialog;
    GdkRGBA *color;
    gchar *cssminsizes;

    static int update_time = AVERAGING_CYCLE;
    static int sum_lcapacity = 0;
    static int sum_ccapacity = 0;
    static int sum_rate = 0;

    static int last_ccapacity = 0;
    static int last_lcapacity = 0;
    static int last_rate = 0;
    static int last_acline = 0;
    static int last_present = 0;

#if defined(__OpenBSD__) || defined(__NetBSD__)
  /* Code for OpenBSD by Joe Ammond <jra@twinight.org>. Using the same
     procedure as for FreeBSD.
     Made to work on NetBSD by Stefan Sperling <stsp@stsp.in-berlin.de>
  */
    struct apm_power_info apm;
    int fd;

    fd = open(APMDEVICE, O_RDONLY);
    if (fd == -1) return;
    if (ioctl(fd, APM_IOC_GETPOWER, &apm) == -1)
        return;
    close(fd);
    charge = apm.battery_life;
    time_remaining = apm.minutes_left;
    acline = apm.ac_state ? TRUE : FALSE;
    method = BM_USE_APM;

#else
    DBG ("Updating battery status...");

    /* Check ACPI for AC adapter and battery presence */
    if (check_acpi() == 0) {
        int i;
        method = BM_USE_ACPI;
        acline = read_acad_state();
        for (i=0;i<batt_count;i++) {
            if (!read_acpi_info(i) || !read_acpi_state(i))
                continue;
            present++;
            lcapacity += acpiinfo->last_full_capacity;
            ccapacity += acpistate->rcapacity;
            rate += acpistate->prate;
        }

        sum_lcapacity += lcapacity;
        sum_ccapacity += ccapacity;
        sum_rate += rate;

        update_time++;
        if (update_time >= AVERAGING_CYCLE || last_acline != acline || last_present != present) {
            if (last_acline != acline || last_present != present) {
                last_ccapacity = ccapacity;
                last_lcapacity = lcapacity;
                last_rate = rate;
            } else {
                last_ccapacity = ccapacity = (float)(sum_ccapacity)/(float)(update_time);
                last_lcapacity = lcapacity = (float)(sum_lcapacity)/(float)(update_time);
                last_rate = rate = (float)(sum_rate)/(float)(update_time);
            }
            update_time = 0;
            sum_ccapacity = sum_lcapacity = sum_rate = 0;
        } else {
            ccapacity = last_ccapacity;
            lcapacity = last_lcapacity;
            rate = last_rate;
        }

        charge = (((float)ccapacity)/((float)lcapacity))*100;

        if (last_acline)
            time_remaining = ((float)(lcapacity-ccapacity)/(float)(rate))*60;
        else
            time_remaining = ((float)(ccapacity)/(float)(rate))*60;

        if (time_remaining < 0)
            time_remaining = 0;

        last_acline = acline;
        last_present = present;
    }
#endif

    DBG("method=%d, acline=%d, time_remaining=%d, charge=%d", method, acline, time_remaining, charge);

    charge = CLAMP (charge, 0, 100);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(battmon->battstatus), charge / 100.0);
    if (battmon->options.display_bar) {
        gtk_widget_show(GTK_WIDGET(battmon->battstatus));
    } else {
        gtk_widget_hide(GTK_WIDGET(battmon->battstatus));
    }

    if (battmon->options.display_label) {
        gtk_widget_show(GTK_WIDGET(battmon->label));
    } else {
        gtk_widget_hide(GTK_WIDGET(battmon->label));
    }

    if (battmon->options.display_icon) {
        if ((method == BM_USE_ACPI && present == 0) || (method == BM_USE_APM && charge == 0)) {
            /* battery missing */
            icon_name = g_strdup("xfce4-battery-missing");
            new_state = BM_MISSING;
        } else if (charge <= battmon->options.critical_percentage) {
            /* battery critical */
            icon_name = g_strdup("xfce4-battery-critical");
            new_state = BM_CRITICAL;
        } else if (charge <= battmon->options.low_percentage) {
            /* battery low */
            icon_name = g_strdup("xfce4-battery-low");
            new_state = BM_LOW;
        } else if (charge < 99) {
            /* battery ok */
            icon_name = g_strdup("xfce4-battery-ok");
            new_state = BM_OK;
        } else {
            /* battery full */
            icon_name = g_strdup("xfce4-battery-full");
            new_state = BM_FULL;
        }

        if (acline && new_state != BM_MISSING) {
            gchar *tmp = icon_name;
            new_state++;
            icon_name = g_strconcat(tmp, "-charging", NULL);
            g_free(tmp);
        }

        DBG("old_state=%d, new_state=%d, icon_name=%s", old_state, new_state, icon_name);

        if (old_state != new_state)
            xfce_panel_image_set_from_source(XFCE_PANEL_IMAGE(battmon->image), icon_name);
        if (icon_name)
            g_free(icon_name);

        old_state = new_state;
        gtk_widget_show(battmon->image);
    } else {
        gtk_widget_hide(battmon->image);
    }

    if (battmon->options.display_percentage && charge > 0 && !(battmon->options.hide_when_full && acline && charge >= 99)) {
        gtk_widget_show(GTK_WIDGET(battmon->charge));
        gtk_widget_show(GTK_WIDGET(battmon->timechargebox));
        g_snprintf(buffer, sizeof(buffer),"%d%% ", charge);
        gtk_label_set_text(battmon->charge,buffer);
    } else {
        gtk_widget_hide(GTK_WIDGET(battmon->charge));
    }

    if (battmon->options.display_time && time_remaining > 0 && !(battmon->options.hide_when_full && acline && charge >= 99 )) {
        gtk_widget_show(GTK_WIDGET(battmon->rtime));
        gtk_widget_show(GTK_WIDGET(battmon->timechargebox));
        g_snprintf(buffer, sizeof(buffer),"%02d:%02d ",time_remaining/60,time_remaining%60);
        gtk_label_set_text(battmon->rtime,buffer);

    } else {
        gtk_widget_hide(GTK_WIDGET(battmon->rtime));
    }

    if ((!battmon->options.display_time && !battmon->options.display_percentage) || (battmon->options.hide_when_full && acline && charge >= 99 )) {
        gtk_widget_hide(GTK_WIDGET(battmon->timechargebox));
    }

    if (acline) {
        char *t;

        if ((method == BM_USE_ACPI && present == 0) || (method == BM_USE_APM && charge == 0)) {
            t=_("(No battery, AC on-line)");
        } else {
            t=(charge<99.9)?_("(Charging from AC)"):_("(AC on-line)");
        }

        if (battmon->options.tooltip_display_percentage) {
            g_snprintf(buffer, sizeof(buffer), "%d%% %s", charge,t);
        }
        else
            g_snprintf(buffer, sizeof(buffer), "%s",t);
    } else {
        if (battmon->options.tooltip_display_percentage && battmon->options.tooltip_display_time)
            g_snprintf(buffer, sizeof(buffer), _("%d%% (%02d:%02d) remaining"), charge, time_remaining / 60, time_remaining % 60);
        else if (battmon->options.tooltip_display_time)
            g_snprintf(buffer, sizeof(buffer), _("%02d:%02d remaining"),time_remaining / 60, time_remaining % 60);
        else if (battmon->options.tooltip_display_percentage)
            g_snprintf(buffer, sizeof(buffer), _("%d%% remaining"), charge);
        else
            g_snprintf(buffer, sizeof(buffer), _("AC off-line"));
    }

    gtk_widget_set_tooltip_text(GTK_WIDGET(battmon->ebox), buffer);

    if (battmon->options.display_power) {
        gtk_widget_show(GTK_WIDGET(battmon->acfan));
        gtk_widget_show(GTK_WIDGET(battmon->temp));
        gtk_widget_show_all(GTK_WIDGET(battmon->actempbox));

        fan=get_fan_status();
        if (acline && fan)
            gtk_label_set_text(battmon->acfan,"AC FAN");
        else if (acline && !fan)
            gtk_label_set_text(battmon->acfan,"AC");
        else if (!acline && fan)
            gtk_label_set_text(battmon->acfan,"FAN");
        else {
            gtk_label_set_text(battmon->acfan,"");
            gtk_widget_hide(GTK_WIDGET(battmon->acfan));
        }

        temp=get_temperature();
        DBG ("Temp: %s", temp);
        if (temp)
            gtk_label_set_text(battmon->temp,temp);
        else {
            gtk_label_set_text(battmon->temp,"");
            gtk_widget_hide(GTK_WIDGET(battmon->temp));
        }
    } else {
        gtk_widget_hide(GTK_WIDGET(battmon->acfan));
        gtk_widget_hide(GTK_WIDGET(battmon->temp));
        gtk_widget_hide(GTK_WIDGET(battmon->actempbox));
    }

    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(battmon->battstatus), NULL);

    /* bar colors and state flags */
    if (acline) {
        battmon->low = battmon->critical = FALSE;
        color = &battmon->options.colorA;
    }
    else {
        if (charge <= battmon->options.critical_percentage) {
            color = &battmon->options.colorC;
        }
        else if (charge <= battmon->options.low_percentage) {
            battmon->critical = FALSE;
            color = &battmon->options.colorL;
        }
        else {
            battmon->low = battmon->critical = FALSE;
            color = &battmon->options.colorH;
        }
    }

    color_str = gdk_rgba_to_string (color);
#if GTK_CHECK_VERSION (3, 20, 0)
    cssminsizes = "min-width: 4px; min-height: 0px";
    if (gtk_orientable_get_orientation(GTK_ORIENTABLE(battmon->battstatus)) == GTK_ORIENTATION_HORIZONTAL)
        cssminsizes = "min-width: 0px; min-height: 4px";
    css = g_strdup_printf("progressbar trough { %s } \
                           progressbar progress { %s ; \
                           background-color: %s; background-image: none; }",
                          cssminsizes, cssminsizes,
#else
    css = g_strdup_printf(".progressbar progress { background-color: %s; background-image: none; }",
#endif
                          color_str);

    gtk_css_provider_load_from_data (battmon->css_provider, css, strlen(css), NULL);
    g_free(css);
    g_free(color_str);

    /* alarms */
    if (method != BM_BROKEN && !acline && charge <= battmon->options.low_percentage) {
        if (!battmon->critical && charge <= battmon->options.critical_percentage) {
            battmon->critical = TRUE;

            if (battmon->options.action_on_critical == BM_MESSAGE) {
do_critical_warn:
                dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
                    _("WARNING: Your battery has reached critical status. You should plug in or shutdown your computer now to avoid possible data loss."));
                g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
                gtk_widget_show_all (dialog);
                return;
            }

            if (battmon->options.action_on_critical == BM_COMMAND ||
                battmon->options.action_on_critical == BM_COMMAND_TERM) {
                int interm=(battmon->options.action_on_critical == BM_COMMAND_TERM)?1:0;
                if (!battmon->options.command_on_critical ||
                    !strlen(battmon->options.command_on_critical)) goto do_critical_warn;
                xfce_spawn_command_line_on_screen(NULL, battmon->options.command_on_critical, interm, FALSE, NULL);
            }
        } else if (!battmon->low) {
            battmon->low = TRUE;

            if (battmon->options.action_on_low == BM_MESSAGE) {
do_low_warn:
                dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
                _("WARNING: Your battery is running low. You should consider plugging in or shutting down your computer soon to avoid possible data loss."));
                g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
                gtk_widget_show_all (dialog);
                return;
            }

            if (battmon->options.action_on_low == BM_COMMAND ||
                battmon->options.action_on_low == BM_COMMAND_TERM) {
                int interm=(battmon->options.action_on_low == BM_COMMAND_TERM)?1:0;
                if (!battmon->options.command_on_low ||
                    !strlen(battmon->options.command_on_low)) goto do_low_warn;
                xfce_spawn_command_line_on_screen(NULL, battmon->options.command_on_low, interm, FALSE, NULL);
            }
        }
    }
}

static void
setup_battmon(t_battmon *battmon)
{
    gint size;

    size = xfce_panel_plugin_get_size (battmon->plugin);
    size /= xfce_panel_plugin_get_nrows (battmon->plugin);

    battmon->ebox = gtk_box_new(xfce_panel_plugin_get_orientation(battmon->plugin), 0);

    battmon->battstatus = gtk_progress_bar_new();

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(battmon->battstatus), 0.0);
    gtk_orientable_set_orientation(GTK_ORIENTABLE(battmon->battstatus),
                                   !xfce_panel_plugin_get_orientation(battmon->plugin));
    gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(battmon->battstatus),
                                  (xfce_panel_plugin_get_orientation(battmon->plugin) == GTK_ORIENTATION_HORIZONTAL));

    battmon->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider(
        GTK_STYLE_CONTEXT (gtk_widget_get_style_context (GTK_WIDGET (battmon->battstatus))),
        GTK_STYLE_PROVIDER (battmon->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    battmon->label = (GtkLabel *)gtk_label_new(_("Battery"));
    gtk_box_pack_start(GTK_BOX(battmon->ebox),GTK_WIDGET(battmon->label),FALSE, FALSE, 2);

    battmon->image = xfce_panel_image_new_from_source("xfce4-battery-plugin");
    xfce_panel_image_set_size(XFCE_PANEL_IMAGE(battmon->image), size);

    gtk_box_pack_start(GTK_BOX(battmon->ebox),GTK_WIDGET(battmon->image), FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(battmon->ebox),GTK_WIDGET(battmon->battstatus), FALSE, FALSE, 0);

    /* percent + rtime */
    /* create the label hvbox with an orientation opposite to the panel */
    battmon->timechargebox = gtk_box_new(!xfce_panel_plugin_get_orientation(battmon->plugin), 0);
    gtk_box_set_homogeneous(GTK_BOX(battmon->timechargebox), TRUE);
    gtk_box_pack_start(GTK_BOX(battmon->ebox), battmon->timechargebox, FALSE, FALSE, 0);

    battmon->charge = (GtkLabel *)gtk_label_new("--");
    gtk_box_pack_start(GTK_BOX(battmon->timechargebox),GTK_WIDGET(battmon->charge),TRUE, TRUE, 0);

    battmon->rtime = (GtkLabel *)gtk_label_new("--");
    gtk_box_pack_start(GTK_BOX(battmon->timechargebox),GTK_WIDGET(battmon->rtime),TRUE, TRUE, 0);

    /* ac-fan-temp */
    /* create the label hvbox with an orientation opposite to the panel */
    battmon->actempbox = gtk_box_new(!xfce_panel_plugin_get_orientation(battmon->plugin), 0);
    gtk_box_set_homogeneous(GTK_BOX(battmon->timechargebox), TRUE);
    gtk_box_pack_start(GTK_BOX(battmon->ebox), battmon->actempbox, FALSE, FALSE, 0);

    battmon->acfan = (GtkLabel *)gtk_label_new("--");
    gtk_box_pack_start(GTK_BOX(battmon->actempbox),GTK_WIDGET(battmon->acfan),TRUE, TRUE, 0);

    battmon->temp = (GtkLabel *)gtk_label_new("--");
    gtk_box_pack_start(GTK_BOX(battmon->actempbox),GTK_WIDGET(battmon->temp),TRUE, TRUE, 0);

    gtk_widget_show_all(battmon->ebox);
    if (!battmon->options.display_bar)
        gtk_widget_hide(GTK_WIDGET(battmon->battstatus));
    if (!battmon->options.display_label)
        gtk_widget_hide(GTK_WIDGET(battmon->label));
    if (!battmon->options.display_icon)
        gtk_widget_hide(battmon->image);
    if (!battmon->options.display_power) {
        gtk_widget_hide(GTK_WIDGET(battmon->acfan));
        gtk_widget_hide(GTK_WIDGET(battmon->temp));
        gtk_widget_hide(GTK_WIDGET(battmon->actempbox));
    }
    if (!battmon->options.display_percentage) {
        gtk_widget_hide(GTK_WIDGET(battmon->charge));
    }
    if (!battmon->options.display_time) {
        gtk_widget_hide(GTK_WIDGET(battmon->rtime));
    }
    if (!battmon->options.display_time && !battmon->options.display_percentage) {
        gtk_widget_hide(GTK_WIDGET(battmon->timechargebox));
    }

    gtk_widget_set_size_request(battmon->ebox, -1, -1);
}

static void
battmon_set_labels_orientation(t_battmon *battmon, GtkOrientation orientation)
{
    gint angle = (orientation == GTK_ORIENTATION_HORIZONTAL ? 0 : 270);
    gtk_label_set_angle(GTK_LABEL(battmon->label), angle);
    gtk_label_set_angle(GTK_LABEL(battmon->charge), angle);
    gtk_label_set_angle(GTK_LABEL(battmon->rtime), angle);
    gtk_label_set_angle(GTK_LABEL(battmon->acfan), angle);
    gtk_label_set_angle(GTK_LABEL(battmon->temp), angle);
}

static gboolean
battmon_set_mode (XfcePanelPlugin *plugin, XfcePanelPluginMode mode,
                  t_battmon *battmon)
{
    GtkOrientation orientation;
    DBG("set_mode(%d)", mode);

    orientation =
      (mode != XFCE_PANEL_PLUGIN_MODE_VERTICAL) ?
      GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    gtk_orientable_set_orientation(GTK_ORIENTABLE(battmon->ebox), xfce_panel_plugin_get_orientation(plugin));
    gtk_orientable_set_orientation(GTK_ORIENTABLE(battmon->timechargebox), !orientation);
    gtk_orientable_set_orientation(GTK_ORIENTABLE(battmon->actempbox), !orientation);

    gtk_orientable_set_orientation(GTK_ORIENTABLE(battmon->battstatus),
                                   !(xfce_panel_plugin_get_orientation(plugin)));
    gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(battmon->battstatus),
                                  (xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_HORIZONTAL));

    battmon_set_labels_orientation(battmon, orientation);
    battmon_set_size(plugin, xfce_panel_plugin_get_size (plugin), battmon);
    update_apm_status(battmon);
    xfce_panel_plugin_set_small(plugin, (mode != XFCE_PANEL_PLUGIN_MODE_DESKBAR));

    return TRUE;
}

static t_battmon*
battmon_create(XfcePanelPlugin *plugin)
{
    t_battmon *battmon;

    battmon = g_new(t_battmon, 1);
    init_options(&(battmon->options));

    battmon->plugin = plugin;

    battmon->low = FALSE;
    battmon->critical = FALSE;

    battmon->timeoutid = 0;

    return battmon;
}

static void
battmon_free(XfcePanelPlugin *plugin, t_battmon *battmon)
{
    if (battmon->timeoutid != 0) {
        g_source_remove(battmon->timeoutid);
        battmon->timeoutid = 0;
    }

    /* cleanup options */
    g_free (battmon->options.command_on_low);
    g_free (battmon->options.command_on_critical);

    g_free(battmon);
}

static void
battmon_read_config(XfcePanelPlugin *plugin, t_battmon *battmon)
{
    const char *value;
    char *file;
    XfceRc *rc;

    if (!(file = xfce_panel_plugin_lookup_rc_file (plugin)))
        return;

    rc = xfce_rc_simple_open (file, TRUE);
    g_free (file);

    if (!rc)
        return;

    battmon->options.display_label = xfce_rc_read_bool_entry (rc, "display_label", FALSE);

    battmon->options.display_icon = xfce_rc_read_bool_entry (rc, "display_icon", FALSE);

    battmon->options.display_power = xfce_rc_read_bool_entry (rc, "display_power", FALSE);

    battmon->options.display_percentage = xfce_rc_read_bool_entry (rc, "display_percentage", TRUE);

    battmon->options.display_bar = xfce_rc_read_bool_entry (rc, "display_bar", TRUE);

    battmon->options.display_time = xfce_rc_read_bool_entry (rc, "display_time", FALSE);

    battmon->options.tooltip_display_percentage = xfce_rc_read_bool_entry (rc, "tooltip_display_percentage", FALSE);

    battmon->options.tooltip_display_time = xfce_rc_read_bool_entry (rc, "tooltip_display_time", FALSE);

    battmon->options.low_percentage = xfce_rc_read_int_entry (rc, "low_percentage", 10);

    battmon->options.critical_percentage = xfce_rc_read_int_entry (rc, "critical_percentage", 5);

    battmon->options.action_on_low = xfce_rc_read_int_entry (rc, "action_on_low", 0);

    battmon->options.action_on_critical = xfce_rc_read_int_entry (rc, "action_on_critical", 0);

    battmon->options.hide_when_full = xfce_rc_read_int_entry (rc, "hide_when_full", 0);

    if ((value = xfce_rc_read_entry (rc, "colorA", NULL)) != NULL)
        gdk_rgba_parse(&battmon->options.colorA, value);
    if ((value = xfce_rc_read_entry (rc, "colorH", NULL)) != NULL)
        gdk_rgba_parse(&battmon->options.colorH, value);
    if ((value = xfce_rc_read_entry (rc, "colorL", NULL)) != NULL)
        gdk_rgba_parse(&battmon->options.colorL, value);
    if ((value = xfce_rc_read_entry (rc, "colorC", NULL)) != NULL)
        gdk_rgba_parse(&battmon->options.colorC, value);

    if ((value =  xfce_rc_read_entry (rc, "command_on_low", NULL)) && *value)
        battmon->options.command_on_low = g_strdup (value);

    if ((value =  xfce_rc_read_entry (rc, "command_on_critical", NULL)) && *value)
        battmon->options.command_on_critical = g_strdup (value);

    xfce_rc_close (rc);
}

static void
battmon_write_config(XfcePanelPlugin *plugin, t_battmon *battmon)
{
    XfceRc *rc;
    gchar *file;
    gchar *color_str;

    if (!(file = xfce_panel_plugin_save_location (plugin, TRUE)))
        return;

    rc = xfce_rc_simple_open (file, FALSE);
    g_free (file);

    if (!rc)
        return;

    xfce_rc_write_bool_entry (rc, "display_label", battmon->options.display_label);

    xfce_rc_write_bool_entry (rc, "display_icon", battmon->options.display_icon);

    xfce_rc_write_bool_entry (rc, "display_power", battmon->options.display_power);

    xfce_rc_write_bool_entry (rc, "display_percentage", battmon->options.display_percentage);

    xfce_rc_write_bool_entry (rc, "display_bar", battmon->options.display_bar);

    xfce_rc_write_bool_entry (rc, "display_time", battmon->options.display_time);

    xfce_rc_write_bool_entry (rc, "tooltip_display_percentage", battmon->options.tooltip_display_percentage);

    xfce_rc_write_bool_entry (rc, "tooltip_display_time", battmon->options.tooltip_display_time);

    xfce_rc_write_int_entry (rc, "low_percentage", battmon->options.low_percentage);

    xfce_rc_write_int_entry (rc, "critical_percentage", battmon->options.critical_percentage);

    xfce_rc_write_int_entry (rc, "action_on_low", battmon->options.action_on_low);

    xfce_rc_write_int_entry (rc, "action_on_critical", battmon->options.action_on_critical);

    xfce_rc_write_int_entry (rc, "hide_when_full", battmon->options.hide_when_full );

    color_str = gdk_rgba_to_string (&battmon->options.colorA);
    xfce_rc_write_entry (rc, "colorA", color_str);
    g_free (color_str);

    color_str = gdk_rgba_to_string (&battmon->options.colorH);
    xfce_rc_write_entry (rc, "colorH", color_str);
    g_free (color_str);

    color_str = gdk_rgba_to_string (&battmon->options.colorL);
    xfce_rc_write_entry (rc, "colorL", color_str);
    g_free (color_str);

    color_str = gdk_rgba_to_string (&battmon->options.colorC);
    xfce_rc_write_entry (rc, "colorC", color_str);
    g_free (color_str);

    xfce_rc_write_entry (rc, "command_on_low", battmon->options.command_on_low ? battmon->options.command_on_low : "");

    xfce_rc_write_entry (rc, "command_on_critical", battmon->options.command_on_critical ? battmon->options.command_on_critical : "");

    xfce_rc_close (rc);
}

static gboolean
battmon_set_size(XfcePanelPlugin *plugin, int size, t_battmon *battmon)
{
    int border_width;

    size /= xfce_panel_plugin_get_nrows (battmon->plugin);
    border_width = size > 26 ? 2 : 1;
    DBG("set_size(%d)", size);

    if (xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_HORIZONTAL)
    {
        /* force size of the panel plugin */
        gtk_widget_set_size_request(GTK_WIDGET(battmon->plugin), -1, size);
        /* size of the progressbar */
        gtk_widget_set_size_request(GTK_WIDGET(battmon->battstatus), 8, -1);
    }
    else
    {
        /* size of the plugin */
        gtk_widget_set_size_request(GTK_WIDGET(battmon->plugin), size, -1);
        /* size of the progressbar */
        gtk_widget_set_size_request(GTK_WIDGET(battmon->battstatus), -1, 8);
    }

    gtk_container_set_border_width (GTK_CONTAINER (battmon->ebox), border_width);
    /* update the icon */
    xfce_panel_image_set_size(XFCE_PANEL_IMAGE(battmon->image), size - (2 * border_width));

    return TRUE;
}


static void
refresh_dialog(t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->sb_low_percentage), battmon->options.low_percentage);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->sb_critical_percentage), battmon->options.critical_percentage);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog->ac_color_button), &battmon->options.colorA);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog->high_color_button), &battmon->options.colorH);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog->low_color_button), &battmon->options.colorL);
    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(dialog->critical_color_button), &battmon->options.colorC);
    gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->co_action_low), battmon->options.action_on_low);

    if (battmon->options.command_on_low)
        gtk_entry_set_text(GTK_ENTRY(dialog->en_command_low), battmon->options.command_on_low);
    else
        gtk_entry_set_text(GTK_ENTRY(dialog->en_command_low), "");

    gtk_combo_box_set_active(GTK_COMBO_BOX(dialog->co_action_critical), battmon->options.action_on_critical);

    if (battmon->options.command_on_critical)
        gtk_entry_set_text(GTK_ENTRY(dialog->en_command_critical), battmon->options.command_on_critical);
    else
        gtk_entry_set_text(GTK_ENTRY(dialog->en_command_critical), "");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_label), battmon->options.display_label);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_icon), battmon->options.display_icon);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_power), battmon->options.display_power);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_percentage), battmon->options.display_percentage);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_bar), battmon->options.display_bar);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_time), battmon->options.display_time);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_tooltip_percentage), battmon->options.tooltip_display_percentage);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_tooltip_time), battmon->options.tooltip_display_time);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_hide_when_full), battmon->options.hide_when_full);
    gtk_widget_set_sensitive(dialog->en_command_low, (battmon->options.action_on_low > 1) ? 1 : 0);
    gtk_widget_set_sensitive(dialog->en_command_critical, (battmon->options.action_on_critical > 1) ? 1 : 0);
}

static void
set_disp_percentage(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.display_percentage = gtk_toggle_button_get_active(tb);
    update_apm_status(dialog->battmon);
}

static void
set_disp_bar(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.display_bar = gtk_toggle_button_get_active(tb);
    update_apm_status(dialog->battmon);
}

static void
set_disp_time(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.display_time = gtk_toggle_button_get_active(tb);
    update_apm_status(dialog->battmon);
}

static void
set_hide_when_full(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.hide_when_full = gtk_toggle_button_get_active(tb);
    update_apm_status(dialog->battmon);
}

static void
set_tooltip_disp_percentage(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.tooltip_display_percentage = gtk_toggle_button_get_active(tb);
}

static void
set_disp_power(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.display_power = gtk_toggle_button_get_active(tb);
    update_apm_status(dialog->battmon);
}

static void
set_disp_label(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.display_label = gtk_toggle_button_get_active(tb);
    update_apm_status(dialog->battmon);
}

static void
set_disp_icon(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.display_icon = gtk_toggle_button_get_active(tb);
    update_apm_status(dialog->battmon);
}

static void
set_tooltip_time(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.tooltip_display_time = gtk_toggle_button_get_active(tb);
}

static void
set_low_percentage(GtkSpinButton *sb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.low_percentage = gtk_spin_button_get_value_as_int(sb);
    update_apm_status(dialog->battmon);
}

static void
set_critical_percentage(GtkSpinButton *sb, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.critical_percentage = gtk_spin_button_get_value_as_int(sb);
    update_apm_status(dialog->battmon);
}

static void
set_action_low(GtkComboBoxText *co, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.action_on_low = gtk_combo_box_get_active(GTK_COMBO_BOX(co));

    gtk_widget_set_sensitive(dialog->en_command_low, (battmon->options.action_on_low > 1) ? 1 : 0);
    update_apm_status(dialog->battmon);
}

static void
set_action_critical(GtkComboBoxText *co, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.action_on_critical = gtk_combo_box_get_active(GTK_COMBO_BOX(co));

    gtk_widget_set_sensitive(dialog->en_command_critical, (battmon->options.action_on_critical > 1) ? 1 : 0);
    update_apm_status(dialog->battmon);
}

static gboolean
set_command_low(GtkEntry *en, GdkEventFocus *event, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;
    const char *temp;

    g_free(battmon->options.command_on_low);
    temp = gtk_entry_get_text(en);
    battmon->options.command_on_low = g_strdup(temp);
    update_apm_status(dialog->battmon);

    /* Prevents a GTK crash */
    return FALSE;
}

static gboolean
set_command_critical(GtkEntry *en, GdkEventFocus *event, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;
    const char *temp;

    g_free(battmon->options.command_on_critical);
    temp = gtk_entry_get_text(en);
    battmon->options.command_on_critical = g_strdup(temp);
    update_apm_status(dialog->battmon);

    /* Prevents a GTK crash */
    return FALSE;
}

static char*
select_file_name (const char *title, const char *path, GtkWidget * parent)
{
    const char *t;
    GtkWidget *fs;
    char *name = NULL;

    t = (title) ? title : _("Select file");

    fs = gtk_file_chooser_dialog_new (t, GTK_WINDOW(parent),
                                      GTK_FILE_CHOOSER_ACTION_OPEN,
                                      "gtk-cancel", GTK_RESPONSE_CANCEL,
                                      "gtk-open", GTK_RESPONSE_ACCEPT,
                                      NULL);

    if (path && *path && g_file_test (path, G_FILE_TEST_EXISTS))
    {
        if (!g_path_is_absolute (path))
        {
            char *current, *full;

            current = g_get_current_dir ();
            full = g_build_filename (current, path, NULL);

            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(fs), full);

            g_free (current);
            g_free (full);
        }
        else
        {
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(fs), path);
        }
    }

    if (gtk_dialog_run (GTK_DIALOG (fs)) == GTK_RESPONSE_ACCEPT)
    {
        name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fs));
    }

    gtk_widget_destroy (fs);

    return name;
}

static void
command_browse_cb(GtkWidget *b, GtkEntry *entry)
{
    char *file = select_file_name(_("Select command"),
                                  gtk_entry_get_text(entry),
                                  gtk_widget_get_toplevel (b));

    if (file) {
        gtk_entry_set_text (entry, file);
        g_free (file);
    }
}

static void
change_color_ac(GtkWidget *button, t_battmon_dialog *dialog) {
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER (button),
                               &dialog->battmon->options.colorA);
    refresh_dialog(dialog);
    update_apm_status(dialog->battmon);
}

static void
change_color_high(GtkWidget *button, t_battmon_dialog *dialog) {
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER (button),
                               &dialog->battmon->options.colorH);
    refresh_dialog(dialog);
    update_apm_status(dialog->battmon);
}

static void
change_color_low(GtkWidget *button, t_battmon_dialog *dialog) {
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER (button),
                               &dialog->battmon->options.colorL);
    refresh_dialog(dialog);
    update_apm_status(dialog->battmon);
}

static void
change_color_critical(GtkWidget *button, t_battmon_dialog *dialog) {
    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER (button),
                               &dialog->battmon->options.colorC);
    refresh_dialog(dialog);
    update_apm_status(dialog->battmon);
}

static void
battmon_dialog_response (GtkWidget *dlg, int response, t_battmon *battmon)
{
    gboolean result;

    if (response == GTK_RESPONSE_HELP)
    {
        /* show help */
        result = g_spawn_command_line_async ("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);

        if (G_UNLIKELY (result == FALSE))
            g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
    }
    else
    {
        if (response == GTK_RESPONSE_HELP)
        {
            xfce_dialog_show_help (GTK_WINDOW (dlg), PACKAGE_NAME, NULL, NULL);
        }
        else
        {
            gtk_widget_destroy (dlg);
            xfce_panel_plugin_unblock_menu (battmon->plugin);
            battmon_write_config (battmon->plugin, battmon);
        }
    }
}

static void
battmon_create_options(XfcePanelPlugin *plugin, t_battmon *battmon)
{
    GtkWidget *dlg;
    GtkWidget *vbox, *vbox2, *hbox, *label, *button, *button2;
    GtkWidget *notebook;
    GtkSizeGroup *sg;
    t_battmon_dialog *dialog;

    dialog = g_new0(t_battmon_dialog, 1);

    dialog->battmon = battmon;

    xfce_panel_plugin_block_menu(plugin);

    dlg = xfce_titled_dialog_new_with_mixed_buttons(_("Battery Monitor"),
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))),
        GTK_DIALOG_DESTROY_WITH_PARENT,
        "window-close", _("_Close"), GTK_RESPONSE_OK,
        "help-browser", _("_Help"), GTK_RESPONSE_HELP,
        NULL);

    gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(dlg), "xfce4-battery-plugin");

    g_signal_connect(dlg, "response", G_CALLBACK (battmon_dialog_response),
                     battmon);

    notebook = gtk_notebook_new();
    gtk_widget_show(notebook);
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 6);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dlg))), GTK_WIDGET(notebook),
                       TRUE, TRUE, 0);

    /* Bar colors */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    /* Create size group to keep widgets aligned */
    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("On AC:"));
    gtk_size_group_add_widget(sg,label);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL (label), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->ac_color_button = gtk_color_button_new_with_rgba(&battmon->options.colorA);
    gtk_widget_set_size_request(dialog->ac_color_button, 64, 12);
    gtk_widget_show(GTK_WIDGET(dialog->ac_color_button));
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(dialog->ac_color_button), FALSE, FALSE, 0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Battery high:"));
    gtk_size_group_add_widget(sg,label);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign (GTK_LABEL(label), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->high_color_button = gtk_color_button_new_with_rgba(&battmon->options.colorH);
    gtk_widget_set_size_request(dialog->high_color_button, 64, 12);
    gtk_widget_show(GTK_WIDGET(dialog->high_color_button));
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(dialog->high_color_button), FALSE, FALSE, 0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Battery low:"));
    gtk_size_group_add_widget(sg,label);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign (GTK_LABEL(label), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->low_color_button = gtk_color_button_new_with_rgba(&battmon->options.colorL);
    gtk_widget_set_size_request(dialog->low_color_button, 64, 12);
    gtk_widget_show(GTK_WIDGET(dialog->low_color_button));
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(dialog->low_color_button), FALSE, FALSE, 0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Battery critical:"));
    gtk_size_group_add_widget(sg,label);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign (GTK_LABEL(label), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->critical_color_button = gtk_color_button_new_with_rgba(&battmon->options.colorC);
    gtk_widget_set_size_request(dialog->critical_color_button, 64, 12);
    gtk_widget_show(GTK_WIDGET(dialog->critical_color_button));
    gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(dialog->critical_color_button), FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("Bar _colors"));
    gtk_widget_show(label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);

    g_object_unref(G_OBJECT(sg));
    /* Create size group to keep widgets aligned */
    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    /* Low and Critical percentage settings */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Low percentage:"));
    gtk_size_group_add_widget(sg, label);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->sb_low_percentage = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox), dialog->sb_low_percentage, FALSE, FALSE, 0);

    /* Low battery action settings */

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Low battery action:"));
    gtk_size_group_add_widget(sg, label);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->co_action_low = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->co_action_low), _("Do nothing"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->co_action_low), _("Display a warning message"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->co_action_low), _("Run command"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->co_action_low), _("Run command in terminal"));
    gtk_box_pack_start(GTK_BOX(hbox), dialog->co_action_low, FALSE, FALSE, 0);

    /* Low battery command */

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Command:"));
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->en_command_low = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), dialog->en_command_low, FALSE, FALSE, 0);

    button = gtk_button_new_with_label("...");
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_top(GTK_WIDGET (hbox), 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Critical percentage:"));
    gtk_size_group_add_widget(sg, label);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->sb_critical_percentage = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox), dialog->sb_critical_percentage, FALSE, FALSE, 0);

    /* Critical battery action settings */

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Critical battery action:"));
    gtk_size_group_add_widget(sg, label);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->co_action_critical = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->co_action_critical), _("Do nothing"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->co_action_critical), _("Display a warning message"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->co_action_critical), _("Run command"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dialog->co_action_critical), _("Run command in terminal"));
    gtk_box_pack_start(GTK_BOX(hbox), dialog->co_action_critical, FALSE, FALSE, 0);

    /* Critical battery command */

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    label = gtk_label_new(_("Command:"));
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->en_command_critical = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), dialog->en_command_critical, FALSE, FALSE, 0);

    button2 = gtk_button_new_with_label("...");
    gtk_box_pack_start(GTK_BOX(hbox), button2, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("Levels and _actions"));
    gtk_widget_show(label);
    gtk_notebook_prepend_page(GTK_NOTEBOOK(notebook), vbox, label);

    g_object_unref(G_OBJECT(sg));
    /* Create size group to keep widgets aligned */
    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    /* Create checkbox options */

    vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 12);

    dialog->cb_disp_label = gtk_check_button_new_with_mnemonic(_("Display label"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_label, FALSE, FALSE, 0);

    dialog->cb_disp_icon = gtk_check_button_new_with_mnemonic(_("Display icon"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_icon, FALSE, FALSE, 0);

    dialog->cb_disp_bar = gtk_check_button_new_with_mnemonic(_("Display bar"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_bar, FALSE, FALSE, 0);

    dialog->cb_disp_percentage = gtk_check_button_new_with_mnemonic(_("Display percentage"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_percentage, FALSE, FALSE, 0);

    dialog->cb_disp_time = gtk_check_button_new_with_mnemonic(_("Display time"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_time, FALSE, FALSE, 0);

    dialog->cb_disp_power = gtk_check_button_new_with_mnemonic(_("Display power"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_power, FALSE, FALSE, 0);

    dialog->cb_hide_when_full = gtk_check_button_new_with_mnemonic(_("Hide time/percentage when full"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_hide_when_full, FALSE, FALSE, 0);

    dialog->cb_disp_tooltip_percentage = gtk_check_button_new_with_mnemonic(_("Display percentage in tooltip"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_tooltip_percentage, FALSE, FALSE, 0);

    dialog->cb_disp_tooltip_time = gtk_check_button_new_with_mnemonic(_("Display time remaining in tooltip"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_tooltip_time, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic(_("_Display"));
    gtk_widget_show(label);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, label);

    /* Signal connections should be set after setting tate of toggle buttons...*/
    refresh_dialog(dialog);

    g_signal_connect(dialog->ac_color_button, "color-set", G_CALLBACK(change_color_ac), dialog);
    g_signal_connect(dialog->high_color_button, "color-set", G_CALLBACK(change_color_high), dialog);
    g_signal_connect(dialog->low_color_button, "color-set", G_CALLBACK(change_color_low), dialog);
    g_signal_connect(dialog->critical_color_button, "color-set", G_CALLBACK(change_color_critical), dialog);
    g_signal_connect(button, "clicked", G_CALLBACK(command_browse_cb), dialog->en_command_low);
    g_signal_connect(button2, "clicked", G_CALLBACK(command_browse_cb), dialog->en_command_critical);
    g_signal_connect(dialog->cb_disp_percentage, "toggled", G_CALLBACK(set_disp_percentage), dialog);
    g_signal_connect(dialog->cb_disp_bar, "toggled", G_CALLBACK(set_disp_bar), dialog);
    g_signal_connect(dialog->cb_disp_time, "toggled", G_CALLBACK(set_disp_time), dialog);
    g_signal_connect(dialog->cb_hide_when_full, "toggled", G_CALLBACK(set_hide_when_full), dialog);
    g_signal_connect(dialog->cb_disp_tooltip_percentage, "toggled", G_CALLBACK(set_tooltip_disp_percentage), dialog);
    g_signal_connect(dialog->cb_disp_power, "toggled", G_CALLBACK(set_disp_power), dialog);
    g_signal_connect(dialog->cb_disp_tooltip_time, "toggled", G_CALLBACK(set_tooltip_time), dialog);
    g_signal_connect(dialog->cb_disp_label, "toggled", G_CALLBACK(set_disp_label), dialog);
    g_signal_connect(dialog->cb_disp_icon, "toggled", G_CALLBACK(set_disp_icon), dialog);

    g_signal_connect(dialog->sb_low_percentage, "value-changed", G_CALLBACK(set_low_percentage), dialog);
    g_signal_connect(dialog->sb_critical_percentage, "value-changed", G_CALLBACK(set_critical_percentage), dialog);
    g_signal_connect(dialog->co_action_low, "changed", G_CALLBACK(set_action_low), dialog);
    g_signal_connect(dialog->co_action_critical, "changed", G_CALLBACK(set_action_critical), dialog);
    g_signal_connect(dialog->en_command_low, "focus-out-event", G_CALLBACK(set_command_low), dialog);
    g_signal_connect(dialog->en_command_critical, "focus-out-event", G_CALLBACK(set_command_critical), dialog);

    gtk_widget_show_all (dlg);
}

static void
battmon_show_about(XfcePanelPlugin *plugin, t_battmon *battmon)
{
   GdkPixbuf *icon;
   const gchar *auth[] = {
        "Benedikt Meurer <benny@xfce.org>", "Edscott Wilson <edscott@imp.mx>",
        "Eduard Roccatello <eduard@xfce.org>", "Florian Rivoal <frivoal@xfce.org>",
        "Landry Breuil <landry@xfce.org>", "Nick Schermer <nick@xfce.org>",
        "Andre Miranda <andreldm@xfce.org>", NULL };

   icon = xfce_panel_pixbuf_from_source("xfce4-battery-plugin", NULL, 32);

   gtk_show_about_dialog(NULL,
        "logo", icon,
        "license", xfce_get_license_text (XFCE_LICENSE_TEXT_GPL),
        "version", PACKAGE_VERSION,
        "program-name", PACKAGE_NAME,
        "comments", _("Show and monitor the battery status"),
        "website", "https://docs.xfce.org/panel-plugins/xfce4-battery-plugin",
        "copyright", _("Copyright (c) 2003-2019\n"),
        "authors", auth, NULL);

   if (icon)
      g_object_unref(G_OBJECT(icon));
}

static void
on_power_change(GDBusProxy  *proxy,
                GVariant    *changed_properties,
                const gchar *invalidated_properties,
                gpointer     battmon)
{
    update_apm_status(battmon);
}

static void
battmon_dbus_monitor(t_battmon *battmon)
{
    GDBusProxy *proxy;

    proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.UPower",
                                          "/org/freedesktop/UPower",
                                          "org.freedesktop.UPower",
                                          NULL, NULL);

    g_return_if_fail(proxy != NULL);

    g_signal_connect(proxy, "g-properties-changed", G_CALLBACK(on_power_change), battmon);
}

static gboolean
update_apm_status_cb(gpointer user_data)
{
    t_battmon *battmon = user_data;
    update_apm_status(battmon);
    return TRUE;
}

/* create the plugin */
static void
battmon_construct(XfcePanelPlugin *plugin)
{
    t_battmon *battmon;

    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    battmon = battmon_create(plugin);

    battmon_read_config(plugin, battmon);

    setup_battmon(battmon);

    g_signal_connect(plugin, "free-data", G_CALLBACK(battmon_free), battmon);

    g_signal_connect(plugin, "save", G_CALLBACK(battmon_write_config), battmon);

    xfce_panel_plugin_menu_show_configure(plugin);
    g_signal_connect(plugin, "configure-plugin", G_CALLBACK(battmon_create_options), battmon);

    xfce_panel_plugin_menu_show_about(plugin);
    g_signal_connect(plugin, "about", G_CALLBACK(battmon_show_about), battmon);

    g_signal_connect(plugin, "size-changed", G_CALLBACK(battmon_set_size), battmon);

    g_signal_connect(plugin, "mode-changed", G_CALLBACK(battmon_set_mode), battmon);
    xfce_panel_plugin_set_small(plugin, TRUE);

    gtk_container_add(GTK_CONTAINER(plugin), battmon->ebox);

    xfce_panel_plugin_add_action_widget(plugin, battmon->ebox);

    xfce_panel_plugin_add_action_widget(plugin, battmon->battstatus);

    /* Read initial battery status */
    update_apm_status(battmon);

    /* Update battery status every 30 seconds */
    if (battmon->timeoutid == 0)
        battmon->timeoutid = g_timeout_add_seconds(30, update_apm_status_cb, battmon);

    /* Update battery status on UPower events like power cord connected, disconnected, lid opened */
    battmon_dbus_monitor(battmon);
}

/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER(battmon_construct);
