/***************************************************************************
                          libacpi.c  -  description
                             -------------------
    August 2003 C.  edscott wilson garcia <edscott@imp.mx>: BSD acpi stuff
    begin                : Feb 10 2003
    copyright            : (C) 2003 by Noberasco Michele
    e-mail               : 2001s098@educ.disi.unige.it
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.              *
 *                                                                         *
 ***************************************************************************/
 
 /***************************************************************************
        Originally written by Costantino Pistagna for his wmacpimon
 ***************************************************************************/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef __libacpi_c__
#define __libacpi_c__
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#if HAVE_SYSCTL
#include <sys/sysctl.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>

#endif

#include "libacpi.h"


static char batteries[MAXBATT][128];
static char battinfo[MAXBATT][128];
#if HAVE_SYSCTL
 static int
name2oid(char *name, int *oidp)
{
	int oid[2];
	int i;
	size_t j;

	oid[0] = 0;
	oid[1] = 3;

	j = CTL_MAXNAME * sizeof(int);
	i = sysctl(oid, 2, oidp, &j, name, strlen(name));
	if (i < 0) 
		return i;
	j /= sizeof(int);
	return (j);
}
static int
oidfmt(int *oid, int len, char *fmt, u_int *kind)
{
	int qoid[CTL_MAXNAME+2];
	u_char buf[BUFSIZ];
	int i;
	size_t j;

	qoid[0] = 0;
	qoid[1] = 4;
	memcpy(qoid + 2, oid, len * sizeof(int));

	j = sizeof(buf);
	i = sysctl(qoid, len + 2, buf, &j, 0, 0);
	if (i)
		err(1, "sysctl fmt %d %d %d", i, j, errno);

	if (kind)
		*kind = *(u_int *)buf;

	if (fmt)
		strcpy(fmt, (char *)(buf + sizeof(u_int)));
	return 0;
}



static int
get_var(int *oid, int nlen)
{
		int retval=0;
	u_char buf[BUFSIZ], *val, *p;
	char name[BUFSIZ], *fmt, *sep;
	int qoid[CTL_MAXNAME+2];
	int i;
	size_t j, len;
	u_int kind;
/*	int (*func)(int, void *);*/

	qoid[0] = 0;
	memcpy(qoid + 2, oid, nlen * sizeof(int));

	qoid[1] = 1;
	j = sizeof(name);
	i = sysctl(qoid, nlen + 2, name, &j, 0, 0);
	if (i || !j)
		err(1, "sysctl name %d %d %d", i, j, errno);

	sep = "=";


	/* find an estimate of how much we need for this var */
	j = 0;
	i = sysctl(oid, nlen, 0, &j, 0, 0);
	j += j; /* we want to be sure :-) */

	val = alloca(j + 1);
	len = j;
	i = sysctl(oid, nlen, val, &len, 0, 0);
	if (i || !len)
		return (1);

	val[len] = '\0';
	fmt = buf;
	oidfmt(oid, nlen, fmt, &kind);
	p = val;
	switch (*fmt) {		
	case 'I':
#ifdef DEBUG
		printf("I:%s%s", name, sep);
#endif
		fmt++;
		val = "";
		while (len >= sizeof(int)) {
			if(*fmt == 'U'){
				retval=*((unsigned int *)p);
#ifdef DEBUG
				printf("x%s%u", val, *(unsigned int *)p);
#endif
			}
			else {
				retval=*((int *)p);
#ifdef DEBUG
				printf("x%s%d", val, *(int *)p);
#endif
			}
			val = " ";
			len -= sizeof(int);
			p += sizeof(int);
		}
		
		return (retval);
	default:
			printf("%s%s", name, sep);
		printf("Format:%s Length:%d Dump:0x", fmt, len);
		while (len-- && (p < val + 16))
			printf("%02x", *p++);
		if (len > 16)
			printf("...");
		return (0);
	}
	return (0);
}

 
#endif

