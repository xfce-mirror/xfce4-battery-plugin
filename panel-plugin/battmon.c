/*
 * Copyright (c) 2003 Nicholas Penwarden <toth64@yahoo.com>
 * Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
 * Copyright (c) 2003 edscott wilson garcia <edscott@users.sourceforge.net>
 * Copyright (c) 2005 Eduard Roccatello <eduard@xfce.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __FreeBSD__
#include <machine/apm_bios.h>
#elif __OpenBSD__
#include <sys/param.h>
#include <machine/apmvar.h>
#elif __linux__
#include <apm.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "battery.h"
#include "libacpi.h"

#define BORDER			8
#define HIGH_COLOR 		"#00ff00"
#define LOW_COLOR 		"#ffff00"
#define CRITICAL_COLOR 	"#ff0000"

typedef struct
{
	gboolean	display_label;	/* Options */
	gboolean	display_icon;	/* Options */
	gboolean	display_power;	/* Options */
	gboolean	display_percentage;	/* Options */
	gboolean	tooltip_display_percentage;
	gboolean	tooltip_display_time;
	int		low_percentage;
	int		critical_percentage;
	int		action_on_low;
	int		action_on_critical;
	char		*command_on_low;
	char		*command_on_critical;
	float		hsize;
	float		vsize;
} t_battmon_options;

typedef struct
{
	XfcePanelPlugin *plugin;
	
	GtkTooltips		*tips;
	GtkWidget		*vbox;		/* Widgets */
	GtkWidget		*ebox;
	GtkWidget		*battstatus;
	int			timeoutid;	/* To update apm status */
	int			method;
	gboolean		flag;
	gboolean		low;
	gboolean		critical;
	t_battmon_options	options;
	GdkColor		colorH;
	GdkColor		colorL;
	GdkColor		colorC;
	GtkLabel		*label;
	GtkLabel		*charge;
	GtkLabel		*rtime;
	GtkLabel		*acfan;
	GtkLabel		*temp;
	GtkWidget		*image;
} t_battmon;

typedef struct
{
	GtkWidget		*cb_disp_power;
	GtkWidget		*cb_disp_label;
	GtkWidget		*cb_disp_percentage;
	GtkWidget		*cb_disp_tooltip_percentage;
	GtkWidget		*cb_disp_tooltip_time;
	GtkWidget		*cb_disp_icon;
	GtkWidget		*sb_low_percentage;
	GtkWidget		*sb_critical_percentage;
	GtkWidget		*om_action_low;
	GtkWidget		*om_action_critical;
	GtkWidget		*en_command_low;
	GtkWidget		*en_command_critical;
	t_battmon		*battmon;
} t_battmon_dialog;

enum {BM_DO_NOTHING, BM_MESSAGE, BM_COMMAND, BM_COMMAND_TERM};
enum {BM_BROKEN, BM_USE_ACPI, BM_USE_APM};


static void
init_options(t_battmon_options *options)
{
	options->display_icon = FALSE;
	options->display_label = FALSE;
	options->display_power = FALSE;
	options->display_percentage = FALSE;
	options->tooltip_display_percentage = FALSE;
	options->tooltip_display_time = FALSE;
	options->low_percentage = 10;
	options->critical_percentage = 5;
	options->action_on_low = 0;
	options->action_on_critical = 0;
	options->command_on_low = NULL;
	options->command_on_critical = NULL;
	options->hsize = 1.75;
	options->vsize = 0.5;
}

