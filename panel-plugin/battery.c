/* Copyright (c) 2003 Nicholas Penwarden <toth64@yahoo.com>
 * Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 * Copyright (c) 2003 Edscott Wilson Garcia <edscott@users.sourceforge.net>
 * Copyright (c) 2005 Eduard Roccatello <eduard@xfce.org>
 * Copyright (c) 2006 Nick Schermer <nick@xfce.org>
 * Copyright (c) 2010 Florian Rivoal <frivoal@xfce.org>
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

#if (defined(__FreeBSD__) || defined(__FreeBSD_kernel__)) && (defined(i386) || defined(__i386__))
#include <machine/apm_bios.h>
#elif __OpenBSD__
#include <sys/param.h>
#include <machine/apmvar.h>
#define APMDEVICE "/dev/apm"
#elif __NetBSD__
#include <sys/param.h>
#include <sys/ioctl.h>
#include <machine/apmvar.h>
#define APMDEVICE "/dev/apm"
#elif __linux__
#include <libapm.h>
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

#define BORDER          8
#define HIGH_COLOR      "#00ff00"
#define LOW_COLOR       "#ffff00"
#define CRITICAL_COLOR  "#ff0000"
#define AVERAGING_CYCLE 5
#define PLUGIN_WEBSITE  "http://goodies.xfce.org/projects/panel-plugins/xfce4-battery-plugin"

typedef struct
{
    gboolean    display_label;    /* Options */
    gboolean    display_icon;    /* Options */
    gboolean    display_power;    /* Options */
    gboolean    display_percentage;    /* Options */
    gboolean    display_time;
        gboolean        hide_when_full;
    gboolean    tooltip_display_percentage;
    gboolean    tooltip_display_time;
    int        low_percentage;
    int        critical_percentage;
    int        action_on_low;
    int        action_on_critical;
    char        *command_on_low;    char        *command_on_critical;
    float        hsize;
    float        vsize;
} t_battmon_options;

typedef struct
{
    XfcePanelPlugin *plugin;

    GtkTooltips        *tips;
    GtkWidget        *vbox;        /* Widgets */
    GtkWidget        *ebox;
    GtkWidget        *battstatus;
    int            timeoutid;    /* To update apm status */
    int            method;
    gboolean        flag;
    gboolean        low;
    gboolean        critical;
    t_battmon_options    options;
    GdkColor        colorH;
    GdkColor        colorL;
    GdkColor        colorC;
    GtkLabel        *label;
    GtkLabel        *charge;
    GtkLabel        *rtime;
    GtkLabel        *alt_rtime;
    GtkLabel        *acfan;
    GtkLabel        *temp;
    GtkWidget        *image;
} t_battmon;

typedef struct
{
    GtkWidget        *cb_disp_power;
    GtkWidget        *cb_disp_label;
    GtkWidget        *cb_disp_percentage;
    GtkWidget        *cb_disp_time;
        GtkWidget               *cb_hide_when_full;
    GtkWidget        *cb_disp_tooltip_percentage;
    GtkWidget        *cb_disp_tooltip_time;
    GtkWidget        *cb_disp_icon;
    GtkWidget        *sb_low_percentage;
    GtkWidget        *sb_critical_percentage;
    GtkWidget        *om_action_low;
    GtkWidget        *om_action_critical;
    GtkWidget        *en_command_low;
    GtkWidget        *en_command_critical;
    t_battmon        *battmon;
} t_battmon_dialog;

enum {BM_DO_NOTHING, BM_MESSAGE, BM_COMMAND, BM_COMMAND_TERM};
enum {BM_BROKEN, BM_USE_ACPI, BM_USE_APM};


static void
init_options(t_battmon_options *options)
{
    options->display_icon = FALSE;
    options->display_label = FALSE;
    options->display_power = FALSE;
    options->display_percentage = TRUE;
    options->display_time = FALSE;
    options->tooltip_display_percentage = FALSE;
    options->tooltip_display_time = FALSE;
    options->low_percentage = 10;
    options->critical_percentage = 5;
    options->action_on_low = 1;
    options->action_on_critical = 1;
    options->command_on_low = NULL;
    options->command_on_critical = NULL;
    options->hsize = 1.75;
    options->vsize = 0.5;
}

gboolean
detect_battery_info(t_battmon *battmon)
{
#ifdef __FreeBSD__
  /* This is how I read the information from the APM subsystem under
     FreeBSD.  Each time this functions is called (once every second)
     the APM device is opened, read from and then closed.

     except that is does not work on FreeBSD

  */
#ifdef APMDEVICE
    struct apm_info apm;
#endif
      int fd;

    /* First check to see if ACPI is available */
    if (check_acpi() == 0) {
        int i;
        /* ACPI detected */
        battmon->method = BM_USE_ACPI;
        /* consider battery 0 first... */
        for (i=0;i<batt_count;i++) {
            if (read_acpi_info(i)) break;
        }
        for (i=0;i<batt_count;i++) {
            if (read_acpi_state(i)) break;
        }
        /*read_acpi_state(0);*/ /* only consider first battery... */

    DBG ("using ACPI");

        return TRUE;
    }

    battmon->method = BM_BROKEN;
#ifdef APMDEVICE
      fd = open(APMDEVICE, O_RDONLY);
      if (fd == -1) return FALSE;

      if (ioctl(fd, APMIO_GETINFO, &apm) == -1) {
        close(fd);
              return FALSE;
      }
      close(fd);
      battmon->method = BM_USE_APM;
#endif
      return TRUE;
#elif defined(__OpenBSD__) || defined(__NetBSD__)
  /* Code for OpenBSD by Joe Ammond <jra@twinight.org>. Using the same
     procedure as for FreeBSD.
     Made to work on NetBSD by Stefan Sperling <stsp@stsp.in-berlin.de>
  */
      struct apm_power_info apm;
      int fd;

      battmon->method = BM_BROKEN;
      fd = open(APMDEVICE, O_RDONLY);
      if (fd == -1) return FALSE;
            if (ioctl(fd, APM_IOC_GETPOWER, &apm) == -1) {
        close(fd);
             return FALSE;
    }
      close(fd);
      battmon->method = BM_USE_APM;

      return TRUE;
#elif __linux__
    struct apm_info apm;

    /* First check to see if ACPI is available */
    if(check_acpi()==0) {
        /* ACPI detected */
        int i;
        battmon->method = BM_USE_ACPI;
        for (i=0;i<batt_count;i++) {
            if (read_acpi_info(i)) break;
        }
        /*read_acpi_info(0);*/ /* only consider first battery... */
        for (i=0;i<batt_count;i++) {
            if (read_acpi_state(i)) break;
        }
        if (batt_count){
           apm.battery_percentage=acpistate->percentage;
           apm.battery_time=acpistate->rtime;
        }
    return TRUE;
    }
    if(apm_read(&apm) == 0) {
        /* ACPI not detected, but APM works */
        battmon->method = BM_USE_APM;
        return TRUE;
    }

    /* Neither ACPI or APM detected/working */
    battmon->method = BM_BROKEN;

    return FALSE;
#endif
}

