/*========================================================================
		E D I S E R V E R

		File:		filter.c
		Author:		Juha Nurmela (JN)
		Date:		Mon Oct 3 02:18:43 EET 1994

	Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines used to access fields in logentries.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: filter.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 31.03.95/JN	OP_NONE accepted to filters (passes thru anything).
  3.02 26.04.95/JN	index optimization to filtering.
  3.03 29.03.96/JN	"finds" are spending almost 20% in wildcompare.
                    Lifted code from tr_wildmat(libruntime)
  3.04 12.05.96/JN	Crash on sortentries in Sun.
  3.05 26.07.96/JN	| conjunctives/primaries for filter-records.
  3.06 06.08.96/JN	Missed zeroing of primaries when clearing filter.
  3.09 19.02.97/JN	Cant use owner for filtering, select bases instead.
  4.00 05.07.02/CD	Column separator for printing.
  4.01 12.07.02/CD	Clear separator in logfilter_clear
                    Allow to sort in reverse INDEX order 
			        Change in logfilter_setorder for the -d and -a option of logview
  4.02 17.06.13/YLI(CG) TX-2409 : pipe "|" in a filter works correctly
					if the nextvalue has only 1 character				
  4.03 23.12.13/YLI(CG) TX-2480 : Correction - Display pages with option -g 
						when maxcount is less than the number of entries displayed 				
  4.04 09.01.14/CGL(CG) TX-2495 Remplace LOGLEVEL_DEVEL (level 3) with LOGLEVEL_BIGDEVEL (level 4)	
  4.05 13.03.18/PGL 	BG-194  Référence à TX-2600, prise en compte du mot clé UPDATE afin qu'il ne soit plus considéré comme un champ de la base lors de la lecture/contruction du filtre.
  Jira TX-3123 24.09.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <ctype.h>

#define OPERATOR_STRINGS

#include "logsystem.h"
#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"
#include "runtime/tr_constants.h"
#include "runtime/tr_prototypes.h"

extern double atof();

static void old_legacy_topieces(char *namevalue, char **name, int *cmp, char **value)
{
	char *cp = namevalue;
	char *name_end;

	while (*cp && isspace(*cp)){
		++cp;	/* to 1st word */
	}

	*name = cp;
	*cmp = OP_NONE;
	*value = NULL;

	/* The fieldname can consist from
	 * letters, digits or _ */
	while ((*cp >= 'A' && *cp <= 'Z') || (*cp >= '0' && *cp <= '9') || (*cp >= 'a' && *cp <= 'z') || (*cp == '_'))
	{
		++cp;
		continue;
	}
	name_end = cp;

	if (*cp && isspace(*cp))
	{
		/* There is whitespace between
		 * fieldname and comparison. */
		*cp++ = 0;
		while (*cp && isspace(*cp)){ 
			++cp;
	}
	}
	switch (*cp++)
	{
		case 0:
			/* No comparison, no value. */
			return;
			break;
		case '=':
			*cmp = OP_EQ;
			break;
		case '!':
			*cmp = OP_NEQ;
			if (*cp == '='){
				++cp;
				}
			break;
		case '<':
			*cmp = OP_LT;
			if (*cp == '>'){
				*cmp = OP_NEQ;
				++cp;
			}
			else if (*cp == '=')
			{
				*cmp = OP_LTE;
				++cp;
			}
			break;
		case '>':
			*cmp = OP_GT;
			if (*cp == '='){
				*cmp = OP_GTE;
				++cp;
			}
			break;
		default :
			break;
	}
	/* Remember to cut the name away, junking the comparison string. */
	*name_end = 0;
	*value = cp;
}

static int old_legacy_atoorder(char *a)
{
	switch (*a)
	{
		case 'a':
		case 'A':
		case '1':
			return (1);
			break;
		case 'd':
		case 'D':
		case '-':
			return (-1);
			break;
		case 'r':
		case 'R':
		case '0':
		case 0:
			return (0);
			break;
		default:
			return (atoi(a));
			break;
	}
}

static LogFilter * old_legacy_alloc_filter(void)
{
	LogFilter *lf;

	lf = old_legacy_log_malloc(sizeof(*lf));
	memset(lf, 0, sizeof(*lf));

	/* 4.00/CD initialize separator */
	lf->separator = NULL;

	return (lf);
}

