/*
 * Copyright (c) 2003 Nicholas Penwarden <toth64@yahoo.com>
 * Copyright (c) 2003 Benedikt Meurer <benedikt.meurer@unix-ag.uni-siegen.de>
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

#include <gtk/gtk.h>

#ifdef __FreeBSD__
#include <machine/apm_bios.h>
#elif __OpenBSD__
#include <sys/param.h>
#include <machine/apmvar.h>
#elif __linux__
#include <apm.h>
#endif

#include <libxfce4util/i18n.h>
#include <libxfcegui4/dialogs.h>
#include <panel/global.h>
#include <panel/controls.h>
#include <panel/plugins.h>
#include <panel/xfce_support.h>

#include "acpi-linux.h"

extern xmlDocPtr xmlconfig;
#define DATA(node) xmlNodeListGetString(xmlconfig, node->children, 1)

#define LOW_COLOR "#ff0000"

typedef struct
{
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
	GtkWidget		*vbox;		/* Widgets */
	GtkWidget		*ebox;
	GtkWidget		*battstatus;
	int			timeoutid;	/* To update apm status */
	int			method;
	gboolean		flag;
	gboolean		low;
	gboolean		critical;
	t_battmon_options	options;
	GdkColor		color;
} t_battmon;

typedef struct
{
	GtkWidget		*cb_disp_percentage;
	GtkWidget		*cb_disp_tooltip_percentage;
	GtkWidget		*cb_disp_tooltip_time;
	GtkWidget		*sb_low_percentage;
	GtkWidget		*sb_critical_percentage;
	GtkWidget		*om_action_low;
	GtkWidget		*om_action_critical;
	GtkWidget		*en_command_low;
	GtkWidget		*en_command_critical;
	Control			*ctrl;
	GtkWidget		*revert;
	t_battmon		*battmon;
	t_battmon_options	backup;
} t_battmon_dialog;

enum {BM_DO_NOTHING, BM_MESSAGE, BM_COMMAND, BM_COMMAND_TERM};
enum {BM_BROKEN, BM_USE_ACPI, BM_USE_APM};

static GtkWidget *top_dialog;

