/*========================================================================
		TradeXpress

		File:		filter.c
		Author:		Juha Nurmela (JN)
		Date:		Mon Oct 3 02:18:43 EET 1994

	Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines used to access fields in logentries.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: filter.c 55239 2019-11-19 13:50:31Z sodifrance $")
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
  4.02 11.03.13/GBY(CG) If the filter's value is empty, we use the TR_SQL_EMPTY 
					TR_SQL_EMPTY is a new keyword. We can use it to add an empty filter 
					Previously, when a empty filter was used, all lines in database where returned 
					Now, with this keywork, only empty lines for desired field are returned 
  4.02 17.06.13/YLI(CG) TX-2409 : pipe "|" in a filter works correctly
						if the nextvalue has only 1 character
  4.03 04.08.14/TCE(CG) TX-2600 : prise en compte du parametre UPDATE pour eviter qu'il soit considere comme un champs
  4.04 05.09.16/SCH(CG) TX-2896 : change field= filter shape by field<0 or 'field<>' by field>0
  10.04.2018 AAU (AMU) BG-198 : Added env_val and env_op to filter by environment
  06.06.2019/CDE TX-3136 : use ascii char 1 instead of number "0" in field comparaison to retrieve values starting by "("
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

#include "logsystem.sqlite.h"
#include "runtime/tr_prototypes.h"

extern double atof();

static void sqlite_topieces(char *namevalue, char **name, int *cmp, char **value)
{
	char *cp = namevalue;
	char *name_end;

	while (*cp && isspace(*cp)) ++cp;	/* to 1st word */

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
		while (*cp && isspace(*cp)) ++cp;
	}
	switch (*cp++)
	{
		case 0:
			/* No comparison, no value. */
			return;
		case '=':
			*cmp = OP_EQ;
			break;
		case '!':
			*cmp = OP_NEQ;
			if (*cp == '=')
				++cp;
			break;
		case '<':
			*cmp = OP_LT;
			if (*cp == '>')
			{
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
			if (*cp == '=')
			{
				*cmp = OP_GTE;
				++cp;
			}
			break;
	}
	/* Remember to cut the name away, junking the comparison string. */
	*name_end = 0;
	*value = cp;
	
	/* TX-2896 05/09/2016 SCH(CG) : change field= filter shape by field<0 or field<> by field >0 */
	if (**value == 0) {
		if (*cmp==OP_EQ) {
			*cmp = OP_LT;
		} else {
			*cmp = OP_GT;
		}
		/* TX-3136 use char with code 001 instead of char "0" */
		*value = strdup("\001");
	}
	/* End TX-2896 */
}

static int sqlite_atoorder(char *a)
{
	switch (*a)
	{
		case 'a':
		case 'A':
		case '1':
			return (1);
		case 'd':
		case 'D':
		case '-':
			return (-1);
		case 'r':
		case 'R':
		case '0':
		case 0:
			return (0);
		default:
			return (atoi(a));
	}
}

static LogFilter * sqlite_alloc_filter()
{
	LogFilter *lf;

	lf = sqlite_log_malloc(sizeof(*lf));
	memset(lf, 0, sizeof(*lf));

	/* 4.00/CD initialize separator */
	lf->separator = NULL;

	return (lf);
}

int sqlite_logfilter_type(char *cmp)
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

void sqlite_logfilter_insert(LogFilter **lfp, char *name, int cmp, char *value)
{
	FilterRecord *f;
	LogFilter *lf;
	char *filter_escape;

	if (name==NULL && value==NULL)
		return;
	
	filter_escape=value;
	while (*filter_escape=='|' && *filter_escape!=0) filter_escape++;
	if (*filter_escape==0)
		return;
	
	if (!*lfp)
		*lfp = sqlite_alloc_filter();
	lf = *lfp;

	if (lf->filter_count >= lf->filter_max)
	{
		int n, omax = lf->filter_max;

		lf->filter_max += 64;
		n = lf->filter_max * sizeof(*lf->filters);

		lf->filters = sqlite_log_realloc(lf->filters, n);
		memset(lf->filters + omax, 0, n - omax * sizeof(*lf->filters));
	}
	f = &lf->filters[lf->filter_count++];

	f->name = sqlite_log_strdup(name);
	f->op = cmp;
	f->value = sqlite_log_strdup(value);
}

