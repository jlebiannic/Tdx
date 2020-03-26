#include "conf/local_config.h"
/*========================================================================
        E D I S E R V E R

        File:		operate.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines used to access fields in logentries.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: operate.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $";
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

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"
#include "runtime/tr_prototypes.h"

extern double atof();

/*
 * Operator can be
 *	=	SET
 *	:=	SET (preferred way)
 *	++	INC
 *	--	DEC
 *	+=	ADD
 *	-=	SUB
 */
static void
old_legacy_topieces(namevalue, name, op, value)
	char *namevalue;
	char **name, **value;
	int *op;				/* operator enum */
{
	char *cp = namevalue;
	char *name_end;

	while (*cp && isspace(*cp)) ++cp;	/* to 1st word */

	*name = cp;
	*op = OP_NONE;
	*value = "";

	/*
	 * The fieldname can consist from
	 * letters, digits or _
	 */
	while ((*cp >= 'A' && *cp <= 'Z')
	    || (*cp >= '0' && *cp <= '9')
	    || (*cp >= 'a' && *cp <= 'z')
	    || (*cp == '_')) {
		++cp;
		continue;
	}
	name_end = cp;

	if (*cp && isspace(*cp)) {
		/*
		 * There is whitespace between
		 * fieldname and operator.
		 */
		*cp++ = 0;
		while (*cp && isspace(*cp)) ++cp;
	}
	switch (*cp++) {
	case 0:
		/*
		 * No operator, no value.
		 */
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
		if (*cp == '-') {
			++cp;
		} else if (*cp == '=') {
			++cp;
			*op = OP_SUB;
		}
		break;
	case '+':
		*op = OP_INC;
		if (*cp == '+') {
			++cp;
		} else if (*cp == '=') {
			++cp;
			*op = OP_ADD;
		}
		break;
	}
	/*
	 * Remember to cut the name away,
	 * junking the operator string.
	 */
	*name_end = 0;
	*value = cp;
}

void
old_legacy_logoperator_insert(lop, name, op, value)
	LogOperator **lop;
	char *name, *value;
	int op;
{
	int n;
	OperatorRecord *r;
	LogOperator *lo = *lop;

	if (!lo) {
		lo = old_legacy_log_malloc(sizeof(*lo));
		memset(lo, 0, sizeof(*lo));
		*lop = lo;
	}
	if (lo->operator_count >= lo->operator_max) {
		lo->operator_max += 64;
		n = lo->operator_max * sizeof(*lo->operators);

		lo->operators = old_legacy_log_realloc(lo->operators, n);
	}
	r = &lo->operators[lo->operator_count++];

	r->name = old_legacy_log_strdup(name);
	r->op = op;
	r->value = old_legacy_log_strdup(value);
}

void
old_legacy_logoperator_add(lop, namevalue)
	LogOperator **lop;
	char *namevalue;
{
	char *name, *value;
	int op;

	old_legacy_topieces(namevalue, &name, &op, &value);

	old_legacy_logoperator_insert(lop, name, op, value);
}

int
old_legacy_logoperator_read(lop, fp)
	LogOperator **lop;
	FILE *fp;
{
	char buf[1024];
	char *name, *value, *cp;
	int op;

	while (fp && tr_mbFgets(buf, sizeof(buf), fp)) {
		if (*buf == '\n' || *buf == '#')
			continue;
		if ((cp = strchr(buf, '\n')) != NULL)
			*cp = 0;

		old_legacy_topieces(buf, &name, &op, &value);

		old_legacy_logoperator_insert(lop, name, op, value);
	}
	return (0);
}

old_legacy_logoperator_write(fp, lo)
	FILE *fp;
	LogOperator *lo;
{
	OperatorRecord *r;

	if (!lo)
		return (0);

	for (r = lo->operators; r < &lo->operators[lo->operator_count]; ++r) {

		tr_mbFprintf(fp, "%s%s%s\n",
			r->name, operator_strings[r->op], r->value);

		if (ferror(fp))
			return (-1);
	}
	return (0);
}

/*
 * Clear fields from operator,
 * but dont free the operator-array itself
 */
void
old_legacy_logoperator_clear(lo)
	LogOperator *lo;
{
	OperatorRecord *r;

	if (!lo)
		return;

	for (r = lo->operators; r < &lo->operators[lo->operator_count]; ++r) {
		if (r->name)
			free(r->name);
		if (r->value)
			free(r->value);
	}
	lo->operator_count = 0;
}

