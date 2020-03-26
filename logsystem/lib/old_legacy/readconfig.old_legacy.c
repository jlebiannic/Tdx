/*========================================================================
        E D I S E R V E R

        File:		readconfig.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Read logsystem description.
	Hardcoded defaults live mostly here.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: readconfig.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 15.01.96/JN	ws stripped off at end of .cfg-lines.
  3.02 14.10.96/JN	Silencing harmless warnings from cc.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

#include "lstool.old_legacy.h"

static char *the_filename;
static int the_lineno;

LogLabel *old_legacy_logsys_arrangefields();

/*
 * These get fixed positions in the record.
 * This must correspond to the definition of DLogRecord.
 */
static struct {
	int type;
	char *tag;
	char *confstr;
	int assigned;	/* assigned logfield index. */
} builtin_fields[] = {
	{ FIELD_INDEX,  "INDEX",    "INDEX,%4.0lf,", },
	{ FIELD_INDEX,  "NEXT",     "NEXT,,", },
	{ FIELD_TIME,   "CTIME",    "CREATED,%d.%m.%y %H:%M,Created", },
	{ FIELD_TIME,   "MTIME",    "MODIFIED,%d.%m.%y %H:%M,Modified", },
	{ FIELD_INTEGER,"GEN",      "GENERATION,%3d,Gen", },
	{ FIELD_INTEGER,"RESERVED1","RESERVED1,,", },
};

char *logsys_Filesdirs[256];
static int Nfilesdirs = 0;

old_legacy_logsys_dirtocreate(dir)
	char *dir;
{
	logsys_Filesdirs[Nfilesdirs++] = old_legacy_log_strdup(dir);
	logsys_Filesdirs[Nfilesdirs] = NULL;
}

typedef union {
	LogIndex idx;
	TimeStamp ts;
	Integer i;
	char b[4];
} BuiltinAlignment;

typedef union {
	Number n;
	BuiltinAlignment ba;
	char b[8];
} Alignment;

static
old_legacy_octal_string(s)
	char *s;
{
	int oct = 0;

	if (sscanf(s, "%o", &oct) != 1) {
		errno = 0;
		old_legacy_bail_out("%s(%d): Bad octal number '%s'",
			the_filename, the_lineno, s);
	}
	return (oct);
}

/*
 * These are defaults.
 * Can be overridden in description file.
 */

#define OPTION_DATA \
EXTERN_REC( "ACCESS_MODE",      Access_mode,       "0600",    old_legacy_octal_string, x)\
EXTERN_REC( "PRE_ALLOCATE",     Pre_allocate,      0,         NULL,         x)\
OPTION_REC( "RECORD_SIZE",      record_size,       0,         NULL,         x)\
OPTION_REC( "CLEANUP_COUNT",    cleanup_count,     0,         NULL,         x)\
OPTION_REC( "CLEANUP_THRESHOLD",cleanup_threshold, 0,         NULL,         x)\
OPTION_REC( "CREATED_HINTS",    creation_hints,    128,       NULL,         x)\
OPTION_REC( "MODIFIED_HINTS",   modification_hints,128,       NULL,         x)\
OPTION_REC( "MAX_COUNT",        max_count,         1000,      NULL,         x)\
OPTION_REC( "MAX_AGING",        max_aging,         0,         NULL,         x)\
OPTION_REC( "AGING_CHECK",      aging_check,       0,         NULL,         x)\
OPTION_REC( "AGING_UNIT",       aging_unit,        0,         NULL,         x)\
OPTION_REC( "SIZE_CHECK",       size_check,        0,         NULL,         x)\
OPTION_REC( "SIZE_UNIT",        size_unit,         0,         NULL,         x)\
OPTION_REC( "OWN_PROG",         own_prog,          0,         NULL,         x)\
OPTION_REC( "ADD_PROG",         add_prog,          0,         NULL,         x)\
OPTION_REC( "REMOVE_PROG",      remove_prog,       0,         NULL,         x)\
OPTION_REC( "THRESHOLD_PROG",   threshold_prog,    0,         NULL,         x)\
OPTION_REC( "CLEANUP_PROG",     cleanup_prog,      0,         NULL,         x)\
OPTION_REC( "MAX_SIZE",         max_diskusage,     0,         NULL,         x)

