/*============================================================================
	E D I S E R V E R

	File:           runtime/tr_parsetime.c
	Programmer:	Mikko Valimaki/Juha Nurmela
	Date:           Wed Aug 12 09:29:29 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations

	Given a string, construct two times, one primary,
	and another giving a range into future.
	(today is 00:00 <-> 23.59).

	Formats:
		d.m.y
		m/d/y
		H:M
		yymmdd
		HHMM
		am/pm
		{now,today,tomorrow,yesterday} + N [dmyHMSw]

============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_parsetime.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN      Created (from ls_timelib).
  3.01 16.10.96/JN	"now - 1M" did not work (M_inutes was taken as m_onths)
  3.02 27.11.96/JN	HH:MM -> HH:MM:SS
  3.03 30.01.98/HT	Plain digits. If the lenght does not fix in
			predefined cases, number is treated as a seconds
			since epoch.
  4.00 06.04.98/HT	Daylight saving time checking forced on.
  4.01 13.01.99/JR	Added yyyymmdd-type
  4.02 15.06.2000/JR	Too big values for localtime should not make seg fault.
							Added yyyymmddHHMM-type
  4.03 15.05.2012/PNT(CG)	WBA-286: Complement of the parsed time are reduced to 1.
							For instance, when a complement is a DAY, the complement shall be DAY_SEC-1
							in order have a gap from ( 00h00m00s to 23h59m59 )
  4.04 16.04.2013/CGL(CG)   TX-2344: Calculate the second values in each month which we have add then
                             and add the option where there is only one number. 
  4.04 25.09.2013/CGL(CG)   TX-2416: Calculate the second values of all months that we have to add or
							subtract then. Calculate the second values of all year that we have add or
							subtract then. Add structures for "zero", "today", "yesterday", "tomorrow"
  4.04 23.10.2013/CGL(CG)   TX-2416: Add the condition for my_month=0 et add equal to assert(my_month>0)
  4.04 28.10.2013/CGL(CG)   TX-2416: Initialization of day, month and year. Remove the previous correction.
  4.05 25.02.2016/CGL(CG)   TX-2816: Add the option yyyymmddHHMMSS
  4.06 20.12.2016/SCH(CG)   TX-2929: Default unit is switched form day to seconds
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include "tr_bottom.h"

static void compile( );

#define MIN_SEC  ( 60)
#define HOUR_SEC ( 60 * MIN_SEC)
#define DAY_SEC  ( 24 * HOUR_SEC)
#define WEEK_SEC (  7 * DAY_SEC)


static time_t NOW;
static long secs;
static struct tm TM;

/* CGL (CG)  TX-2344 16/04/2013  */
int daysInMonths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
int MON_SEC=0;
/* Fin CGL (CG) TX-2344 16/04/2013 */

/* CGL (CG)  TX-2416 25/09/2013 */
int YEAR_SEC=0;
/* CGL (CG)  TX-2416 28/10/2013  Initialization of day, month and year*/
int day = 1;
int month = 1; 
int year = 1970;
/* Fin CGL (CG) TX-2416 28/10/2013 */
/* Fin CGL (CG) TX-2416 25/09/2013 */

/* prototypes
 */

bool IsLeapYear(int my_year); 
int GetDaysInMonth(int my_year, int my_month);
int GetDaysInYear(int my_year);