int sqlite_logfilter_printables(LogFilter *lf, char *name, char **cmp, char **value)
{
	FilterRecord *f;

	if (!lf)
		return (0);

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

char * sqlite_logfilter_getseparator(LogFilter *lf)
{
	/* 4.00/CD */
	if (!lf)
		return (NULL);
	return (lf->separator);
}

char * sqlite_logfilter_getkey(LogFilter *lf)
{
	if (!lf)
		return (NULL);
	return (lf->sort_key);
}

int sqlite_logfilter_getorder(LogFilter *lf)
{
	if (!lf)
		return (0);
	return (lf->sort_order);
}

int sqlite_logfilter_getoffset(LogFilter *lf)
{
	if (!lf)
		return (0);
	return (lf->offset);
}

int sqlite_logfilter_getmaxcount(LogFilter *lf)
{
	if (!lf)
		return (0);
	return (lf->max_count);
}

/* Return textual representation of filter (just the filter)
 * Use next function to free up space. */
char ** sqlite_logfilter_textual(LogFilter *lf)
{
	FilterRecord *f;
	char **v;
	int i;

	if (!lf || !lf->filter_count)
		return (NULL);

	v = sqlite_log_malloc((lf->filter_count + 1) * sizeof(*v));
	i = 0;

	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		v[i++] = sqlite_log_str3dup(f->name, operator_strings[f->op], f->value);
	}
	v[i] = NULL;

	return (v);
}

/* Return textual representation of filter (just the filter)
 * Use next function to free up space. */
char ** sqlite_logfilter_textualparam(LogFilter *lf)
{
	FilterRecord *f;
	char **v;
	int i;

	if (!lf)
		return (NULL);

	/* 1 for each filters + 1 for each parameters + 1 for end */
	v = sqlite_log_malloc((lf->filter_count + 6) * sizeof(*v));
	i = 0;

	if (lf->filter_count)
	{
		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
		{
			v[i] = sqlite_log_malloc(1 /* p|f|'' */ + strlen(f->name) + strlen(operator_strings[f->op]) + strlen(f->value) + 1);
			strcpy(v[i], "f");
			strcat(v[i], f->name);
			strcat(v[i], operator_strings[f->op]);
			strcat(v[i], f->value);
			i++;
		}
	}

	if (lf->sort_key)
		v[i++] = sqlite_log_str3dup("SORTKEY", "=", lf->sort_key);

	if (lf->sort_order)
	{
		/* XXX use MAX_DIGIT or better alloc */
		v[i] = sqlite_log_malloc(sizeof("SORTORDER=") + 2);
		sprintf(v[i], "SORTORDER=%d", lf->sort_order);
		i++;
	}

	if (lf->max_count)
	{
		/* XXX use MAX_DIGIT or better alloc */
		v[i] = sqlite_log_malloc(sizeof("MAXCOUNT=") + 10);
		sprintf(v[i], "MAXCOUNT=%d", lf->max_count);
		i++;
	}

	if (lf->offset)
	{
		/* XXX use MAX_DIGIT or better alloc */
		v[i] = sqlite_log_malloc(sizeof("OFFSET=") + 10);
		sprintf(v[i], "OFFSET=%d", lf->offset);
		i++;
	}

	if (lf->separator)
		v[i++] = sqlite_log_str3dup("SEPARATOR", "=", lf->separator);

	v[i] = NULL;

	return (v);
}

void sqlite_logfilter_textual_free(char **v)
{
	int i;

	if (v)
	{
		for (i = 0; v[i]; ++i)
			free(v[i]);
		free(v);
	}
}