static gboolean
battmon_time_labels_fits(t_battmon *battmon)
{
  int plugin_size, labels_size;

    GtkRequisition widget_size;
    gtk_widget_size_request( GTK_WIDGET(battmon->plugin), &widget_size );
    plugin_size = widget_size.height;

    labels_size = 0;
    gtk_widget_size_request( GTK_WIDGET(battmon->charge), &widget_size );
    labels_size += widget_size.height;
    gtk_widget_size_request( GTK_WIDGET(battmon->rtime), &widget_size );
    labels_size += widget_size.height;

    return labels_size <= plugin_size;
}

static gboolean
update_apm_status(t_battmon *battmon)
{
    int charge=0, rate;

    int lcapacity, ccapacity;
    gboolean fan=FALSE;
    const char *temp;
    int time_remaining=0;
    gboolean acline;
    gchar buffer[128];

    static int update_time = AVERAGING_CYCLE;
    static int sum_lcapacity = 0;
    static int sum_ccapacity = 0;
    static int sum_rate = 0;

    static int last_ccapacity = 0;
    static int last_lcapacity = 0;
    static int last_rate = 0;
    static int last_acline = 0;

#if defined(__OpenBSD__) || defined(__NetBSD__)
  /* Code for OpenBSD by Joe Ammond <jra@twinight.org>. Using the same
     procedure as for FreeBSD.
     Made to work on NetBSD by Stefan Sperling <stsp@stsp.in-berlin.de>
  */
      struct apm_power_info apm;
      int fd;

      battmon->method = BM_BROKEN;
      fd = open(APMDEVICE, O_RDONLY);
      if (fd == -1) return TRUE;
      if (ioctl(fd, APM_IOC_GETPOWER, &apm) == -1)
            return TRUE;
      close(fd);
      charge = apm.battery_life;
      time_remaining = apm.minutes_left;
      acline = apm.ac_state ? TRUE : FALSE;

#else
#if defined(__linux__) || defined(APMDEVICE)
    struct apm_info apm;
#endif
    DBG ("Updating battery status...");

    if(battmon->method == BM_BROKEN) {
      /* See if ACPI or APM support has been enabled yet */
        if(!detect_battery_info(battmon)) return TRUE;
        if(battmon->timeoutid != 0) g_source_remove(battmon->timeoutid);
        /* Poll only once per minute if using ACPI due to a bug */
#ifdef TUTTLE_UPDATES
        /* what bug? I don't see any bug here. */
        if(battmon->method == BM_USE_ACPI) {
            battmon->timeoutid = g_timeout_add(60 * 1024,
                    (GSourceFunc) update_apm_status, battmon);
        }
            else
#endif
            battmon->timeoutid = g_timeout_add(2 * 1024,
                    (GSourceFunc) update_apm_status, battmon);
    }

    /* Show initial state if using ACPI rather than waiting a minute */
    if(battmon->flag) {
        g_source_remove(battmon->timeoutid);
        /* we hit ACPI 4-5 times per poll, so polling every 2 seconds
	 * generates ~10 interrupts per second. updating every 30 seconds
	 * should be more than enough, and comes down to only 0.16
	 * interrupts per second, adding significant sleep time */
        battmon->timeoutid = g_timeout_add(30 * 1024,
                (GSourceFunc) update_apm_status, battmon);
    }

    if(battmon->method == BM_USE_ACPI) {
        int i;
        acline = read_acad_state();
        lcapacity = rate = ccapacity = 0;
        for (i=0;i<batt_count;i++) {
          if ( !read_acpi_info(i) || !read_acpi_state(i) )
            continue;
          lcapacity += acpiinfo->last_full_capacity;
          ccapacity += acpistate->rcapacity;
          rate += acpistate->prate;
        }

        if ( battmon->flag ) {
          last_ccapacity = ccapacity;
          last_lcapacity = lcapacity;
          last_rate = rate;
        }

        sum_lcapacity += lcapacity;
        sum_ccapacity += ccapacity;
        sum_rate += rate;

        update_time++;
        if ( update_time >= AVERAGING_CYCLE || last_acline != acline ) {
          if ( last_acline != acline ) {
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

        if ( last_acline )
            time_remaining = ((float)(lcapacity-ccapacity)/(float)(rate))*60;
        else
            time_remaining = ((float)(ccapacity)/(float)(rate))*60;

        if ( time_remaining < 0 )
            time_remaining = 0;

        last_acline = acline;

    }
#ifdef __linux__
    else {
        DBG ("Trying apm_read()...");
        apm_read(&apm);    /* not broken and not using ACPI, assume APM */
        charge = apm.battery_percentage;
        time_remaining = apm.battery_time;
        acline = apm.ac_line_status ? TRUE : FALSE;

    }
#elif __FreeBSD__
    else {
 /* This is how I read the information from the APM subsystem under
     FreeBSD.  Each time this functions is called (once every second)
     the APM device is opened, read from and then closed.

     except it don't work with 5.x:
battmon.c: In function `update_apm_status':
battmon.c:241: `APMDEVICE' undeclared (first use in this function)
battmon.c:241: (Each undeclared identifier is reported only once
battmon.c:241: for each function it appears in.)
*** Error code 1

  */
#ifdef APMDEVICE
       int fd;

       battmon->method = BM_BROKEN;
       fd = open(APMDEVICE, O_RDONLY);
       if (fd == -1) return TRUE;

       if (ioctl(fd, APMIO_GETINFO, &apm) == -1) {
        close(fd);
        return TRUE;
     }

       close(fd);

       acline = apm.ai_acline ? TRUE : FALSE;
       time_remaining = apm.ai_batt_time;
     time_remaining = time_remaining / 60; /* convert from seconds to minutes */
       charge = apm.ai_batt_life;
#else
     /* FIXME: apm stuff needs fix for 5.x kernels */
     acline=0;
     time_remaining=0;
     charge=0;
#endif
    }
#endif
#endif
    battmon->flag = FALSE;

    charge = CLAMP (charge, 0, 100);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(battmon->battstatus), charge / 100.0);

    if(battmon->options.display_label){
        gtk_widget_show((GtkWidget *)battmon->label);
    } else {
        gtk_widget_hide((GtkWidget *)battmon->label);
    }

    if(battmon->options.display_icon){
        gtk_widget_show(battmon->image);
    } else {
        gtk_widget_hide(battmon->image);
    }

    if(battmon->options.display_percentage && !(battmon->options.hide_when_full && acline && charge >= 99)){
        gtk_widget_show((GtkWidget *)battmon->charge);
        g_snprintf(buffer, sizeof(buffer),"%d%% ", charge);
        gtk_label_set_text(battmon->charge,buffer);
    } else {
        gtk_widget_hide((GtkWidget *)battmon->charge);
    }

    if (battmon->options.display_time && !(battmon->options.hide_when_full && acline && charge >= 99 )){
        GtkLabel *active_label;
        if ( battmon_time_labels_fits( battmon ) ) {
            active_label = battmon->rtime;
            gtk_widget_hide( (GtkWidget*)battmon->alt_rtime );
        } else {
            active_label = battmon->alt_rtime;
            gtk_widget_hide( (GtkWidget*)battmon->rtime );
        }

        gtk_widget_show((GtkWidget *)active_label);
        g_snprintf(buffer, sizeof(buffer),"%02d:%02d ",time_remaining/60,time_remaining%60);
        gtk_label_set_text(active_label,buffer);

    } else {
        gtk_widget_hide((GtkWidget *)battmon->rtime);
        gtk_widget_hide((GtkWidget *)battmon->alt_rtime);
    }


    if(acline) {
        char *t;
        if((battmon->method == BM_USE_ACPI) && (acpiinfo->present == 0)) {
            t=_("(No battery, AC on-line)");
        } else {
            t=(charge<99.9)?_("(Charging from AC)"):_("(AC on-line)");
        }
        if(battmon->options.tooltip_display_percentage) {
            g_snprintf(buffer, sizeof(buffer), "%d%% %s", charge,t);
        }
        else
            g_snprintf(buffer, sizeof(buffer), "%s",t);
    }
    else {
        if(battmon->options.tooltip_display_percentage && battmon->options.tooltip_display_time)
             g_snprintf(buffer, sizeof(buffer), _("%d%% (%02d:%02d) remaining"), charge, time_remaining / 60, time_remaining % 60);
        else if(battmon->options.tooltip_display_time)
             g_snprintf(buffer, sizeof(buffer), _("%02d:%02d remaining"),time_remaining / 60, time_remaining % 60);
        else if(battmon->options.tooltip_display_percentage)
             g_snprintf(buffer, sizeof(buffer), _("%d%% remaining"), charge);
           else
             g_snprintf(buffer, sizeof(buffer), _("AC off-line"));
    }

    gtk_tooltips_set_tip (battmon->tips, battmon->ebox, buffer, NULL);

    if(battmon->options.display_power){
      gtk_widget_show((GtkWidget *)battmon->acfan);
      gtk_widget_show((GtkWidget *)battmon->temp);

      fan=get_fan_status();
      if(acline && fan)
        gtk_label_set_text(battmon->acfan,"AC FAN");
      else if(acline && !fan)
        gtk_label_set_text(battmon->acfan,"AC");
      else if(!acline && fan)
        gtk_label_set_text(battmon->acfan,"FAN");
      else {
          gtk_label_set_text(battmon->acfan,"");
          gtk_widget_hide((GtkWidget *)battmon->acfan);
      }

      temp=get_temperature();
      DBG ("Temp: %s", temp);
      if(temp)
        gtk_label_set_text(battmon->temp,temp);
      else {
          gtk_label_set_text(battmon->temp,"");
          gtk_widget_hide((GtkWidget *)battmon->temp);
      }
    } else {
      gtk_widget_hide((GtkWidget *)battmon->acfan);
      gtk_widget_hide((GtkWidget *)battmon->temp);
    }

    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(battmon->battstatus), NULL);

    /* bar colors and state flags */
    if (acline) {
      battmon->low = battmon->critical = FALSE;
      gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_PRELIGHT, NULL);
    }
    else {
      if(charge <= battmon->options.critical_percentage) {
        gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_PRELIGHT, &(battmon->colorC));
        gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_SELECTED, &(battmon->colorC));
      }
      else if(charge <= battmon->options.low_percentage) {
        gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_PRELIGHT, &(battmon->colorL));
        gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_SELECTED, &(battmon->colorL));
        battmon->critical = FALSE;
      }
      else {
            battmon->low = battmon->critical = FALSE;
        gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_PRELIGHT, &(battmon->colorH));
        gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_SELECTED, &(battmon->colorH));
      }
    }

    /* alarms */
    if (!acline && charge <= battmon->options.low_percentage){
        if(!battmon->critical && charge <= battmon->options.critical_percentage) {
               battmon->critical = TRUE;
	       GtkWidget *dialog;
            if(battmon->options.action_on_critical == BM_MESSAGE){
do_critical_warn:
                dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
                _("WARNING: Your battery has reached critical status. You should plug in or shutdown your computer now to avoid possible data loss."));
                g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
		gtk_widget_show_all (dialog);
                return TRUE;
            }
            if(battmon->options.action_on_critical == BM_COMMAND ||
               battmon->options.action_on_critical == BM_COMMAND_TERM){
                int interm=(battmon->options.action_on_critical == BM_COMMAND_TERM)?1:0;
                if (!battmon->options.command_on_critical ||
                    !strlen(battmon->options.command_on_critical)) goto do_critical_warn;
                xfce_spawn_command_line_on_screen(NULL, battmon->options.command_on_critical, interm, FALSE, NULL);
            }
        } else if (!battmon->low){
                battmon->low = TRUE;
	       GtkWidget *dialog;
            if(battmon->options.action_on_low == BM_MESSAGE){
do_low_warn:
                dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE,
                _("WARNING: Your battery is running low. You should consider plugging in or shutting down your computer soon to avoid possible data loss."));
                g_signal_connect_swapped (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
		gtk_widget_show_all (dialog);
                return TRUE;
            }
            if(battmon->options.action_on_low == BM_COMMAND ||
               battmon->options.action_on_low == BM_COMMAND_TERM){
                int interm=(battmon->options.action_on_low == BM_COMMAND_TERM)?1:0;
                if (!battmon->options.command_on_low ||
                    !strlen(battmon->options.command_on_low)) goto do_low_warn;
                xfce_spawn_command_line_on_screen(NULL, battmon->options.command_on_low, interm, FALSE, NULL);
            }
        }
    }

    return TRUE;
}