gboolean
detect_battery_info(t_battmon *battmon)
{
#ifdef __OpenBSD__
	struct apm_power_info apm;
#else
	struct apm_info apm;
#endif

#ifdef __FreeBSD__
  /* This is how I read the information from the APM subsystem under
     FreeBSD.  Each time this functions is called (once every second)
     the APM device is opened, read from and then closed.

     except that is does not work on FreeBSD  
     
  */
	
  	int fd;

	/* First check to see if ACPI is available */
	if(check_acpi()==0) {
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
#ifdef DEBUG
	printf("using ACPI\n");
#endif
		return TRUE;
	}

	battmon->method = BM_BROKEN;
#ifdef APMDEVICE
  	fd = open(APMDEVICE, O_RDONLY);
  	if (fd == -1) return FALSE;

  	if (ioctl(fd, APMIO_GETINFO, &apm) == -1)
     		return FALSE;

  	close(fd);
  	battmon->method = BM_USE_APM;
#endif
  	return TRUE;
#elif __OpenBSD__
  /* Code for OpenBSD by Joe Ammond <jra@twinight.org>. Using the same
     procedure as for FreeBSD.
  */
  	int fd;

  	battmon->method = BM_BROKEN;
  	fd = open(APMDEVICE, O_RDONLY);
  	if (fd == -1) return FALSE;
  	if (ioctl(fd, APM_IOC_GETPOWER, &apm) == -1)
    		return FALSE;
  	close(fd);
  	battmon->method = BM_USE_APM;
  
  	return TRUE;
#elif __linux__
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
update_apm_status(t_battmon *battmon)
{
#ifdef __OpenBSD__
	struct apm_power_info apm;
#else
	struct apm_info apm;
#endif
	int charge=0;
	gboolean fan=FALSE;
	const char *temp;
	int time_remaining=0;
	gboolean acline;
	gchar buffer[128];


#ifdef __OpenBSD__
  /* Code for OpenBSD by Joe Ammond <jra@twinight.org>. Using the same
     procedure as for FreeBSD.
  */
  	int fd;

  	battmon->method = BM_BROKEN;
  	fd = open(APMDEVICE, O_RDONLY);
  	if (fd == -1) return TRUE;
  	if (ioctl(fd, APM_IOC_GETPOWER, &apminfo) == -1)
    		return TRUE;
  	close(fd);
  	charge = apm.battery_life;
  	time_remaining = apm.minutes_left;
  	acline = apm.ac_state ? TRUE : FALSE;
	
#else
#ifdef DEBUG
	printf("updating battery status...\n");
#endif
	if(battmon->method == BM_BROKEN) {
		/* See if ACPI or APM support has been enabled yet */
		if(!detect_battery_info(battmon)) return TRUE;
		if(battmon->timeoutid != 0) g_source_remove(battmon->timeoutid);
		/* Poll only once per minute if using ACPI due to a bug */
#ifdef TURTLE_UPDATES
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
		battmon->flag = FALSE;
		g_source_remove(battmon->timeoutid);
		battmon->timeoutid = g_timeout_add(2 * 1024, 
				(GSourceFunc) update_apm_status, battmon);
	}
	if(battmon->method == BM_USE_ACPI) {
		int i;
		acline = read_acad_state();
		for (i=0;i<batt_count;i++) {
		    if (read_acpi_state(i)) break; 
		}
		/*read_acpi_state(0);*/ /* only consider first battery... */
		if (batt_count) {
		   charge = acpistate->percentage;
		   time_remaining = acpistate->rtime;
		}
	}
#ifdef __linux__	
	else {
		apm_read(&apm);	/* not broken and not using ACPI, assume APM */
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

  	 if (ioctl(fd, APMIO_GETINFO, &apm) == -1)
     		return TRUE;

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
		
	if(battmon->options.display_percentage){
		gtk_widget_show((GtkWidget *)battmon->charge);
		gtk_widget_show((GtkWidget *)battmon->rtime);
		g_snprintf(buffer, sizeof(buffer),"%d%% ", charge);
		gtk_label_set_text(battmon->charge,buffer);
		g_snprintf(buffer, sizeof(buffer),"%02d:%02d ",time_remaining/60,time_remaining%60);
		gtk_label_set_text(battmon->rtime,buffer);
	} else {
		gtk_widget_hide((GtkWidget *)battmon->charge);
		gtk_widget_hide((GtkWidget *)battmon->rtime);
	}
		

	if(acline) {
		char *t=(charge<99.9)?_("(Charging from AC)"):_("(AC on-line)");
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
  	  if(acline && fan) gtk_label_set_text(battmon->acfan,"AC FAN");
	  else if(acline && !fan) gtk_label_set_text(battmon->acfan,"AC");
	  else if(!acline && fan) gtk_label_set_text(battmon->acfan,"FAN");
	  else gtk_label_set_text(battmon->acfan,"");
	
	  temp=get_temperature();
	  if(temp) gtk_label_set_text(battmon->temp,temp);
	  else gtk_label_set_text(battmon->temp,"");
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
	  } 
	  else if(charge <= battmon->options.low_percentage) {
		gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_PRELIGHT, &(battmon->colorL));
		battmon->critical = FALSE;
	  } 
	  else {
	    	battmon->low = battmon->critical = FALSE;
		gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_PRELIGHT, &(battmon->colorH));
	  }
	}

	/* alarms */
	/* FIXME: should put in a timeout to terminate the alarm boxes after one
	 * minute because if they are left open, they block the event loop for
	 * the panel, and that means the critical action will not be performed! */
	if (!acline && charge <= battmon->options.low_percentage){
		if(!battmon->critical && charge <= battmon->options.critical_percentage) {
		   	battmon->critical = TRUE;
			if(battmon->options.action_on_critical == BM_MESSAGE){
do_critical_warn:
				xfce_warn(_("WARNING: Your battery has reached critical status. You should plug in or shutdown your computer now to avoid possible data loss."));
				return TRUE;
			}
			if(battmon->options.action_on_critical == BM_COMMAND || 
			   battmon->options.action_on_critical == BM_COMMAND_TERM){
				int interm=(battmon->options.action_on_critical == BM_COMMAND_TERM)?1:0;
				if (!battmon->options.command_on_critical ||
				    !strlen(battmon->options.command_on_critical)) goto do_critical_warn;
				xfce_exec (battmon->options.command_on_critical, interm, 0, NULL);
			}
		} else if (!battmon->low){
		        battmon->low = TRUE;
			if(battmon->options.action_on_low == BM_MESSAGE){
do_low_warn:
				xfce_warn(_("WARNING: Your battery is running low. You should consider plugging in or shutting down your computer soon to avoid possible data loss."));
				return TRUE;
			}
			if(battmon->options.action_on_low == BM_COMMAND ||
			   battmon->options.action_on_low == BM_COMMAND_TERM){
				int interm=(battmon->options.action_on_low == BM_COMMAND_TERM)?1:0;
				if (!battmon->options.command_on_low ||
				    !strlen(battmon->options.command_on_low)) goto do_low_warn;
				xfce_exec(battmon->options.command_on_low, interm, 0, NULL);
			}
		}
	}
	return TRUE;
}