int old_legacy_logfilter_type(char *cmp)
{
	unsigned int i;

	for (i = 0; i < SIZEOF(operator_strings); ++i)
	{
		if (!strcmp(operator_strings[i], cmp))
		{
			return (i);
		}
	}
	return (OP_NONE);
}

void old_legacy_logfilter_insert(LogFilter **lfp, char *name, int cmp, char *value)
{
	FilterRecord *f;
	LogFilter *lf;

	/*CGL (CG) TX-2495 Remplace LOGLEVEL_DEVEL (level 3) with LOGLEVEL_BIGDEVEL (level 4)*/
	writeLog(LOGLEVEL_BIGDEVEL, "insert [%s %d %s] in old legacy filter", name, cmp, value);
	if (!*lfp){
		*lfp = old_legacy_alloc_filter();
		}
	lf = *lfp;

	if (lf->filter_count >= lf->filter_max)
	{
		int n, omax = lf->filter_max;
		
		writeLog(LOGLEVEL_BIGDEVEL, "too much rules, we need to add some space in old legacy filter");
	/*Fin CGL (CG)*/
		lf->filter_max += 64;
		n = lf->filter_max * sizeof(*lf->filters);

		lf->filters = old_legacy_log_realloc(lf->filters, n);
		memset(lf->filters + omax, 0, n - omax * sizeof(*lf->filters));
	}
	f = &lf->filters[lf->filter_count++];

	f->name = old_legacy_log_strdup(name);
	f->op = cmp;
	f->value = old_legacy_log_strdup(value);
}

int old_legacy_logfilter_printables(LogFilter *lf, char *name, char **cmp, char **value)
{
	FilterRecord *f;

	if (!lf){
		return (0);
		}

	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		if (!strcmp(name, f->name))
		{
			*cmp = operator_strings[f->op];
			*value = f->value;
			return (1);
		}
	}
	return (0);
}

char * old_legacy_logfilter_getseparator(LogFilter *lf)
{
	/* 4.00/CD */
	if (!lf){
		return (NULL);
	}
	return (lf->separator);
}

char * old_legacy_logfilter_getkey(LogFilter *lf)
{
	if (!lf){
		return (NULL);
		}
	return (lf->sort_key);
}

int old_legacy_logfilter_getorder(LogFilter *lf)
{
	if (!lf){
		return (0);
		}
	return (lf->sort_order);
}

int old_legacy_logfilter_getoffset(LogFilter *lf)
{
	if (!lf){
		return (0);
		}
	return (lf->offset);
}

int old_legacy_logfilter_getmaxcount(LogFilter *lf)
{
	if (!lf){
		return (0);
		}
	return (lf->max_count);
}

/* Return textual representation of filter (just the filter)
 * Use next function to free up space. */
char ** old_legacy_logfilter_textual(LogFilter *lf)
{
	FilterRecord *f;
	char **v;
	int i;

	if (!lf || !lf->filter_count){
		return (NULL);
		}

	v = old_legacy_log_malloc((lf->filter_count + 1) * sizeof(*v));
	i = 0;

	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		v[i++] = old_legacy_log_str3dup(f->name, operator_strings[f->op], f->value);
	}
	v[i] = NULL;

	return (v);
}

/* Return textual representation of filter (just the filter)
 * Use next function to free up space. */
