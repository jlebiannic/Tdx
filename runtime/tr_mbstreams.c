/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_mbstreams.c
	Programmer:	Olivier Remaud (Sodifrance)
	Date:		12.06.2019	

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_mbstreams.c $")

/*========================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 12.06.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3123 24.09.2019 - Olivier REMAUD - UNICODE adaptation  
  Jira TX-3123 10.02.2020 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <iconv.h>
#include <errno.h>

#include "tr_rte_globals.h"
#include "tr_prototypes.h"

int tr_GetOutputDecoder( char **encoding, iconv_t **converter );
int tr_GetInputDecoder( char **text, size_t textSize, char **encoding, iconv_t **converter, int *read_pad  );


int _tr_mbFputs( char *string, FILE *stream, int maxCount /* -1 = no limit */, int withNL )
{
	static int stdout_initd = 0;
	static int stderr_initd = 0;
	static char *stdout_encoding = NULL;
	static char* stderr_encoding = NULL;
	static iconv_t *stdout_converter = NULL;
	static iconv_t *stderr_converter = NULL;

	int current_initd = 0;
	char *current_encoding = NULL;
	iconv_t *current_converter = NULL;
		
	if ( stream == stdout ) {
		current_initd = stdout_initd;
		stdout_initd = 1; /* will be initd */
		current_encoding = stdout_encoding;
		current_converter = stdout_converter;
	} 
	else if (stream == stderr ) {
		current_initd = stderr_initd;
		stderr_initd = 1; /* will be initd */
		current_encoding = stderr_encoding;
		current_converter = stderr_converter;
	}
	else {
		int unused;
		current_converter = (iconv_t*) tr_GetIconvInfo( stream, &current_initd, &unused, &current_encoding );
	}
	if ( !current_initd ) {
		current_encoding = NULL;
		if ( stream == stdout ) {
			current_encoding = tr_am_textget(taPARAMETERS, "OUTPUT.ENCODING");
			if (current_encoding == NULL || *current_encoding == '\0') {
				current_encoding = getenv( "OUTPUT_ENCODING" );
			}
		}
		else if (stream == stderr) {
			current_encoding = tr_am_textget(taPARAMETERS, "LOGGING.ENCODING");
			if (current_encoding == NULL || *current_encoding == '\0') {
				current_encoding = getenv( "LOGGING_ENCODING" );
			}
		}	
		else {
			current_encoding = tr_am_textget(taPARAMETERS, "FILEOUT.ENCODING");
			if (current_encoding == NULL || *current_encoding == '\0') {
				current_encoding = getenv( "FILEOUT_ENCODING" );
			}
		}
		if (current_encoding != NULL && *current_encoding == '\0') {
			current_encoding = NULL;
		}
		int status = tr_GetOutputDecoder( &current_encoding, &current_converter );
		/* persist encoding configuration */
		if (stream == stdout ) {
			stdout_initd = 1;
			stdout_encoding = (current_encoding == NULL) ? NULL : tr_strdup( current_encoding );
			stdout_converter = current_converter;
		} else if (stream == stderr) {
			stderr_initd = 1;
			stderr_encoding = (current_encoding == NULL) ? NULL : tr_strdup( current_encoding );
			stderr_converter = current_converter;
		} else {
			tr_SetIconvInfo( stream, 0, current_encoding, (void *) current_converter );
		}
	}

	int bytesToWrite;
	if (maxCount < 0 ) {
		bytesToWrite = strlen( string );
	} else {
		size_t toSkip = (size_t) maxCount;
		bytesToWrite = tr_mbSkipChars( &toSkip, string );
	}
	if ( current_converter != NULL ) {
		/* conversion needed */
		size_t bytesLen = bytesToWrite;
		int szIn = (int) bytesLen;
		int szOut = szIn * 4; // allocate maximum possible size
		char *buffer = (char*) tr_calloc( szOut, sizeof(char) );
		char *pOut = buffer;
		char *pInUtf8 = string;
		size_t leftIn = szIn;
		size_t leftOut = szOut;
		size_t res = iconv( current_converter, &pInUtf8, &leftIn, &pOut, &leftOut );
		if (res == (size_t) -1 ) {
			/* conversion failed */
			tr_free( buffer ); buffer = NULL;
			/* TODO : Externalize message */
			tr_Log( "Iconv conversion failed : bad encoding" );
			return EOF;
		}
		/* Conversion OK */
		size_t outputBytes = szOut - leftOut;
		size_t written = fwrite( buffer, sizeof(char), outputBytes, stream );
		tr_free( buffer ); buffer = NULL;
		if ( written != outputBytes ) {
			return EOF;
		}
		if ( withNL ) {
			_tr_mbFputs( "\n", stream, -1, 0 );
			++written;
		}
		return (int) written;
	}
	
	/* No conversion needed since it's already UTF-8 */
	size_t written = fwrite( string, sizeof(char), bytesToWrite, stream );
	if (written != bytesToWrite) {
		return EOF;
	}
	if ( withNL ) {
		fputc( '\n', stream );
		++written;
	}
	return (int) written;	
}