/*
**       ORACLE
**   OPTION_REC( "ODBUSER",    odbuser,     0,         NULL,         x)\
**   OPTION_REC( "ODBLINK",    odblink,     0,         NULL,         x)\
**   OPTION_REC( "ODBLOGIN",  odblogin,     0,         NULL,         x)\
**   OPTION_REC( "AVG_PARMS,  avg_parms,   10,         NULL,         x)\
**   OPTION_REC( "AVG_FILES,  avg_files,    1,         NULL,         x)\
**   OPTION_REC( "AVG_FSIZE,  avg_fsize,    1,         NULL,         x)
*/

#define EXTERN_REC(name, var, defvalue, converter, rsved) int var;
#define OPTION_REC(name, var, defvalue, converter, rsved) static int var;

OPTION_DATA

#undef EXTERN_REC
#undef OPTION_REC
#define EXTERN_REC OPTION_REC

static struct {
	char *tag;
	int *address;
	void *defvalue;
	int (*converter)();
} preferences[] = {
#define OPTION_REC(name, var, defvalue, converter, rsved) \
	{ name, & var, (void *) defvalue, converter, },

OPTION_DATA

#undef OPTION_REC
};


static int getBuiltinSizeFromType(int type) {
    switch (type)
    {
        case FIELD_INTEGER:
            return sizeof(Integer);
        case FIELD_INDEX:
            return sizeof(LogIndex);
        case FIELD_TIME:
            return sizeof(TimeStamp);
        default:
            return sizeof(int);
    }
}



int old_legacy_logsys_fillinternal(field, type, string)
	LogField *field;
	int type;
	char *string;
{
	char *name = NULL;
	char *format = NULL;
	char *header = NULL;

	name = string;
	if ((format = strchr(name, ',')) != NULL) {
		*format++ = 0;
		if ((header = strchr(format, ',')) != NULL) {
			*header++ = 0;
		}
	}
	if (!header)
		return (1);

	field->name.string   = old_legacy_log_strdup(name);
	field->format.string = old_legacy_log_strdup(format);
	field->header.string = old_legacy_log_strdup(header);

	field->type = type;
	field->size = getBuiltinSizeFromType(type);

	return (0);
}


/*
 * Read configuration and return label.
 */
LogLabel *
old_legacy_logsys_readconfig(filename)
	char *filename;
{
	FILE *fp;
	char buffer[8192];
	char *cp, *tag;
	int n;

	LogField *fields;
	int fieldmax;	/* number of fields allocated */
	int nfields;	/* number of fields added so far */

	the_filename = filename;
	the_lineno = 0;

	if ((fp = fopen(filename, "r")) == NULL)
		return (NULL);

	/*
	 * Default preferences first.
	 */
	for (n = 0; n < SIZEOF(preferences); ++n) {
		if (preferences[n].converter)
			*preferences[n].address =
			 (*preferences[n].converter)(preferences[n].defvalue);
		else
			*preferences[n].address =
			 (int) preferences[n].defvalue;
	}
	/*
	 * Insert builtin internal fields first.
	 * Make a copy from the string in table, they might be 'pure',
	 * and we might be called more than once.
	 */
	nfields = 0;
	fieldmax = SIZEOF(builtin_fields) + 64;
	fields = (void *) malloc(fieldmax * sizeof(*fields));
	if (fields == NULL)
		old_legacy_bail_out("Out of memory");

	for (n = 0; n < SIZEOF(builtin_fields); ++n) {

		strcpy(buffer, builtin_fields[n].confstr);
		builtin_fields[n].assigned = nfields;

		if (old_legacy_logsys_fillinternal(&fields[nfields],
				builtin_fields[n].type, buffer)) {
			errno = 0;
			old_legacy_bail_out("Internal: Malformed field %s",
				builtin_fields[n].confstr);
		}
		++nfields;
	}
	/*
	 * Now read in user-defined stuff from description file.
	 * Set errno to Invalid argument... fishy, but sorta descriptive.
	 */
	while (fgets(buffer, sizeof(buffer), fp)) {

		++the_lineno;
		errno = EINVAL;

#if 0
		if ((cp = strchr(buffer, '\n')) != NULL)
			*cp = 0;
#else
		for (cp = buffer; *cp; ++cp)
			;
		while (--cp >= buffer && isspace(*cp))
			*cp = 0;
#endif
		cp = buffer;
		while (*cp && isspace(*cp))
			++cp;
		if (*cp == '#' || *cp == 0)
			continue;

		/*
		 * Ok, non-comment. Separate the tag.
		 */
		tag = cp;
		while (*cp && *cp != '=')
			++cp;

		if (*cp == 0) {
			errno = 0;
			old_legacy_bail_out("%s(%d): Malformed line: %s",
				the_filename, the_lineno, buffer);
		}
		*cp++ = 0;

		/*
		 * First see if it is one of the internal fields.
		 * If so, update that.
		 */
		for (n = 0; n < SIZEOF(builtin_fields); ++n)
			if (!strcmp(builtin_fields[n].tag, tag))
				break;

		if (n < SIZEOF(builtin_fields)) {

			if (old_legacy_logsys_fillinternal(
					&fields[builtin_fields[n].assigned],
					builtin_fields[n].type, cp)) {
				errno = 0;
				old_legacy_bail_out("%s(%d): Malformed field: %s",
					the_filename, the_lineno, cp);
			}
			continue;
		}
		/*
		 * Not an internal field. Maybe user-preference ?
		 */
		for (n = 0; n < SIZEOF(preferences); ++n)
			if (!strcmp(preferences[n].tag, tag))
				break;

		if (n < SIZEOF(preferences)) {
			*preferences[n].address =
				preferences[n].converter ?
					(*preferences[n].converter)(cp) :
					atoi(cp);
			continue;
		}
		if (!strcmp(tag, "FIELD")) {
			/*
			 * Add a field into label.
			 */
			if (nfields >= fieldmax - 2) {
				fieldmax += 32;
				fields = (void *) realloc(fields,
					fieldmax * sizeof(*fields));
				if (fields == NULL)
					old_legacy_bail_out("Out of memory");
			}
			if (old_legacy_logsys_fillfield(&fields[nfields], cp)) {
				errno = 0;
				old_legacy_bail_out("%s(%d): Malformed field: %s",
					the_filename, the_lineno, cp);
			}
			++nfields;
			continue;
		}
		if (!strcmp(tag, "SUBDIR")) {
			old_legacy_logsys_dirtocreate(cp);
			continue;
		}
		fprintf(stderr, "%s(%d): Unknown tag %s ignored\n",
			the_filename, the_lineno, tag);
	}
	the_lineno = 0;

	fclose(fp);

	/*
	 * Arrange collected fields into good order
	 * and fill rest of information.
	 */
	fields[nfields].type = 0;

	return (old_legacy_logsys_arrangefields(fields));
}

