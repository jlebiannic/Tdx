/*============================================================================
	E D I S E R V E R

	File:		runtime/tr_timestring.c
	Programmer:	Mikko Valimaki
	Date:		Wed Aug 12 09:29:29 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_timestring.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  3.01 09.02.96/JN	Lifted the YearWeek from 3.1.1 logsys/timelib.
  4.00 01.03.06/LM	"Novomber" typo fixed BugZ 5364
  ============================================================================

	This routine prints the given time
	(elapsed seconds since 1.1.1970 GMT)
	in desired format.

	The format is like printf's: it supports special tokens
	that correspond to various elements of time.

	%T	Time in AM/PM notation (HH:MM AM|PM)
	%t	Time in AM/PM notation (HH:MM:SS AM|PM)

	%M	Minutes (00-59)
	%H	Hours (00-24)
	%S	Seconds (00-59)

	%d	Day of the month (01-31)
	%m	Month of the year (01-12)
	%y	Year since 1900
	%Y	Absolute year (e.g. 1993 or 2004)

	%W	Day of the week in long format
	%w	Day of the week in abbreviated format

	%k	Week of the year (1 - 53)
	%j	Day of the year (1 - 366)
	%a	Absolute value (in seconds since Jan 1, 1970 GMT)

	%eM	Elapsed minutes to current system time
	%eH	Elapsed hours
	%ed	Elapsed days
	%ew	Elapsed weeks

	A negative value in elapsed time indicates that the given time
	is in the future.
======================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

static char *weekDays[] = {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
};

static char *months[] = {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

static int tr_YearWeek(int yday, int wday, int relativeYear);

char *tr_timestring(char *fmt, time_t timer)
{
	static char result[512];
	char	   *resp = result;
	time_t      now = 0;
	struct tm  *tm = NULL;
	long	    elapsed;

	if (!fmt) {
		/*
		**  Print the default.
		*/
		fmt = "%d.%m.%y %H:%M:%S";
	}
	for ( ; *fmt && resp < &result[sizeof(result) - 2]; fmt++) {
		if (*fmt != '%') {
			*resp++ = *fmt;
			continue;
		}
		if (!tm && !(tm = localtime(&timer))) {
			/*
			**  Exceptionally rare error condition.
			*/
			return ("(no time)");
		}
		fmt++;
		switch (*fmt) {
		default:
			break;
		case '\0':
			fmt--;
			break;
		case '%':
			*resp++ = '%';
			break;
		case 'S':
			resp += sprintf (resp, "%02d", tm->tm_sec);
			break;
		case 'M':
			resp += sprintf (resp, "%02d", tm->tm_min);
			break;
		case 'H':
			resp += sprintf (resp, "%02d", tm->tm_hour);
			break;
		case 'd':
			resp += sprintf (resp, "%02d", tm->tm_mday);
			break;
		case 'm':
			resp += sprintf (resp, "%02d", tm->tm_mon + 1);
			break;
		case 'y':
			resp += sprintf (resp, "%02d", tm->tm_year % 100);
			break;
		case 'Y':
			resp += sprintf (resp, "%4d", tm->tm_year + 1900);
			break;
		case 'N':
			resp += sprintf (resp, "%s", months[tm->tm_mon]);
			break;
		case 'n':
			resp += sprintf (resp, "%.*s", 3, months[tm->tm_mon]);
			break;
		case 'W':
			resp += sprintf (resp, "%s", weekDays[tm->tm_wday]);
			break;
		case 'w':
			resp += sprintf (resp, "%.*s", 3, weekDays[tm->tm_wday]);
			break;
		case 'k':
			resp += sprintf (resp, "%d", tr_YearWeek(tm->tm_yday, tm->tm_wday, tm->tm_year));
			break;
		case 'j':
			resp += sprintf (resp, "%d", tm->tm_yday + 1);
			break;
		case 'T':
			if (tm->tm_hour < 12)
				resp += sprintf (resp, "%02d:%02d AM", tm->tm_hour, tm->tm_min);
			else if (tm->tm_hour == 12)
				resp += sprintf (resp, "12:%02d PM", tm->tm_min);
			else
				resp += sprintf (resp, "%02d:%02d PM", tm->tm_hour - 12, tm->tm_min);
			break;
		case 't':
			if (tm->tm_hour < 12)
				resp += sprintf (resp, "%02d:%02d:%02d AM", tm->tm_hour, tm->tm_min, tm->tm_sec);
			else if (tm->tm_hour == 12)
				resp += sprintf (resp, "12:%02d:%02d PM", tm->tm_min, tm->tm_sec);
			else
				resp += sprintf (resp, "%02d:%02d:%02d PM", tm->tm_hour - 12, tm->tm_min, tm->tm_sec);
			break;
		case 'a':
			resp += sprintf (resp, "%ld", timer);
			break;
		case 'e':
			if (!now) {
				time (&now);
				elapsed = now - timer;
			}
			fmt++;
			switch (*fmt) {
			default:
				break;
			case '\0':
				fmt--;
				break;
			case 'M':
				resp += sprintf (resp, "%ld", elapsed / 60L);
				break;
			case 'H':
				resp += sprintf (resp, "%ld", elapsed / (60L * 60L));
				break;
			case 'd':
				resp += sprintf (resp, "%ld", elapsed / (60L * 60L * 24L));
				break;
			case 'w':
				resp += sprintf (resp, "%ld", elapsed / (60L * 60L * 24L * 7L));
				break;
			}
			break;
		}
	}
	*resp = '\0';

	return (result);
}

