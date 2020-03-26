#include "conf/local_config.h"
/*========================================================================
        E D I S E R V E R

        File:		operate.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines used to access fields in logentries.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: operate.c 55239 2019-11-19 13:50:31Z sodifrance $";
static char *uname = UNAME;
extern char *liblogsystem_version;
static char **lib = &liblogsystem_version;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  Jira TX-3123 24.09.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>

#define OPERATOR_STRINGS

#include "logsystem.sqlite.h"
#include "runtime/tr_prototypes.h"


extern double atof();

void sqlite_log_strsubrm(char *txt, char *sub);

/*
 * Operator can be
 *	=	SET
 *	:=	SET (preferred way)
 *	++	INC
 *	--	DEC
 *	+=	ADD
 *	-=	SUB
 */
static void sqlite_topieces(char *namevalue, char **name, int *op/* operator enum */, char **value)
{
	char *cp = namevalue;
	char *name_end;

	while (*cp && isspace(*cp)) ++cp;	/* to 1st word */

	*name = cp;
	*op = OP_NONE;
	*value = "";

	/* The fieldname can consist from
	 * letters, digits or _ */
	while ((*cp >= 'A' && *cp <= 'Z')
	    || (*cp >= '0' && *cp <= '9')
	    || (*cp >= 'a' && *cp <= 'z')
	    || (*cp == '_'))
    {
		++cp;
		continue;
	}
	name_end = cp;

	if (*cp && isspace(*cp))
    {
		/* There is whitespace between fieldname and operator. */
		*cp++ = 0;
		while (*cp && isspace(*cp)) ++cp;
	}
	switch (*cp++)
    {
	case 0:
		/* No operator, no value. */
		return;

	case '=':
		*op = OP_SET;
		break;
	case ':':
		*op = OP_SET;
		if (*cp == '=')
			++cp;
		break;
	case '-':
		*op = OP_DEC;
		if (*cp == '-')
        {
			++cp;
		}
        else if (*cp == '=')
        {
			++cp;
			*op = OP_SUB;
		}
		break;
	case '+':
		*op = OP_INC;
		if (*cp == '+')
        {
			++cp;
		}
        else if (*cp == '=')
        {
			++cp;
			*op = OP_ADD;
		}
		break;
	}
	/* Remember to cut the name away, junking the operator string. */
	*name_end = 0;
	*value = cp;
}

void sqlite_logoperator_insert(LogOperator **lop, char *name, int op, char *value)
{
	int n;
	OperatorRecord *r;
	LogOperator *lo = *lop;

	if (!lo)
    {
		lo = sqlite_log_malloc(sizeof(*lo));
		memset(lo, 0, sizeof(*lo));
		*lop = lo;
	}
	if (lo->operator_count >= lo->operator_max)
    {
		lo->operator_max += 64;
		n = lo->operator_max * sizeof(*lo->operators);

		lo->operators = sqlite_log_realloc(lo->operators, n);
	}
	r = &lo->operators[lo->operator_count++];

	r->name = sqlite_log_strdup(name);
	r->op = op;
	r->value = sqlite_log_strdup(value);
}

void sqlite_logoperator_add(LogOperator **lop, char *namevalue)
{
	char *name, *value;
	int op;

	sqlite_topieces(namevalue, &name, &op, &value);
	sqlite_logoperator_insert(lop, name, op, value);
}

int sqlite_logoperator_read(LogOperator **lop, FILE *fp)
{
	char buf[1024];
	char *name, *value, *cp;
	int op;

	if (!lop || !fp)
		return (0);

	while (fp && tr_mbFgets(buf, sizeof(buf), fp))
    {
		if (*buf == '\n' || *buf == '#')
			continue;
		if ((cp = strchr(buf, '\n')) != NULL)
			*cp = 0;

		sqlite_topieces(buf, &name, &op, &value);
		sqlite_logoperator_insert(lop, name, op, value);
	}
	return (0);
}

int sqlite_logoperator_write(FILE *fp, LogOperator *lo)
{
	OperatorRecord *r;

	if (!lo || !fp)
		return (0);

	for (r = lo->operators; r < &lo->operators[lo->operator_count]; ++r)
    {
		tr_mbFprintf(fp, "%s%s%s\n", r->name, operator_strings[r->op], r->value);

		if (ferror(fp))
			return (-1);
	}
	return (0);
}

/* Clear fields from operator, but dont free the operator-array itself */
void sqlite_logoperator_clear(LogOperator *lo)
{
	OperatorRecord *r;

	if (!lo)
		return;

	for (r = lo->operators; r < &lo->operators[lo->operator_count]; ++r)
    {
		if (r->name)
			free(r->name);
		if (r->value)
			free(r->value);
	}
	lo->operator_count = 0;
}