int old_legacy_logsys_fillfield(field, string)
	LogField *field;
	char *string;
{
	char *name = NULL;
	char *type = NULL;
	char *format = NULL;
	char *header = NULL;

	name = string;
	if ((type = strchr(name, ',')) != NULL) {
		*type++ = 0;
		if ((format = strchr(type, ',')) != NULL) {
			*format++ = 0;
			if ((header = strchr(format, ',')) != NULL) {
				*header++ = 0;
			}
		}
	}
	if (!header)
		return (1);

	field->name.string   = old_legacy_log_strdup(name);
	field->format.string = old_legacy_log_strdup(format);
	field->header.string = old_legacy_log_strdup(header);

	if (!strcmp(type, "T")) {
		field->type = FIELD_TIME;
		field->size = sizeof(TimeStamp);

	} else if (!strcmp(type, "N")) {
		field->type = FIELD_NUMBER;
		field->size = sizeof(Number);

	} else if (atoi(type) > 0) {
		/*
		 * Add one for the terminating zero.
		 */
		field->type = FIELD_TEXT;
		field->size = atoi(type) + 1;

	} else {
		return (1);
	}

	return (0);
}

/*
 * Arrange label so that fields of same type are contiguous
 * in fields-array.
 * This is done to speed up the lookup of a field from it's name.
 *
 * Offsets are adjusted so that builtin fields stay in the beginning
 * and that user-fields of same type get next to each other (reduces slack).
 */

static
old_legacy_sorter_f(const void *a, const void *b)
{
	return (strcmp(((LogField *)a)->name.string, ((LogField *)b)->name.string));
}