void sqlite_logfilter_setseparator(LogFilter **lfp, char *sep)
{
	/* 4.00/CD */
	if (!*lfp)
		*lfp = sqlite_alloc_filter();
	if (sep)
		sep = sqlite_log_strdup(sep); /* dup before free, in case they are same... */
	if ((*lfp)->separator)
		free((*lfp)->separator);

	(*lfp)->separator = sep;
}

void sqlite_logfilter_setkey(LogFilter **lfp, char *key)
{
	if (!*lfp)
		*lfp = sqlite_alloc_filter();
	if (key)
		key = sqlite_log_strdup(key); /* dup before free, in case they are same... */
	if ((*lfp)->sort_key)
		free((*lfp)->sort_key);

	(*lfp)->sort_key = key;
}

void sqlite_logfilter_setoffset(LogFilter **lfp, int offset)
{
	if (!*lfp)
		*lfp = sqlite_alloc_filter();
	if (offset > 0)
		(*lfp)->offset = offset;
	else
		(*lfp)->offset = 0;
}

void sqlite_logfilter_setorder(LogFilter **lfp, int order)
{
	/* 4.01/CD changed the < and > into <= and >= so we can now set the order in logview using -a and -d !! */
	if (!*lfp)
		*lfp = sqlite_alloc_filter();
	if (order <= -1)
		order = -1;
	if (order >= 1)
		order = 1;
	(*lfp)->sort_order = order;
}

void sqlite_logfilter_setmaxcount(LogFilter **lfp, int maxcount)
{
	if (!*lfp)
		*lfp = sqlite_alloc_filter();
	(*lfp)->max_count = maxcount;
}

void sqlite_logfilter_add(LogFilter **lfp, char *namevalue)
{
	char *name, *value;
	int cmp;

	sqlite_topieces(namevalue, &name, &cmp, &value);

	if (name && value)
		sqlite_logfilter_insert(lfp, name, cmp, value);
}

void sqlite_logfilter_setenv(LogFilter **lfp, int cmp, char *value)
{
	if (!*lfp)
		*lfp = sqlite_alloc_filter();
	
	if (value)
		value = sqlite_log_strdup(value); /* dup before free, in case they are same... */
	if ((*lfp)->env_val)
		free((*lfp)->env_val);

	(*lfp)->env_val = value;
	(*lfp)->env_op = cmp;
}



/* Format of input file:
 * FIELDNAME blanks comparison onespace VALUE */
int sqlite_logfilter_read(LogFilter **lfp, FILE *fp)
{
	char buf[1024];
	char *name, *value, *cp;
	int cmp;

	while (fp && tr_mbFgets(buf, sizeof(buf), fp)) {
		if (*buf == '\n' || *buf == '#')
			continue;
		if ((cp = strchr(buf, '\n')) != NULL)
			*cp = 0;

		sqlite_topieces(buf, &name, &cmp, &value);

		if (!strcmp(name, "SORTKEY"))
		{
			sqlite_logfilter_setkey(lfp, value);
			continue;
		}
		if (!strcmp(name, "SORTORDER"))
		{
			sqlite_logfilter_setorder(lfp, sqlite_atoorder(value));
			continue;
		}
		if (!strcmp(name, "MAXCOUNT"))
		{
			sqlite_logfilter_setmaxcount(lfp, atoi(value));
			continue;
		}
		if (!strcmp(name, "OFFSET"))
        {
			sqlite_logfilter_setoffset(lfp, atoi(value));
			continue;
		}
		/* 4.00/CD */
		if (!strcmp(name, "SEPARATOR"))
		{
			sqlite_logfilter_setseparator(lfp, value);
			continue;
		}
		/*TX-2600 prise en compte du parametre UPDATE pour eviter qu'il soit considere comme un champs*/
		if (!strcmp(name, "UPDATE"))
		{
			continue;
		}
		if(!strcmp(name,"ENV"))
		{
			sqlite_logfilter_setenv(lfp, cmp, value);
			continue;
		}
		/*END TX-2600*/
		sqlite_logfilter_insert(lfp, name, cmp, value);
	}
	return (0);
}