char ** old_legacy_logfilter_textualparam(LogFilter *lf)
{
	FilterRecord *f;
	char **v;
	int i;

	if (!lf){
		return (NULL);
		}

	/* 1 for each filters + 1 for each parameters + 1 for end */
	v = old_legacy_log_malloc((lf->filter_count + 6) * sizeof(*v));
	i = 0;

	if (lf->filter_count)
	{
		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
		{
			v[i] = old_legacy_log_malloc(1 /* p|f|'' */ + strlen(f->name) + strlen(operator_strings[f->op]) + strlen(f->value) + 1);
			strcpy(v[i], "f");
			strcat(v[i], f->name);
			strcat(v[i], operator_strings[f->op]);
			strcat(v[i], f->value);
			i++;
		}
	}

	if (lf->sort_key){
		v[i++] = old_legacy_log_str3dup("SORTKEY", "=", lf->sort_key);
		}

	if (lf->sort_order)
	{
		/* XXX use MAX_DIGIT or better alloc */
		v[i] = old_legacy_log_malloc(sizeof("SORTORDER=") + 2);
		sprintf(v[i], "SORTORDER=%d", lf->sort_order);
		i++;
	}

	if (lf->max_count)
	{
		/* XXX use MAX_DIGIT or better alloc */
		v[i] = old_legacy_log_malloc(sizeof("MAXCOUNT=") + 10);
		sprintf(v[i], "MAXCOUNT=%d", lf->max_count);
		i++;
	}

	if (lf->offset)
	{
		/* XXX use MAX_DIGIT or better alloc */
		v[i] = old_legacy_log_malloc(sizeof("OFFSET=") + 10);
		sprintf(v[i], "OFFSET=%d", lf->offset);
		i++;
	}

	if (lf->separator){
		v[i++] = old_legacy_log_str3dup("SEPARATOR", "=", lf->separator);
		}
	v[i] = NULL;

	return (v);
}

void old_legacy_logfilter_textual_free(char **v)
{
	int i;

	if (v)
	{
		for (i = 0; v[i]; ++i){
			free(v[i]);
			}
		free(v);
	}
}

void old_legacy_logfilter_setseparator(LogFilter **lfp, char *sep)
{
	/* 4.00/CD */
	writeLog(LOGLEVEL_BIGDEVEL, "Set separator %s in old legacy filter", sep);
	if (!*lfp){
		*lfp = old_legacy_alloc_filter();
		}
	if (sep){
		sep = old_legacy_log_strdup(sep); /* dup before free, in case they are same... */
		}
	if ((*lfp)->separator){
		free((*lfp)->separator);
		}
	(*lfp)->separator = sep;
}

void old_legacy_logfilter_setkey(LogFilter **lfp, char *key)
{
	writeLog(LOGLEVEL_BIGDEVEL, "set Key %s in old legacy filter", key);
	if (!*lfp){
		*lfp = old_legacy_alloc_filter();
		}
	if (key){
		key = old_legacy_log_strdup(key); /* dup before free, in case they are same... */
		}
	if ((*lfp)->sort_key){
		free((*lfp)->sort_key);
		}
	(*lfp)->sort_key = key;
}

void old_legacy_logfilter_setoffset(LogFilter **lfp, int offset) {
	writeLog(LOGLEVEL_BIGDEVEL, "set offset %d in old legacy filter", offset);
	
	if (!*lfp) {
		writeLog(LOGLEVEL_BIGDEVEL, "Filter not exist, we create it");
		*lfp = old_legacy_alloc_filter();
	}
	if (offset > 0){
		(*lfp)->offset = offset;
		}
	else{
		(*lfp)->offset = 0;
}
}

void old_legacy_logfilter_setorder(LogFilter **lfp, int order)
{
	/* 4.01/CD changed the < and > into <= and >= so we can now set the order in logview using -a and -d !! */
	writeLog(LOGLEVEL_BIGDEVEL, "set order %d in old legacy filter", order);
	if (!*lfp){
		*lfp = old_legacy_alloc_filter();
		}
	if (order <= -1){
		order = -1;
		}
	if (order >= 1){
		order = 1;
		}
	(*lfp)->sort_order = order;
}

void old_legacy_logfilter_setmaxcount(LogFilter **lfp, int maxcount)
{
	writeLog(LOGLEVEL_BIGDEVEL, "set maxcount %d in old legacy filter", maxcount);
	if (!*lfp){
		*lfp = old_legacy_alloc_filter();
		}
	(*lfp)->max_count = maxcount;
}

void old_legacy_logfilter_add(LogFilter **lfp, char *namevalue)
{
	char *name, *value;
	int cmp;

	writeLog(LOGLEVEL_BIGDEVEL, "add [%s] in old legacy filter", namevalue);
	old_legacy_topieces(namevalue, &name, &cmp, &value);

	if (name && value){
		old_legacy_logfilter_insert(lfp, name, cmp, value);
}
}

/* Format of input file:
 * FIELDNAME blanks comparison onespace VALUE */