void sqlite_logoperator_free(LogOperator *lo)
{
	if (!lo)
		return ;

	sqlite_logoperator_clear(lo);

	if (lo->operators)
		free(lo->operators);

	free(lo);
}

/*
 * Operators for different types of fields.
 *
 * text fields can be set (:=) to a value,
 * the value can be appended (+=),
 * or every occurrence of value can be removed (-=).
 */
int sqlite_logoperator_text(char *txt, OperatorRecord *r)
{
	LogField *field;

	if (!txt || !r)
		return 0;
	field = r->field;
	switch (r->op) 
    {
	case OP_SET: sqlite_log_strncpy(txt, r->compiled.text, field->size); break;
	case OP_ADD: sqlite_log_strncat(txt, r->compiled.text, field->size); break;
	case OP_SUB: sqlite_log_strsubrm(txt, r->compiled.text); break;
	}
    return 0;
}

int sqlite_logoperator_time(TimeStamp *stamp, OperatorRecord *r)
{
	if (!stamp || !r)
		return 0;
	switch (r->op)
    {
	case OP_INC: *stamp += 1; break;
	case OP_DEC: *stamp -= 1; break;
	case OP_ADD: *stamp += r->compiled.times[0]; break;
	case OP_SUB: *stamp -= r->compiled.times[0]; break;
	case OP_SET: *stamp  = r->compiled.times[0]; break;
	}
    return 0;
}

int sqlite_logoperator_number(Number *n, OperatorRecord *r)
{
	if (!n || !r)
		return 0;
	switch (r->op)
    {
	case OP_INC: *n += 1.0; break;
	case OP_DEC: *n -= 1.0; break;
	case OP_ADD: *n += r->compiled.number; break;
	case OP_SUB: *n -= r->compiled.number; break;
	case OP_SET: *n  = r->compiled.number; break;
	}
    return 0;
}

int sqlite_logoperator_integer(Integer *i, OperatorRecord *r)
{
	if (!i || !r)
		return 0;
	switch (r->op)
    {
	case OP_INC: *i += 1; break;
	case OP_DEC: *i -= 1; break;
	case OP_ADD: *i += r->compiled.integer; break;
	case OP_SUB: *i -= r->compiled.integer; break;
	case OP_SET: *i  = r->compiled.integer; break;
	}
    return 0;
}

int sqlite_logoperator_unknown()
{
    return 0;
}

/* Compile the fields in the operator to point to corresponding
 * fields in the logsystem.
 * Unknown fields are ignored.
 * Do atois and atofs here. */
int sqlite_logsys_compileoperator(LogSystem *ls, LogOperator *lo)
{
	OperatorRecord *r;

	if (!lo || !ls)
		return 0;

	lo->logsys_compiled_to = ls;

	for (r = lo->operators; r < &lo->operators[lo->operator_count]; ++r)
    {
		r->field = NULL;
		r->func = sqlite_logoperator_unknown;

		if (r->op == OP_NONE)
			continue;

		r->field = sqlite_logsys_getfield(ls, r->name);
		if (!r->field)
			continue;

		switch (r->field->type)
        {
		default:
			break;

		case FIELD_TEXT:
			r->compiled.text = r->value;
			r->func = sqlite_logoperator_text;
			break;
		case FIELD_TIME:
			tr_parsetime(r->value, (time_t *) r->compiled.times);
			r->func = sqlite_logoperator_time;
			break;
		case FIELD_INDEX:
		case FIELD_INTEGER:
			r->compiled.integer = atoi(r->value);
			r->func = sqlite_logoperator_integer;
			break;
		case FIELD_NUMBER:
			r->compiled.number = atof(r->value);
			r->func = sqlite_logoperator_number;
			break;
		}
	}
    return 0;
}

void sqlite_logentry_operate(LogEntry *entry, LogOperator *o)
{
	OperatorRecord *r;
	void *value;

	if (!o || !entry)
		return;

	if (o->logsys_compiled_to != entry->logsys)
        sqlite_logsys_compileoperator(entry->logsys, o);

	for (r = o->operators; r < &o->operators[o->operator_count]; ++r)
    {
		if (r->field == NULL)
			continue;

		value = &entry->record_buffer[r->field->offset];

		(*r->func)(value, r);
	}
}

void sqlite_logsys_operatebyfilter(LogSystem *log, LogFilter *filter, LogOperator *ops)
{
    LogEntry **entries;
    int resultIndex = 0;

    /* read the entries corresponding to the filter */
    entries = sqlite_logsys_makelist(log,filter);

    /* no matching entries */
    if (entries == NULL)
        return;
    
    /* modify and rewrite */
    while (entries[resultIndex] != NULL)
    {
			sqlite_logentry_operate(entries[resultIndex], ops);
			sqlite_logentry_write(entries[resultIndex]);
            resultIndex++;
    }
    free(entries);
}