/* see if we have ACPI support */
int check_acpi(void)
{
  DIR *battdir;
  struct dirent *batt;
  char *name;
#ifdef __linux__
  FILE *acpi;

  if (!(acpi = fopen ("/proc/acpi/info", "r")))
  {
#ifdef DEBUG
	  printf("DBG:no acpi: /proc/acpi/info not found!\n");
#endif
    return 1;
  }

  /* yep, all good */
  fclose (acpi);

  /* now enumerate batteries */
  batt_count = 0;
  battdir = opendir ("/proc/acpi/battery");
  if (battdir == 0)
  {
#ifdef DEBUG
	  printf("DBG:No battery. /proc/acpi/battery not found!\n");
#endif
    return 2;
  }
  while ((batt = readdir (battdir)))
  {
    name = batt->d_name;

    /* skip . and .. */
    if (!strncmp (".", name, 1) || !strncmp ("..", name, 2)) continue;
    
    sprintf (batteries[batt_count], "/proc/acpi/battery/%s/state", name);
    if (!(acpi = fopen (batteries[batt_count], "r"))) {
       sprintf (batteries[batt_count], "/proc/acpi/battery/%s/status", name);
    }
    else fclose (acpi);
    
#if 0    
    if (!(acpi = fopen ("/proc/acpi/battery/1/status", "r")))
	    sprintf (batteries[batt_count], "/proc/acpi/battery/%s/state", name);
    else
	    sprintf (batteries[batt_count], "/proc/acpi/battery/%s/status", name);
#endif    
    sprintf (battinfo[batt_count], "/proc/acpi/battery/%s/info", name);
#ifdef DEBUG
	  printf("DBG:battery number %d at:\n",batt_count);
	  printf("DBG:info->%s\n",battinfo[batt_count]);
	  printf("DBG:state->%s\n",batteries[batt_count]);
	  printf("DBG:------------------------\n");
#endif

    batt_count++;
  }
  closedir (battdir);
  return 0;
#else
#ifdef HAVE_SYSCTL
  {
    static char buf[BUFSIZ];
    char *bufp=buf;
    char fmt[BUFSIZ];
    void *oldp=(void *)buf;
    size_t oldlenp=BUFSIZ;
    int len,mib[CTL_MAXNAME];
    u_int kind;
/*  snprintf(buf, BUFSIZ, "%s", "hw.acpi.battery.time");*/
    snprintf(buf, BUFSIZ, "%s", "hw.acpi.battery.units");
    len = name2oid(bufp, mib);
    if (len <=0) return 1;
    if (oidfmt(mib, len, fmt, &kind)) return 1;
    if ((kind & CTLTYPE) == CTLTYPE_NODE) return 1;
    batt_count=get_var(mib, len);
  
  }
  return 0;
#else
  return 1;
#endif
#endif
}

int read_acad_state(void)
{
#ifdef __linux__
  FILE *acpi;
  char *ptr;
  char stat;

  if (!(acpi = fopen ("/proc/acpi/ac_adapter/0/status", "r")))
    if (!(acpi = fopen ("/proc/acpi/ac_adapter/ACAD/state", "r")))
      if (!(acpi = fopen ("/proc/acpi/ac_adapter/AC/state", "r")))
        if (!(acpi = fopen ("/proc/acpi/ac_adapter/ADP1/state", "r")))
          if (!(acpi = fopen ("/proc/acpi/ac_adapter/ADP0/state", "r")))
	        return -1;

  fread (buf, 512, 1, acpi);
  fclose (acpi);
  if (!acadstate) acadstate=(ACADstate *)malloc(sizeof(ACADstate));

  if ( (ptr = strstr(buf, "state:")) )
  {
    stat = *(ptr + 26);
    if (stat == 'n') acadstate->state = 1;
    if (stat == 'f')
  	{
	    acadstate->state = 0;
	    return 0;
  	}
  }

  if ( (ptr = strstr (buf, "Status:")) )
  {
    stat = *(ptr + 26);
    if (stat == 'n') acadstate->state = 1;
    if (stat == 'f')
  	{
	    acadstate->state = 0;
	    return 0;
	  }
  }

  return 1;
#else
#ifdef HAVE_SYSCTL
  static char buf[BUFSIZ];
  char fmt[BUFSIZ];
  void *oldp=(void *)buf;
  char *bufp=buf;
  size_t oldlenp=BUFSIZ;
  int len,mib[CTL_MAXNAME];
  u_int kind;
  int retval;
  snprintf(buf, BUFSIZ, "%s", "hw.acpi.acline");
  len = name2oid(bufp, mib);
  if (oidfmt(mib, len, fmt, &kind))
	err(1, "couldn't find format of oid '%s'", bufp);
  if (len < 0) errx(1, "unknown oid '%s'", bufp);
  if ((kind & CTLTYPE) == CTLTYPE_NODE) {
	printf("oh-oh...\n");
  } else {
	retval=get_var(mib, len);
#ifdef DEBUG
	printf("retval=%d\n",retval);
#endif
  }
  return retval;
#else
  return 0;
#endif
#endif
}