int old_legacy_logfilter_read(LogFilter **lfp, FILE *fp)
{
	char buf[1024];
	char *name, *value, *cp;
	int cmp;

	writeLog(LOGLEVEL_BIGDEVEL, "Begin old_legacy_logfilter_read");
	while (fp && tr_mbFgets(buf, sizeof(buf), fp)) {
		if (*buf == '\n' || *buf == '#'){
			continue;
			}
		if ((cp = strchr(buf, '\n')) != NULL){
			*cp = 0;
			}
		old_legacy_topieces(buf, &name, &cmp, &value);

		if (!strcmp(name, "SORTKEY"))
		{
			old_legacy_logfilter_setkey(lfp, value);
			continue;
		}
		if (!strcmp(name, "SORTORDER"))
		{
			old_legacy_logfilter_setorder(lfp, old_legacy_atoorder(value));
			continue;
		}
		if (!strcmp(name, "MAXCOUNT"))
		{
			old_legacy_logfilter_setmaxcount(lfp, atoi(value));
			continue;
		}
		if (!strcmp(name, "OFFSET"))
        {
			old_legacy_logfilter_setoffset(lfp, atoi(value));
			continue;
		}
		/* 4.00/CD */
		if (!strcmp(name, "SEPARATOR"))
		{
			old_legacy_logfilter_setseparator(lfp, value);
			continue;
		}
		/* BG-194 - Debut */
		if (!strcmp(name, "UPDATE"))
		{
			continue;
		}
		/* BG-194 - fin */
		old_legacy_logfilter_insert(lfp, name, cmp, value);
	}
	return (0);
}

void old_legacy_logfilter_addparam(LogFilter **lfp, PrintFormat **pfp, char *namevalue)
{
	char *name, *value;
	int cmp;

	old_legacy_topieces(namevalue, &name, &cmp, &value);

	if (!strcmp(name, "SORTKEY"))
	{
		old_legacy_logfilter_setkey(lfp, value);
		return;
	}
	if (!strcmp(name, "SORTORDER"))
	{
		old_legacy_logfilter_setorder(lfp, old_legacy_atoorder(value));
		return;
	}
	if (!strcmp(name, "MAXCOUNT"))
	{
		old_legacy_logfilter_setmaxcount(lfp, atoi(value));
		return;
	}
	if (!strcmp(name, "OFFSET"))
	{
		old_legacy_logfilter_setoffset(lfp, atoi(value));
		return;
	}
	/* 4.00/CD */
	if (!strcmp(name, "SEPARATOR"))
	{
		old_legacy_logfilter_setseparator(lfp, value);
		return;
	}
	/* BG-194 - Debut */
	if (!strcmp(name, "UPDATE"))
	{
		return;
	}
	/* BG-194 - fin */
	switch (*name)
	{
		default:
			old_legacy_logfilter_insert(lfp, name, cmp, value);
			old_legacy_printformat_insert(pfp, name, NULL);
			break;
		case 'f': old_legacy_logfilter_insert(lfp, name + 1, cmp, value); break;
		case 'p': old_legacy_printformat_insert(pfp, name + 1, NULL); break;
	}
}

int old_legacy_logfilter_printformat_read(LogFilter **lfp, PrintFormat **pfp, FILE *fp)
{
	char buf[1024];
	char *cp;

	while (fp && tr_mbFgets(buf, sizeof(buf), fp))
	{
		if (*buf == '\n' || *buf == '#'){
			continue;
		}
		if ((cp = strchr(buf, '\n')) != NULL){
			*cp = 0;
		}

		old_legacy_logfilter_addparam(lfp, pfp, buf);
	}
	return (0);
}

int old_legacy_logfilter_write(LogFilter *lf, FILE *fp)
{
	FilterRecord *f;

	if (!lf){
		return (0);
		}
	if (lf->sort_key){
		tr_mbFprintf(fp, "SORTKEY=%s\n", lf->sort_key);
		}
	if (lf->sort_order){
		tr_mbFprintf(fp, "SORTORDER=%d\n", lf->sort_order);
		}
	if (lf->max_count){
		tr_mbFprintf(fp, "MAXCOUNT=%d\n", lf->max_count);
		}
	if (lf->offset){
		tr_mbFprintf(fp, "OFFSET=%d\n", lf->offset);
		}
	if (lf->separator){ /* 4.00/CD */
		tr_mbFprintf(fp, "SEPARATOR=%s\n", lf->separator);
		}
	if (ferror(fp)){
		return (-1);
		}
	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		tr_mbFprintf(fp, "%s%s%s\n", f->name, operator_strings[f->op], f->value);
		if (ferror(fp)){
			return (-1);
		}
	}
	return (0);
}