static GdkPixbuf *
battmon_icon (t_battmon *battmon)
{
    GdkPixbuf      *icon;
    GtkOrientation  orientation;
    gint            width, height, size;

    /* panel info */
    orientation = xfce_panel_plugin_get_orientation (battmon->plugin);

#if defined (LIBXFCE4PANEL_CHECK_VERSION) && LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
    size = xfce_panel_plugin_get_size (battmon->plugin);
    size /= xfce_panel_plugin_get_nrows (battmon->plugin);
    size -= 6;
#else
    size = xfce_panel_plugin_get_size (battmon->plugin) - 6;
#endif

        /* icon size is 41x64px */
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        height = size;
        width = height * 0.625;
    }
    else /* vertical */
    {
        width = size;
        height = width * 1.6;
    }

    /* try to load battery icon from your current icon theme */
    icon = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
                                     "battery", size , 0, NULL);

    return icon;
}

static void setup_battmon(t_battmon      *battmon,
                          GtkOrientation  orientation,
                          GtkOrientation  panel_orientation)
{
    GtkWidget *box,*vbox;
    GdkPixbuf *icon;
    gint size;

    size = xfce_panel_plugin_get_size (battmon->plugin);
    battmon->battstatus = gtk_progress_bar_new();

    if (panel_orientation == GTK_ORIENTATION_HORIZONTAL)
    {
       gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(battmon->battstatus),
               GTK_PROGRESS_BOTTOM_TO_TOP);
       gtk_widget_set_size_request(GTK_WIDGET(battmon->battstatus),
               BORDER, size);
       box=gtk_hbox_new(FALSE, 0);
       battmon->vbox = gtk_hbox_new(FALSE, 0);
       gtk_widget_set_size_request(GTK_WIDGET(battmon->plugin),
               -1, size);
    } else {
       gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(battmon->battstatus),
               GTK_PROGRESS_LEFT_TO_RIGHT);
       gtk_widget_set_size_request(GTK_WIDGET(battmon->battstatus),
               size, BORDER);
       box=gtk_vbox_new(FALSE, 0);
       battmon->vbox = gtk_vbox_new(FALSE, 0);
       gtk_widget_set_size_request(GTK_WIDGET(battmon->plugin),
               size, -1);
    }

    gtk_container_set_border_width(GTK_CONTAINER(battmon->vbox), BORDER / 2);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(battmon->battstatus), 0.0);

    icon  = battmon_icon (battmon);
    if (icon)
    {
        battmon->image = gtk_image_new_from_pixbuf (icon);
        g_object_unref (G_OBJECT (icon));
    }
    else
    {
        battmon->image = gtk_image_new_from_icon_name ("battery", GTK_ICON_SIZE_BUTTON);
    }

    gtk_box_pack_start(GTK_BOX(box),GTK_WIDGET(battmon->image), FALSE, FALSE, 2);
    /* init hide the widget */
    gtk_widget_hide(battmon->image);

      battmon->label = (GtkLabel *)gtk_label_new(_("Battery"));
        gtk_box_pack_start(GTK_BOX(box),GTK_WIDGET(battmon->label),FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box),  GTK_WIDGET(battmon->battstatus), FALSE, FALSE, 2);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
       vbox = gtk_vbox_new(FALSE, 0);
    else
       vbox = gtk_hbox_new(FALSE, 0);

    /* percent + rtime */
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(vbox), FALSE, FALSE, 0);

    battmon->charge = (GtkLabel *)gtk_label_new("50%%");
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
      gtk_box_pack_start(GTK_BOX(vbox),GTK_WIDGET(battmon->charge),TRUE, TRUE, 0);
    else
      gtk_box_pack_end(GTK_BOX(vbox),GTK_WIDGET(battmon->charge),TRUE, TRUE, 0);

    battmon->rtime = (GtkLabel *)gtk_label_new("01:00");
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
       gtk_box_pack_start(GTK_BOX(vbox),GTK_WIDGET(battmon->rtime),TRUE, TRUE, 0);
    else
       gtk_box_pack_end(GTK_BOX(vbox),GTK_WIDGET(battmon->rtime),TRUE, TRUE, 0);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
       vbox = gtk_vbox_new(FALSE, 0);
    else
       vbox = gtk_hbox_new(FALSE, 0);

    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(vbox), FALSE, FALSE, 0);

    battmon->alt_rtime = (GtkLabel *)gtk_label_new("01:00");
    gtk_box_pack_start(GTK_BOX(vbox),GTK_WIDGET(battmon->alt_rtime),TRUE, TRUE, 0);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
       vbox = gtk_vbox_new(FALSE, 0);
    else
       vbox = gtk_hbox_new(FALSE, 0);

    /* ac-fan-temp */
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(vbox), FALSE, FALSE, 0);

    battmon->acfan = (GtkLabel *)gtk_label_new("AC FAN");
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
       gtk_box_pack_start(GTK_BOX(vbox),GTK_WIDGET(battmon->acfan),TRUE, TRUE, 0);
    else
       gtk_box_pack_end(GTK_BOX(vbox),GTK_WIDGET(battmon->acfan),TRUE, TRUE, 0);

      battmon->temp = (GtkLabel *)gtk_label_new("40°C");
    if (orientation == GTK_ORIENTATION_HORIZONTAL)
       gtk_box_pack_start(GTK_BOX(vbox),GTK_WIDGET(battmon->temp),TRUE, TRUE, 0);
    else
       gtk_box_pack_end(GTK_BOX(vbox),GTK_WIDGET(battmon->temp),TRUE, TRUE, 0);

    if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
       gtk_label_set_angle(GTK_LABEL(battmon->label), 0);
       gtk_label_set_angle(GTK_LABEL(battmon->charge), 0);
       gtk_label_set_angle(GTK_LABEL(battmon->rtime), 0);
       gtk_label_set_angle(GTK_LABEL(battmon->alt_rtime), 0);
       gtk_label_set_angle(GTK_LABEL(battmon->acfan), 0);
       gtk_label_set_angle(GTK_LABEL(battmon->temp), 0);
    }
    else
    {
       gtk_label_set_angle(GTK_LABEL(battmon->label), 270);
       gtk_label_set_angle(GTK_LABEL(battmon->charge), 270);
       gtk_label_set_angle(GTK_LABEL(battmon->rtime), 270);
       gtk_label_set_angle(GTK_LABEL(battmon->alt_rtime), 270);
       gtk_label_set_angle(GTK_LABEL(battmon->acfan), 270);
       gtk_label_set_angle(GTK_LABEL(battmon->temp), 270);
    }

    gtk_box_pack_start(GTK_BOX(battmon->vbox), box, FALSE, FALSE, 0);
    gtk_widget_show_all(battmon->vbox);
    if(!battmon->options.display_label)
        gtk_widget_hide((GtkWidget *)battmon->label);
    if(!battmon->options.display_power){
        gtk_widget_hide((GtkWidget *)battmon->acfan);
        gtk_widget_hide((GtkWidget *)battmon->temp);
    }
    if(!battmon->options.display_percentage){
        gtk_widget_hide((GtkWidget *)battmon->charge);
    }
    if (!battmon->options.display_time){
        gtk_widget_hide((GtkWidget *)battmon->rtime);
        gtk_widget_hide((GtkWidget *)battmon->alt_rtime);
    } else {
        if ( battmon_time_labels_fits(battmon) ) {
        gtk_widget_hide((GtkWidget*)battmon->alt_rtime);
        } else {
        gtk_widget_hide((GtkWidget*)battmon->rtime);
        }
    }

    gtk_container_add(GTK_CONTAINER(battmon->ebox),GTK_WIDGET(battmon->vbox));
    gtk_widget_show(battmon->ebox);


    gdk_color_parse(HIGH_COLOR, &(battmon->colorH));
    gdk_color_parse(LOW_COLOR, &(battmon->colorL));
    gdk_color_parse(CRITICAL_COLOR, &(battmon->colorC));
    gtk_widget_set_size_request(battmon->ebox, -1, -1);
}

