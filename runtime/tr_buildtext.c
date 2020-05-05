/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_buildtext.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992
			Mon Jan 11 21:35:06 EET 1993

	Copyright (c) 1992 Telecom Finland/EDI Operations
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_buildtext.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	Created.
  3.01 05.12.94/JN	Removed unnecessary memset() and srcline variable.
			Moved scratch-buffer to bss from stack,
			and increased the size.
			Overflowing stack-frame caused problems
			at least on hpux (vsprintf-check did not help).
			The change makes tr_BuildText() non-reentrant,
			but it does not matter.
  3.02 30.03.99/JR,KP	varargs->stdargs
  Bug 1219: 08.04.11/JFC create tr_FreeMemPoolArgs for use in other functions
  Bug 6766: 08.04.11/JFC Allow buffer to override the 32Kb
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#ifndef MACHINE_WNT
#include <strings.h>
#endif
#include <string.h>
/* #include <varargs.h> */
#include "tr_constants.h"
#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

#ifdef MACHINE_WNT
#define vsnprintf _vsnprintf
#endif
/*==============================================================================
	tr_FreeMemPoolArgs : free given parameters if they are in the memory pool
	When RTE function use as parameter a call to a function returning its result
	in the mem pool, this parameter was not freed resulting in memory leak.
==============================================================================*/
void tr_FreeMemPoolArgs(char *fmt, va_list ap)
{
	char *p; 
	int d, in_percent = 0;
	char c;

	while (*fmt && fmt[0] != '\0') {
		switch (*fmt) {
			case '%':
				fmt++;
				in_percent = 1;
				while (*fmt && fmt[0] != '\0' && in_percent) {
					switch (*fmt) {
						case '0':
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
						case '.':
						case '*':
							fmt++;
							break;
						case 's':
							p = va_arg(ap, char *);
							fmt++;
							tr_MemPoolFree(p);
							in_percent = 0;
							break;
						case 'c':
						case 'd':
							d = va_arg(ap, int);
							fmt++;
							in_percent = 0;
							break;
						default:
							fmt++;
							in_percent = 0;
					}
				}
				break;
			default:
				fmt++;
				break;
		}
	}
}

/*==============================================================================
tr_getBufSize : get the estimate size need to allocate the buffer.
   On Windows, vsnprintf returns -1 instead of the required size
==============================================================================*/
size_t tr_getBufSize(size_t minsize, const char *fmt, va_list ap)
{
	char *p; 
	int d, in_percent = 0;
	char c;
	size_t result = minsize;

	while (*fmt && fmt[0] != '\0') {
		switch (*fmt) {
			case '%':
				fmt++;
				in_percent = 1;
				while (*fmt && fmt[0] != '\0' && in_percent) {
					switch (*fmt) {
						case '0':
						case '1':
						case '2':
						case '3':
						case '4':
						case '5':
						case '6':
						case '7':
						case '8':
						case '9':
						case '.':
						case '*':
							fmt++;
							break;
						case 's':
							p = va_arg(ap, char *);
							fmt++;
							result += strlen(p);
							in_percent = 0;
							break;
						case 'c':
						case 'd':
							d = va_arg(ap, int);
							fmt++;
							result += 10;
							in_percent = 0;
							break;
						case 'l':
							va_arg(ap, double);
							fmt++;
							result += 10;
							in_percent = 0;
							break;
						default:
							fmt++;
							in_percent = 0;
					}
				}
				break;
			default:
				fmt++;
				break;
		}
	}
	return result;
}

/*==============================================================================
  Varargs strdup().

  Warning: This function is not re-entrant,
  but as it calls only vsprintf() and tr_MemPool()
  it does not matter.
  Maximum string-length this can build is sizeof(buf) - 1,

  previously this was machdependant 2 * BUFSIZ.
  As BUFSIZ is very small (often just 512 or 1024) it could have 
  overflowed easily.

  XXX Change build() to use snprintf !
==============================================================================*/

static char buf[TR_STRING_MAX_SIZE + 1];

char * tr_BuildText( char *args, ... )
{
	va_list ap;
	int size, resultsize;
	char *result = NULL;

	va_start(ap, args);
	resultsize = vsnprintf (buf, sizeof(buf), args, ap);
	va_end (ap);
	tr_Trace(1, 1, "tr_BuildText : size = %d, sizeof(buf) %d\n", resultsize, sizeof(buf));

	if (resultsize == -1) { /* For Windows */
		va_start(ap, args);
		resultsize = tr_getBufSize(TX_BUFTRACESIZE, args, ap);
		va_end(ap);
	}
	if (resultsize >= sizeof(buf)) {
		size = resultsize;
		tr_Trace(1, 1, "tr_BuildText : size = %d\n", size);
		result = tr_zalloc(size+1);
		if (!result) {
			tr_Fatal (TR_FATAL_BUILD_OVERFLOW);  /* raise an exception */
		}
		va_start(ap, args);
		vsprintf(result, args, ap);
		va_end(ap);
	}

	/* check in the list of arguments if they are in mempool 
	 * i.e. by use of call to build inside the build function */
	va_start(ap, args);
	tr_FreeMemPoolArgs(args, ap);
	va_end (ap);

	if (result)
		return tr_MemPoolPass(result);
	else
		return tr_MemPool(buf);
}