void tr_parsetime(char *text, time_t *times)
{
	char *cp;
	time_t other[2];
	/* CGL (CG)  TX-2344 16/04/2013 */
	/* 	CGL (CG) TX-2416 25/09/2013 */
	int month_more=0;
	int month_less=0;
	/* Fin CGL (CG) TX-2416 25/09/2013 */

	int i = 0;
	/* Fin CGL (CG) TX-2344 16/04/2013 */

	/* CGL (CG) TX-2416 25/09/2013 */
	int add_number=0;
	int days_add=0;
	int days_current_month=0;
	int days_next_month=0;
	int days_previous_month=0;
	int days_month=0;
	int days_end_month = 0;
	
	int month_m=0;
	int	month_y =0;
	
	int month_next=0;
	int	month_previous=0;
	int month_end=0;

	
	int year_m=0;
	int year_y=0;

	int days_year = 0;
	int year_end=0;
	int year_next=0;
	int year_previous=0;
	int j=0;
	
	int year_sec_more=0; 
	int year_sec_less=0;
		
	/* Fin CGL (CG) tx-2416 25/09/2013 */

	time(&NOW);
	TM = *localtime(&NOW);
	/* CGL (CG)  TX-2344 16/04/2013 : Calculate the second values in each month which we have add then. */
	cp = text;

		while (*cp >= 'a' && *cp <= 'z') {
		++cp;
		}
		
		while (*cp && isspace(*cp)) {
		++cp;
		}
				
 switch (*cp) {
			
		case '+':	
			/* CGL (CG)  TX-2416 25/09/2013 : Calculate the second values in each month which we have add then. */
			month_m=month;
			year_m=year;
			add_number = atoi(++cp);
			month_end =(month_m+add_number);	
		
			for (i=1; i<=add_number-1; i++){
			
				month_next=(month_m+i);
			
				if (month_next >=12){
					month_next = month_next%12;
					if (month_next==0){
						month_next=12;	
					}
					if (month_next==1){
						year_m=year_m+1;
					}
				}
	
				days_next_month = GetDaysInMonth(year_m, month_next);
				month_more = month_more + days_next_month;
			}

			if (month_end >=12){
				month_end = month_end%12;
				if (month_end==0){
					month_end=12;	
				}
				if (month_end==1){
					year_m=year_m+1;
				}
			}
	
			days_current_month = GetDaysInMonth(year_m, month_m);
			days_end_month = GetDaysInMonth(year_m, month_end);
			
			if (day>days_end_month) {
				days_add=days_end_month;
			} else {
				days_add = day;
			}
			
			/* Calculate the second values in each year which we have add then.*/
			month_y=month;
			year_y=year;
			year_end=year_y+add_number;
			for(j=0; j<=add_number-1; j++) {
				year_next=(year_y+j)+1;
				days_year = GetDaysInYear(year_next);
				year_sec_more = year_sec_more + (days_year * DAY_SEC);
			}
			
			if(GetDaysInYear(year_end)>GetDaysInYear(year_y)&&(month_y<2)) {
				year_sec_more = year_sec_more - DAY_SEC;
			}

			MON_SEC=((days_current_month-day)+month_more+days_add)*DAY_SEC;
			YEAR_SEC=year_sec_more;
			/* Fin CGL (CG) TX-2416 25/09/2013 */
			break;
			
		case '-':	
			/* CGL (CG)  TX-2416 25/09/2013 : Calculate the second values in each month which we have subtract then. */
			add_number = atoi(++cp);
			month_m=month;
			year_m=year;
			month_end =(month_m-add_number);	
			for (i=1; i<=add_number-1; i++) {
				month_previous = (month_m-i);
					
				if (month_previous <= 0){
					month_previous = ((month_previous)%12)+12;
					if (month_previous==0){
						month_previous=12;	
					}
					if (month_previous==12){
						year_m=year_m-1;
					}
				}
	
				days_previous_month = GetDaysInMonth(year_m, month_previous);
				month_less = (month_less + days_previous_month) ;
			}
				
			if (month_end <= 0){
				month_end = ((month_end)%12)+12;
				if (month_end==0){
					month_end=12;	
				}
				if (month_end==12){
					year_m=year_m-1;
				}
			}
	
			days_current_month = GetDaysInMonth(year_m, month_m);
			days_end_month = GetDaysInMonth(year_m, month_end);
			
			if (day>days_end_month){
				days_add=1;
			} else {
				days_add = (days_end_month-(day-1));
			}
			days_month=day-1;
		
			/* Calculate the second values in each year which we have add then.*/
			month_y=month;
			year_y=year;
			year_end=(year_y-add_number);
	
			for(j=0; j<=add_number-1; j++){
				year_previous=(year_y-j)-1;
				days_year = GetDaysInYear(year_previous);
				year_sec_less = year_sec_less + (days_year * DAY_SEC);
			}
			
			if(GetDaysInYear(year_end)>GetDaysInYear(year_y)&&(month_y>2)){
				year_sec_less = year_sec_less - DAY_SEC;
			}
			
			MON_SEC=(days_month+month_less+days_add)*DAY_SEC;
			YEAR_SEC=year_sec_less;
			/* Fin CGL (CG) TX-2416 25/09/2013 */
		
			break;
		default:
			break;
	}
	/*	Fin CGL (CG) TX-2344 30/04/2013 */
	
	TM.tm_isdst = -1; /*  4.00/HT  */

	if ((cp = strstr(text, "<->")) != NULL) {
		/* An explicit range is given. */
		*cp = 0;
		cp += 3;
		compile(text, times);
		compile(cp, other);
		times[1] = other[0];
	} else {
		compile(text, times);
	}
}