#if defined (LIBXFCE4PANEL_CHECK_VERSION) && LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
static gboolean
battmon_set_mode (XfcePanelPlugin *plugin, XfcePanelPluginMode mode,
                  t_battmon *battmon)
{
    GtkOrientation orientation, panel_orientation;

    if (battmon->timeoutid) g_source_remove(battmon->timeoutid);
    gtk_container_remove(GTK_CONTAINER(battmon->ebox), GTK_WIDGET(battmon->vbox));
    orientation =
      (mode != XFCE_PANEL_PLUGIN_MODE_VERTICAL) ?
      GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    panel_orientation =
      (mode == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL) ?
      GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    setup_battmon(battmon,orientation,panel_orientation);
    update_apm_status( battmon );
    battmon->timeoutid = g_timeout_add(1 * 1024, (GSourceFunc) update_apm_status, battmon);

    return TRUE;
}

#else

static gboolean
battmon_set_orientation (XfcePanelPlugin *plugin, GtkOrientation orientation,
                         t_battmon *battmon)
{
    if (battmon->timeoutid) g_source_remove(battmon->timeoutid);
    gtk_container_remove(GTK_CONTAINER(battmon->ebox), GTK_WIDGET(battmon->vbox));
    setup_battmon(battmon,GTK_ORIENTATION_HORIZONTAL,orientation);
    update_apm_status( battmon );
    battmon->timeoutid = g_timeout_add(1 * 1024, (GSourceFunc) update_apm_status, battmon);

    return TRUE;
}
#endif