static int tr_YearWeek(int yday, int wday, int relativeYear)
{
	int week;
   
    /* First Day Of ... */
	int fdoy;  /*      Year */
	int fdony; /* Next Year */
	int fdopy; /* Prev Year */
    
    int year;
    int isBissextil;     
    int isPrevBissextil;     
	/*
	**  Adjust the days to run from 1 to 366.
	*/
	yday++;
	/*
	**  Adjust the weekday to be 0 for manday.
	*/
	wday--;
	if (wday == -1)
		wday = 6;
	/*
     * year is store as year - 1900 
     * but we need real year to know 
     * whether years are bissextils
     */
    year = relativeYear + 1900;
    
    /* 
     * 
     * Year begin with a : ...
     * ... and fdoy equal : ...
     * 
     * Monday   : 7
     * Tuesday  : 6
     * ...
     * Saturday : 2
     * Sunday   : 1
     * 
     */ 
    
    fdoy = (yday - wday + 7) % 7 - 1;
    if (fdoy == 0 ) fdoy = 7;
    if (fdoy == -1) fdoy = 6; 
    
    week = (yday - fdoy + 6) / 7 + (fdoy > 3);
    
    /*
     *        every 4   years, year is bissextil
     * except every 100 years, year is not bissextil
     * except every 400 years, year is bissextil
     */
    
    /* if this year is bissextil, next year the first day of week will be two more instead of one */
    isBissextil     = ( ( ( year    % 4 == 0) && !( year    % 100 == 0) ) || ( year    % 400 == 0) );
    fdony = fdoy - 1 - isBissextil;
    if (fdony == 0 ) fdony = 7;
    if (fdony == -1) fdony = 6; 
    /* if this prev year was bissextil, ... */
    isPrevBissextil = ( ( ((year-1) % 4 == 0) && !((year-1) % 100 == 0) ) || ((year-1) % 400 == 0) );
    fdopy = (fdoy + 1 + isPrevBissextil) % 7;
    if (fdopy == 0) fdopy = 7;

    if (week == 0)
    {
        if ( ( (fdopy > 3) && (yday < 7  ) ) /* start of year */
          || ( (fdony < 5) && (yday > 359) ) /* end   of year */
           )
        {
            week = 53;
        }
        else
        {
            week = 52;
        }
    }
    else if (week == 53)
    {
        if ( ( (fdopy < 5) && (yday < 7  ) ) /* start of year */
          || ( (fdony > 3) && (yday > 359) ) /* end   of year */
           )
        {
            week = 1; 
        }
    }
    
	return week;
}