LogLabel *
old_legacy_logsys_arrangefields(fields)
	LogField *fields;
{
	LogLabel *label;
	LogField *f, *f2;
	int n, nfields, offset, alignment;
	char *cp;

	/*
	 * Create label from field-array.
	 * First calculate the amount of memory needed
	 * and the number of fields.
	 */
	nfields = 0;
	n = sizeof(*label);
	for (f = fields; f->type; ++f) {
		++nfields;
		n += sizeof(*f);
		n += 1 + strlen(f->name.string);
		n += 1 + strlen(f->format.string);
		n += 1 + strlen(f->header.string);
	}

	++nfields;
	n += sizeof(*f);

	/*
	 * Allocate label and copy fields
	 */
	label = (void *) malloc(n);
	memset(label, 0, n);
	label->nfields = nfields;
	cp = (char *) &label->fields[nfields];
	f2 = label->fields;
	for (f = fields; f->type; ++f, ++f2) {
		*f2 = *f;
		f2->offset = -1;

		strcpy(cp, f->name.string);
		free(f->name.string);
		f2->name.string = cp;
		cp += 1 + strlen(cp);

		strcpy(cp, f->format.string);
		free(f->format.string);
		f2->format.string = cp;
		cp += 1 + strlen(cp);

		strcpy(cp, f->header.string);
		free(f->header.string);
		f2->header.string = cp;
		cp += 1 + strlen(cp);

	}
	label->labelsize = cp - (char *) label;

	/*
	 * Fix offsets.
	 * Fields have been marked as having no position.
	 * Insert builtin fields in correct order.
	 * User-defined fields get inserted last.
	 *
	 * Remember to keep built-in fields in the beginning.
	 */
	offset = 0;

	for (n = 0; n < SIZEOF(builtin_fields); ++n) {

		f = &label->fields[builtin_fields[n].assigned];
		alignment = f->size;

		while (offset & (alignment - 1))
			++offset;

		f->offset = offset;
		offset += f->size;
	}
	old_legacy_logsys_sortfieldnames(label);

	for (f = label->fields; f->type; ++f) {

		if (f->offset != -1)
			continue;

		switch (f->type) {
		default:
			alignment = 1;
			break;

		case FIELD_INTEGER:
			alignment = sizeof(Integer);
			break;

		case FIELD_INDEX:
		case FIELD_TIME:
		case FIELD_NUMBER:
			alignment = f->size;
			break;

		case FIELD_TEXT:
			alignment = 1;
			break;
		}
		if (!alignment)
			alignment = 1;
		while (offset & (alignment - 1))
			++offset;

		f->offset = offset;
		offset += f->size;
	}
	/*
	 * Field-array ends with type-zero field,
	 */
	f->offset = offset;
	f->size = 0;

	/*
	 * Insert preferences into label.
	 */
	label->maxcount = max_count;
	label->recordsize = record_size;
	label->max_diskusage = max_diskusage;

	label->cleanup_count = cleanup_count;
	label->cleanup_threshold = cleanup_threshold;

	label->creation_hints = creation_hints;
	label->modification_hints = modification_hints;

	label->run_addprog       = add_prog;
	label->run_removeprog    = remove_prog;
	label->run_thresholdprog = threshold_prog;
	label->run_cleanupprog   = cleanup_prog;

	/*
	 * If not specified, align recordsize to first big-enough
	 * power of two.
	 *
	 * Otherwise, use the specified size, but adjust to fit.
	 *
	 * Make sure no unaligned references happen with data
	 * in records.
	 */
	if (label->recordsize <= 0) {
		alignment = 256;

		while (alignment < offset)
			alignment <<= 1;

		while (offset & (alignment - 1))
			++offset;

		label->recordsize = offset;
	}
	if (label->recordsize < offset)
		label->recordsize = offset;

	while (label->recordsize & (sizeof(Alignment) - 1))
		++label->recordsize;

	return (label);
}

int old_legacy_logsys_sortfieldnames(label)
	LogLabel *label;
{
	LogLabel *unsorted;
	LogField *f;
	int n, type, start;
	int offset, alignment;

	/*
	 * Make a copy of the field-array.
	 * Scan the field-array for each possible type.
	 */
	unsorted = old_legacy_loglabel_alloc(LABELSIZE(label));
	memcpy(unsorted, label, LABELSIZE(label));
	n = 0;

	for (type = 0; type < SIZEOF(label->types); ++type) {

		start = -1;

		for (f = unsorted->fields; f->type; ++f) {

			if (f->type != type)
				continue;
			if (start == -1)
				start = n;

			label->fields[n++] = *f;
		}
		if (start == -1) {
			/*
			 * Not a single field with this type.
			 */
			label->types[type].start = 0;
			label->types[type].end = 0;
			continue;
		}
		/*
		 * Sort the fieldnames.
		 */
		label->types[type].start = start;
		label->types[type].end = n;
		qsort(&label->fields[start], n - start,
			sizeof(label->fields[0]), old_legacy_sorter_f);
	}
	old_legacy_loglabel_free(unsorted);
    return 0;
}