int tr_mbFputs( char *string, FILE *stream ) {
	return _tr_mbFputs( string, stream, -1, 0 );
}

int tr_mbFputsNL( char *string, FILE *stream ) {
	return _tr_mbFputs( string, stream, -1, 1 );
}

int tr_mbFnputs( char *string, FILE *stream, size_t buffsz ) {
	return _tr_mbFputs( string, stream, buffsz, 0 );
}



char* tr_mbFgets( char *buffer, size_t buffsz, FILE *stream )
{
	static int stdin_initd = 0;
	static char *stdin_encoding = NULL;
	static int stdin_read_padding = 0;
	static iconv_t *stdin_converter = NULL;

	int current_initd = 0;
	char *current_encoding = NULL;
	int current_read_padding = 0;
	iconv_t *current_converter = NULL;
		
	memset( buffer, 0, buffsz );
	char *res = fgets( buffer, buffsz, stream );
	if (res == NULL) {
		return NULL;
	}

	char *text = buffer;
	if ( stream == stdin ) {
		current_initd = stdin_initd;
		current_encoding = stdin_encoding;
		current_read_padding = stdin_read_padding;
		current_converter = stdin_converter;
	} else {
		current_converter = (iconv_t*) tr_GetIconvInfo( stream, &current_initd, &current_read_padding, &current_encoding );
	}
	if ( !current_initd ) {
		current_encoding = NULL;
		if ( stream == stdin ) {
			current_encoding = tr_am_textget(taPARAMETERS, "INPUT.ENCODING" );
			if ( current_encoding == NULL || *current_encoding == '\0' ) {
				current_encoding = getenv( "INPUT_ENCODING" );
			}
		} else {
			current_encoding = tr_am_textget(taPARAMETERS, "FILEIN.ENCODING" );
			if ( current_encoding == NULL || *current_encoding == '\0' ) {
				current_encoding = getenv( "FILEIN_ENCODING" );
			}
		}
		if ( current_encoding != NULL && *current_encoding == '\0' ) {
			current_encoding = NULL;
		}
		int status = tr_GetInputDecoder( &text, buffsz, &current_encoding, &current_converter, &current_read_padding );
		/* persist encoding configuration */
		if (stream == stdin ) {
			stdin_initd = 1;
			stdin_encoding = (current_encoding == NULL) ? NULL : tr_strdup( current_encoding );
			stdin_converter = current_converter;			stdin_read_padding = current_read_padding;
			stdin_read_padding = current_read_padding;
		} else {
			tr_SetIconvInfo( stream, current_read_padding, current_encoding, (void *) current_converter );
		}
	}
	if ( current_converter != NULL) {
		/* fix length to decode : having a multiple of 4 bytes is valid for UTF-16 && UTF-32 */
		size_t leftIn = buffsz - buffsz % 4;

		/* Allocate enough space for various encodings */
		/* utf-8 to utf-8 -> same size */
		/* utf-16 to utf-8 -> double the space if every UTF-16 char takes 4 bytes in UTF-8 */
		/* utf-32 to utf-8 -> same size or less */
		/* 8 bytes ASCII/EBCDIC based charmap : some chars may take 2 bytes in UTF-8 */
		size_t leftOut = buffsz * 2;

		char *bufferUtf8 = (char*) tr_calloc( leftOut, sizeof(char) );
		char *pOutUtf8 = bufferUtf8;
		char *pIn = text;
		size_t res = iconv( current_converter, &pIn, &leftIn, &pOutUtf8, &leftOut );
		if (res == (size_t) -1 ) {
			/* conversion failed */
			tr_free( bufferUtf8 ); bufferUtf8 = NULL;
			/* TODO : Externalize message */
			tr_Log( "Iconv conversion failed : bad encoding" );
			return NULL;
		}
		/* Conversion OK */
		int len = strlen( bufferUtf8 );
		if (len >= buffsz ) {
			tr_free( bufferUtf8 ); bufferUtf8 = NULL;
			/* TODO : Externalize message */
			tr_Log( "Iconv conversion failed : decoded string too long" );
			return NULL;	
		}
		strncpy( buffer, bufferUtf8, buffsz - 1 );
		tr_free( bufferUtf8 ); bufferUtf8 = NULL;
		text = buffer;
		int i;
		for (i = 0; i < current_read_padding; ++i) {
			fgetc( stream );
		}
	}
	return text;
}

int tr_mbFprintf( FILE *stream, char *format, ... ) 
{
    va_list ap;
    int sz;

    va_start( ap, format );
    sz = tr_mbVfprintf( stream, format, ap );
    va_end(ap);

	return sz;
}

int tr_mbVfprintf( FILE *stream, char *format, va_list ap )
{
	va_list ap_copy;
	int sz;

	va_copy( ap_copy, ap );
    sz = 1 + vsnprintf( NULL, 0, format, ap_copy );
    va_end(ap_copy);

    char *tmp = (char*) tr_calloc( sz, sizeof(char) );

    vsnprintf( tmp, sz, format, ap );

	tr_mbFputs( tmp, stream );
	tr_free( tmp ); tmp = NULL;

	return sz - 1;
}
