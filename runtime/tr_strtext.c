/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_strtext.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Thu Jul 16 07:52:25 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include <string.h>
#include "tr_prototypes.h"

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_strtext.c 55100 2019-08-21 13:30:39Z sodifrance $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation

==============================================================================*/

#include "tr_externals.h"

/*============================================================================
============================================================================*/
int tr_strtextset (char *string, double index, char *value)
{
	if (!string || !value || index <= 0)
		return -1;

	int iIndex = (int)index - 1;	
	/*
 	 * Test length of replacing char && length of replaced char
 	 * If they are both the same size then everything is ok,
 	 * else we have some computations to do 
 	 */
	int replacingSz = strlen( value );
	size_t toSkip = (size_t) iIndex;
	int offset = tr_mbSkipChars( &toSkip, string );
	if (offset < 0 ) {
		/* TODO : externalize message */
		tr_Log( "Invalid encoding" );
		return -1;
	}
	if (toSkip > 0 ) {
		/* no such char */
		return -1;
	}
	char *p = string + offset;
	int replacedSz = mblen( p, MB_CUR_MAX );
	if ( replacedSz == replacingSz ) {
		/* same size : just replace */
		int count = 0;
		char *pv = value;
		while ( count < replacingSz ) {
			*p++ = *pv++;
			++count;
		}
	} else if ( replacedSz > replacingSz ) {
		/* allocated size is enough */
		/* start copying value in place */
		int count = 0;
		char *pv = value;
		while ( count < replacingSz ) {
			*p++ = *pv++;
			++count;
		}
		char *p2 = p + replacedSz - replacingSz;
		while ( *p2 ) {
			*p++ = *p2++;
		}
		*p = '\0';
	} else {
		/* need to allocate some more space, this is bad */
		return -1; /* TODO */
	}

	return 0;
}

/*============================================================================
============================================================================*/
char *tr_strtextget (char *string, double index)
{
	int iIndex = (int)index - 1;
	char tmp[MB_CUR_MAX + 1];
	size_t toSkip = iIndex;
	int skipped;

	if (!string || iIndex < 0) {
		return TR_EMPTY;
	}

	skipped = tr_mbSkipChars( &toSkip, string );
	if (skipped < 0 || toSkip > 0) {
		return TR_EMPTY;
	}

	char *ptr = string + skipped;
	size_t charbytes = mblen( ptr, MB_CUR_MAX);

	if (charbytes <= 0) {
		return TR_EMPTY;
	}
	memset( tmp, 0, sizeof(tmp) ); 
	strncpy( tmp, ptr, charbytes );
	
	return tr_MemPool (tmp);
}