void sqlite_logfilter_addparam(LogFilter **lfp, PrintFormat **pfp, char *namevalue)
{
	char *name, *value;
	int cmp;

	sqlite_topieces(namevalue, &name, &cmp, &value);

	if (!strcmp(name, "SORTKEY"))
	{
		sqlite_logfilter_setkey(lfp, value);
		return;
	}
	if (!strcmp(name, "SORTORDER"))
	{
		sqlite_logfilter_setorder(lfp, sqlite_atoorder(value));
		return;
	}
	if (!strcmp(name, "MAXCOUNT"))
	{
		sqlite_logfilter_setmaxcount(lfp, atoi(value));
		return;
	}
	if (!strcmp(name, "OFFSET"))
	{
		sqlite_logfilter_setoffset(lfp, atoi(value));
		return;
	}
	/* 4.00/CD */
	if (!strcmp(name, "SEPARATOR"))
	{
		sqlite_logfilter_setseparator(lfp, value);
		return;
	}

	/*TX-2600 prise en compte du parametre UPDATE pour éviter qu'il soit considere comme un champs*/
	if (!strcmp(name, "UPDATE"))
	{
		return;
	}
	/*END TX-2600*/
	switch (*name)
	{
		default:
			sqlite_logfilter_insert(lfp, name, cmp, value);
			sqlite_printformat_insert(pfp, name, NULL);
			break;
		case 'f': sqlite_logfilter_insert(lfp, name + 1, cmp, value); break;
		case 'p': sqlite_printformat_insert(pfp, name + 1, NULL); break;
	}
}

int sqlite_logfilter_printformat_read(LogFilter **lfp, PrintFormat **pfp, FILE *fp)
{
	char buf[1024];
	char *cp;

	while (fp && tr_mbFgets(buf, sizeof(buf), fp))
	{
		if (*buf == '\n' || *buf == '#')
			continue;
		if ((cp = strchr(buf, '\n')) != NULL)
			*cp = 0;

		sqlite_logfilter_addparam(lfp, pfp, buf);
	}
	return (0);
}

int sqlite_logfilter_write(LogFilter *lf, FILE *fp)
{
	FilterRecord *f;

	if (!lf)
		return (0);

	if (lf->sort_key)
		tr_mbFprintf(fp, "SORTKEY=%s\n", lf->sort_key);
	if (lf->sort_order)
		tr_mbFprintf(fp, "SORTORDER=%d\n", lf->sort_order);
	if (lf->max_count)
		tr_mbFprintf(fp, "MAXCOUNT=%d\n", lf->max_count);
	if (lf->offset)
		tr_mbFprintf(fp, "OFFSET=%d\n", lf->offset);
	if (lf->separator) /* 4.00/CD */
		tr_mbFprintf(fp, "SEPARATOR=%s\n", lf->separator);

	if (ferror(fp))
		return (-1);

	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		tr_mbFprintf(fp, "%s%s%s\n", f->name, operator_strings[f->op], f->value);
		if (ferror(fp))
			return (-1);
	}
	return (0);
}

int sqlite_logfilter_printformat_write(LogFilter *lf, PrintFormat *pf, FILE *fp)
{
	FilterRecord *f;
	FormatRecord *r;

	if (lf)
	{
		if (lf->sort_key)
			tr_mbFprintf(fp, "SORTKEY=%s\n", lf->sort_key);
		if (lf->sort_order)
			tr_mbFprintf(fp, "SORTORDER=%d\n", lf->sort_order);
		if (lf->max_count)
			tr_mbFprintf(fp, "MAXCOUNT=%d\n", lf->max_count);
		if (lf->separator) /* 4.0/CD */
			tr_mbFprintf(fp, "SEPARATOR=%s\n", lf->separator);

		if (ferror(fp))
			return (-1);

		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
		{
			if (f->op == OP_NONE || !f->value)
				continue;

			tr_mbFprintf(fp, "f%s%s%s\n", f->name, operator_strings[f->op], f->value);

			if (ferror(fp))
				return (-1);
		}
	}
	if (pf)
	{
		for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
		{
			tr_mbFprintf(fp, "p%s\n", r->name);

			if (ferror(fp))
				return (-1);
		}
	}
	return (0);
}