static void setup_battmon(t_battmon *battmon, GtkOrientation orientation)
{	
	GtkWidget *box,*vbox;
	GdkPixbuf *icon;
	
	battmon->battstatus = gtk_progress_bar_new();
	if (orientation == GTK_ORIENTATION_HORIZONTAL) 
	{
	   gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(battmon->battstatus), 
			   GTK_PROGRESS_BOTTOM_TO_TOP);
	   box=gtk_hbox_new(FALSE, 0);
	   battmon->vbox = gtk_hbox_new(FALSE, 0);
	} else {
	   gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(battmon->battstatus), 
			   GTK_PROGRESS_LEFT_TO_RIGHT);
	   box=gtk_vbox_new(FALSE, 0);
	   battmon->vbox = gtk_vbox_new(FALSE, 0);
	}
	
	gtk_container_set_border_width(GTK_CONTAINER(battmon->vbox), BORDER / 2);

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(battmon->battstatus), 0.0);
	
	
	icon  = xfce_inline_icon_at_size (battery_pixbuf, 20, 32);
	battmon->image = gtk_image_new_from_pixbuf (icon);
	gtk_box_pack_start(GTK_BOX(box),GTK_WIDGET(battmon->image), FALSE, FALSE, 2);
	g_object_unref (icon);

  	battmon->label = (GtkLabel *)gtk_label_new(_("Battery"));
    	gtk_box_pack_start(GTK_BOX(box),GTK_WIDGET(battmon->label),FALSE, FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(box),  GTK_WIDGET(battmon->battstatus), FALSE, FALSE, 2);

	vbox = gtk_vbox_new(FALSE, 0);
	
	/* percent + rtime */
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(vbox), FALSE, FALSE, 0);
	
  	battmon->charge = (GtkLabel *)gtk_label_new("50%%");
    	gtk_box_pack_start(GTK_BOX(vbox),GTK_WIDGET(battmon->charge),FALSE, FALSE, 0);
	
  	battmon->rtime = (GtkLabel *)gtk_label_new("01:00");
    	gtk_box_pack_start(GTK_BOX(vbox),GTK_WIDGET(battmon->rtime),FALSE, FALSE, 0);

	vbox=gtk_vbox_new(FALSE, 0);
	
	/* ac-fan-temp */
	gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(vbox), FALSE, FALSE, 0);
  	
	battmon->acfan = (GtkLabel *)gtk_label_new("AC FAN");
    	gtk_box_pack_start(GTK_BOX(vbox),GTK_WIDGET(battmon->acfan),FALSE, FALSE, 0);
	 
  	battmon->temp = (GtkLabel *)gtk_label_new("40°C");
    	gtk_box_pack_start(GTK_BOX(vbox),GTK_WIDGET(battmon->temp),FALSE, FALSE, 0);


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
		gtk_widget_hide((GtkWidget *)battmon->rtime);
	} 

	gtk_container_add(GTK_CONTAINER(battmon->ebox),GTK_WIDGET(battmon->vbox));
	gtk_widget_show(battmon->ebox);


	gdk_color_parse(HIGH_COLOR, &(battmon->colorH));
	gdk_color_parse(LOW_COLOR, &(battmon->colorL));
	gdk_color_parse(CRITICAL_COLOR, &(battmon->colorC));
	gtk_widget_set_size_request(battmon->ebox, -1, -1);
}

