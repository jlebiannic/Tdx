/*========================================================================
 *         E D I S E R V E R   T R A N S L A T O R
 *
 *         File:           tr_encoding.c
 *         Programmer:     Olivier Remaud (Sodifrance)
 *         Date:           06/2019 
 *
 *========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_locale.c $")

/*========================================================================
 *   Record all changes here and update the above string accordingly.
 *
 *   TX-3123 - 18.06.2019 - Olivier REMAUD - UNICODE adaptation
 *   Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
 *
 *   ========================================================================*/

#include <stdio.h>
#include <malloc.h>
#include <locale.h>
#include <errno.h>

#define TX_ENV_LOCALE "TX_ENCODING"
static char *DEFAULT_LOCALE = "en_US.UTF-8";
/* Set current locale to be a UTF-8 compatible locale
 * This only sets the locale for the current threada
 * 
 * UTF-8 locale is computed once and cached in a local static variable
 * to avoid cost when calling this function multiple times
 * 
 * First, try to get the locale from TX_ENCODING environment variable
 * Use en_US.UTF-8 if not found
 * 
 * Returns 0 in case of success and non-zero in case of failure
 */
int tr_UseLocaleUTF8()
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