static t_battmon*
battmon_create(XfcePanelPlugin *plugin)
{
    t_battmon *battmon;
    GtkOrientation panel_orientation;
#if defined (LIBXFCE4PANEL_CHECK_VERSION) && LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
    GtkOrientation orientation;
    XfcePanelPluginMode mode;
#endif

    battmon = g_new(t_battmon, 1);
    init_options(&(battmon->options));

        battmon->plugin = plugin;

    battmon->low = FALSE;
    battmon->critical = FALSE;
    battmon->ebox = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(battmon->ebox), FALSE);

#if defined (LIBXFCE4PANEL_CHECK_VERSION) && LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
    mode = xfce_panel_plugin_get_mode (plugin);
    orientation =
      (mode != XFCE_PANEL_PLUGIN_MODE_VERTICAL) ?
      GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
    panel_orientation =
      (mode == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL) ?
      GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;
#else
    panel_orientation = xfce_panel_plugin_get_orientation (plugin);
#endif
    setup_battmon(battmon, GTK_ORIENTATION_HORIZONTAL, panel_orientation);

    battmon->timeoutid = 0;
    battmon->flag = FALSE;
    battmon->tips = gtk_tooltips_new ();
    g_object_ref (G_OBJECT (battmon->tips));
    gtk_object_sink (GTK_OBJECT (battmon->tips));

    return battmon;
}