int old_legacy_logfilter_printformat_write(LogFilter *lf, PrintFormat *pf, FILE *fp)
{
	FilterRecord *f;
	FormatRecord *r;

	if (lf)
	{
		if (lf->sort_key){
			tr_mbFprintf(fp, "SORTKEY=%s\n", lf->sort_key);
			}
		if (lf->sort_order){
			tr_mbFprintf(fp, "SORTORDER=%d\n", lf->sort_order);
			}
		if (lf->max_count){
			tr_mbFprintf(fp, "MAXCOUNT=%d\n", lf->max_count);
			}
		if (lf->separator){ /* 4.0/CD */
			tr_mbFprintf(fp, "SEPARATOR=%s\n", lf->separator);
			}
		if (ferror(fp)){
			return (-1);
			}
		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
		{
			if (f->op == OP_NONE || !f->value){
				continue;
				}
			tr_mbFprintf(fp, "f%s%s%s\n", f->name, operator_strings[f->op], f->value);
			if (ferror(fp)){
				return (-1);
			}
	}
	}
	if (pf)
	{
		for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
		{
			tr_mbFprintf(fp, "p%s\n", r->name);

			if (ferror(fp)){
				return (-1);
			}
		}
	}
	return (0);
}

/* Clear fields from filter,
 * but dont free the filter-array itself */
void old_legacy_logfilter_clear(LogFilter *lf)
{
	FilterRecord *f;

	if (!lf){
		return;
		}
	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		if (f->name){
			free(f->name);
			}
		if (f->value){
			free(f->value);
			}
		if (f->primaries)
		{
			/*
			 * exhausting check to catch
			 * textual filters with just one conjunctive
			 * in the record. 
			 */
			union FilterPrimary *p = f->primaries;

			if (f->n_primaries != 1 && f->field && f->field->type == FIELD_TEXT)
			{
				while (f->n_primaries--){
					free(p++->text);
			}
			}
			free(f->primaries);
			f->primaries = NULL;
		}
		f->n_primaries = 0;
	}
	if (lf->sort_key)
	{
		free(lf->sort_key);
		lf->sort_key = NULL;
	}
	if (lf->separator)
	{
		/* 4.01/CD */
		free(lf->separator);
		lf->separator = NULL;
	}

	lf->sort_order = 0;
	lf->max_count = 0;
	lf->filter_count = 0;
}

void old_legacy_logfilter_free(LogFilter *lf)
{
	if (!lf){
		return;
		}
	old_legacy_logfilter_clear(lf);

	if (lf->filters){
		free(lf->filters);
		}
	free(lf);
}

/*
 * Does the string in txt match with pattern in pat.
 *
 * Supported expressions:
 *	*	Anything
 *	?	Single character
 *	[abc]	Single character a, b or c
 *	[a-z]	Characters from a to z, inclusive.
 *	\x	escape any special meaning from x
 *
 * 0        a match
 * nonzero  no match, tries to imitate strcmp()
 */

static int old_legacy_wildcompare(register char *s/* string */, register char *w/* pattern */)
{
	while (*w)
	{
		switch (*w)
		{
		case '*':
			/* Star matches anything at all.
			 * If the pattern ends in here, it is a hit,
			 * otherwise we must recurse for the rest of string. */
			if (*++w == 0){
				return (0);
				}
			while (*s)
			{
				if (old_legacy_wildcompare(s++, w) == 0){
					return (0);
			}
			}
			break;

		case '?':
			/* Any single character (except NUL of course). */
			if (*s == 0){
				goto out;
			}
			w++;
			s++;
			break;

		case '[':
			/* Set of characters, ending in ']'. */
			{
				int range_from = 0; /* start of range */
				int got_range = 0;  /* no range seen so far */
				int in_set = 0;     /* assume not in set */

				while (*++w != 0 && *w != ']')
				{
					switch (*w)
					{
						case '-':
							got_range = 1;
							break;

						case '\\':
							if (w[1]){
								w++;
								}
							break;
							/* fallthru */
						default:
							if (*w == *s){
								in_set = 1;
								}
							if (got_range){
								got_range = 0;
								if (*s >= range_from
								 && *s <= *w){
									in_set = 1;
							}
							}
							range_from = *w;
							break;
					}
				}
				/* Truncated set (no ]) or no hit ? */
				if (!in_set || *w == 0){
					goto out;
					}
				w++;
				s++;
			}
			break;

		case '\\':
			/* Escaped character follows and has to literally match. */
			if (w[1]){
				w++;
				}
			break;
			/* fallthru */
		default:
			if (*w != *s){
				goto out;
				}
			w++;
			s++;
			break;
		}
	}
	/* End of pattern, has to be end of string too. */
out:
	return (*s - *w);
}

