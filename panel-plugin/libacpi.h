/***************************************************************************
                          libacpi.h  -  description
                             -------------------
    begin                : Feb 10 2003
    copyright            : (C) 2003 by Noberasco Michele
    			 : (C) 2003 edscott wilson garcia with xfce4 modifications
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

#define MAXBATT 8

typedef enum
{
  POWER,			/* on AC, Battery charged  */
  DISCHARGING,			/* on Battery, Discharging */
  CHARGING,			/* on AC, Charging         */
  UNKNOW			/* unknow                  */
}
Charging;

typedef struct
{
  int present;			/* 1 if present, 0 if no battery         */
  Charging state;		/* charging state enum                   */
  int prate;			/* present rate                          */
  int rcapacity;		/* rameining capacity                    */
  int pvoltage;			/* present voltage                       */

  /* not present in /proc */
  int rtime;			/* remaining time                        */
  int percentage;		/* battery percentage (-1 if no battery) */
}
ACPIstate;

typedef struct
{
  int present;			/* 1 if present, 0 if no battery        */
  int design_capacity;		/* design capacity                      */
  int last_full_capacity;	/* last_full_capacity                   */
  int battery_technology;	/* 1 non-rechargeable, 0 rechargeable   */
  int design_voltage;		/* design voltage                       */
  int design_capacity_warning;	/* design capacity warning (critical)   */
  int design_capacity_low;	/* design capacity low (low level)      */
}
ACPIinfo;


typedef struct
{
  int state;			/* 1 if online, 0 if offline            */
}
ACADstate;

int check_acpi (void);
int read_acad_state (void);
int read_acpi_info (int battery);
int read_acpi_state (int battery);
int get_fan_status(void);
const char *get_temperature(void);

#ifdef __libacpi_c__
/* global */
char battery_type;
ACPIstate *acpistate=NULL;
ACPIinfo *acpiinfo=NULL;
ACADstate *acadstate=NULL;
/* batteries detected */
int batt_count;
/* temp buffer */
char buf[512];
#else
extern ACPIstate *acpistate;
extern ACPIinfo *acpiinfo;
extern ACADstate *acadstate;
#endif



