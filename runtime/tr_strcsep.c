/*==========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_strcsep.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 11:34:18 EET 1992
==========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_strcsep.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*==========================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	Created.
  3.01 14.02.96/JN	evil strtok changed to work without internal state,
			name also changed to catch all calls.
  3.02 04.01.07 AVD     Evolution SPLIT function support the splitting with delimitor 
	                and escape characters.
  3.03 05.03.07 AVD	Gestion error  and add Warning log message in case of lack of delimitor.
==========================================================================*/

#include <stdio.h>
#include <string.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"

/*==========================================================================
  Separate next c-separated token in *sp,
  keep position in *s.
  No internal state.

  Still modifies the input string !
  Functionality when c == 0 ?
==========================================================================*/


char *tr_strcpick(char **sp, char c, char d, char e, int *ap)
{
	char *s = *sp;
	char *r;

	char ch;

	int position = 0;
	if ((r = (char *) malloc(1)) == NULL) {
		tr_Fatal (TR_FATAL_OUT_OF_MEMORY);
		return TR_EMPTY;
	}

	r[position] = 0;

	if (s && s != TR_EMPTY) {
		/*
		 *  Nonnull ("" possible) input.
		 *  Seek to terminating character
		 *  and set next location.
		 */

		if ( d != '\0' && e != '\0') {
			char *p;
			int hasDelimitor = 0;

			for (p = s; ; ++p) {
				if (*p == 0) {
					*sp = NULL;
					break;
				}

				ch = *p;
				/* 3.03 AVD Add a pointer to count text's position in case 
				 * 	    of lack of delimitor before a separator
				 */
				(*ap)++;
				if (!hasDelimitor) {	
					if (*p == c) {
						/* In case of p is caracter seperator, the split string is returned */
						p++;
						*sp = p;
						break;
					}
					else if (*p == d) {
						/*In case of p is the character delimitor */ 
						hasDelimitor = 1;
						ch = '\0';
					} else if (*p == e) {
						/*In case of p is the character escape */ 
						p++;
						(*ap)++;
						ch= *p;
					}
				}
				else {
					if ((*p == d && d != e) || (*p == d && d == e)) {
						p++;
						(*ap)++;
						if (*p == d) {
						  	/* In case of p+1 is character delimitor, the first delimitor is rejected */
							ch = *p;
						} else if ( *p != d && *p == c ) {
							/* In case of p is character seperator, the split string is returned */
							p++; 
							*sp = p;
							break;
						} else {
							/* In case of p is normal character, ignore character delimitor and pick the character */
							ch = *p;
							/* 3.03 AVD Warning log message to display to user in case of lack of delimitor
							 *          before a separator
							 */
							tr_Log (TR_WARN_ILLEGAL_TEXT_POSITION, *ap-1);
						}
					}
					else if (*p == e && d != e) {	
						/* In case of p is the character escape, the character escape is rejected also */ 
						p++;
						(*ap)++;
						ch = *p;
					} 
				}

				if ((r = (char *) realloc(r, position+2)) == NULL) {
					tr_Fatal (TR_FATAL_OUT_OF_MEMORY);
					return TR_EMPTY;
				}
				
				if (ch)	{
					r[position] = ch;
					r[++position] = 0;
				}
			}
			/*
			 *  Can be empty string (not converted to TR_EMPTY)
			 */
			return (r);
		}
		else if (d == '\0' && e == '\0') {
			s = tr_strcsep(sp, c);
			return (s);
		}
	}
	return (NULL);
}

char *tr_strcsep(char **sp, char c)
{
	char *s = *sp;

	if (s && s != TR_EMPTY) {
		/*
		 *  Nonnull ("" possible) input.
		 *  Seek to terminating character
		 *  and set next location.
		 */
		char *p;
	
		for (p = s; ; ++p) {
			if (*p == 0) {
				*sp = NULL;
				break;
			}
			if (*p == c) {
				*p++ = 0;
				*sp = p;
				break;
			}
		}
		/*
		 *  Can be empty string (not converted to TR_EMPTY)
		 */
		return (s);
	}
	return (NULL);
}

#if 0
	if (s) {
		if (p = strchr (s, c))
			*p = '\0';
		next = p ? p + 1 : NULL;
		return s;
	} else {
		if (!next || !*next)
			return NULL;
		if (p = strchr (p2 = next, c)) {
			*p = '\0';
			next = p + 1;
			if (p == p2)
				return TR_EMPTY;
		} else
			next = "";
			return p2;
	}
#endif

