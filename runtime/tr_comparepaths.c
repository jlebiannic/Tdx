/*============================================================================
        E D I S E R V E R   T R A N S L A T O R

        File:           tr_comparepaths.c
        Programmer:     Jani Raiha
        Date:           Wed Aug 26 12:47:25 EET DST 1998

        Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_comparepaths.c 47429 2013-10-29 15:27:44Z cdemory $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  4.00 26.08.98/JR      Created
  
==============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

int tr_comparepaths(char *s, char *w)
{
	char *snew, *wnew, *stemp, *wtemp;
	char last;
	int rvalue = 0;

	stemp = malloc( 1024 * sizeof(char) );
	wtemp = malloc( 1024 * sizeof(char) );

	snew = stemp;
	wnew = wtemp;

	last = '\0';
	while( (*s != '\0') && ((stemp-snew) < 1023) ){
		if( !((last == '/') && ((*s == '\\')||(*s == '/'))) ){
			if( *s == '\\') *stemp = '/';
			else
			{
				if( (*s > 'a') && (*s < 'z') ) *stemp = tr_toupper(*s);
				else *stemp = *s;
			}
			last = *stemp;
			stemp++;
		}
		else if(stemp-snew == 1) *stemp++ = '/';
		s++;
	}

	last = '\0';
	while( (*w != '\0') && ((wtemp-wnew) < 1023) ){
		if( !((last == '/') && ((*w == '\\')||(*w == '/'))) ){
			if( *w == '\\') *wtemp = '/';
			else
			{
				if( (*w > 'a') && (*w < 'z') ) *wtemp = tr_toupper(*w);
				else *wtemp = *w;
			}
			last = *wtemp;
			wtemp++;
		}
		else if(wtemp-wnew == 1) *wtemp++ = '/';
		w++;
	}

	*stemp = '\0'; *wtemp = '\0';

	rvalue = tr_wildmat(snew, wnew);

	free( wnew );
	free( snew );

	return( rvalue );
}

int bfComparePaths(char *s, char *w)
{
	return (int) tr_comparepaths( s, w );
}