int read_acpi_info(int battery)
{
#ifdef __linux__
  FILE *acpi;
  char *ptr;
  char stat;
  int temp;

  if (battery > MAXBATT) {
#ifdef DEBUG
	  printf("DBG: error, battery > MAXBATT (%d)\n",MAXBATT);
#endif
	  return 0;
  }
  if (!(acpi = fopen (battinfo[battery], "r"))) {
#ifdef DEBUG
	  printf("DBG:cannot open %s for read!\n",battinfo[battery]);
#endif
	  return 0;
  }

#ifdef DEBUG
  { 
	  int jj= fread (buf, 1,512, acpi);
	  printf("DBG:%d characters read from %s\n",jj,battinfo[battery]);
  }
#else
  fread (buf, 1,512, acpi);
#endif
  fclose (acpi);

  if (!acpiinfo) acpiinfo=(ACPIinfo *)malloc(sizeof(ACPIinfo));

  if ((ptr = strstr (buf, "present:")) || (ptr = strstr (buf, "Present:")))
  {
#ifdef DEBUG
	  printf("DBG:Battery present... and its called %s\n",battinfo[battery]);
#endif
    stat = *(ptr + 25);
    if (stat == 'y')
  	{
	    acpiinfo->present = 1;
  	  if ((ptr = strstr (buf, "design capacity:")) || (ptr = strstr (buf, "Design Capacity:")))
	    {
	      ptr += 25;
	      sscanf (ptr, "%d", &temp);
	      acpiinfo->design_capacity = temp;
#ifdef DEBUG
	  printf("DBG:design capacity:%d\n",temp);
#endif
	    }
	    if ((ptr = strstr (buf, "last full capacity:")) || (ptr = strstr (buf, "Last Full Capacity:")))
	    {
	      ptr += 25;
	      sscanf (ptr, "%d", &temp);
	      acpiinfo->last_full_capacity = temp;
#ifdef DEBUG
	  printf("DBG:last full capacity:%d\n",temp);
#endif
	    }
	    if ((ptr = strstr (buf, "battery technology:")) || (ptr = strstr (buf, "Battery Technology:")))
	    {
	      stat = *(ptr + 25);
	      switch (stat)
		    {
		      case 'n':
		        acpiinfo->battery_technology = 1;
		        break;
		      case 'r':
		        acpiinfo->battery_technology = 0;
		        break;
		    }
	    }
	    if ((ptr = strstr (buf, "design voltage:")) || (ptr = strstr (buf, "Design Voltage:")))
	    {
	      ptr += 25;
	      sscanf (ptr, "%d", &temp);
	      acpiinfo->design_voltage = temp;
#ifdef DEBUG
	  printf("DBG:design voltage:%d\n",temp);
#endif
	    }
	    if ((ptr = strstr (buf, "design capacity warning:")) || (ptr = strstr (buf, "Design Capacity Warning:")))
	    {
	      ptr += 25;
	      sscanf (ptr, "%d", &temp);
	      acpiinfo->design_capacity_warning = temp;
#ifdef DEBUG
	  printf("DBG:design capacity warning:%d\n",temp);
#endif
	    }
  	  if ((ptr = strstr (buf, "design capacity low:")) || (ptr = strstr (buf, "Design Capacity Low:")))
	    {
	      ptr += 25;
	      sscanf (ptr, "%d", &temp);
	      acpiinfo->design_capacity_low = temp;
#ifdef DEBUG
	  printf("DBG:design capacity low:%d\n",temp);
#endif
	    }
#ifdef DEBUG
	  printf("DBG:ALL Battery information read...\n");
#endif
	  }
    else /* Battery not present */
	  {
#ifdef DEBUG
	  printf("DBG:Battery not present!... and its called %s\n",battinfo[battery]);
#endif
	    acpiinfo->present = 0;
	    acpiinfo->design_capacity = 0;
	    acpiinfo->last_full_capacity = 0;
	    acpiinfo->battery_technology = 0;
	    acpiinfo->design_voltage = 0;
	    acpiinfo->design_capacity_warning = 0;
	    acpiinfo->design_capacity_low = 0;
	    return 0;
	  }
  }
	
  return 1;
#else
#ifdef HAVE_SYSCTL
  static char buf[BUFSIZ];
  char *bufp=buf;
  int len,mib[CTL_MAXNAME];
  char fmt[BUFSIZ];
  u_int kind;
  int retval;
  if (!acpiinfo) acpiinfo=(ACPIinfo *)malloc(sizeof(ACPIinfo));
	    acpiinfo->present = 0;
	    acpiinfo->design_capacity = 0;
	    acpiinfo->last_full_capacity = 0;
	    acpiinfo->battery_technology = 0;
	    acpiinfo->design_voltage = 0;
	    acpiinfo->design_capacity_warning = 0;
	    acpiinfo->design_capacity_low = 0;
  snprintf(buf, BUFSIZ, "%s", "hw.acpi.battery.units");
  len = name2oid(bufp, mib);
  if (oidfmt(mib, len, fmt, &kind))
	err(1, "couldn't find format of oid '%s'", bufp);
  if (len < 0) errx(1, "unknown oid '%s'", bufp);
  if ((kind & CTLTYPE) == CTLTYPE_NODE) {
	printf("oh-oh...\n");
  } else {
	retval=get_var(mib, len);
#ifdef DEBUG
	printf("retval=%d\n",retval);
#endif
  }
  acpiinfo->present = retval;
  return 1;
#else
  return 0;
#endif
#endif
  
}

