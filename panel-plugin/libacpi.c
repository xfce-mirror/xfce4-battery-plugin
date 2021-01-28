/* Copyright (c) 2002 Costantino Pistagna <valvoline@vrlteam.org>
 * Copyright (c) 2003 Noberasco Michele <2001s098@educ.disi.unige.it>
 * Copyright (c) 2003 Edscott Wilson Garcia <edscott@imp.mx>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

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
#include <glob.h>
#include <unistd.h>

#include <libxfce4util/libxfce4util.h>

#ifdef __FreeBSD__
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dev/acpica/acpiio.h>
#define ACPIDEV         "/dev/acpi"
static int      acpifd;
#define UNKNOWN_CAP 0xffffffff
#define UNKNOWN_VOLTAGE 0xffffffff
#endif

#ifdef HAVE_SYSCTL

#if defined(__NetBSD__) || defined (__OpenBSD__)
#include <sys/param.h>
/* CTLTYPE does not exist in NetBSD headers.
 * Defining it to 0x0f here won't do any harm though. */
#define CTLTYPE 0x0f
#endif

#ifndef __linux__
#include <sys/sysctl.h>
#include <err.h>
#include <errno.h>
#endif

#endif

#include "libacpi.h"

static char batteries[MAXBATT][128];
/* path to AC adapter because not all AC adapter are listed
in /sys/class/power_supply/AC/ this obviously only supports one AC adapter. */
static char sysfsacdir[280];

#ifndef __linux__
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
        err(1, "sysctl fmt %d %zu %d", i, j, errno);

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

    qoid[0] = 0;
    memcpy(qoid + 2, oid, nlen * sizeof(int));

    qoid[1] = 1;
    j = sizeof(name);
    i = sysctl(qoid, nlen + 2, name, &j, 0, 0);
    if (i || !j)
        err(1, "sysctl name %d %zu %d", i, j, errno);

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
    fmt = (char *)buf;
    oidfmt(oid, nlen, fmt, &kind);
    p = val;
    switch (*fmt) {
    case 'I':
        DBG("I:%s%s", name, sep);
        fmt++;
        val = "";
        while (len >= sizeof(int)) {
            if (*fmt == 'U') {
                retval = *((unsigned int *)p);
                DBG("x%s%u", val, *(unsigned int *)p);
            }
            else {
                retval = *((int *)p);
                DBG("x%s%d", val, *(int *)p);
            }
            val = " ";
            len -= sizeof(int);
            p += sizeof(int);
        }

        return (retval);
    default:
        printf("%s%s", name, sep);
        printf("Format:%s Length:%zu Dump:0x", fmt, len);
        while (len-- && (p < val + 16))
            printf("%02x", *p++);
        if (len > 16)
            printf("...");

        return (0);
    }

    return (0);
}

#endif
#endif

static int
check_acpi_sysfs(void)
{
    DIR *sysfs;
    struct dirent *batt;
    char *name;
    FILE *typefile;
    char typepath[300];
    char tmptype[8];

    acpi_sysfs = 0;
    batt_count = 0;

    sysfs = opendir("/sys/class/power_supply");
    if (sysfs == 0)
    {
        DBG("No acpi support for sysfs.");
        return 2;
    }

    while ((batt = readdir(sysfs)))
    {
        name = batt->d_name;
        if (!strncmp(".", name, 1))
            continue;

        /* check whether /sys/class/power_supply/$name/type exists and
        contains "Battery" or "Mains" */
        sprintf(typepath, "/sys/class/power_supply/%s/type",name);
        if (!(typefile = fopen(typepath, "r")))
            continue;

        fgets(tmptype, 8, typefile);
        fclose(typefile);

        if (strncmp("Battery", tmptype, 7)==0)
        {
            acpi_sysfs = 1;
            sprintf(batteries[batt_count], "/sys/class/power_supply/%s", name);
            DBG("battery number %d at:", batt_count);
            DBG("sysfs dir->%s", batteries[batt_count]);
            DBG("------------------------");
            batt_count++;
        }
        /* I guess that the type of the AC adapter is always "Mains" (?) */
        else if (strncmp("Mains", tmptype, 5)==0) {
            acpi_sysfs = 1;
            sprintf(sysfsacdir, "/sys/class/power_supply/%s", name);
            DBG("sysfs AC dir->%s", sysfsacdir);
            DBG(":------------------------");
        }
    }

    closedir(sysfs);

    if (acpi_sysfs == 0)
    {
        DBG("No acpi support for sysfs.");
        return 2;
    }

    return 0;
}

/* expand file name and fopen first match */
static FILE *
fopen_glob(const char *name, const char *mode)
{
    glob_t globbuf;
    FILE *fd;

    if (glob(name, 0, NULL, &globbuf) != 0)
        return NULL;

    fd = fopen(globbuf.gl_pathv[0], mode);
    globfree(&globbuf);

    return fd;
}