/* Clear fields from filter,
 * but dont free the filter-array itself */
void sqlite_logfilter_clear(LogFilter *lf)
{
	FilterRecord *f;

	if (!lf)
		return;

	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		if (f->name)
			free(f->name);
		if (f->value)
			free(f->value);
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
				while (f->n_primaries--)
					free(p++->text);
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

void sqlite_logfilter_free(LogFilter *lf)
{
	if (!lf)
		return;

	sqlite_logfilter_clear(lf);

	if (lf->filters)
		free(lf->filters);

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

static int sqlite_wildcompare(register char *s/* string */, register char *w/* pattern */)
{
	while (*w)
	{
		switch (*w)
		{
		case '*':
			/* Star matches anything at all.
			 * If the pattern ends in here, it is a hit,
			 * otherwise we must recurse for the rest of string. */
			if (*++w == 0)
				return (0);

			while (*s)
			{
				if (sqlite_wildcompare(s++, w) == 0)
					return (0);
			}
			break;

		case '?':
			/* Any single character (except NUL of course). */
			if (*s == 0)
				goto out;

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
							if (w[1])
								w++;

							/* fallthru */
						default:
							if (*w == *s)
								in_set = 1;

							if (got_range)
							{
								got_range = 0;
								if (*s >= range_from
								 && *s <= *w)
									in_set = 1;
							}
							range_from = *w;
					}
				}
				/* Truncated set (no ]) or no hit ? */
				if (!in_set || *w == 0)
					goto out;

				w++;
				s++;
			}
			break;

		case '\\':
			/* Escaped character follows and has to literally match. */
			if (w[1])
				w++;

			/* fallthru */
		default:
			if (*w != *s)
				goto out;

			w++;
			s++;
		}
	}
	/* End of pattern, has to be end of string too. */
out:
	return (*s - *w);
}

static union FilterPrimary * sqlite_new_primary(FilterRecord *f)
{
	f->primaries = sqlite_log_realloc(f->primaries, (f->n_primaries + 1) * sizeof(*f->primaries));
	return (&f->primaries[f->n_primaries++]);
}
	
/* Compile the fields in the filter to point to corresponding
 * fields in the logsystem.
 * Unknown fields are ignored.
 * Do atois and atofs here. */
int sqlite_logsys_compilefilter(LogSystem *ls, LogFilter *lf)
{
	int nunk = 0;		/* number of unknown fields encountered */
	FilterRecord *f;

	char *value;
	char *nextvalue;
	union FilterPrimary *p;

	if (!lf)
		return (0);

	lf->logsys_compiled_to = ls;

	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
	{
		f->field = NULL;

		if (f->value == NULL)
			continue;

		f->field = sqlite_logsys_getfield(ls, f->name);
		if (!f->field)
		{
			++nunk;
			continue;
		}
		value = f->value;
		while (*value=='|') value++; /* skip useless pipes */

		if (*value)
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
				p = sqlite_new_primary(f);
				if (value == f->value && !nextvalue)
				{
					p->text = value;
				}
				else if (nextvalue)
				{
					*nextvalue = 0;
					p->text = sqlite_log_strdup(value);
					*nextvalue = '|';
				}
				else
				{
					p->text = sqlite_log_strdup(value);
				}
				break;
			case FIELD_TIME:
				p = sqlite_new_primary(f);
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
				p = sqlite_new_primary(f);
				p->integer = atoi(value);
				break;
			case FIELD_NUMBER:
				p = sqlite_new_primary(f);
				p->number = atof(value);
				break;
			case PSEUDOFIELD_OWNER:
				break;
			}
			/* Skip the '|' if one or plus was found. */
			if (nextvalue)
			{
				nextvalue++;
				while (*nextvalue=='|') nextvalue++;
			}
			/*YLI(GC) : 17/06/2013 - TX-2409 : pipe "|" in a filter works correctly */
		} while ((value = nextvalue) && strlen(nextvalue)>=1);
		
	}
	return (nunk);
}

