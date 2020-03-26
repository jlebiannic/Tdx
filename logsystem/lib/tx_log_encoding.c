/*========================================================================
 *         TradeXpress
 *
 *                 File:       logsystem/tx_log_encoding.c
 *                 Author:     Olivier Remaud (ORD - Sodifrance)
 *                 Date:       2019 07 11
 *
 *                 Copyright Generix-Group 2019
 *
 *                 Routines used to access fields in logentries.
 *========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tx_log_encoding.c $")
/*========================================================================
 *   Record all changes here and update the above string accordingly.
 *   TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation
 *
 *========================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <locale.h>
#include <errno.h>

int mbsncpy(char* to, char* from, size_t size) {

	int toCopy = size;
	char *source = from;
	char *dest = to;

	while (toCopy > 0) {
		int bytes = mblen( source, MB_CUR_MAX);
 		if ( bytes < 0 ) {
			*dest = '\0';
 			return -1; /* error */
		}
		if (bytes == 0) {
			break; /* end of string */
		}
 		/* copy sz bytes = 1 unicode char*/
		int  copied = 0;
		while ( copied < bytes ) {
			*dest++ = *source++;
			++copied;
		}
		--toCopy;
	}
	*dest = '\0';
	return size - toCopy;
}

int mbsncpypad(char* to, char* from, size_t size, char padding) {

	int toCopy = size;
	char *source = from;
	char *dest = to;

	while (toCopy > 0) {
		int bytes = mblen( source, MB_CUR_MAX);
 		if ( bytes < 0 ) {
			*dest = '\0';
 			return -1; /* error */
		}
		if (bytes == 0) {
			break; /* end of string */
		}
 		/* copy sz bytes = 1 unicode char*/
		int copied = 0;
		while ( copied < bytes ) {
			*dest++ = *source++;
			++copied;
		}
		--toCopy;
	}
	while( toCopy ) {
		*dest++ = padding;
		--toCopy;
	}
	*dest = '\0';
	return size;
}

#define TX_ENV_LOCALE "TX_ENCODING"
static char *DEFAULT_LOCALE = "en_US.UTF-8";
/* Set current locale to be a UTF-8 compatible locale
 ** This only sets the locale for the current threada
 **
 ** UTF-8 locale is computed once and cached in a local static variable
 ** to avoid cost when calling this function multiple times
 **
 ** First, try to get the locale from TX_ENCODING environment variable
 ** Use en_US.UTF-8 if not found
 **
 ** Returns 0 in case of success and non-zero in case of failure
 **/
int log_UseLocaleUTF8()
{
        static locale_t utf8 = NULL;
        static int initd = 0;

        if ( !initd ) {
                /* init */
                initd = 1;

                char *localeName = getenv( TX_ENV_LOCALE );
                if (localeName == NULL) {
                        localeName = DEFAULT_LOCALE;
                }

                /* keep a copy of original locale */
                locale_t oldLocale = uselocale( (locale_t) NULL );

                /* change values of global locale */
                char *ctypeOldLocale = setlocale( LC_CTYPE, localeName );
                char *collateOldLocale = setlocale( LC_COLLATE, localeName );

                /* duplicate current locale and store for futur use */
                utf8 = duplocale( uselocale( (locale_t) NULL) );

                /* restore previous globale locale */
                setlocale( LC_CTYPE, ctypeOldLocale );
                setlocale( LC_COLLATE, collateOldLocale );
        }
        if ( utf8 == NULL ) {
                /* initialization went wrong */
                return -1;
        }

        /* set current thread locale */
        locale_t previous = uselocale( utf8 );
        if ( previous == NULL ) {
                /* unable to set locale */
                return -2;
        }

        return 0;
}