/* see if we have ACPI support */
int
check_acpi(void)
{
#ifdef __linux__
    return check_acpi_sysfs();
#else
#ifdef HAVE_SYSCTL
    static char buf[BUFSIZ];
    char *bufp=buf;
    char fmt[BUFSIZ];
    void *oldp=(void *)buf;
    size_t oldlenp=BUFSIZ;
    int len,mib[CTL_MAXNAME];
    u_int kind;
    snprintf(buf, BUFSIZ, "%s", "hw.acpi.battery.units");
    len = name2oid(bufp, mib);
    if (len <=0) return 1;
    if (oidfmt(mib, len, fmt, &kind)) return 1;
    if ((kind & CTLTYPE) == CTLTYPE_NODE) return 1;
    batt_count=get_var(mib, len);

    return 0;
#else
  return 1;
#endif
#endif
}

static int
read_sysfs_int(char* filename)
{
    FILE* f;
    int out;

    f = fopen(filename,"r");
    if (!f)
    {
        DBG("Could not open %s", filename);
        return 0;
    }

    fscanf(f,"%d",&out);
    fclose(f);
    return out;
}

static char*
read_sysfs_string(char* filename)
{
    FILE* f;
    f = fopen(filename,"r");
    if (!f)
    {
        DBG("Could not open %s", filename);
        return NULL;
    }
    fscanf(f,"%s",buf2);
    fclose(f);
    return buf2;
}

static int
read_acad_state_sysfs(void)
{
    DIR *sysfs;
    char onlinefilepath[300];

    sysfs = opendir(sysfsacdir);
    if (sysfs == 0)
    {
        DBG("Can't open %s", sysfsacdir);
        return 0;
    }
    closedir(sysfs);

    if (!acadstate)
        acadstate=(ACADstate *)malloc(sizeof(ACADstate));

    sprintf(onlinefilepath, "%s/online", sysfsacdir);
    /* if onlinefilepath doesn't exist read_sysfs_int() will return 0
    so acadstate->state will be 0, that should be ok */
    acadstate->state = (read_sysfs_int(onlinefilepath) == 1);

    return acadstate->state;
}

int
read_acad_state(void)
{
#ifdef __linux__
    return acpi_sysfs ? read_acad_state_sysfs() : 0;
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
    if (len <= 0) return(-1);
    if (oidfmt(mib, len, fmt, &kind))
        err(1, "couldn't find format of oid '%s'", bufp);
    if (len < 0) errx(1, "unknown oid '%s'", bufp);
    if ((kind & CTLTYPE) == CTLTYPE_NODE) {
        DBG("oh-oh...");
    } else {
        retval=get_var(mib, len);
        DBG("retval=%d",retval);
  }
  return retval;
#else
  return 0;
#endif
#endif
}

static int
read_acpi_info_sysfs(int battery)
{
    DIR *sysfs;
    struct dirent *propety;
    char *name;

    sysfs = opendir(batteries[battery]);
    if (sysfs == 0)
    {
        DBG("Can't open %s!", batteries[battery]);
        return 0;
    }
    /* malloc.. might explain the random battery level values on 2.6.24
    systems (energe_full is called charge_full so the value isn't initialised
    and some random data from the heap is displayed..)
    if (!acpiinfo) acpiinfo=(ACPIinfo *)malloc(sizeof(ACPIinfo));
    */
    if (!acpiinfo) acpiinfo=(ACPIinfo *)calloc(1, sizeof(ACPIinfo));

    while ((propety = readdir(sysfs)))
    {
        name = propety->d_name;
        if (!strncmp(".", name, 1) || !strncmp("..", name, 2)) continue;
        /* at least on my system this is called charge_full */
        if ((strcmp(name,"energy_full") == 0) || (strcmp(name,"charge_full") == 0))
        {
            sprintf(buf,"%s/%s",batteries[battery], name);
            acpiinfo->last_full_capacity = read_sysfs_int(buf);
        }
        if ((strcmp(name,"energy_full_design") == 0) || (strcmp(name,"charge_full_design") == 0))
        {
            sprintf(buf,"%s/%s",batteries[battery], name);
            acpiinfo->design_capacity = read_sysfs_int(buf);
        }
        if (strcmp(name,"technology") == 0)
        {
            char *tmp;
            sprintf(buf,"%s/%s",batteries[battery], name);
            tmp = read_sysfs_string(buf);
            if (tmp != NULL)
            {
                if (strcmp(tmp,"Li-ion") == 0)
                    acpiinfo->battery_technology = 1;
                else
                    acpiinfo->battery_technology = 0;
            }
        }
        if (strcmp(name,"present") == 0)
        {
            sprintf(buf,"%s/%s",batteries[battery], name);
            acpiinfo->present = read_sysfs_int(buf);
        }
    }
    closedir(sysfs);
    return acpiinfo->present;
}