int read_acpi_state(int battery)
{
#ifdef __linux__
  FILE *acpi;
  char *ptr;
  char stat;

  int percent = 100;		/* battery percentage */
  int ptemp, rate, rtime = 0;

  if (!(acpi = fopen (batteries[battery], "r"))) {
#ifdef DEBUG
	  printf("DBG:Could not open %s (%d)\n",batteries[battery],battery);
#endif
	  return 0;
  }

  fread (buf, 512, 1, acpi);
  fclose (acpi);
  if (!acpistate) acpistate=(ACPIstate *)malloc(sizeof(ACPIstate));

  if ((ptr = strstr (buf, "present:")) || (ptr = strstr (buf, "Present:")))
  {
#ifdef DEBUG
	  printf("DBG:Battery state present...\n");
#endif
    stat = *(ptr + 25);
    if (stat == 'y')
  	{
	    acpistate->present = 1;
	    if ((ptr = strstr (buf, "charging state:")) || (ptr = strstr (buf, "State:")))
	    {
	      stat = *(ptr + 25);
	      switch (stat)
		    {
		      case 'd':
		        acpistate->state = 1;
		        break;
		      case 'c':
		        if (*(ptr + 33) == '/')
		          acpistate->state = 0;
		        else
		          acpistate->state = 2;
		        break;
		      case 'u':
		        acpistate->state = 3;
		        break;
		    }
	    }
	    /* This section of the code will calculate "percentage remaining"
	     * using battery capacity, and the following formula 
	     * (acpi spec 3.9.2):
	     * 
	     * percentage = (current_capacity / last_full_capacity) * 100;
	     *
	     */
	    if ((ptr = strstr (buf, "remaining capacity:")) || (ptr = strstr (buf, "Remaining Capacity:")))
	    {
	      ptr += 25;
	      sscanf (ptr, "%d", &ptemp);
	      acpistate->rcapacity = ptemp;
	      percent =	(float) ((float) ptemp / (float) acpiinfo->last_full_capacity) * 100;
	      acpistate->percentage = percent;
#ifdef DEBUG
	  printf("DBG:remaining capacity:100 * %d/%d = %d\n",
			  ptemp,acpiinfo->last_full_capacity,acpistate->percentage);
#endif
	    }
	    if ((ptr = strstr (buf, "present rate:")) || (ptr = strstr (buf, "Present Rate:")))
	    {
	      ptr += 25;
	      sscanf (ptr, "%d", &rate);

	      /* if something wrong */
	      if (rate <= 0) rate = 0;

	      acpistate->prate = rate;
				
	      /* time remaining in minutes */
	      rtime = ((float) ((float) acpistate->rcapacity /
				(float) acpistate->prate)) * 60;
	      if (rtime <= 0) rtime = 0;
	      
				acpistate->rtime = rtime;
	    }
	    if ((ptr = strstr (buf, "present voltage:")) || (ptr = strstr (buf, "Battery Voltage:")))
	    {
	      ptr += 25;
	      sscanf (ptr, "%d", &ptemp);
	      acpistate->pvoltage = ptemp;
	    }
	  }
    else /* Battery not present */
	  {
	    acpistate->present = 0;
	    acpistate->state = UNKNOW;
	    acpistate->prate = 0;
	    acpistate->rcapacity = 0;
	    acpistate->pvoltage = 0;
	    acpistate->rtime = 0;
	    acpistate->percentage = 0;
	    return 0;
	  }
  }
	
  return 1;
#else
#ifdef HAVE_SYSCTL
  char *string;
  static char buf[BUFSIZ];
  char fmt[BUFSIZ];
  char *bufp=buf;
  void *oldp=(void *)buf;
  size_t oldlenp=BUFSIZ;
  int len,mib[CTL_MAXNAME];
  int retval;
  u_int kind;
  if (!acpistate) acpistate=(ACPIstate *)malloc(sizeof(ACPIstate));
  acpistate->present = 0;
  acpistate->state = UNKNOW;
  acpistate->prate = 0;
  acpistate->rcapacity = 0;
  acpistate->pvoltage = 0;
  acpistate->rtime = 0;
  acpistate->percentage = 0;
  
  snprintf(buf, BUFSIZ, "%s", "hw.acpi.battery.time");
  len = name2oid(bufp, mib);
  if (oidfmt(mib, len, fmt, &kind))
	err(1, "couldn't find format of oid '%s'", bufp);
  if (len < 0) errx(1, "unknown oid '%s'", bufp);
  if ((kind & CTLTYPE) == CTLTYPE_NODE) {
	printf("oh-oh...\n");
  } else {
	retval=get_var(mib, len);
#ifdef DEBUG
	printf("retval=%d\n",retval);
#endif
  }
  acpistate->rtime =(retval<0)?0:retval;
  
  snprintf(buf, BUFSIZ, "%s", "hw.acpi.battery.life");
  len = name2oid(bufp, mib);
  if (oidfmt(mib, len, fmt, &kind))
	err(1, "couldn't find format of oid '%s'", bufp);
  if (len < 0) errx(1, "unknown oid '%s'", bufp);
  if ((kind & CTLTYPE) == CTLTYPE_NODE) {
	printf("oh-oh...\n");
  } else {
	retval=get_var(mib, len);
#ifdef DEBUG
	printf("retval=%d\n",retval);
#endif
  }
  acpistate->percentage =retval;
  return 1;
#else
  return 0;
#endif
#endif
}