static void
init_options(t_battmon_options *options)
{
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
  */
  	int fd;

	battmon->method = BM_BROKEN;
  	fd = open(APMDEVICE, O_RDONLY);
  	if (fd == -1) return FALSE;

  	if (ioctl(fd, APMIO_GETINFO, &apm) == -1)
     		return FALSE;

  	close(fd);
  	battmon->method = BM_USE_APM;

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
	if(acpi_linux_read(&apm)) {
		/* ACPI detected and working */
		battmon->method = BM_USE_ACPI;
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
	int charge;
	int time_remaining;
	gboolean acline;
	gchar buffer[128];

	if(battmon->method == BM_BROKEN) {
		/* See if ACPI or APM support has been enabled yet */
		if(!detect_battery_info(battmon)) return TRUE;
		if(battmon->timeoutid != 0) g_source_remove(battmon->timeoutid);
		/* Poll only once per minute if using ACPI due to a bug */
		if(battmon->method == BM_USE_ACPI) {
			battmon->timeoutid = g_timeout_add(60 * 1000, (GSourceFunc) update_apm_status, battmon);
		}
	        else {
			battmon->timeoutid = g_timeout_add(2 * 1000, (GSourceFunc) update_apm_status, battmon);
		}
	}

	/* Show initial state if using ACPI rather than waiting a minute */
	if(battmon->flag) {
		battmon->flag = FALSE;
		g_source_remove(battmon->timeoutid);
		battmon->timeoutid = g_timeout_add(60 * 1000, (GSourceFunc) update_apm_status, battmon);
	}

#ifdef __FreeBSD__
  /* This is how I read the information from the APM subsystem under
     FreeBSD.  Each time this functions is called (once every second)
     the APM device is opened, read from and then closed.
  */
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
#elif __OpenBSD__
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
#elif __linux__
	if(battmon->method == BM_USE_ACPI) acpi_linux_read(&apm);
	else apm_read(&apm);	/* not broken and not using ACPI, assume APM */
	charge = apm.battery_percentage;
	if(battmon->method == BM_USE_ACPI) time_remaining = 0; /* no such info for ACPI :( */
	else time_remaining = apm.battery_time;
	acline = apm.ac_line_status ? TRUE : FALSE;
#endif
	
	if(charge < 0) charge = 0;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(battmon->battstatus), charge / 100.0);

	if(battmon->options.tooltip_display_percentage && battmon->options.tooltip_display_time) {
		if(acline) 
			g_snprintf(buffer, sizeof(buffer), _("%d%% (AC on-line)"), charge);
		else if(time_remaining > 0)
			g_snprintf(buffer, sizeof(buffer), _("%d%% (%d:%.2d) remaining"), charge, time_remaining / 60, time_remaining % 60);
		else
			g_snprintf(buffer, sizeof(buffer), _("%d%% (AC off-line)"), charge);

		add_tooltip(battmon->ebox, buffer);
	}
	else if(battmon->options.tooltip_display_percentage) {
		g_snprintf(buffer, sizeof(buffer), _("%d%%"), charge);
		add_tooltip(battmon->ebox, buffer);
	}
	else if(battmon->options.tooltip_display_time) {
		if(acline) 
			g_snprintf(buffer, sizeof(buffer), _("AC on-line"));
		else if(time_remaining > 0)
			g_snprintf(buffer, sizeof(buffer), _("%d:%.2d remaining"), time_remaining / 60, time_remaining % 60);
		else
			g_snprintf(buffer, sizeof(buffer), _("AC off-line"));

		add_tooltip(battmon->ebox, buffer);
	}
	else add_tooltip(battmon->ebox, NULL);

	if(battmon->options.display_percentage) {
		g_snprintf(buffer, sizeof(buffer), _("%d%%"), charge);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(battmon->battstatus), buffer);	
	}
	else gtk_progress_bar_set_text(GTK_PROGRESS_BAR(battmon->battstatus), NULL);

	if(!battmon->critical && charge <= battmon->options.critical_percentage) {
		battmon->critical = TRUE;
		battmon->low = TRUE;

		gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_PRELIGHT, &(battmon->color));

		if(battmon->options.action_on_critical == BM_MESSAGE)
			xfce_warn(_("WARNING: Your battery has reached critical status. You should plug in or shutdown your computer now to avoid possible data loss."));
		if(battmon->options.action_on_critical == BM_COMMAND) {
			if(battmon->options.command_on_critical && battmon->options.command_on_critical[0])
				exec_cmd(battmon->options.command_on_critical, 0, 0);
		}
		if(battmon->options.action_on_critical == BM_COMMAND_TERM) {
			if(battmon->options.command_on_critical && battmon->options.command_on_critical[0])
				exec_cmd(battmon->options.command_on_critical, 1, 0);
		}
	}
	else if(battmon->critical && charge > battmon->options.critical_percentage)
		battmon->critical = FALSE;

	if(!battmon->low && charge <= battmon->options.low_percentage) {
		battmon->low = TRUE;

		gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_PRELIGHT, &(battmon->color));

		if(battmon->options.action_on_low == BM_MESSAGE)
			xfce_warn(_("WARNING: Your battery is running low. You should consider plugging in or shutting down your computer soon to avoid possible data loss."));
		if(battmon->options.action_on_low == BM_COMMAND) {
			if(battmon->options.command_on_low && battmon->options.command_on_low[0])
				exec_cmd(battmon->options.command_on_low, 0, 0);
		}
		if(battmon->options.action_on_low == BM_COMMAND_TERM) {
			if(battmon->options.command_on_low && battmon->options.command_on_low[0])
				exec_cmd(battmon->options.command_on_low, 1, 0);
		}
	}
	else if(battmon->low && charge > battmon->options.low_percentage) {
		battmon->low = FALSE;
		gtk_widget_modify_bg(battmon->battstatus, GTK_STATE_PRELIGHT, NULL);
	}

	return TRUE;
}