/* Sorters for different types of fields.
 * Two for each type we support, forward and reverse. */
#define A &(*(LogEntry **)a)->record_buffer[(*(LogEntry **)a)->logsys->sorter_offset]
#define B &(*(LogEntry **)b)->record_buffer[(*(LogEntry **)b)->logsys->sorter_offset]
#define AL (const void *a,const void*b) 

static int sqlite_logfilter_textsort_forward    AL{return(strcmp((char*) A, (char *)       B));}
static int sqlite_logfilter_textsort_reverse    AL{return(strcmp((char*) B, (char *)       A));}
static int sqlite_logfilter_timesort_forward    AL{return(*(TimeStamp*)  A - *(TimeStamp*) B);}
static int sqlite_logfilter_timesort_reverse    AL{return(*(TimeStamp*)  B - *(TimeStamp*) A);}
static int sqlite_logfilter_numbersort_forward  AL{return(*(Number*)     A - *(Number*)    B);}
static int sqlite_logfilter_numbersort_reverse  AL{return(*(Number*)     B - *(Number*)    A);}
static int sqlite_logfilter_integersort_forward AL{return(*(Integer*)    A - *(Integer*)   B);}
static int sqlite_logfilter_integersort_reverse AL{return(*(Integer*)    B - *(Integer*)   A);}

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
int sqlite_logsys_sortentries(LogEntry **entries, int num_entries, LogFilter *lf)
{
	LogSystem *log;
	LogField *keyfield;
	int (*sorter)(const void*,const void *);

	if (num_entries <= 1
	 || lf == NULL
	 || lf->sort_order == 0
	 || lf->sort_key == NULL
	 || ((strcmp(lf->sort_key, "INDEX") == 0) && (lf->sort_order>0)))
		/* 4.01/CD Allow to sort in reverse INDEX order */
		goto skip_sort;

	{
		log = entries[0]->logsys;

		keyfield = sqlite_logsys_getfield(log, lf->sort_key);
		if (!keyfield)
			goto skip_sort;

		log->sorter_offset = keyfield->offset;
	}
	switch (keyfield->type)
	{
		default:
			/* Dont know how to sort with this kind of field. */
			goto skip_sort;
		case FIELD_TEXT:
			if (lf->sort_order > 0)
				sorter = sqlite_logfilter_textsort_forward;
			else
				sorter = sqlite_logfilter_textsort_reverse;
			break;
		case FIELD_TIME:
			if (lf->sort_order > 0)
				sorter = sqlite_logfilter_timesort_forward;
			else
				sorter = sqlite_logfilter_timesort_reverse;
			break;
		case FIELD_INDEX:
		case FIELD_INTEGER:
			if (lf->sort_order > 0)
				sorter = sqlite_logfilter_integersort_forward;
			else
				sorter = sqlite_logfilter_integersort_reverse;
			break;
		case FIELD_NUMBER:
			if (lf->sort_order > 0)
				sorter = sqlite_logfilter_numbersort_forward;
			else
				sorter = sqlite_logfilter_numbersort_reverse;
			break;
	}

	qsort(entries, num_entries, sizeof(*entries), sorter);

skip_sort:

	/* If there is a limit for the count, strip off from the end. */
	if (lf && lf->max_count > 0 && lf->max_count < num_entries)
	{
		int i;
		for (i = lf->max_count; i < num_entries; ++i)
			sqlite_logentry_free(entries[i]);
		num_entries = lf->max_count;
	}
	return num_entries;
}