/* Comparisons for different types of fields. */
static int old_legacy_logfilter_textcomp(char *txt, int op, union FilterPrimary *p)
{
	int i = old_legacy_wildcompare(txt, p->text);

	switch (op)
	{
		case OP_NONE:return (1); break;
		case OP_EQ:  return (i == 0); break;
		case OP_NEQ: return (i != 0); break;
		case OP_LT:  return (i <  0); break;
		case OP_LTE: return (i <= 0); break;
		case OP_GTE: return (i >= 0); break;
		case OP_GT:  return (i >  0); break;
		default:     return (0); break;
	}
}

static int old_legacy_logfilter_timecomp(TimeStamp *stamp, int op, union FilterPrimary *p)
{
	switch (op)
	{
		case OP_NONE:return (1);
		case OP_EQ:  return (*stamp >= p->times[0] && *stamp <  p->times[1]); break;
		case OP_NEQ: return (*stamp <  p->times[0] || *stamp >= p->times[1]); break;
		case OP_LT:  return (*stamp <  p->times[0]); break;
		case OP_LTE: return (*stamp <= p->times[1]); break;
		case OP_GTE: return (*stamp >= p->times[0]); break;
		case OP_GT:  return (*stamp >= p->times[1]); break;
		default:     return (0); break;
	}
}

static int old_legacy_logfilter_numbercomp(Number *n, int op, union FilterPrimary *p)
{
	switch (op)
	{
		case OP_NONE:return (1); break;
		case OP_EQ:  return (*n == p->number); break;
		case OP_NEQ: return (*n != p->number); break;
		case OP_LT:  return (*n <  p->number); break;
		case OP_LTE: return (*n <= p->number); break;
		case OP_GTE: return (*n >= p->number); break;
		case OP_GT:  return (*n >  p->number); break;
		default:     return (0); break;
	}
}

static int old_legacy_logfilter_integercomp(Integer *i, int op, union FilterPrimary *p)
{
	switch (op)
	{
		case OP_NONE:return (1); break;
		case OP_EQ:  return (*i == p->integer); break;
		case OP_NEQ: return (*i != p->integer); break;
		case OP_LT:  return (*i <  p->integer); break;
		case OP_LTE: return (*i <= p->integer); break;
		case OP_GTE: return (*i >= p->integer); break;
		case OP_GT:  return (*i >  p->integer); break;
		default:     return (0); break;
	}
}

static int old_legacy_logfilter_unknowncomp(void)
{
	return 1;
}

static union FilterPrimary * old_legacy_new_primary(FilterRecord *f)
{
	f->primaries = old_legacy_log_realloc(f->primaries, (f->n_primaries + 1) * sizeof(*f->primaries));
	return (&f->primaries[f->n_primaries++]);
}
	
/* Compile the fields in the filter to point to corresponding
 * fields in the logsystem.
 * Unknown fields are ignored.
 * Do atois and atofs here. */