static gboolean
battmon_set_orientation (XfcePanelPlugin *plugin, GtkOrientation orientation,
						 t_battmon *battmon)
{
    if (battmon->timeoutid) g_source_remove(battmon->timeoutid);
    gtk_container_remove(GTK_CONTAINER(battmon->ebox), GTK_WIDGET(battmon->vbox));
    setup_battmon(battmon,orientation);
    battmon->timeoutid = g_timeout_add(1 * 1024, (GSourceFunc) update_apm_status, battmon);

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
	battmon->ebox = gtk_event_box_new();
	setup_battmon(battmon, xfce_panel_plugin_get_orientation (plugin));
	battmon->timeoutid = 0;
	battmon->flag = FALSE;
	battmon->tips = gtk_tooltips_new ();
	g_object_ref (battmon->tips);
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

	g_object_unref (battmon->tips);
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

	battmon->options.tooltip_display_percentage = xfce_rc_read_bool_entry (rc, "tooltip_display_percentage", FALSE);

	battmon->options.tooltip_display_time = xfce_rc_read_bool_entry (rc, "tooltip_display_time", FALSE);

	battmon->options.low_percentage = xfce_rc_read_int_entry (rc, "low_percentage", 10);

	battmon->options.critical_percentage = xfce_rc_read_int_entry (rc, "critical_percentage", 5);

	battmon->options.action_on_low = xfce_rc_read_int_entry (rc, "action_on_low", 0);

	battmon->options.action_on_critical = xfce_rc_read_int_entry (rc, "action_on_critical", 0);

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
	char *file;

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

	xfce_rc_write_bool_entry (rc, "tooltip_display_percentage", battmon->options.tooltip_display_percentage);

	xfce_rc_write_bool_entry (rc, "tooltip_display_time", battmon->options.tooltip_display_time);

	xfce_rc_write_int_entry (rc, "low_percentage", battmon->options.low_percentage);

	xfce_rc_write_int_entry (rc, "critical_percentage", battmon->options.critical_percentage);

	xfce_rc_write_int_entry (rc, "action_on_low", battmon->options.action_on_low);

	xfce_rc_write_int_entry (rc, "action_on_critical", battmon->options.action_on_critical);

	xfce_rc_write_entry (rc, "command_on_low", battmon->options.command_on_low ? battmon->options.command_on_low : "");

	xfce_rc_write_entry (rc, "command_on_critical", battmon->options.command_on_critical ? battmon->options.command_on_critical : 0);

	xfce_rc_close (rc);
}