static void
battmon_free(XfcePanelPlugin *plugin, t_battmon *battmon)
{
    if(battmon->timeoutid != 0) {
        g_source_remove(battmon->timeoutid);
        battmon->timeoutid = 0;
    }

    /* cleanup options */
    g_free (battmon->options.command_on_low);
    g_free (battmon->options.command_on_critical);

    /* free tooltip */
    gtk_tooltips_set_tip (battmon->tips, battmon->ebox, NULL, NULL);
    g_object_unref (G_OBJECT (battmon->tips));

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

    battmon->options.display_percentage = xfce_rc_read_bool_entry (rc, "display_percentage", FALSE);

    battmon->options.display_time = xfce_rc_read_bool_entry (rc, "display_time", FALSE);

    battmon->options.tooltip_display_percentage = xfce_rc_read_bool_entry (rc, "tooltip_display_percentage", FALSE);

    battmon->options.tooltip_display_time = xfce_rc_read_bool_entry (rc, "tooltip_display_time", FALSE);

    battmon->options.low_percentage = xfce_rc_read_int_entry (rc, "low_percentage", 10);

    battmon->options.critical_percentage = xfce_rc_read_int_entry (rc, "critical_percentage", 5);

    battmon->options.action_on_low = xfce_rc_read_int_entry (rc, "action_on_low", 0);

    battmon->options.action_on_critical = xfce_rc_read_int_entry (rc, "action_on_critical", 0);

    battmon->options.hide_when_full = xfce_rc_read_int_entry (rc, "hide_when_full", 0);

    if ((value =  xfce_rc_read_entry (rc, "command_on_low", NULL)) && *value)
        battmon->options.command_on_low = g_strdup (value);

    if((value =  xfce_rc_read_entry (rc, "command_on_critical", NULL)) && *value)
        battmon->options.command_on_critical = g_strdup (value);

    xfce_rc_close (rc);
}

static void
battmon_write_config(XfcePanelPlugin *plugin, t_battmon *battmon)
{
    XfceRc *rc;
    gchar *file;

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

    xfce_rc_write_bool_entry (rc, "display_time", battmon->options.display_time);

    xfce_rc_write_bool_entry (rc, "tooltip_display_percentage", battmon->options.tooltip_display_percentage);

    xfce_rc_write_bool_entry (rc, "tooltip_display_time", battmon->options.tooltip_display_time);

    xfce_rc_write_int_entry (rc, "low_percentage", battmon->options.low_percentage);

    xfce_rc_write_int_entry (rc, "critical_percentage", battmon->options.critical_percentage);

    xfce_rc_write_int_entry (rc, "action_on_low", battmon->options.action_on_low);

    xfce_rc_write_int_entry (rc, "action_on_critical", battmon->options.action_on_critical);

    xfce_rc_write_int_entry (rc, "hide_when_full", battmon->options.hide_when_full );

    xfce_rc_write_entry (rc, "command_on_low", battmon->options.command_on_low ? battmon->options.command_on_low : "");

    xfce_rc_write_entry (rc, "command_on_critical", battmon->options.command_on_critical ? battmon->options.command_on_critical : "");

    xfce_rc_close (rc);
}

static gboolean
battmon_set_size(XfcePanelPlugin *plugin, int size, t_battmon *battmon)
{
    GdkPixbuf *icon;

    if (xfce_panel_plugin_get_orientation (plugin) ==
            GTK_ORIENTATION_HORIZONTAL)
    {
        /* force size of the panel plugin */
        gtk_widget_set_size_request(GTK_WIDGET(battmon->plugin),
                                -1, size);
        /* size of the progressbar */
        gtk_widget_set_size_request(GTK_WIDGET(battmon->battstatus),
                BORDER, size);
    }
    else
    {
        /* size of the plugin */
        gtk_widget_set_size_request(GTK_WIDGET(battmon->plugin),
                size, -1);
        /* size of the progressbar */
        gtk_widget_set_size_request(GTK_WIDGET(battmon->battstatus),
                size, BORDER);
    }

    /* update the icon */
    icon  = battmon_icon (battmon);
    if (icon)
    {
        gtk_image_set_from_pixbuf (GTK_IMAGE (battmon->image), icon);
        g_object_unref (G_OBJECT (icon));
    }

    return TRUE;
}