void
old_legacy_logoperator_free(lo)
	LogOperator *lo;
{
	if (!lo)
		return;

	old_legacy_logoperator_clear(lo);

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
int
old_legacy_logoperator_text(txt, r)
	char *txt;
	OperatorRecord *r;
{
	LogField *field = r->field;

	switch (r->op) {
	case OP_SET: old_legacy_log_strncpy(txt, r->compiled.text, field->size); break;
	case OP_ADD: old_legacy_log_strncat(txt, r->compiled.text, field->size); break;
	case OP_SUB: old_legacy_log_strsubrm(txt, r->compiled.text); break;
	}
    return 0;
}

int
old_legacy_logoperator_time(stamp, r)
	TimeStamp *stamp;
	OperatorRecord *r;
{
	switch (r->op) {
	case OP_INC: *stamp += 1; break;
	case OP_DEC: *stamp -= 1; break;
	case OP_ADD: *stamp += r->compiled.times[0]; break;
	case OP_SUB: *stamp -= r->compiled.times[0]; break;
	case OP_SET: *stamp  = r->compiled.times[0]; break;
	}
    return 0;
}

int
old_legacy_logoperator_number(n, r)
	Number *n;
	OperatorRecord *r;
{
	switch (r->op) {
	case OP_INC: *n += 1.0; break;
	case OP_DEC: *n -= 1.0; break;
	case OP_ADD: *n += r->compiled.number; break;
	case OP_SUB: *n -= r->compiled.number; break;
	case OP_SET: *n  = r->compiled.number; break;
	}
    return 0;
}

int
old_legacy_logoperator_integer(i, r)
	Integer *i;
	OperatorRecord *r;
{
	switch (r->op) {
	case OP_INC: *i += 1; break;
	case OP_DEC: *i -= 1; break;
	case OP_ADD: *i += r->compiled.integer; break;
	case OP_SUB: *i -= r->compiled.integer; break;
	case OP_SET: *i  = r->compiled.integer; break;
	}
    return 0;
}

int
old_legacy_logoperator_unknown()
{
    return 0;
}

/*
 * Compile the fields in the operator to point to corresponding
 * fields in the logsystem.
 * Unknown fields are ignored.
 * Do atois and atofs here.
 */
old_legacy_logsys_compileoperator(ls, lo)
	LogSystem *ls;
	LogOperator *lo;
{
	OperatorRecord *r;

	if (!lo)
		return 0;

	lo->logsys_compiled_to = ls;

	for (r = lo->operators; r < &lo->operators[lo->operator_count]; ++r) {
		r->field = NULL;
		r->func = old_legacy_logoperator_unknown;

		if (r->op == OP_NONE)
			continue;

		r->field = old_legacy_logsys_getfield(ls, r->name);
		if (!r->field)
			continue;

		switch (r->field->type) {
		default:
			break;

		case FIELD_TEXT:
			r->compiled.text = r->value;
			r->func = old_legacy_logoperator_text;
			break;
		case FIELD_TIME:
			tr_parsetime(r->value, (time_t *) r->compiled.times);
			r->func = old_legacy_logoperator_time;
			break;
		case FIELD_INDEX:
		case FIELD_INTEGER:
			r->compiled.integer = atoi(r->value);
			r->func = old_legacy_logoperator_integer;
			break;
		case FIELD_NUMBER:
			r->compiled.number = atof(r->value);
			r->func = old_legacy_logoperator_number;
			break;
		}
	}
    return 0;
}

void
old_legacy_logentry_operate(entry, o)
	LogEntry *entry;
	LogOperator *o;
{
	OperatorRecord *r;
	void *value;

	if (!o)
		return;

	if (o->logsys_compiled_to != entry->logsys)
		old_legacy_logsys_compileoperator(entry->logsys, o);

	for (r = o->operators; r < &o->operators[o->operator_count]; ++r) {

		if (r->field == NULL)
			continue;

		value = &entry->record_buffer[r->field->offset];

		(*r->func)(value, r);
	}
}

void
old_legacy_logsys_operatebyfilter(log, filter, ops)
	LogSystem *log;
	LogFilter *filter;
	LogOperator *ops;
{
	LogEntry *entry;
	int maxcount;
	int count = 0;
	time_t now;

	old_legacy_logsys_rewind(log);
	time(&now);
	old_legacy_logsys_refreshmap(log);

	maxcount = log->map->numused;

	while (count < maxcount) {
		if ((entry = old_legacy_logentry_get(log)) == NULL)
			break;

		if (entry->record_ctime < now)
			++count;

		if (old_legacy_logentry_passesfilter(entry, filter)) {

			old_legacy_logentry_operate(entry, ops);
			old_legacy_logentry_write(entry);
		}
		old_legacy_logentry_free(entry);
	}
}