int get_fan_status(void)
{
  FILE *fp;
  char * proc_fan_status="/proc/acpi/toshiba/fan";
  char line[256];
  /*int result;*/

   /* Check for fan status in PROC filesystem */
    if ( (fp=fopen(proc_fan_status, "r")) != NULL ) {
 	  fgets(line,255,fp);
  	  fclose(fp);
  	  if (strlen(line) && strstr(line,"1")) return 1;
    	  else return 0;
    }
    proc_fan_status="/proc/acpi/fan/FAN/state";
    if ( (fp=fopen(proc_fan_status, "r")) == NULL ) return 0; 
    
    fgets(line,255,fp);
    fclose(fp);

    if (strlen(line) && strstr(line,"off")) return 0;
    else return 1;
}

const char *get_temperature(void)
{
#ifdef __linux__
  FILE *fp;
  char *proc_temperature="/proc/acpi/thermal_zone/THRM/temperature";
  static char *p,line[256];

  if ( (fp=fopen(proc_temperature, "r")) == NULL) return NULL;
  fgets(line,255,fp);
  fclose(fp);
  p=strtok(line," ");
  if (!p) return NULL;
  p=p+strlen(p)+1;
  while (p && *p ==' ') p++;
  if (*p==0) return NULL;
  if (strchr(p,'\n')) p=strtok(p,"\n");
  return (const char *)p;
#else
#ifdef HAVE_SYSCTL
  static char buf[BUFSIZ];
  char fmt[BUFSIZ];
  char *bufp=buf;
  void *oldp=(void *)buf;
  size_t oldlenp=BUFSIZ;
  int len,mib[CTL_MAXNAME];
  int retval;
  u_int kind;
  snprintf(buf, BUFSIZ, "%s", "hw.acpi.thermal.tz0.temperature");
  len = name2oid(bufp, mib);
  if (oidfmt(mib, len, fmt, &kind))
	err(1, "couldn't find format of oid '%s'", bufp);
  if (len < 0) errx(1, "unknown oid '%s'", bufp);
  if ((kind & CTLTYPE) == CTLTYPE_NODE) {
	printf("oh-oh...\n");
  } else {
	retval=get_var(mib, len);
#ifdef DEBUG
	printf("retval=%d\n",retval);
#endif
  }
  snprintf(buf, BUFSIZ, "%d C",(retval-2735)/10);
  return (const char *)buf;
#else
  return "";
#endif
#endif
}