static void refresh_dialog(t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->sb_low_percentage), battmon->options.low_percentage);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->sb_critical_percentage), battmon->options.critical_percentage);
    gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->om_action_low), battmon->options.action_on_low);
    if(battmon->options.command_on_low)
        gtk_entry_set_text(GTK_ENTRY(dialog->en_command_low), battmon->options.command_on_low);
    else
        gtk_entry_set_text(GTK_ENTRY(dialog->en_command_low), "");
    gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->om_action_critical), battmon->options.action_on_critical);
    if(battmon->options.command_on_critical)
        gtk_entry_set_text(GTK_ENTRY(dialog->en_command_critical), battmon->options.command_on_critical);
    else
        gtk_entry_set_text(GTK_ENTRY(dialog->en_command_critical), "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_label), battmon->options.display_label);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_icon), battmon->options.display_icon);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_power), battmon->options.display_power);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_percentage), battmon->options.display_percentage);
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
set_action_low(GtkOptionMenu *om, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.action_on_low = gtk_option_menu_get_history(om);

    gtk_widget_set_sensitive(dialog->en_command_low, (gtk_option_menu_get_history(om) > 1) ? 1 : 0);
    update_apm_status(dialog->battmon);
}

static void
set_action_critical(GtkOptionMenu *om, t_battmon_dialog *dialog)
{
    t_battmon *battmon = dialog->battmon;

    battmon->options.action_on_critical = gtk_option_menu_get_history(om);

    gtk_widget_set_sensitive(dialog->en_command_critical, (gtk_option_menu_get_history(om) > 1) ? 1 : 0);
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

static char *
select_file_name (const char *title, const char *path, GtkWidget * parent)
{
    const char *t;
    GtkWidget *fs;
    char *name = NULL;

    t = (title) ? title : _("Select file");

    fs = gtk_file_chooser_dialog_new (t, GTK_WINDOW(parent),
                               GTK_FILE_CHOOSER_ACTION_OPEN,
                               GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                               GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
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
command_browse_cb (GtkWidget *b, GtkEntry *entry)
{
    char *file = select_file_name(_("Select command"), gtk_entry_get_text(entry), gtk_widget_get_toplevel (b));

    if (file) {
        gtk_entry_set_text (entry, file);
        g_free (file);
    }
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
        gtk_widget_destroy (dlg);
        xfce_panel_plugin_unblock_menu (battmon->plugin);
        battmon_write_config (battmon->plugin, battmon);
    }
}

static void
battmon_create_options(XfcePanelPlugin *plugin, t_battmon *battmon)
{
    GtkWidget *dlg;
    GtkWidget *vbox, *vbox2, *hbox, *label, *menu, *mi, *button, *button2;
    GtkSizeGroup *sg;
    t_battmon_dialog *dialog;

    dialog = g_new0(t_battmon_dialog, 1);

    dialog->battmon = battmon;

    xfce_panel_plugin_block_menu (plugin);

    dlg = xfce_titled_dialog_new_with_buttons (_("Battery Monitor"),
                                                  GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                  GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                                  GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                                  GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                                  NULL);

    gtk_window_set_position   (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name  (GTK_WINDOW (dlg), "xfce4-settings");

    g_signal_connect (dlg, "response", G_CALLBACK (battmon_dialog_response),
                      battmon);

    gtk_container_set_border_width (GTK_CONTAINER (dlg), 2);

    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER - 2);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox,
                        TRUE, TRUE, 0);

    /* Create size group to keep widgets aligned */

    sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

    /* Low and Critical percentage settings */

    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Low percentage:"));
    gtk_size_group_add_widget(sg, label);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->sb_low_percentage = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox), dialog->sb_low_percentage, FALSE, FALSE, 0);

    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Critical percentage:"));
    gtk_size_group_add_widget(sg, label);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    dialog->sb_critical_percentage = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_box_pack_start(GTK_BOX(hbox), dialog->sb_critical_percentage, FALSE, FALSE, 0);

    /* Low battery action settings */

    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Low battery action:"));
    gtk_size_group_add_widget(sg, label);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    menu = gtk_menu_new();
        mi = gtk_menu_item_new_with_label(_("Do nothing"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
     mi = gtk_menu_item_new_with_label(_("Display a warning message"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    mi = gtk_menu_item_new_with_label(_("Run command"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
     mi = gtk_menu_item_new_with_label(_("Run command in terminal"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);


    dialog->om_action_low = gtk_option_menu_new();
    gtk_option_menu_set_menu(GTK_OPTION_MENU(dialog->om_action_low), menu);
    gtk_box_pack_start(GTK_BOX(hbox), dialog->om_action_low, FALSE, FALSE, 0);

    /* Low battery command */

    hbox = gtk_hbox_new(FALSE, BORDER);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

        label = gtk_label_new(_("Command:"));
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        dialog->en_command_low = gtk_entry_new();
        gtk_box_pack_start(GTK_BOX(hbox), dialog->en_command_low, FALSE, FALSE, 0);

        button = gtk_button_new_with_label("...");
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    /* Critical battery action settings */

    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(_("Critical battery action:"));
    gtk_size_group_add_widget(sg, label);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    menu = gtk_menu_new();
        mi = gtk_menu_item_new_with_label(_("Do nothing"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
     mi = gtk_menu_item_new_with_label(_("Display a warning message"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
    mi = gtk_menu_item_new_with_label(_("Run command"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
     mi = gtk_menu_item_new_with_label(_("Run command in terminal"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

    dialog->om_action_critical = gtk_option_menu_new();
    gtk_option_menu_set_menu(GTK_OPTION_MENU(dialog->om_action_critical), menu);
    gtk_box_pack_start(GTK_BOX(hbox), dialog->om_action_critical, FALSE, FALSE, 0);

    /* Critical battery command */

    hbox = gtk_hbox_new(FALSE, BORDER);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

        label = gtk_label_new(_("Command:"));
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

        dialog->en_command_critical = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), dialog->en_command_critical, FALSE, FALSE, 0);

        button2 = gtk_button_new_with_label("...");
        gtk_box_pack_start(GTK_BOX(hbox), button2, FALSE, FALSE, 0);

    /* Create checkbox options */

    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new(NULL);
    gtk_size_group_add_widget(sg, label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    vbox2 = gtk_vbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);

    dialog->cb_disp_label = gtk_check_button_new_with_mnemonic(_("Display label"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_label, FALSE, FALSE, 0);

    dialog->cb_disp_percentage = gtk_check_button_new_with_mnemonic(_("Display percentage"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_percentage, FALSE, FALSE, 0);

    dialog->cb_disp_time = gtk_check_button_new_with_mnemonic(_("Display time"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_time, FALSE, FALSE, 0);

    dialog->cb_hide_when_full = gtk_check_button_new_with_mnemonic(_("Hide time/percentage when full"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_hide_when_full, FALSE, FALSE, 0);

    dialog->cb_disp_tooltip_percentage = gtk_check_button_new_with_mnemonic(_("Display percentage in tooltip"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_tooltip_percentage, FALSE, FALSE, 0);

    dialog->cb_disp_tooltip_time = gtk_check_button_new_with_mnemonic(_("Display time remaining in tooltip"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_tooltip_time, FALSE, FALSE, 0);

    dialog->cb_disp_power = gtk_check_button_new_with_mnemonic(_("Display power"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_power, FALSE, FALSE, 0);

    dialog->cb_disp_icon = gtk_check_button_new_with_mnemonic(_("Display icon"));
    gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_icon, FALSE, FALSE, 0);

    /* Signal connections should be set after setting tate of toggle buttons...*/
    refresh_dialog(dialog);

    g_signal_connect(button, "clicked", G_CALLBACK(command_browse_cb), dialog->en_command_low);
    g_signal_connect(button2, "clicked", G_CALLBACK(command_browse_cb), dialog->en_command_critical);
    g_signal_connect(dialog->cb_disp_percentage, "toggled", G_CALLBACK(set_disp_percentage), dialog);
    g_signal_connect(dialog->cb_disp_time, "toggled", G_CALLBACK(set_disp_time), dialog);
    g_signal_connect(dialog->cb_hide_when_full, "toggled", G_CALLBACK(set_hide_when_full), dialog);
    g_signal_connect(dialog->cb_disp_tooltip_percentage, "toggled", G_CALLBACK(set_tooltip_disp_percentage), dialog);
    g_signal_connect(dialog->cb_disp_power, "toggled", G_CALLBACK(set_disp_power), dialog);
    g_signal_connect(dialog->cb_disp_tooltip_time, "toggled", G_CALLBACK(set_tooltip_time), dialog);
    g_signal_connect(dialog->cb_disp_label, "toggled", G_CALLBACK(set_disp_label), dialog);
    g_signal_connect(dialog->cb_disp_icon, "toggled", G_CALLBACK(set_disp_icon), dialog);

    g_signal_connect(dialog->sb_low_percentage, "value-changed", G_CALLBACK(set_low_percentage), dialog);
    g_signal_connect(dialog->sb_critical_percentage, "value-changed", G_CALLBACK(set_critical_percentage), dialog);
    g_signal_connect(dialog->om_action_low, "changed", G_CALLBACK(set_action_low), dialog);
    g_signal_connect(dialog->om_action_critical, "changed", G_CALLBACK(set_action_critical), dialog);
    g_signal_connect(dialog->en_command_low, "focus-out-event", G_CALLBACK(set_command_low), dialog);
    g_signal_connect(dialog->en_command_critical, "focus-out-event", G_CALLBACK(set_command_critical), dialog);

    gtk_widget_show_all (dlg);
}

/* create the plugin */
static void
battmon_construct (XfcePanelPlugin *plugin)
{
    t_battmon *battmon;

    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    battmon = battmon_create (plugin);

    battmon_read_config (plugin, battmon);

    g_signal_connect (plugin, "free-data", G_CALLBACK (battmon_free), battmon);

    g_signal_connect (plugin, "save", G_CALLBACK (battmon_write_config), battmon);

    xfce_panel_plugin_menu_show_configure (plugin);
    g_signal_connect (plugin, "configure-plugin", G_CALLBACK (battmon_create_options), battmon);

    g_signal_connect (plugin, "size-changed", G_CALLBACK (battmon_set_size), battmon);

#if defined (LIBXFCE4PANEL_CHECK_VERSION) && LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
    g_signal_connect (plugin, "mode-changed", G_CALLBACK (battmon_set_mode), battmon);
#else
    g_signal_connect (plugin, "orientation-changed", G_CALLBACK (battmon_set_orientation), battmon);
#endif

    gtk_container_add(GTK_CONTAINER(plugin), battmon->ebox);

    xfce_panel_plugin_add_action_widget (plugin, battmon->ebox);

    xfce_panel_plugin_add_action_widget (plugin, battmon->battstatus);

    /* Determine what facility to use and initialize reading */
    battmon->method = BM_BROKEN;
    update_apm_status(battmon);

    /* If neither ACPI nor APM are enabled, check for either every 60 seconds */
    if(battmon->timeoutid == 0)
        battmon->timeoutid = g_timeout_add(60 * 1024, (GSourceFunc) update_apm_status, battmon);

    /* Required for the percentage and tooltip to be initially displayed due to the long timeout for ACPI */
    if(battmon->method == BM_USE_ACPI) {
        battmon->flag = TRUE;
        g_source_remove(battmon->timeoutid);
        battmon->timeoutid = g_timeout_add(1000, (GSourceFunc) update_apm_status, battmon);
    }
}

/* register the plugin */
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (battmon_construct);