static t_battmon *
battmon_new(void)
{
	t_battmon *battmon;

	battmon = g_new(t_battmon, 1);
	init_options(&(battmon->options));

	battmon->low = FALSE;
	battmon->critical = FALSE;
	battmon->vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(battmon->vbox);
	gtk_container_set_border_width(GTK_CONTAINER(battmon->vbox), border_width);

	battmon->battstatus = gtk_progress_bar_new();
	gtk_widget_show(battmon->battstatus);
	gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(battmon->battstatus), GTK_PROGRESS_LEFT_TO_RIGHT);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(battmon->battstatus), 0.0);
		
	battmon->ebox = gtk_event_box_new();
	gtk_widget_show(battmon->ebox);
	gtk_container_add(GTK_CONTAINER(battmon->ebox), battmon->battstatus);
	gtk_box_pack_start(GTK_BOX(battmon->vbox), battmon->ebox, FALSE, FALSE, 0);

	battmon->timeoutid = 0;
	battmon->flag = FALSE;

	gdk_color_parse(LOW_COLOR, &(battmon->color));
	
	return(battmon);
}

static gboolean
battmon_control_new(Control *ctrl)
{
	t_battmon *battmon;

	battmon = battmon_new();

	gtk_container_add(GTK_CONTAINER(ctrl->base), battmon->vbox);

	ctrl->data = (gpointer)battmon;
	ctrl->with_popup = FALSE;

	gtk_widget_set_size_request(ctrl->base, -1, -1);

	/* Determine what facility to use and initialize reading */
	battmon->method = BM_BROKEN;
	update_apm_status(battmon);	

	/* If neither ACPI nor APM are enabled, check for either every 30 seconds */
	if(battmon->timeoutid == 0)
		battmon->timeoutid = g_timeout_add(30 * 1000, (GSourceFunc) update_apm_status, battmon);

	/* Required for the percentage and tooltip to be initially displayed due to the long timeout for ACPI */
	if(battmon->method == BM_USE_ACPI) {
		battmon->flag = TRUE;
		g_source_remove(battmon->timeoutid);
		battmon->timeoutid = g_timeout_add(2 * 1000, (GSourceFunc) update_apm_status, battmon);
	}

	return(TRUE);
}

static void
battmon_free(Control *ctrl)
{
	t_battmon *battmon;

	g_return_if_fail(ctrl != NULL);
	g_return_if_fail(ctrl->data != NULL);

	battmon = (t_battmon *)ctrl->data;

	if(battmon->timeoutid != 0) {
		g_source_remove(battmon->timeoutid);
		battmon->timeoutid = 0;
	}

	g_free(battmon);
}

static void
battmon_read_config(Control *ctrl, xmlNodePtr parent)
{
	xmlChar *value;
	xmlNodePtr child;
	
	t_battmon *battmon = (t_battmon *) ctrl->data;

	if(!parent || !parent->children) return;

	child = parent->children;

	if(!xmlStrEqual(child->name, "BatteryMonitor")) return;

	if(value = xmlGetProp(child, (const xmlChar *) "display_percentage")) {
		battmon->options.display_percentage = atoi(value);
		g_free(value);
	}
	if(value = xmlGetProp(child, (const xmlChar *) "tooltip_display_percentage")) {
		battmon->options.tooltip_display_percentage = atoi(value);
		g_free(value);
	}
	if(value = xmlGetProp(child, (const xmlChar *) "tooltip_display_time")) {
		battmon->options.tooltip_display_time = atoi(value);
		g_free(value);
	}
	if(value = xmlGetProp(child, (const xmlChar *) "low_percentage")) {
		battmon->options.low_percentage = atoi(value);
		g_free(value);
	}
	if(value = xmlGetProp(child, (const xmlChar *) "critical_percentage")) {
		battmon->options.critical_percentage = atoi(value);
		g_free(value);
	}
	if(value = xmlGetProp(child, (const xmlChar *) "action_on_low")) {
		battmon->options.action_on_low = atoi(value);
		g_free(value);
	}
	if(value = xmlGetProp(child, (const xmlChar *) "action_on_critical")) {
		battmon->options.action_on_critical = atoi(value);
		g_free(value);
	}

	if(!child || !child->children) return;

	for(child = child->children; child; child = child->next) {
		if(xmlStrEqual(child->name, (const xmlChar *) "command_on_low")) {
			value = DATA(child);
			if(battmon->options.command_on_low) g_free(battmon->options.command_on_low);
			if(value) battmon->options.command_on_low = (char *)value;
		}
		if(xmlStrEqual(child->name, (const xmlChar *) "command_on_critical")) {
			value = DATA(child);
			if(battmon->options.command_on_critical) g_free(battmon->options.command_on_critical);
			if(value) battmon->options.command_on_critical = (char *)value;
		}
	}
}