int
read_acpi_info(int battery)
{
#ifdef __linux__
    if (battery > MAXBATT) {
        DBG("error, battery > MAXBATT (%d)",MAXBATT);
        return 0;
  }

    return acpi_sysfs ? read_acpi_info_sysfs(battery) : 0;
#else
#ifdef HAVE_SYSCTL
    static char buf[BUFSIZ];
    char *bufp=buf;
    int len,mib[CTL_MAXNAME];
    char fmt[BUFSIZ];
    u_int kind;
    int retval;
    if (!acpiinfo)
        acpiinfo=(ACPIinfo *)malloc(sizeof(ACPIinfo));
    acpiinfo->present = 0;
    acpiinfo->design_capacity = 0;
    acpiinfo->last_full_capacity = 0;
    acpiinfo->battery_technology = 0;
    acpiinfo->design_voltage = 0;
    acpiinfo->design_capacity_warning = 0;
    acpiinfo->design_capacity_low = 0;
    snprintf(buf, BUFSIZ, "%s", "hw.acpi.battery.units");
    len = name2oid(bufp, mib);
    if (len <= 0) return(-1);
    if (oidfmt(mib, len, fmt, &kind))
        err(1, "couldn't find format of oid '%s'", bufp);
    if (len < 0) errx(1, "unknown oid '%s'", bufp);
    if ((kind & CTLTYPE) == CTLTYPE_NODE) {
        DBG("oh-oh...");
    } else {
        retval=get_var(mib, len);
        DBG("retval=%d",retval);
    }
    acpiinfo->present = retval;

#ifdef __FreeBSD__
    union acpi_battery_ioctl_arg battio;
    acpifd = open(ACPIDEV, O_RDONLY);

    battio.unit = battery;
    if (ioctl(acpifd, ACPIIO_BATT_GET_BIF, &battio) == -1) {
        return 0;
    }
    close(acpifd);

    acpiinfo->design_capacity = battio.bif.dcap;
    acpiinfo->last_full_capacity = battio.bif.lfcap;
    acpiinfo->battery_technology = battio.bif.btech;
    acpiinfo->design_voltage = battio.bif.dvol;
    acpiinfo->design_capacity_warning = battio.bif.wcap;
    acpiinfo->design_capacity_low = battio.bif.lcap;
#endif
    return 1;
#else
    return 0;
#endif
#endif

}

static int
read_acpi_state_sysfs(int battery)
{
    DIR *sysfs;
    struct dirent *propety;
    char *name;

    sysfs = opendir(batteries[battery]);
    if (sysfs == 0)
    {
        DBG("Can't open %s!", batteries[battery]);
        return 0;
    }

    /* again it might be better to use calloc */
    if (!acpistate)
        acpistate=(ACPIstate *)calloc(1, sizeof(ACPIstate));

    while ((propety = readdir(sysfs)))
    {
        name = propety->d_name;
        if (!strncmp(".", name, 1) || !strncmp("..", name, 2))
            continue;

        if (strcmp(name,"status") == 0)
        {
            char *tmp;
            sprintf(buf,"%s/%s",batteries[battery], name);
            tmp = read_sysfs_string(buf);
            if (tmp != NULL)
            {
                if (strcmp(tmp,"Charging") == 0)
                    acpistate->state = CHARGING;
                else if (strcmp(tmp,"Discharging") == 0)
                    acpistate->state = DISCHARGING;
                else if (strcmp(tmp,"Full") == 0)
                    acpistate->state = POWER;
                else
                    acpistate->state = UNKNOW;
            }
        }

        /* on my system this is called charge_now */
        if ((strcmp(name,"energy_now") == 0) || (strcmp(name,"charge_now") == 0))
        {
            sprintf(buf,"%s/%s",batteries[battery], name);
            acpistate->rcapacity = read_sysfs_int(buf);
            acpistate->percentage = (((float) acpistate->rcapacity)/acpiinfo->last_full_capacity) * 100;
        }

        if ((strcmp(name,"current_now") == 0) || (strcmp(name,"power_now") == 0))
        {
            sprintf(buf,"%s/%s",batteries[battery], name);
            acpistate->prate = read_sysfs_int(buf);
            if (acpistate->prate < 0)
                acpistate->prate = 0;
            if (acpistate->prate > 0)
                acpistate->rtime = (((float) acpistate->rcapacity) / acpistate->prate) * 60;
        }

        if (strcmp(name,"voltage_now") == 0)
        {
            sprintf(buf,"%s/%s",batteries[battery], name);
            acpistate->pvoltage = read_sysfs_int(buf);
        }
    }

    closedir(sysfs);

    return acpiinfo->present;
}

