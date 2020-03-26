/* TradeXpress tr_regcmpown.c */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "tr_prototypes.h"

/*===================================================================
  \      escape special characters.
  \n     embedded newline.
  $      end of string if last
  []*.^  range, 0..n times, character, beginning, complement, as usual.
  -      range inside [], not special if 1st or last.
  RE+    1 or more RE
  {m} {m,} {m,u}
	 minimum, maximum. both less 256. {m} exact, {m,} = {m,infinity}
	 (+ = {1,} and * = {0,})
  ( ... )$n
	 Enclosed RE is returned in nth argument to regex. 0..9
  ( ... )
	 Grouping. (RE)*
===================================================================*/

enum {
	DOT = '.',
};

enum {
	BOS,
	EOS,
	RANGE,
	ENDRANGE,
	COMPLEMENT,
	ANY,
};

static void comp(char **sp, char **rep)
{
	char *s  = *sp;
	char *re = *rep;
	char c;

	if (*s == '^') {
		*re++ = BOS;
		s++;
	}
	for (;;) {
		switch (c = *s++) {
		case 0:
			*sp  = s - 1;
			*rep = re;
			return;
		default:
	normal:
			*re++ = c;
			break;
		case '\\':
			if (*s == 0)
				goto normal;
			if ((c = *s++) == 'n')
				*re++ = '\n';
			else
				*re++ = c;
			break;
		case '$':
			if (*s)
				goto normal;
			*re++ = EOS;
			break;
		case '.':
			*re++ = ANY;
			break;
		case '[':
			if (*s == 0)
				goto normal;
			if (*s == '^') {
				if (*++s == 0)
					goto normal;
				*re++ = COMPLEMENT;
			} else
				*re++ = RANGE;
			do {
				*re++ = *s++;
				if (*s == '-' && s[1])
					;
			} while (*s && *s != ']');
			*re++ = ENDRANGE;
			break;
		case '{':
			break;
		case '}':
			break;
		case '(':
			comp(&s, &re);
			if (*s == ')')
				;
			break;
		case ')':
			break;
		}
	}
}

char *__loc1;
char *subs[10];

/*
 *  Compile the RE from concatenated arguments (last NULL).
 *  Return opaque pointer, malloced space.
 *  NULL on any error.
 */
char *regcmp(char *args, ...)
{
	va_list ap;
	char *re, *s, *ssave, *cp;
	int len;

	len = 1;
	va_start(ap, args);
	cp = args;
	do {
		len += strlen(cp);
	} while (cp = va_arg(ap, char *));
	va_end(ap);

	if ((re = malloc(3 * len)) == NULL)
		return (NULL);
	if ((s = malloc(len)) == NULL) {
		free(re);
		return (NULL);
	}
	ssave = s;
	*s = 0;
	va_start(ap, args);
	cp = args;
	do {
		strcat(s, cp);
	} while (cp = va_arg(ap, char *));
	va_end(ap);

	/*
	 *  Has to be unpaired grouping
	 *  if something still left in string after compilation.
	 */
	comp(&s, &re);
	if (*s)
		return (NULL);

	free(ssave);
	return (re);
}

/*
 *  Execute RE against the subject string.
 *  Values are passed back in additional 10 arguments (char []).
 *  NULL on mismatch, end of match on success.
 *  __loc1 points to start of match.
 *  not re-entrant.
 */
char *regex(char *re, char *subject, ...)
{
	va_list ap;
	int i;

	va_start(ap, subject);
	for (i = 0; i < 10; ++i)
		subs[i] = va_arg(ap, char *);
	va_end(ap);

	if (*re == BOS) {
		re++;
		if (match(re, subject))
			;
	}
	while (0) {
		if (match(re, subject))
			;
	}
	return NULL;
}

static match(char *re, char *subject)
{
	char c;

	for (;;) {
		switch (*re) {

		default:
			/*
			 *  Not special. Has to match.
			 */
			if (*re != *subject)
				return (0);
			break;
		case DOT:
			/*
			 *  Any character matches, except newline.
			 */
			if (*subject == 0 || *subject == '\n')
				return (0);
			break;
		case RANGE:
		case COMPLEMENT:
			/*
			 *  Any character from range-expression matches.
			 */
			break;
		}
	}
}

