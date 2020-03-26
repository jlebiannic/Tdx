/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_time.c
	Programmer:	Mikko Valimaki
	Date:		Wed Sep 23 13:11:05 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_time.c 47429 2013-10-29 15:27:44Z cdemory $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  3.01 22.07.93/MV	Added one required argument to "ls_PrintTime" call since
			that interface changed in logsystem.  Argument does not
			alter the behaviour here.
  3.02 06.02.95/JN	Moved time-handling from logsystem to runtime.
			ls_XXX -> tr_XXX.
  3.03 30.03.99/JR	varargs->stdargs
==============================================================================*/

#include <stdio.h>
#include <sys/types.h>
/* #include <varargs.h> */
#include <time.h>
#include <signal.h>

#include "tr_strings.h"
#include "tr_prototypes.h"

/*============================================================================
============================================================================*/
char *tr_GetTime ( int args, ... )
{
	va_list ap;
	time_t  clock = (time_t)0;
	char	*tmp;
	char	*fmt;
  char  *result;

	va_start (ap, args);
	switch (args) {
	case 0:
		/*
		**  This is of type time_t
		*/
		if ((clock = va_arg (ap, time_t)) == (time_t)-1)
			clock = time (NULL);
		break;
	case 1:
		/*
		**  This is of type char *
		*/
		if ((tmp = va_arg (ap, char *)) == (char *)-1)
			clock = time (NULL);
		else {
			time_t tims[2];
			tr_parsetime (tmp, tims);
			clock = tims[0];
		}
		break;
	}
	if (!(fmt = va_arg (ap, char *)))
		fmt = TR_DEFAULT_DATE_FORMAT;

  result = tr_MemPool (tr_timestring (fmt, clock));
	va_end (ap);

	return result;
}