static void
battmon_write_config(Control *ctrl, xmlNodePtr parent)
{
	xmlNodePtr root;
	char value[MAXSTRLEN + 1];

	t_battmon *battmon = (t_battmon *) ctrl->data;

	root = xmlNewTextChild(parent, NULL, "BatteryMonitor", NULL);
	/* Display Percentage */
	g_snprintf(value, 2, "%d", battmon->options.display_percentage);
	xmlSetProp(root, "display_percentage", value);
	/* Display Percentage in Tooltip */
	g_snprintf(value, 2, "%d", battmon->options.tooltip_display_percentage);
	xmlSetProp(root, "tooltip_display_percentage", value);
	/* Display Time in Tooltip */
	g_snprintf(value, 2, "%d", battmon->options.tooltip_display_time);
	xmlSetProp(root, "tooltip_display_time", value);
	/* Low Percentage */
	g_snprintf(value, 4, "%d", battmon->options.low_percentage);
	xmlSetProp(root, "low_percentage", value);
	/* Critical Percentage */
	g_snprintf(value, 4, "%d", battmon->options.critical_percentage);
	xmlSetProp(root, "critical_percentage", value);
	/* Action on Low */
	g_snprintf(value, 2, "%d", battmon->options.action_on_low);
	xmlSetProp(root, "action_on_low", value);
	/* Action on Critical */
	g_snprintf(value, 2, "%d", battmon->options.action_on_critical);
	xmlSetProp(root, "action_on_critical", value);
	/* Command on Low */
	xmlNewTextChild(root, NULL, "command_on_low", battmon->options.command_on_low);
	/* Command on Critical */
	xmlNewTextChild(root, NULL, "command_on_critical", battmon->options.command_on_critical);
}

static void
battmon_attach_callback(Control *ctrl, const gchar *signal, GCallback cb, gpointer data)
{
	t_battmon *battmon;

	battmon = (t_battmon *)ctrl->data;
	g_signal_connect(battmon->vbox, signal, cb, data);
	g_signal_connect(battmon->ebox, signal, cb, data);
	g_signal_connect(battmon->battstatus, signal, cb, data);
}

static void
battmon_set_size(Control *ctrl, int size)
{
	t_battmon *battmon = (t_battmon *)ctrl->data;
	
	gtk_widget_set_size_request(battmon->vbox, icon_size[size] * battmon->options.hsize, icon_size[size] /* * battmon->options.vsize */);
	gtk_widget_set_size_request(battmon->battstatus, icon_size[size] * battmon->options.hsize, icon_size[size] * battmon->options.vsize);
	if(battmon->options.vsize < 1)
		gtk_box_set_child_packing(GTK_BOX(battmon->vbox), battmon->ebox, FALSE, FALSE, icon_size[size] * (1 - battmon->options.vsize) / 2, GTK_PACK_START);
	else
		gtk_box_set_child_packing(GTK_BOX(battmon->vbox), battmon->ebox, FALSE, FALSE, 0, GTK_PACK_START);
}

/* No longer needed as Revert is no longer a part of the XFce4 panel plugin API

static void battmon_backup(t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;

	dialog->backup.display_percentage = battmon->options.display_percentage;
	dialog->backup.tooltip_display_percentage = battmon->options.tooltip_display_percentage;
	dialog->backup.tooltip_display_time = battmon->options.tooltip_display_time;
	dialog->backup.low_percentage = battmon->options.low_percentage;
	dialog->backup.critical_percentage = battmon->options.critical_percentage;
	dialog->backup.action_on_low = battmon->options.action_on_low;
	dialog->backup.action_on_critical = battmon->options.action_on_critical;
	g_free(dialog->backup.command_on_low);
	dialog->backup.command_on_low = g_strdup(battmon->options.command_on_low);
	g_free(dialog->backup.command_on_critical);
	dialog->backup.command_on_critical = g_strdup(battmon->options.command_on_critical);
}

*/

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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_percentage), battmon->options.display_percentage);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_tooltip_percentage), battmon->options.tooltip_display_percentage);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->cb_disp_tooltip_time), battmon->options.tooltip_display_time);
	gtk_widget_set_sensitive(dialog->en_command_low, (battmon->options.action_on_low > 1) ? 1 : 0);
	gtk_widget_set_sensitive(dialog->en_command_critical, (battmon->options.action_on_critical > 1) ? 1 : 0);
}

