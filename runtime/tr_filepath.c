/*========================================================================
        TradeXpress

        File:		runtime/tr_filepath.c
        Author:	Jean-François CLAVIER 
        Date:		Fri Aug  27

        Copyright (c) 2010 Generix group
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_filepath.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
 $Log:  $
========================================================================*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <tr_prototypes.h>
#include <stdlib.h>

#ifndef MACHINE_WNT
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#else
#include <fcntl.h>
#include <io.h>
#include <sys/timeb.h>
#endif


/*
 * Return a string where all environment variables are substituted by their content.
 * string : the string to parse
 * mask : a bit mask to disable some type of var.
 *   ENV_VAR : allow default variable $envname $ENVNAME
 *   ENV_PAREN : allow parentheses $(envname)
 *   ENV_BRACE : allow parentheses ${envname}
 *   ENV_TIME :$% return time in 8 digits, hex
 *   ENV_PID : $$ return our PID, 5 digits
 */
char *tfSubstituteEnv(char *string, double mask)
{
	char c, *cp, *val, *bufp;
	static char buffer[1024];

	if (mask == 0)
		mask = ENV_VAR;   /* Always allow environnement variable if no mask is defined */

	bufp = buffer;
	*bufp = 0;

	while ((c = *string++)) {
		/*
		 *  Lame check for overflow.
		 */
		if (bufp >= &buffer[sizeof(buffer) - 2]) {
			fprintf(stderr, "String overflow in fSubstituteEnv");
		}
		/*
		if (c == '\\') {
			if (*string)
				*bufp++ = *string++;
			continue;
		}
		*/
		if (c != '$') {
			*bufp++ = c;
			continue;
		}
		/*
		 *  $$ Our PID, 5 digits
		 *  $% Time in 8 digits, hex
		 *  $(from)
		 *  $from
		 *  $(USER)
		 *  $USER
		 */
		c = *string++;
		if ((c == '$') && (ENV_PID)) {
			sprintf(bufp, "%05d", getpid());
			bufp += strlen(bufp);
			continue;
		}
		if ((c == '%') && (ENV_TIME)) {
			sprintf(bufp, "%08X", time(NULL));
			bufp += strlen(bufp);
			continue;
		}
		/*
		 *  Remember where we were in the buffer.
		 *  Collect either upto ')' or an identifier.
		 */
		val = bufp;

        #define is_idchar(c) (isalpha(c) || isdigit(c) || c == '_')

		if ((c == '(') && (ENV_PAREN)) {
			/*
			 *  If we hit end of input-string,
			 *  adjust so that while() above terminates correctly.
			 */
			while ((c = *string++) != 0 && c != ')')
				*bufp++ = c;
			if (c == 0)
				--string;
		} else if ((c == '{') && (ENV_BRACE)) {
			/*
			 *  If we hit end of input-string,
			 *  adjust so that while() above terminates correctly.
			 */
			while ((c = *string++) != 0 && c != '}')
				*bufp++ = c;
			if (c == 0)
				--string;
		} else if (ENV_VAR  && is_idchar(c)) {
			/*
			 *  Whatever terminates this scan,
			 *  needs to be re-looked at while() above.
			 */
			*bufp++ = c;
			while ((c = *string++) != 0 && is_idchar(c))
				*bufp++ = c;
			--string;
		}
		else {
			*bufp++ = '$';
			--string;
			continue;
		}
		/*
		 *  Terminate collected variable-name.
		 *  Search headers and environment.
		 *  Header-fields are in lowercase, so we dont
		 *  need to separate the name-spaces.
		 */
		*bufp = 0;
		bufp = val;

		if ((cp = getenv(bufp)))
			strcpy(bufp, cp);
		else
			strcpy(bufp, "");
		bufp += strlen(bufp);
	}
	*bufp = 0;
	return(tr_BuildText("%s",buffer));
}