/* CGL (CG)  tx-2344 16/04/2013. */
/*Function to know if a year is a leap year */
bool IsLeapYear(int my_year) 
{
bool leap_year=false;

	if ((my_year % 4 == 0)&&(my_year % 400 == 0)){
		leap_year=true;
	} else if ((my_year % 4 == 0)&&(my_year % 100 != 0)){
		leap_year=true;
	}else{
	leap_year=false;
	}
	
	return leap_year;
}

/*Function to look for the number of days in the months*/
int GetDaysInMonth(int my_year, int my_month)
{
int days=0;
/* CGL (CG)  TX-2416 23/10/2013. Add the condition for my_month=0 et add equal to assert(my_month>0) */
	
	assert(my_month >0);
	assert(my_month <= 12);
	
	days = daysInMonths[my_month-1];

	if (my_month == 2 && IsLeapYear(my_year)){ /* February of a leap year*/
		days += 1;
}
	
	return days;
}
/* Fin CGL (CG) TX-2344 16/04/2013*/

/* CGL (CG) TX-2416 25/09/2013:  Function to look for the number of days in the years */
int GetDaysInYear(int my_year)
{
	int days=0;
	
	if(IsLeapYear(my_year)){
		days=366;
	} else {
		days=365;
	}
	
	return days;
}
/* Fin CGL (CG) TX-2416 25/09/2013 */