int old_legacy_logsys_compilefilter(LogSystem *ls, LogFilter *lf)
{
	int nunk = 0;		/* number of unknown fields encountered */
	FilterRecord *f;

	char *value;
	char *nextvalue;
	union FilterPrimary *p;

	if (!lf){
		return (0);
		}
	lf->logsys_compiled_to = ls;

	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		f->field = NULL;
		f->func = old_legacy_logfilter_unknowncomp;

		if (f->value == NULL){
			continue;
			}
		f->field = old_legacy_logsys_getfield(ls, f->name);
		if (!f->field)
		{
			++nunk;
			continue;
		}
		value = f->value;
		while (*value=='|'){ value++; }/* skip useless pipes */

		do
		{
			/* See if there are multiple primaries in this record. */
			nextvalue = strchr(value, '|');

			switch (f->field->type)
			{
			default:
				nextvalue = NULL; /* break to next filter */
				break;

			case FIELD_TEXT:
				/* Most often there is only one conjunctive per record,
				 * must assign the same data then. */
				f->func = old_legacy_logfilter_textcomp;
				p = old_legacy_new_primary(f);
				if (value == f->value && !nextvalue)
				{
					p->text = value;
				}
				else if (nextvalue)
				{
					*nextvalue = 0;
					p->text = old_legacy_log_strdup(value);
					*nextvalue = '|';
				}
				else
				{
					p->text = old_legacy_log_strdup(value);
				}
				break;
			case FIELD_TIME:
				f->func = old_legacy_logfilter_timecomp;
				p = old_legacy_new_primary(f);
				if (nextvalue)
				{
					*nextvalue = 0;
					tr_parsetime(value, (time_t *) p->times);
					*nextvalue = '|';
				}
				else
				{
					tr_parsetime(value, (time_t *) p->times);
				}
				break;
			case FIELD_INDEX:
			case FIELD_INTEGER:
				f->func = old_legacy_logfilter_integercomp;
				p = old_legacy_new_primary(f);
				p->integer = atoi(value);
				break;
			case FIELD_NUMBER:
				f->func = old_legacy_logfilter_numbercomp;
				p = old_legacy_new_primary(f);
				p->number = atof(value);
				break;
			case PSEUDOFIELD_OWNER:
				break;
			}
			/* Skip the '|' if one or plus was found. */
			if (nextvalue)
			{
				nextvalue++;
				while (*nextvalue=='|'){
					nextvalue++;
			}
			}
		/*YLI(GC) : 17/06/2013 - TX-2409 : pipe "|" in a filter works correctly */
		} while ((value = nextvalue) && strlen(nextvalue)>=1);
	}
	return (nunk);
}

int old_legacy_logindex_passesfilter(LogIndex idx, LogFilter *lf)
{
	FilterRecord *f;
	union FilterPrimary *p;
	LogIndex filter_idx;

	if (!lf){
		return (1);
		}
	idx /= LS_GENERATION_MOD;

	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		if (f->field == NULL || f->field->type != FIELD_INDEX){
			continue;
			}
		for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p)
		{
			filter_idx = p->integer / LS_GENERATION_MOD;
			switch (f->op)
			{
				case OP_NONE:break;
				case OP_EQ:  if (idx == filter_idx) {goto match;}break;
				case OP_NEQ: if (idx != filter_idx) {goto match;}break;
				case OP_LT:  if (idx <  filter_idx) {goto match;} break;
				case OP_LTE: if (idx <= filter_idx) {goto match;} break;
				case OP_GTE: if (idx >= filter_idx) {goto match;} break;
				case OP_GT:  if (idx >  filter_idx) {goto match;} break;
				default:     return (0); break;
			}
		}
		return (0);
	match:
		;
	}
	return (1);
}

int old_legacy_logentry_passesfilter(LogEntry *entry, LogFilter *lf)
{
	FilterRecord *f;
	union FilterPrimary *p;
	void *value;

	if (!lf){
		return (1);
		}
	if (lf->logsys_compiled_to != entry->logsys){
		old_legacy_logsys_compilefilter(entry->logsys, lf);
		}
	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{

		if (f->field == NULL || f->field->type == PSEUDOFIELD_OWNER){
			continue;
			}
		value = &entry->record_buffer[f->field->offset];

		for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p)
		{
			if ((*f->func)(value, f->op, p))
			{
				goto match;
			}
		}
		return (0);
		match:
			;
	}
	return (1);
}

/* Sorters for different types of fields.
 * Two for each type we support, forward and reverse. */
#define A &(*(LogEntry **)a)->record_buffer[(*(LogEntry **)a)->logsys->sorter_offset]
#define B &(*(LogEntry **)b)->record_buffer[(*(LogEntry **)b)->logsys->sorter_offset]
#define AL (const void *a,const void*b) 