/* No longer required for the new plugin API

static void battmon_restore_backup(t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;

	battmon->options.display_percentage = dialog->backup.display_percentage;
	battmon->options.tooltip_display_percentage = dialog->backup.tooltip_display_percentage;
	battmon->options.tooltip_display_time = dialog->backup.tooltip_display_time;
	battmon->options.low_percentage = dialog->backup.low_percentage;
	battmon->options.critical_percentage = dialog->backup.critical_percentage;
	battmon->options.action_on_low = dialog->backup.action_on_low;
	battmon->options.action_on_critical = dialog->backup.action_on_critical;
	g_free(battmon->options.command_on_low);
	battmon->options.command_on_low = g_strdup(dialog->backup.command_on_low);
	g_free(battmon->options.command_on_critical);
	battmon->options.command_on_critical = g_strdup(dialog->backup.command_on_critical);

	refresh_dialog(dialog);
}

*/

static void
set_disp_percentage(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;

	battmon->options.display_percentage = gtk_toggle_button_get_active(tb);

/*	gtk_widget_set_sensitive(dialog->revert, TRUE);	*/
}

static void
set_tooltip_disp_percentage(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;

	battmon->options.tooltip_display_percentage = gtk_toggle_button_get_active(tb);

/*	gtk_widget_set_sensitive(dialog->revert, TRUE);	*/
}

static void
set_tooltip_time(GtkToggleButton *tb, t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;

	battmon->options.tooltip_display_time = gtk_toggle_button_get_active(tb);

/*	gtk_widget_set_sensitive(dialog->revert, TRUE);	*/
}

static void
set_low_percentage(GtkSpinButton *sb, t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;

	battmon->options.low_percentage = gtk_spin_button_get_value_as_int(sb);

/*	gtk_widget_set_sensitive(dialog->revert, TRUE);	*/
}

static void
set_critical_percentage(GtkSpinButton *sb, t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;

	battmon->options.critical_percentage = gtk_spin_button_get_value_as_int(sb);

/*	gtk_widget_set_sensitive(dialog->revert, TRUE);	*/
}

static void
set_action_low(GtkOptionMenu *om, t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;

	battmon->options.action_on_low = gtk_option_menu_get_history(om);

	gtk_widget_set_sensitive(dialog->en_command_low, (gtk_option_menu_get_history(om) > 1) ? 1 : 0);
/*	gtk_widget_set_sensitive(dialog->revert, TRUE);	*/
}

static void
set_action_critical(GtkOptionMenu *om, t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;

	battmon->options.action_on_critical = gtk_option_menu_get_history(om);

	gtk_widget_set_sensitive(dialog->en_command_critical, (gtk_option_menu_get_history(om) > 1) ? 1 : 0);
/*	gtk_widget_set_sensitive(dialog->revert, TRUE);	*/
}

gboolean
set_command_low(GtkEntry *en, GdkEventFocus *event, t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;
	const char *temp;

	g_free(battmon->options.command_on_low);
	temp = gtk_entry_get_text(en);
	battmon->options.command_on_low = g_strdup(temp); 

/*	gtk_widget_set_sensitive(dialog->revert, TRUE);	*/

	/* Prevents a GTK crash */
	return FALSE;
}

gboolean
set_command_critical(GtkEntry *en, GdkEventFocus *event, t_battmon_dialog *dialog)
{
	t_battmon *battmon = dialog->battmon;
	const char *temp;

	g_free(battmon->options.command_on_critical);
	temp = gtk_entry_get_text(en);
	battmon->options.command_on_critical = g_strdup(temp); 

/*	gtk_widget_set_sensitive(dialog->revert, TRUE);	*/

	/* Prevents a GTK crash */
	return FALSE;
}

static void
command_browse_cb (GtkWidget *b, GtkEntry *entry)
{
    char *file = select_file_name(_("Select command"), gtk_entry_get_text(entry), top_dialog);

    if (file) {
	gtk_entry_set_text (entry, file);
	g_free (file);
    }
}