static gboolean
battmon_set_size(XfcePanelPlugin *plugin, int size, t_battmon *battmon)
{
	if (xfce_panel_plugin_get_orientation (plugin) == 
			GTK_ORIENTATION_HORIZONTAL)
	{
	      gtk_widget_set_size_request(GTK_WIDGET(battmon->battstatus),
                                      BORDER, size);
	}
	else 
	{
	      gtk_widget_set_size_request(GTK_WIDGET(battmon->battstatus),
                                      size, BORDER);
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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_tooltip_percentage), battmon->options.tooltip_display_percentage);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_tooltip_time), battmon->options.tooltip_display_time);
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
    gtk_widget_destroy (dlg);
    xfce_panel_plugin_unblock_menu (battmon->plugin);
    battmon_write_config (battmon->plugin, battmon);
}

static void
battmon_create_options(XfcePanelPlugin *plugin, t_battmon *battmon)
{
    GtkWidget *dlg, *header;
	GtkWidget *vbox, *vbox2, *hbox, *label, *menu, *mi, *button, *button2;
	GtkSizeGroup *sg;
	t_battmon_dialog *dialog;

	dialog = g_new0(t_battmon_dialog, 1);

	dialog->battmon = battmon;
    
    xfce_panel_plugin_block_menu (plugin);
    
    dlg = gtk_dialog_new_with_buttons (_("Configure Battery Monitor"), 
                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                GTK_DIALOG_DESTROY_WITH_PARENT |
                GTK_DIALOG_NO_SEPARATOR,
                GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                NULL);
    
    g_signal_connect (dlg, "response", G_CALLBACK (battmon_dialog_response),
                      battmon);

    gtk_container_set_border_width (GTK_CONTAINER (dlg), 2);
    
	header = xfce_create_header (NULL, _("Battery Monitor"));
    gtk_widget_set_size_request (GTK_BIN (header)->child, -1, 32);
    gtk_container_set_border_width (GTK_CONTAINER (header), BORDER - 2);
    gtk_widget_show (header);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), header,
                        FALSE, TRUE, 0);
    
    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER - 2);
    gtk_widget_show(vbox);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox,
                        TRUE, TRUE, 0);
    
	/* Create size group to keep widgets aligned */

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Low and Critical percentage settings */

	hbox = gtk_hbox_new(FALSE, BORDER);	
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Low percentage:"));
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	dialog->sb_low_percentage = gtk_spin_button_new_with_range(1, 100, 1);
	gtk_widget_show(dialog->sb_low_percentage);
	gtk_box_pack_start(GTK_BOX(hbox), dialog->sb_low_percentage, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, BORDER);	
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Critical percentage:"));
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	dialog->sb_critical_percentage = gtk_spin_button_new_with_range(1, 100, 1);
	gtk_widget_show(dialog->sb_critical_percentage);
	gtk_box_pack_start(GTK_BOX(hbox), dialog->sb_critical_percentage, FALSE, FALSE, 0);

	/* Low battery action settings */

	hbox = gtk_hbox_new(FALSE, BORDER);	
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Low battery action:"));
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	menu = gtk_menu_new();
 	mi = gtk_menu_item_new_with_label(_("Do nothing"));
    	gtk_widget_show(mi);
    	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
 	mi = gtk_menu_item_new_with_label(_("Display a warning message"));
    	gtk_widget_show(mi);
    	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_menu_item_new_with_label(_("Run command"));
    	gtk_widget_show(mi);
    	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
 	mi = gtk_menu_item_new_with_label(_("Run command in terminal"));
    	gtk_widget_show(mi);
    	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

	dialog->om_action_low = gtk_option_menu_new();
	gtk_widget_show(dialog->om_action_low);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(dialog->om_action_low), menu);
	gtk_box_pack_start(GTK_BOX(hbox), dialog->om_action_low, FALSE, FALSE, 0);

	/* Low battery command */

	hbox = gtk_hbox_new(FALSE, BORDER);
    	gtk_widget_show(hbox);
  	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    	label = gtk_label_new(_("Command:"));
    	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    	gtk_size_group_add_widget(sg, label);
    	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    	dialog->en_command_low = gtk_entry_new();
	gtk_widget_show(dialog->en_command_low);
    	gtk_box_pack_start(GTK_BOX(hbox), dialog->en_command_low, FALSE, FALSE, 0);

    	button = gtk_button_new_with_label("...");
    	gtk_widget_show(button);
    	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

	/* Critical battery action settings */

	hbox = gtk_hbox_new(FALSE, BORDER);	
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Critical battery action:"));
	gtk_size_group_add_widget(sg, label);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	menu = gtk_menu_new();
 	mi = gtk_menu_item_new_with_label(_("Do nothing"));
    	gtk_widget_show(mi);
    	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
 	mi = gtk_menu_item_new_with_label(_("Display a warning message"));
    	gtk_widget_show(mi);
    	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
	mi = gtk_menu_item_new_with_label(_("Run command"));
    	gtk_widget_show(mi);
    	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);
 	mi = gtk_menu_item_new_with_label(_("Run command in terminal"));
    	gtk_widget_show(mi);
    	gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi);

	dialog->om_action_critical = gtk_option_menu_new();
	gtk_widget_show(dialog->om_action_critical);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(dialog->om_action_critical), menu);
	gtk_box_pack_start(GTK_BOX(hbox), dialog->om_action_critical, FALSE, FALSE, 0);

	/* Critical battery command */

	hbox = gtk_hbox_new(FALSE, BORDER);
    	gtk_widget_show(hbox);
  	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

    	label = gtk_label_new(_("Command:"));
    	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    	gtk_size_group_add_widget(sg, label);
    	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    	dialog->en_command_critical = gtk_entry_new();
	gtk_widget_show(dialog->en_command_critical);
	gtk_box_pack_start(GTK_BOX(hbox), dialog->en_command_critical, FALSE, FALSE, 0);

    	button2 = gtk_button_new_with_label("...");
    	gtk_widget_show(button2);
    	gtk_box_pack_start(GTK_BOX(hbox), button2, FALSE, FALSE, 0);

	/* Create checkbox options */

	hbox = gtk_hbox_new(FALSE, BORDER);	
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(NULL);
	gtk_size_group_add_widget(sg, label);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(vbox2);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);

	dialog->cb_disp_label = gtk_check_button_new_with_mnemonic(_("Display label"));
	gtk_widget_show(dialog->cb_disp_label);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_label, FALSE, FALSE, 0);
	
	dialog->cb_disp_percentage = gtk_check_button_new_with_mnemonic(_("Display percentage"));
	gtk_widget_show(dialog->cb_disp_percentage);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_percentage, FALSE, FALSE, 0);

	dialog->cb_disp_tooltip_percentage = gtk_check_button_new_with_mnemonic(_("Display percentage in tooltip"));
	gtk_widget_show(dialog->cb_disp_tooltip_percentage);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_tooltip_percentage, FALSE, FALSE, 0);

	dialog->cb_disp_tooltip_time = gtk_check_button_new_with_mnemonic(_("Display time remaining in tooltip"));
	gtk_widget_show(dialog->cb_disp_tooltip_time);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_tooltip_time, FALSE, FALSE, 0);
	
	dialog->cb_disp_power = gtk_check_button_new_with_mnemonic(_("Display power"));
	gtk_widget_show(dialog->cb_disp_power);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_power, FALSE, FALSE, 0);

	dialog->cb_disp_icon = gtk_check_button_new_with_mnemonic(_("Display icon"));
	gtk_widget_show(dialog->cb_disp_icon);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_icon, FALSE, FALSE, 0);

	/* Signal connections should be set after setting tate of toggle buttons...*/
	refresh_dialog(dialog);

	g_signal_connect(button, "clicked", G_CALLBACK(command_browse_cb), dialog->en_command_low);
	g_signal_connect(button2, "clicked", G_CALLBACK(command_browse_cb), dialog->en_command_critical);
	g_signal_connect(dialog->cb_disp_percentage, "toggled", G_CALLBACK(set_disp_percentage), dialog);
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

	gtk_widget_show (dlg);
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
	
	g_signal_connect (plugin, "orientation-changed", G_CALLBACK (battmon_set_orientation), battmon);
	
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