static int old_legacy_logfilter_textsort_forward    AL{return(strcmp((char*) A, (char *)       B));}
static int old_legacy_logfilter_textsort_reverse    AL{return(strcmp((char*) B, (char *)       A));}
static int old_legacy_logfilter_timesort_forward    AL{return(*(TimeStamp*)  A - *(TimeStamp*) B);}
static int old_legacy_logfilter_timesort_reverse    AL{return(*(TimeStamp*)  B - *(TimeStamp*) A);}
static int old_legacy_logfilter_numbersort_forward  AL{return(*(Number*)     A - *(Number*)    B);}
static int old_legacy_logfilter_numbersort_reverse  AL{return(*(Number*)     B - *(Number*)    A);}
static int old_legacy_logfilter_integersort_forward AL{return(*(Integer*)    A - *(Integer*)   B);}
static int old_legacy_logfilter_integersort_reverse AL{return(*(Integer*)    B - *(Integer*)   A);}

#undef A
#undef B
#undef AL

/* Sort log-entries in array with filter.
 *
 * It there is nothing to sort, or nothing to sort with,
 * we are done early.
 * As the entries are read in order,
 * they are already sorted with index.
 *
 * Excess (the MAXCOUNT) entries are dropped from end. */
int old_legacy_logsys_sortentries(LogEntry **entries, int num_entries, LogFilter *lf)
{
	LogSystem *log;
	LogField *keyfield;
	int (*sorter)(const void*,const void *);

	/* needed to paginate */
	int i, maxcount, offset;

	if (num_entries <= 1
	 || lf == NULL
	 || lf->sort_order == 0
	 || lf->sort_key == NULL
	 || ((strcmp(lf->sort_key, "INDEX") == 0) && (lf->sort_order>0)))
		/* 4.01/CD Allow to sort in reverse INDEX order */
		goto skip_sort;

	{
		log = entries[0]->logsys;

		keyfield = old_legacy_logsys_getfield(log, lf->sort_key);
		if (!keyfield){
			goto skip_sort;
			}
		log->sorter_offset = keyfield->offset;
	}
	switch (keyfield->type)
	{
		default:
			/* Dont know how to sort with this kind of field. */
			goto skip_sort;
		case FIELD_TEXT:
			if (lf->sort_order > 0){
				sorter = old_legacy_logfilter_textsort_forward;
				}
			else{
				sorter = old_legacy_logfilter_textsort_reverse;
				}
			break;
		case FIELD_TIME:
			if (lf->sort_order > 0){
				sorter = old_legacy_logfilter_timesort_forward;
				}
			else{
				sorter = old_legacy_logfilter_timesort_reverse;
				}
			break;
		case FIELD_INDEX:
		case FIELD_INTEGER:
			if (lf->sort_order > 0){
				sorter = old_legacy_logfilter_integersort_forward;
				}
			else{
				sorter = old_legacy_logfilter_integersort_reverse;
				}
			break;
		case FIELD_NUMBER:
			if (lf->sort_order > 0){
				sorter = old_legacy_logfilter_numbersort_forward;
				}
			else{
				sorter = old_legacy_logfilter_numbersort_reverse;
				}
			break;
	}

	qsort(entries, num_entries, sizeof(*entries), sorter);

skip_sort:

	if (lf)
	{
	
		maxcount = num_entries;
		if (lf->max_count > 0){
			maxcount = lf->max_count;
			}
		offset = 0;
		if (lf->offset > 0){
			offset = lf->offset;
			}
		if (num_entries < offset)
		{
			offset = 0;
			maxcount = 0;
		}
		else
		{
			if (offset + maxcount > num_entries)
			{
				maxcount = num_entries - offset;
			}
		}
		
		/* clean unneeded entries */
		for (i = 0; i < offset - 1; i++){
			old_legacy_logentry_free(entries[i]);
			}
		for (i = offset + maxcount; i < num_entries; i++){
			old_legacy_logentry_free(entries[i]);
			}
		/* place good entries at start */
		if (offset > 0){
			for (i = 0; i < maxcount; i++){
				entries[i] = entries[i+offset];
				}
		num_entries = maxcount;
	}
		/*YLI(CG) 23.12.2013 when offset is 0 and the number of the sorting entries is greater than maxcount ,its value is maxcount*/
		if(offset == 0 && num_entries > maxcount){
			num_entries = maxcount; 
	}
		/*YLI(CG) End*/
	}
	return num_entries;
}

