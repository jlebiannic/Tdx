/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_wildmat.c
	Programmer:	Juha Nurmela (JN)
	Date:		Wed May  6 16:01:43 EET 1992
========================================================================*/
#include "../conf/config.h"
MODULE("@(#)TradeXpress $Id: tr_wildmat.c 55061 2019-07-18 14:53:36Z sodifrance $")
/*========================================================================
  3.00 12.01.93/JN	Created.
  3.01 04.12.95/JN	Phantom ranges were introduced in []-sets.
			Total reorganization too...
  TX-3123 - 18.07.2019 - Olivier REMAUD - UNICODE adaptation

========================================================================*/

#include <stdio.h>
#include <string.h>

/*
 *  Return nonzero (TRUE) if string s matches the pattern in w.
 *
 *  *   matches anything
 *  ?   matches a single character
 *  []  matches a single character from a set.
 *         set can contain ranges, introduced with X-Y,
 *         or individual characters. These can be grouped.
 *
 *  Anything can (and all specials must !) be escaped with \ as usual.
 *
 *  - does not lose the range-meaning in front or end of set,
 *  this is unlike the common way...
 */

int tr_wildmat(char *s /* string under inspection */, char *w /* wildcarded pattern */)
{
	size_t bytes = 0;
	size_t idx = 0;

	/*
	 * Hmm ?
	 * Should not false instead ??? legacy...
	 */
	if (!s || !w)
		return (1);

	while (*w) {
		switch (*w) {
		case '*':
			++w; /* skip '*' in pattern */
			/*
			 *  Star matches anything at all.
			 *  If the pattern ends in here, it is a hit,
			 *  otherwise we must recurse for the rest of string.
			 */
			if ( *w == '\0' )
				return (1);

			while (*s) {
				if (tr_wildmat(s, w))
					return (1);
				s += mblen( s, MB_CUR_MAX ); /* skip next MB char */
			}
			break;

		case '?':
			/*
			 *  Any single character (except NUL of course).
			 */
			if (*s == 0)
				return (0);

			++w; /* skip '?' in pattern */
			s += mblen( s, MB_CUR_MAX ); /* skip next MB char */
			break;

		case '[':
			/*
			 *  Set of characters, ending in ']'.
			 */
			{
				wchar_t range_from = L'\0'; /* start of range */
				wchar_t wc_w = L'\0';
				wchar_t wc_s = L'\0';

				int got_range = 0;  /* no range seen so far */
				int in_set = 0;     /* assume not in set */

				++w; /* skip '[' */

				while (*w != 0 && *w != ']') {

					switch (*w) {
					case '-':
						got_range = 1;
						break;

					case '\\':
						if (w[1])
							++w;

						/* fallthru */
					default:
						mbtowc( &wc_w, w, MB_CUR_MAX );
						mbtowc( &wc_s, s, MB_CUR_MAX );

						if (wc_w == wc_s)
							in_set = 1;

						if (got_range) {
							got_range = 0;
							if (wc_s >= range_from
							 && wc_s <= wc_w)
								in_set = 1;
						}
						range_from = wc_w;
					}
					w += mblen( w, MB_CUR_MAX );
				}
				/*
				 *  Truncated set (no ]) or no hit ?
				 */
				if (!in_set || *w == 0)
					return (0);

				w += mblen( w, MB_CUR_MAX );
				s += mblen( s, MB_CUR_MAX );
			}
			break;

		case '\\':
			/*
			 *  Escaped character follows
			 *  and has to literally match.
			 */
			if (w[1])
				++w; /* skip escape prefix */

			/* fallthru */
		default:
			bytes = mblen(s, MB_CUR_MAX);
			if ( bytes == 0)
				return 0;

			/* test all bytes of multibyte char */
			for ( idx = 0; idx < bytes; ++idx ) {
				if ( w[idx] != s[idx] )
					return (0);
			}
			
			/* multibyte chars are the same, skip all bytes */
			w += bytes;
			s += bytes;
		}
	}
	/*
	 *  End of pattern, has to be end of string too.
	 */
	return (*s == 0);
}

