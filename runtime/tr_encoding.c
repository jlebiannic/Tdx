/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_encoding.c
	Programmer:	Olivier Remaud (Sodifrance)
	Date:		12.06.2019	

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_encoding.c $")

/*========================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 12.06.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 24.09.2019 - Olivier REMAUD - Passage au 64 bits  
  Jira TX-3123 10.02.2020 - Olivier REMAUD - UNICODE adaptation 
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <iconv.h>
#include <errno.h>

#include "tr_externals.h"
/*
#include "tr_privates.h"
#include "tr_prototypes.h"
*/

typedef struct iconvinfo {
	iconv_t *converter;
	int read_pad;
	char *encoding;
} iconvinfo;

int tr_GetInputDecoder( char **text, size_t textSize, char **encoding, iconv_t **converter, int *read_pad  )
{
	char *inEncoding = *encoding;
	if ( inEncoding == NULL || *inEncoding == '\0' ) {
		inEncoding = "UTF-8"; /* default */
	}

	/* Try to remove BOM if needed */
	/* UTF-8 : EF BB BF */
	if ( textSize >= 3 
		&& (unsigned char) (*text)[0] == 0xef 
		&& (unsigned char) (*text)[1] == 0xbb 
		&& (unsigned char) (*text)[2] == 0xbf ) 
	{
		inEncoding = "UTF-8";
		*text += 3; /* skip BOM */
	} 

	/* UTF-32 BE : 00 00 FE FF */
	else if ( textSize >= 4 
		&& (unsigned char) (*text)[0] == 0x00 
		&& (unsigned char) (*text)[1] == 0x00 
		&& (unsigned char) (*text)[2] == 0xfe 
		&& (unsigned char) (*text)[3] == 0xff ) 
	{
		inEncoding = "UTF-32BE";
		*text += 4;
	}

	/* UTF-32 LE : FF FE 00 00 */
	else if ( textSize >= 4 
		&& (unsigned char) (*text)[0] == 0xff 
		&& (unsigned char) (*text)[1] == 0xfe 
		&& (unsigned char) (*text)[2] == 0x00 
		&& (unsigned char) (*text)[3] == 0x00 ) 
	{
		inEncoding = "UTF-32LE";
		*text += 4;
	}

	/* UTF-16 BE : FE FF */
	else if ( textSize >= 2 
		&& (unsigned char) (*text)[0] == 0xfe 
		&& (unsigned char) (*text)[1] == 0xff ) 
	{
		inEncoding = "UTF-16BE";
		*text += 2;
	}

	/* UTF-16 LE : FF FE */
	else if ( textSize >= 2 
		&& (unsigned char) (*text)[0] == 0xff 
		&& (unsigned char) (*text)[1] == 0xfe ) 
	{
		inEncoding = "UTF-16LE";
		*text += 2;
	}

	*encoding = inEncoding;

	if ( strcmp(inEncoding, "UTF-8") == 0 ) {
		*converter = NULL;
		return 0; /* nothing to decode */
	}

	*converter = iconv_open( "UTF-8", inEncoding );
	if ( *converter == NULL ) {
		/* error : unknown encoding */
		return -2;
	}
	/* configure iconv behavior as needed */
	/*		const int ICONV_TRUE = 1; */
	/*		iconvctl( *converter, 2 = ICONV_SET_TRANSLITERATE, &ICONV_TRUE ); */
	/*		iconvctl( *converter, 4 = ICONV_SET_DISCARD_ILSEQ, &ICONV_TRUE ); */
	*read_pad = 0;
	if (strncmp("UTF-16LE", inEncoding, 8) == 0) {
		*read_pad = 1;
	} else if (strncmp("UTF-32LE", inEncoding, 8) == 0) {
		*read_pad = 3;

	}
	return 0;
}


int tr_GetOutputDecoder( char **encoding, iconv_t **converter )
{
	if ( *encoding == NULL || **encoding == '\0' ) {
		*encoding = "UTF-8"; /* default */
	}

	if ( strcmp(*encoding, "UTF-8") == 0 ) {
		*converter = NULL;
		return 0; /* nothing to decode */
	}

	*converter = iconv_open( *encoding, "UTF-8" );
	if ( *converter == NULL ) {
		/* error : unknown encoding */
		return -2;
	}
	/* configure iconv behavior as needed */
	/*		const int ICONV_TRUE = 1; */
	/*		iconvctl( *converter, 2 = ICONV_SET_TRANSLITERATE, &ICONV_TRUE ); */
	/*		iconvctl( *converter, 4 = ICONV_SET_DISCARD_ILSEQ, &ICONV_TRUE ); */
	return 0;
}

void tr_FreeIconvInfo( void *converter ) {
	iconv_close( (iconv_t *) converter );
}