int
read_acpi_state(int battery)
{
#ifdef __linux__
    return acpi_sysfs ? read_acpi_state_sysfs(battery) : 0;
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
    if (!acpistate)
        acpistate=(ACPIstate *)malloc(sizeof(ACPIstate));
    acpistate->present = 0;
    acpistate->state = UNKNOW;
    acpistate->prate = 0;
    acpistate->rcapacity = 0;
    acpistate->pvoltage = 0;
    acpistate->rtime = 0;
    acpistate->percentage = 0;

    snprintf(buf, BUFSIZ, "%s", "hw.acpi.battery.time");
    len = name2oid(bufp, mib);
    if (len <= 0) return(-1);
    if (oidfmt(mib, len, fmt, &kind))
        err(1, "couldn't find format of oid '%s'", bufp);
    if (len < 0) errx(1, "unknown oid '%s'", bufp);
    if ((kind & CTLTYPE) == CTLTYPE_NODE) {
        DBG("oh-oh...");
    } else {
        retval=get_var(mib, len);
        DBG("retval=%d",retval);
    }
    acpistate->rtime =(retval<0)?0:retval;

    snprintf(buf, BUFSIZ, "%s", "hw.acpi.battery.life");
    len = name2oid(bufp, mib);
    if (len <= 0) return(-1);
    if (oidfmt(mib, len, fmt, &kind))
        err(1, "couldn't find format of oid '%s'", bufp);
    if (len < 0) errx(1, "unknown oid '%s'", bufp);
    if ((kind & CTLTYPE) == CTLTYPE_NODE) {
        DBG("oh-oh...");
    } else {
        retval=get_var(mib, len);
        DBG("retval=%d",retval);
    }
    acpistate->percentage =retval;

#ifdef __FreeBSD__
    union acpi_battery_ioctl_arg battio;
    acpifd = open(ACPIDEV, O_RDONLY);

    battio.unit = battery;
    if (ioctl(acpifd, ACPIIO_BATT_GET_BATTINFO, &battio) == -1) {
        return 0;
    }

    acpistate->state = battio.battinfo.state;
    acpistate->prate = battio.battinfo.rate;
    acpistate->rcapacity = acpiinfo->last_full_capacity * battio.battinfo.cap / 100;
    acpistate->rtime = battio.battinfo.min;
    acpistate->percentage = battio.battinfo.cap;

    battio.unit = battery;
    if (ioctl(acpifd, ACPIIO_BATT_GET_BATTINFO, &battio) == -1) {
        return 0;
    }
    close(acpifd);
    acpistate->pvoltage = battio.bst.volt;
#endif
    return 1;
#else
    return 0;
#endif
#endif
}

int
get_fan_status(void)
{
    FILE *fp;
    char *proc_fan_status = "/proc/acpi/toshiba/fan";
    char line[256];

   /* Check for fan status in PROC filesystem */
    if ((fp=fopen(proc_fan_status, "r")) != NULL) {
       fgets(line,255,fp);
        fclose(fp);
        if (strlen(line) && strstr(line,"1"))
            return 1;

        return 0;
    }

    proc_fan_status="/proc/acpi/fan/*/state";
    if ((fp=fopen_glob(proc_fan_status, "r")) == NULL)
        return 0;

    fgets(line,255,fp);
    fclose(fp);

    if (strlen(line) && strstr(line,"off"))
        return 0;
    
    return 1;
}

const char*
get_temperature(void)
{
#ifdef __linux__
    FILE *fp;
    char *sys_temperature="/sys/class/thermal/thermal_zone*/temp";
    static char *p,*p2,line[256];

    if ((fp=fopen_glob(sys_temperature, "r")) != NULL)
    {
        fgets(line,255,fp);
        fclose(fp);
        p = line;
        if (strchr(p,'\n')) *strchr(p,'\n') = 0;
        if (strlen(p) <= 3) return NULL;
        p2 = p + strlen(p) - 3;
        strcpy(p2, " C");
        return (const char *)p;
    }

    return NULL;
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
    if (len <= 0) return(NULL);
    if (oidfmt(mib, len, fmt, &kind))
        err(1, "couldn't find format of oid '%s'", bufp);
    if (len < 0) errx(1, "unknown oid '%s'", bufp);
    if ((kind & CTLTYPE) == CTLTYPE_NODE) {
        DBG("oh-oh...");
    } else {
        retval=get_var(mib, len);
        DBG("retval=%d",retval);
    }
    snprintf(buf, BUFSIZ, "%d C",(retval-2735)/10);
    return (const char *)buf;
#else
    return "";
#endif
#endif
}
