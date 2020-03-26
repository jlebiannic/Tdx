/*===========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_regexec.c
	Programmer:	Juha Nurmela (JN)
	Date:		Mon Mar 22 18:19:31 EET 1993
===========================================================================*/
#include "conf/config.h"
/*LIBRARY(libruntime_version)
*/
MODULE("@(#)TradeXpress $Id: tr_regexec.c 55238 2019-11-19 13:30:06Z sodifrance $")
/*===========================================================================
  Record all changes here and update the above string accordingly.
  3.01 29.03.93/JN	Temporary fix, HP regcomp is from perse.
  3.02 08.02.95/JN	Removed casts and added prototypes, (alpha...).
  3.03 21.04.95/JN	Split compile and execute.
  3.04 26.01.99/JR   Support for POSIX regcomp
  4.00 28.03.07/LM	Must put terminating 0 when using strncpy BugZ 7269
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
===========================================================================*/

#include <stdio.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

#ifdef MACHINE_LINUX
#include <regex.h>
#include <string.h>
#define POSIX_REGCOMP
#endif

/* NT4 Doesn't have regex
 * Maybe in near future
#ifdef MACHINE_WNT
#include "regex.h"
#define POSIX_REGCOMP
#endif
*/
extern char *__loc1;

static char *regular_loc1 = NULL;
static char subex[10][512];

#ifdef POSIX_REGCOMP
/* This function is for stripping $n away from regexp string.
 * (unsupported in posix) We also have to save placement information.
 * wrap_sysv_to_posix(regular expression string) returns placement vector
 */
char *wrap_sysv_to_posix(char *expr)
{
	int n=0, i=0;
	int pos = 0;
	char *result_vector = (char *) malloc(10);

	if(result_vector){
		memset(result_vector, -1, 10);
	
		while(expr[n])
		{
			if((expr[n] == '$')&&(expr[n+1] >= '0')&&(expr[n+1] <= '9'))
			{
				result_vector[i++] = expr[n+1] - '0';
				n += 2;
			}
			else
			{	
				if(pos != n)
					expr[pos] = expr[n];
				pos++; n++;
			}
		}
		if(pos != n)
			expr[pos] = 0;
		return(result_vector);
	}
	return(NULL);
}

/* This function copies regexec results to subex. Just to be
 * compliant with regcmp.
 * move_subs_to_subex(placement vector, regcomp(regmatch), original text string)
 */
void move_subs_to_subex(char *rv, regmatch_t *s, char *bot)
{
	int i=0;
	while((rv[i] >= 0)&&(rv[i] < 10)&&(i < 10))
	{
		if(s[i+1].rm_so != -1){
			strncpy(&subex[rv[i]][0], bot + s[1+i].rm_so, s[1+i].rm_eo - s[1+i].rm_so); 
            		subex[rv[i]][s[1+i].rm_eo - s[1+i].rm_so] = 0;
        	}
		i++;
	}
	if(rv) free(rv);
}
#endif

void tr_regular_clear()
{
	int i;

	for (i = 0; i < 10; ++i)
		subex[i][0] = 0;
}

int tr_regular_getsubs(char **vect, int maxcount)
{
	int i;

	if (maxcount > 10)
		maxcount = 10;

	for (i = maxcount; --i >= 0; )
		vect[i] = subex[i];

	return (maxcount);
}

/*
 * Match string to regular expression.
 * Return pointer after last matched character in string;
 * 0 on no match.
 *
 * The sub-exps in re are stored into subex[][].
 * These should be cleared by caller before use.
 */
char *tr_regular_execute(void *re, char *text)
{
	char *endp = 0;
#ifdef POSIX_REGCOMP
/* Urgh... This one is _very_ ugly, but had to be done :(
 * regcomp is executed here instead beeing executed in RTE
 * preprosessing phase. (tr_regcomp.c/tr_regular_compile)
 * This major modification is made because regcmp and regcomp
 * return values/structures are completely different.
 */
	int n;
	char *rv, *expr = (char *) strdup(re);
	regmatch_t subs[11], *sp = subs;
	regex_t *re_posix; 
	re_posix = (regex_t *)malloc(sizeof(regex_t));
	if(re_posix == NULL) return (NULL);
		
	rv = wrap_sysv_to_posix(expr);
	if(rv == NULL) return(NULL);

	if(regcomp(re_posix, expr, REG_EXTENDED) != 0){
		free(re_posix);
		return(NULL);
	}
	
	/* we have strduped re so lets free some unneeded mem */
	if(expr) free(expr);
	
	/* subs[0] is irrelevant to us containing concatenated substrings. */
	n = regexec(re_posix, text, 11, subs, 0);
	if(n == 0)
	{
		regular_loc1 = text + sp->rm_so;
		endp = text + sp->rm_eo;
		move_subs_to_subex(rv, subs, text);
	}
	if(re_posix) free(re_posix);
#else /* Quite different abroach with posix eh...*/
	endp = regex((char *)re, text,
		subex[0], subex[1], subex[2], subex[3], subex[4],
		subex[5], subex[6], subex[7], subex[8], subex[9]);
	regular_loc1 = __loc1;
#endif

	return (endp);
}

char *tr_regular_lastmatch()
{
	return (regular_loc1);
}