static void compile(char *text, time_t *times)
{
	char *end_text;
	char *cp;
	int i, complement = 0;
	struct tm t, *t2;
	int zero = 0;

	/* CGL (CG) TX-2416 25/09/2013: Variables to zero, yesterday, today, tomorrow et now */
	struct tm begin,yesterday,today,tomorrow,now;
	bool add=false;
	/* Fin CGL (CG) TX-2416 25/09/2013 */
	
	times[0] = times[1] = 0;

	/* Clear ws from start and end. */
	while (*text && isspace(*text)){
		++text;
		}
	cp = text + strlen(text);
	while (--cp >= text && isspace(*cp)){
		;
		}
	end_text = cp + 1;

	if (text >= end_text) {
		/* Empty expression. */
		 ;
	} else if (isdigit(*text)) {
		/*
		 * Expression starts with a number.
		 * Find the first non-digit character
		 * and act accordingly.
		 */
		t = TM;
		while (text < end_text) {
			cp = text;
			while (*cp && isdigit(*cp)) {
			++cp;
			}

			switch (*cp) {
			case '.':	/* 31.1.95 */
					/* PNT le 15/05/2012 : Decrease the complement to stay at the current day (00:00:00 to 23:59:59) */
				complement = DAY_SEC -1;
					/* fin PNT */
				sscanf(text, "%d.%d.%d", &t.tm_mday, &t.tm_mon, &t.tm_year);
				--t.tm_mon;
				t.tm_hour = t.tm_min = t.tm_sec = 0;
				while (*text && (*text == '.' || isdigit(*text))){
				++text;
				}
				break;

			case '/':	/* 1/31/95 */
					/*	PNT le 15/05/2012 : Decrease the complement to stay at the current day (00:00:00 to 23:59:59) */
				complement = DAY_SEC -1;
					/* fin PNT */
				sscanf(text, "%d/%d/%d", &t.tm_mon, &t.tm_mday, &t.tm_year);
				--t.tm_mon;
				t.tm_hour = t.tm_min = t.tm_sec = 0;
				while (*text && (*text == '/' || isdigit(*text))) {
				++text;
				}
				break;

				/*
				 *  HH:MM or [3.02] HH:MM:SS
				 *  If seconds were specified,
				 *  the time-range is just one second.
				 */
			case ':':	/* 23:45:01 */
				i = sscanf(text, "%d:%d:%d", &t.tm_hour, &t.tm_min, &t.tm_sec);
				if (i == 3){
					complement = 0;
					} else {
						/*	PNT le 15/05/2012 : Decrease the complement to stay at the current minute ( [min]:00 to [min]:59) */
					complement = MIN_SEC-1;
						/* fin PNT */
					}
				while (*text && (*text == ':' || isdigit(*text))) {
				++text;
				}
				break;

			case 'p':	/* pm */
				t.tm_hour += 12;
			case 'a':	/* am */
				text += 2;
				break;

			case 'G':	/* GMT */
			case 'U':	/* UTC */
				text += 2;
			case 'Z':	/* Z */
				text += 1;

#ifdef __LTZNMAX
				t.__tm_tzadj = 0;
				strcpy(t.__tm_name, "GMT");
#endif
#ifdef LTZNMAX
				t.tm_tzadj = 0;
				strcpy(t.tm_name, "GMT");
#endif
#ifdef MACHINE_OSF1
				t.__tm_gmtoff = 0;
				t.__tm_zone = "GMT";
#endif
				t.tm_isdst = 0;
				break;

			default:	/* just numbers */
		
				switch (cp - text) {
				
				default: /* seconds */
					sscanf( text, "%ld", &secs );
					/* 4.02/JR Added return value check
					 * Some implementations of localtime() (NT) returns NULL pointer
					 * if secs is too big.
					 */
					t2 = localtime( (time_t *)&secs );
					if(t2 != NULL) {
						memcpy( &t, t2, sizeof(t) );
					} else {
						/* Epoch */
						t2 = localtime( (time_t *)&zero );
						memcpy( &t, t2, sizeof(t) );
					}
					break;
				/*CGL(CG) TX-2816: Add the option yyyymmddHHMMSS */
				case 14:	/* yyyymmddHHMMSS  CGL(CG)/4.05 */
					complement = DAY_SEC-1;
					sscanf(text, "%04d%02d%02d%02d%02d%02d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min, &t.tm_sec);
					--t.tm_mon;
					break;
				case 12:	/* yyyymmddHHMM  4.02/JR */
							/*	PNT le 15/05/2012 : Decrease the complement to stay at the current day (00:00:00 to 23:59:59) */
					complement = DAY_SEC-1;
							/*	fin PNT	*/
					sscanf(text, "%04d%02d%02d%02d%02d", &t.tm_year, &t.tm_mon, &t.tm_mday, &t.tm_hour, &t.tm_min);
					--t.tm_mon;
					t.tm_sec = 0;
					break;
				case 8:	/* yyyymmdd  4.01/JR */
							/*	PNT le 15/05/2012 : Decrease the complement to stay at the current day (00:00:00 to 23:59:59) */
					complement = DAY_SEC-1;
							/* fin PNT	*/
					sscanf(text, "%04d%02d%02d", &t.tm_year, &t.tm_mon, &t.tm_mday);
					--t.tm_mon;
					t.tm_hour = t.tm_min = t.tm_sec = 0;
					break;
				case 6:	/* yymmdd */
							/* PNT le 15/05/2012 : Decrease the complement to stay at the current day (00:00:00 to 23:59:59) */
					complement = DAY_SEC-1;
							/* fin PNT */
					sscanf(text, "%02d%02d%02d", &t.tm_year, &t.tm_mon, &t.tm_mday);
					--t.tm_mon;
					t.tm_hour = t.tm_min = t.tm_sec = 0;
					break;
				case 4:	/* HHMM */
							/* PNT le 15/05/2012 : Decrease the complement to stay at the current minute ( [min]:00 to [min]:59) */
					complement = MIN_SEC-1;
							/*	fin PNT	*/
					sscanf(text, "%02d%02d", &t.tm_hour, &t.tm_min);
					break;

				case 2:
							/*	PNT le 15/05/2012 : Decrease the complement to stay at the current minute ( [hour]:00:00 to [hour]:59:59) */
					complement = HOUR_SEC-1;
							/*	fin PNT	*/
					sscanf(text, "%02d", &t.tm_hour);
					break;
				case 0:
					cp++; /* prevent loop... */
					break;
				}
				text = cp; /* over the sscanfed stuff. */
				break;
			}
			while (text < end_text && isspace(*text)) {
				++text;
			}
		} /* end while (text less than end_text) */
		
		if (t.tm_year >= 1900){
			t.tm_year -= 1900;
			}
		
		if (t.tm_year < 38) {
			t.tm_year += 100;
			}

		times[0] = mktime(&t);
		times[1] = times[0] + complement;
	} else {
		/*
		 * Expression starts with a keyword.
		 * {zero,now,today,tomorrow,yesterday} {+,-} stuff
		 * seek over first word and set defaults.
		 */
		cp = text;
		while (*cp >= 'a' && *cp <= 'z') {
		++cp;
		}

		times[0] = NOW;
		/* CGL (CG) TX-2416 25/09/2013: Day, month and year of now */
		now=*localtime(&times[0]);
		day=now.tm_mday;
		month=((now.tm_mon)+1);
		year = now.tm_year;
		/* Fin CGL (CG) TX-2416 25/09/2013 */

		/* PNT le 15/05/2012 : Decrease the complement to stay at the current day (00:00:00 to 23:59:59) */
		complement = DAY_SEC-1;
		/* fin PNT	*/
		
		if (!strncmp(text, "now", cp - text)) {
			complement = 1;

		} else if (!strncmp(text, "zero", cp - text)) {
			times[0] = 0;
			complement = 1;
			/* CGL (CG) TX-2416 25/09/2013: Day, month and year of zero	*/
			begin=*localtime(&times[0]);
			day=begin.tm_mday;
			month=((begin.tm_mon)+1);
			year = begin.tm_year;
			/* Fin CGL (CG) TX-2416 25/09/2013 */
		} else if (!strncmp(text, "today", cp - text)) {
			times[0] -= TM.tm_hour * HOUR_SEC + TM.tm_min * MIN_SEC + TM.tm_sec;
			/* CGL (CG) TX-2416 25/09/2013: Day, month and year of today */
			today=*localtime(&times[0]);
			day=today.tm_mday;
			month=((today.tm_mon)+1);
			year = today.tm_year;
			/*	Fin CGL (CG) TX-2416 25/09/2013 */			
		} else if (!strncmp(text, "tomorrow", cp - text)) {
			times[0] -= TM.tm_hour * HOUR_SEC + TM.tm_min * MIN_SEC + TM.tm_sec;
			times[0] += DAY_SEC;
			/* CGL (CG) TX-2416 25/09/2013: Day, month and year of tomorrow	*/
			tomorrow=*localtime(&times[0]);
			day=tomorrow.tm_mday;
			month=((tomorrow.tm_mon)+1);
			year = tomorrow.tm_year;
			/* Fin CGL (CG) TX-2416 25/09/2013 */	
		} else if (!strncmp(text, "yesterday", cp - text)) {
			times[0] -= TM.tm_hour * HOUR_SEC + TM.tm_min * MIN_SEC + TM.tm_sec;
			times[0] -= DAY_SEC;
			/* CGL (CG) TX-2416 25/09/2013: Day, month and year of yesterday */
			yesterday=*localtime(&times[0]);
			day=yesterday.tm_mday;
			month=((yesterday.tm_mon)+1);
			year = yesterday.tm_year;
			/*	Fin CGL (CG) TX-2416 25/09/2013 */	
		} else {
			/* unknown */
			;
		}
		/* There can be a + or - range after any keyword. */

		while (*cp && isspace(*cp)) {
		++cp;
		}
		if (*cp) {
			i = 0;
			switch (*cp) {
			case '+':
				/* CGL (CG) TX-2416 25/09/2013 */
				add=true;
				/*	Fin CGL (CG) TX-2416 25/09/2013 */
				i =  atoi(++cp);
				break;
			case '-':
			
			
				i = -atoi(++cp);
				break;
			}
			while (*cp && (isspace(*cp) || isdigit(*cp))){
				++cp;
			}				
			
			switch (*cp) {
			/* PNT le 15/05/2012 : Decrease the complement to stay at the current year/month/day/week/hour/min	*/
			/* CGL (CG)  TX-2416 25/09/2013: Year's complement and month's complement is the same as seconde's complement.
											i is the number of secondes corresponding to the number of months and the number of year respectively
											what we had add or subtract. */
			case 'y': complement = 1; 
			
				if(!add){
					i=-YEAR_SEC;
				} else {
					i=YEAR_SEC;
				}
			break;
			case 'm': complement = 1;
			
				if(!add){
					i=-MON_SEC;
				} else {
					i=MON_SEC;
				}
			break;
			/* Fin CGL (CG) TX-2416 25/09/2013	*/
			case 'd': complement = DAY_SEC;  break;
			case 'w': complement = WEEK_SEC; break;
			case 'H': complement = HOUR_SEC; break;
			case 'M': complement = MIN_SEC;  break;
			case 'S': 
			complement = 1;  break;
			
			/* CGL (CG) TX-2433 30/04/2013 : Add the option where there is only one number 
			   SCH (CG) TX-2929 20/12/2016 : Default unit is switched from day to second  */
			default:    
				complement = 1;
				break;
			/* Fin SCH (CG) TX-2929 20/12/2016
			   Fin CGL (CG) TX-2433 30/04/2013 */
			
			/*fin PNT*/
			}
			i *= complement;
			
			times[0] += i;
		}
		times[1] = times[0] + complement - 1;
	}
}

#if 0
main(ac, av)
	int ac;
	char **av;
{
	time_t times[2];

	time(&times[0]);
	printf("\t%s", ctime(&times[0]));

	while (ac > 1) {
		tr_parsetime(av[1], times);
		printf("%s\n", av[1]);
		printf("\t%s", ctime(&times[0]));
		printf("\t%s", ctime(&times[1]));
		ac--;
		av++;
	}
}

#endif