static void
battmon_create_options(Control *ctrl, GtkContainer *container, GtkWidget *done)
{
	t_battmon *battmon = ctrl->data;
	t_battmon_dialog *dialog;

	GtkWidget *vbox, *vbox2, *hbox, *label, *menu, *mi, *button, *button2;
	GtkSizeGroup *sg;

	dialog = g_new0(t_battmon_dialog, 1);
	init_options(&(dialog->backup));
	top_dialog = gtk_widget_get_toplevel(done);

	dialog->ctrl = ctrl;
	dialog->battmon = battmon;
/*	dialog->revert = revert;	*/

	/* Backup current options */

/*	battmon_backup(dialog);	*/

	/* Create main container */

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(vbox);
	gtk_container_add(container, vbox);

	/* Create size group to keep widgets aligned */

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Low and Critical percentage settings */

	hbox = gtk_hbox_new(FALSE, 4);	
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

	hbox = gtk_hbox_new(FALSE, 4);	
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

	hbox = gtk_hbox_new(FALSE, 4);	
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

	hbox = gtk_hbox_new(FALSE, 4);
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

	hbox = gtk_hbox_new(FALSE, 4);	
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

	hbox = gtk_hbox_new(FALSE, 4);
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

	hbox = gtk_hbox_new(FALSE, 4);	
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(NULL);
	gtk_size_group_add_widget(sg, label);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(vbox2);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);

	dialog->cb_disp_percentage = gtk_check_button_new_with_mnemonic(_("Display percentage"));
	gtk_widget_show(dialog->cb_disp_percentage);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_percentage, FALSE, FALSE, 0);

	dialog->cb_disp_tooltip_percentage = gtk_check_button_new_with_mnemonic(_("Display percentage in tooltip"));
	gtk_widget_show(dialog->cb_disp_tooltip_percentage);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_tooltip_percentage, FALSE, FALSE, 0);

	dialog->cb_disp_tooltip_time = gtk_check_button_new_with_mnemonic(_("Display time remaining in tooltip"));
	gtk_widget_show(dialog->cb_disp_tooltip_time);
	gtk_box_pack_start(GTK_BOX(vbox2), dialog->cb_disp_tooltip_time, FALSE, FALSE, 0);

	/* Signal connections */

	g_signal_connect(button, "clicked", G_CALLBACK(command_browse_cb), dialog->en_command_low);
	g_signal_connect(button2, "clicked", G_CALLBACK(command_browse_cb), dialog->en_command_critical);
	g_signal_connect(dialog->cb_disp_percentage, "toggled", G_CALLBACK(set_disp_percentage), dialog);
	g_signal_connect(dialog->cb_disp_tooltip_percentage, "toggled", G_CALLBACK(set_tooltip_disp_percentage), dialog);
	g_signal_connect(dialog->cb_disp_tooltip_time, "toggled", G_CALLBACK(set_tooltip_time), dialog);
	g_signal_connect(dialog->sb_low_percentage, "value-changed", G_CALLBACK(set_low_percentage), dialog);
	g_signal_connect(dialog->sb_critical_percentage, "value-changed", G_CALLBACK(set_critical_percentage), dialog);
	g_signal_connect(dialog->om_action_low, "changed", G_CALLBACK(set_action_low), dialog);
	g_signal_connect(dialog->om_action_critical, "changed", G_CALLBACK(set_action_critical), dialog);
	g_signal_connect(dialog->en_command_low, "focus-out-event", G_CALLBACK(set_command_low), dialog);
	g_signal_connect(dialog->en_command_critical, "focus-out-event", G_CALLBACK(set_command_critical), dialog);
/*	g_signal_connect_swapped(revert, "clicked", G_CALLBACK(battmon_restore_backup), dialog);	*/

	refresh_dialog(dialog);
}

G_MODULE_EXPORT void
xfce_control_class_init(ControlClass *cc)
{

#ifdef ENABLE_NLS
    /* This is required for UTF-8 at least - Please don't remove it */
    bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

	cc->name		= "batterymonitor";
	cc->caption		= _("Battery Monitor");

	cc->create_control	= (CreateControlFunc)battmon_control_new;

	cc->free		= battmon_free;
	cc->read_config		= battmon_read_config;
	cc->write_config	= battmon_write_config;
	cc->attach_callback	= battmon_attach_callback;

	cc->set_size		= battmon_set_size;
	cc->create_options		= battmon_create_options;
}

XFCE_PLUGIN_CHECK_INIT
