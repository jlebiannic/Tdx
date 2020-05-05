/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_mbtolower.c
	Programmer:	Olivier Remaud (Sodifrance)
	Date:		12.06.2019	

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_mbtolower.c $")

/*========================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 12.06.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3123 19.11.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <string.h>
#include <malloc.h>
#include <wctype.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"



char* tr_mbToLower( char* string ) 
{
	wchar_t wc;

	if (string == NULL || *string == '\0' )
		return TR_EMPTY;


	int count = mbtowc( &wc, string, 32 );
	if ( count <= 0 )
		return TR_EMPTY;

	wint_t wci = wc;
	wint_t wcl = towlower( wci );
	if ( wcl == WEOF )
		return TR_EMPTY;

	char *result = (char *) tr_calloc( MB_CUR_MAX * 2 + 1, sizeof(char) );
	wctomb( result, (wchar_t) wcl );

	return tr_MemPoolPass( result );
}



char* tr_mbToUpper( char* string ) 
{
	wchar_t wc;

	if (string == NULL || *string == '\0' )
		return TR_EMPTY;


	int count = mbtowc( &wc, string, 32 );
	if ( count <= 0 )
		return TR_EMPTY;

	wint_t wci = wc;
	wint_t wcu = towupper( wci );
	if ( wcu == WEOF )
		return TR_EMPTY;

	char *result = (char *) tr_calloc( MB_CUR_MAX * 2 + 1, sizeof(char) );
	wctomb( result, (wchar_t) wcu );

	return tr_MemPoolPass( result );
}


char* tr_mbStrToLower( char *string )
{
	if (string == NULL || *string == '\0' )
		return TR_EMPTY;

	size_t count = mbstowcs( NULL, string, 0 );
	wchar_t *wcstring = (wchar_t*) tr_calloc( count + 1, sizeof(wchar_t) );
	mbstowcs( wcstring, string, count );

	size_t bufSz = (count + 2) * MB_CUR_MAX;
	char *lowerString = (char*) tr_malloc( bufSz );
	char *ptr = lowerString;
	wchar_t *wptr = wcstring;
	size_t bytes = 0;

	while ( *wptr != 0 ) {

		if (bytes + MB_CUR_MAX > bufSz) {
			bufSz += 128;
			lowerString = (char*) tr_realloc( lowerString, bufSz );
			ptr = lowerString;
			ptr += bytes;
		}

		wchar_t wc = *wptr;
		wint_t wcl = towlower( (wint_t) wc );
		if (wcl == WEOF) {
			tr_free( lowerString );
			tr_free( wcstring );
			return TR_EMPTY;
		}
		int charBytes = wctomb( ptr, (wchar_t) wcl );
		if ( charBytes < 0 ) {
			tr_free( lowerString );
			tr_free( wcstring );
			return TR_EMPTY;
		}
		ptr += charBytes;
		bytes += charBytes;

		++wptr;
	}
	*ptr = '\0';
	tr_free( wcstring );

	return tr_MemPoolPass( lowerString );
}


char* tr_mbStrToUpper( char *string )
{
	if (string == NULL || *string == '\0' )
		return TR_EMPTY;

	size_t count = mbstowcs( NULL, string, 0 );
	wchar_t *wcstring = (wchar_t*) tr_calloc( count + 1, sizeof(wchar_t) );
	mbstowcs( wcstring, string, count );

	size_t bufSz = (count + 2) * MB_CUR_MAX;
	char *upperString = (char*) tr_malloc( bufSz );
	char *ptr = upperString;
	wchar_t *wptr = wcstring;
	size_t bytes = 0;

	while ( *wptr != 0 ) {

		if (bytes + MB_CUR_MAX > bufSz) {
			bufSz += 128;
			upperString = (char*) tr_realloc( upperString, bufSz );
			ptr = upperString;
			ptr += bytes;
		}

		wchar_t wc = *wptr;
		wint_t wcu = towupper( (wint_t) wc );
		if (wcu == WEOF) {
			tr_free( upperString );
			tr_free( wcstring );
			return TR_EMPTY;
		}
		int charBytes = wctomb( ptr, (wchar_t) wcu );
		if ( charBytes < 0 ) {
			tr_free( upperString );
			tr_free( wcstring );
			return TR_EMPTY;
		}
		ptr += charBytes;
		bytes += charBytes;

		++wptr;
	}
	*ptr = '\0';

	tr_free( wcstring );

	return tr_MemPoolPass( upperString );
}

