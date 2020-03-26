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
MODULE("@(#)TradeXpress $Id: readconfig.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 15.01.96/JN	ws stripped off at end of .cfg-lines.
  3.02 14.10.96/JN	Silencing harmless warnings from cc.
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>

#include "private.h"
#include "logsystem.sqlite.h"

#include "lstool.h"

static char *the_filename;
static int the_lineno;

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
	{ FIELD_INDEX,  "INDEX",    "INDEX,%4d,", 0 },
	{ FIELD_INDEX,  "NEXT",     "NEXT,,", 0 },
	{ FIELD_TIME,   "CTIME",    "CREATED,%d.%m.%y %H:%M,Created", 0 },
	{ FIELD_TIME,   "MTIME",    "MODIFIED,%d.%m.%y %H:%M,Modified", 0 },
	{ FIELD_INTEGER,"GEN",      "GENERATION,%3d,Gen", 0 },
	{ FIELD_INTEGER,"RESERVED1","RESERVED1,,", 0 },
};

char *logsys_Filesdirs[256];
static int Nfilesdirs = 0;

int sqlite_loglabel_free(LogLabel *lab);

int sqlite_logsys_dirtocreate(char *dir)
{
	logsys_Filesdirs[Nfilesdirs++] = sqlite_log_strdup(dir);
	logsys_Filesdirs[Nfilesdirs] = NULL;
    return 0;
}

typedef union {
	LogIndex idx;
	TimeStamp ts;
	Integer i;
	char b[8];
} BuiltinAlignment;

typedef union {
	Number n;
	BuiltinAlignment ba;
	char b[8];
} Alignment;

static int sqlite_octal_string(char *s)
{
	int oct = 0;

	if (sscanf(s, "%o", &oct) != 1)
    {
		errno = 0;
		sqlite_bail_out("%s(%d): Bad octal number '%s'", the_filename, the_lineno, s);
	}
	return (oct);
}

/* These are defaults. Can be overridden in description file. */

#define OPTION_DATA \
EXTERN_REC( "ACCESS_MODE",      Access_mode,       "0600",    sqlite_octal_string, x)\
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
#define OPTION_REC(name, var, defvalue, converter, rsved) { name, & var, (void *) defvalue, converter, },

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

int sqlite_logsys_fillinternal(LogField *field, int type, char *string)
{
	char *name = NULL;
	char *format = NULL;
	char *header = NULL;

	name = string;
	if ((format = strchr(name, ',')) != NULL)
    {
		*format++ = 0;
		if ((header = strchr(format, ',')) != NULL)
			*header++ = 0;
	}
	if (!header)
		return (1);

	field->name.string   = sqlite_log_strdup(name);
	field->format.string = sqlite_log_strdup(format);
	field->header.string = sqlite_log_strdup(header);

	field->type = type;
	field->size = getBuiltinSizeFromType(type);

	return (0);
}

int sqlite_logsys_fillfield(LogField *field, char *string)
{
	char *name = NULL;
	char *type = NULL;
	char *format = NULL;
	char *header = NULL;

	name = string;
	if ((type = strchr(name, ',')) != NULL)
    {
		*type++ = 0;
		if ((format = strchr(type, ',')) != NULL)
        {
			*format++ = 0;
			if ((header = strchr(format, ',')) != NULL)
				*header++ = 0;
		}
	}
	if (!header)
		return (1);

	field->name.string   = sqlite_log_strdup(name);
	field->format.string = sqlite_log_strdup(format);
	field->header.string = sqlite_log_strdup(header);

	if (!strcmp(type, "T"))
    {
		field->type = FIELD_TIME;
		field->size = sizeof(TimeStamp);
	}
    else if (!strcmp(type, "N"))
    {
		field->type = FIELD_NUMBER;
		field->size = sizeof(Number);
	}
    else if (atoi(type) > 0)
    {
		/* Add one for the terminating zero. */
		field->type = FIELD_TEXT;
		field->size = atoi(type) + 1;
	}
    else
		return (1);

	return (0);
}

/* Read configuration and return label. */
LogLabel * sqlite_logsys_readconfig(char *filename, int onRebuild)
{
	FILE *fp;
	char buffer[8192];
	char *cp, *tag;
	unsigned int n;

	LogField *fields;
	int fieldmax;	/* number of fields allocated */
	int nfields;	/* number of fields added so far */

	the_filename = filename;
	the_lineno = 0;

	if ((fp = fopen(filename, "r")) == NULL)
		return (NULL);

	/* Default preferences first. */
	for (n = 0; n < SIZEOF(preferences); ++n)
    {
		if (preferences[n].converter)
			*preferences[n].address = (*preferences[n].converter)(preferences[n].defvalue);
		else
			*preferences[n].address = (int) preferences[n].defvalue;
	}
	/* Insert builtin internal fields first.
	 * Make a copy from the string in table, they might be 'pure',
	 * and we might be called more than once. */
	nfields = 0;
	fieldmax = SIZEOF(builtin_fields) + 64;
	fields = (void *) malloc(fieldmax * sizeof(*fields));
	if (fields == NULL)
		sqlite_bail_out("Out of memory");

	for (n = 0; n < SIZEOF(builtin_fields); ++n)
    {
		strcpy(buffer, builtin_fields[n].confstr);
		builtin_fields[n].assigned = nfields;

		if (sqlite_logsys_fillinternal(&fields[nfields], builtin_fields[n].type, buffer))
        {
			errno = 0;
			sqlite_bail_out("Internal: Malformed field %s", builtin_fields[n].confstr);
		}
		++nfields;
	}
	/* Now read in user-defined stuff from description file.
	 * Set errno to Invalid argument... fishy, but sorta descriptive. */
	while (fgets(buffer, sizeof(buffer), fp))
    {
		++the_lineno;
		errno = EINVAL;

		for (cp = buffer; *cp; ++cp)
			;
		while (--cp >= buffer && isspace(*cp))
			*cp = 0;
		cp = buffer;
		while (*cp && isspace(*cp))
			++cp;
		if (*cp == '#' || *cp == 0)
			continue;

		/* Ok, non-comment. Separate the tag. */
		tag = cp;
		while (*cp && *cp != '=')
			++cp;

		if (*cp == 0)
        {
			errno = 0;
			sqlite_bail_out("%s(%d): Malformed line: %s", the_filename, the_lineno, buffer);
		}
		*cp++ = 0;

		/* First see if it is one of the internal fields.
		 * If so, update that. */
		for (n = 0; n < SIZEOF(builtin_fields); ++n)
        {
			if (!strcmp(builtin_fields[n].tag, tag))
				break;
        }

		if (n < SIZEOF(builtin_fields))
        {
			if (sqlite_logsys_fillinternal( &fields[builtin_fields[n].assigned], builtin_fields[n].type, cp))
            {
				errno = 0;
				sqlite_bail_out("%s(%d): Malformed field: %s", the_filename, the_lineno, cp);
			}
			continue;
		}
		/* Not an internal field. Maybe user-preference ? */
		for (n = 0; n < SIZEOF(preferences); ++n)
        {
			if (!strcmp(preferences[n].tag, tag))
				break;
        }

		if (n < SIZEOF(preferences))
        {
            if (preferences[n].converter)
                *preferences[n].address = (*preferences[n].converter)(cp);
            else
                *preferences[n].address = atoi(cp);
			continue;
		}
		if (!strcmp(tag, "FIELD"))
        {
			/* Add a field into label. */
			if (nfields >= fieldmax - 2)
            {
				fieldmax += 32;
				fields = (void *) realloc(fields, fieldmax * sizeof(*fields));
				if (fields == NULL)
					sqlite_bail_out("Out of memory");
			}
			if (sqlite_logsys_fillfield(&fields[nfields], cp))
            {
				errno = 0;
				sqlite_bail_out("%s(%d): Malformed field: %s", the_filename, the_lineno, cp);
			}
			++nfields;
			continue;
		}
		if (!strcmp(tag, "SUBDIR"))
        {
			sqlite_logsys_dirtocreate(cp);
			continue;
		}
		if (!strcmp(tag, "SQL_INDEX"))
		{
			continue;
		}
		fprintf(stderr, "%s(%d): Unknown tag %s ignored\n", the_filename, the_lineno, tag);
	}
	the_lineno = 0;

	fclose(fp);

	/* Arrange collected fields into good order and fill rest of information. */
	fields[nfields].type = 0;

	return (sqlite_logsys_arrangefields(fields,onRebuild));
}

/* we have done a logcreate -R (reread), lets try to see if we can apply the changes */
int sqlite_loglabel_compareconfig(LogLabel *oldLabel, LogLabel *newLabel)
{
	int comparison = 0;
	LogField *oldField = NULL;
	LogField *newField = NULL;
	int newFormatNumSize = 0;
	int oldFormatNumSize = 0;
	int newFormatNumSize1 = 0;
	int oldFormatNumSize1 = 0;
	int newFormatNumSize2 = 0;
	int oldFormatNumSize2 = 0;

	/* - Regarding the columns, we only need to check whether we have all the columns
	 * cause at the moment removing a column in SQLite means 
	 * rebuilding the table !!
	 * - Regarding the fields, we must 
	 * 		- warn the user if he/she shrinks the fields size
	 * 		- report an error if he/she changes the type/format(?)
	 */

	for (oldField = oldLabel->fields; oldField->type; ++oldField)
	{
		/* fields are type ordered */
		newField = newLabel->fields;
		/* last field has NULL type */
		while ( (newField->type) && ( strcmp(oldField->name.string, newField->name.string) != 0 ))
		{
			++newField;
		}
		/* we have not found the old field in the new label -> error */
		if (!newField->type)
		{
			comparison = -1;
			fprintf(stderr, "Error : we have not found the field : %s in the new cfg file, please add it.\n", oldField->name.string);
			break;
		}
		/* we have a field */
		/* type must be the same */
		if (oldField->type != newField->type)
		{
			comparison = -1;
			fprintf(stderr, "Error : we have found the field : %s in the new cfg file, but it has not the same type as before. Was : %s and you specify : %s\n", oldField->name.string,sqlite_logfield_typename(oldField->type),sqlite_logfield_typename(newField->type));
			break;
		}
		
		/* format does not change, its ok, lets go next. */
		if ( strcmp(newField->format.string, oldField->format.string) == 0)
		{
			continue;
		}

		/* size, type dependant except for text at the moment */
		if ( (newField->type == FIELD_TEXT) && (newField->size < oldField->size) )
		{
			/* warning if shrink */
			fprintf(stderr, "Warning : you reduced the size of the field : %s from %d tp %d !!\nThis may return/write truncated values from/to the database.\n", oldField->name.string,oldField->size,newField->size);
		}
		/* same type, format changes ? */
		switch (newField->type) {
			case FIELD_INTEGER:
				sscanf(newField->format.string,"%%%dd",&newFormatNumSize);
				sscanf(oldField->format.string,"%%%dd",&oldFormatNumSize);
				if ( newFormatNumSize < oldFormatNumSize )
				{
					/* warning if shrink */
					fprintf(stderr, "Warning : you reduced the size of the field : %s from %d tp %d !!\nThis may return/write truncated values from/to the database.\n", oldField->name.string,oldField->size,newField->size);
				}
				break;
			case FIELD_NUMBER:
				sscanf(newField->format.string,"%%%d.%dlf",&newFormatNumSize1,&newFormatNumSize2);
				sscanf(oldField->format.string,"%%%d.%dlf",&oldFormatNumSize1,&oldFormatNumSize2);
				if ( ( newFormatNumSize1 < oldFormatNumSize1 ) ||  ( newFormatNumSize2 < oldFormatNumSize2 ) )
				{
					/* warning if shrink */
					fprintf(stderr, "Warning : you reduced the size of the field : %s from %d tp %d !!\nThis may return/write truncated values from/to the database.\n", oldField->name.string,oldField->size,newField->size);
				}
				break;
			case FIELD_TEXT:
				sscanf(newField->format.string,"%%-%d.%ds",&newFormatNumSize1,&newFormatNumSize2);
				sscanf(oldField->format.string,"%%-%d.%ds",&oldFormatNumSize1,&oldFormatNumSize2);
				if ( ( newFormatNumSize1 < oldFormatNumSize1 ) ||  ( newFormatNumSize2 < oldFormatNumSize2 ) )
				{
					/* warning if shrink */
					fprintf(stderr, "Warning : you reduced the size of the field : %s from %d tp %d !!\nThis may return/write truncated values from/to the database.\n", oldField->name.string,oldField->size - 1 ,newField->size - 1 );
					/* NB : do not include the terminal \0 when displaying field size */
				}
				break;
		}
	}

	return comparison;
}

/* apply detected changes to the SQLite database */
int sqlite_logdata_reconfig(LogLabel *oldLabel, LogSystem *log)
{
	LogLabel *newLabel = log->label;
	LogField *oldField = NULL;
	LogField *newField = NULL;

	/* - Regarding the columns, we only need to check whether we have all the columns
	 * cause at the moment removing a column in SQLite means 
	 * rebuilding the table !!
	 * - Regarding the fields, we must 
	 * 		- warn the user if he/she shrinks the fields size
	 * 		- report an error if he/she changes the type/format(?)
	 */
	for (newField = newLabel->fields; newField->type; ++newField)
	{
		/* we cannot suppose anymore fields are type ordered (cause of logcreate -R)
		 * but we still finish with a NULL type field */
		oldField = oldLabel->fields;
		while ( (oldField->type) && ( strcmp(oldField->name.string, newField->name.string) != 0 ))
			++oldField;

		if (!oldField->type)                               /* we have not found the new field in the old label */
			if ( log_sqlitecolumnadd(log,newField) == -1 ) /* lets add it to the SQLite database ! */
				return -1;                                 /* do NOT go on if there were an error */

		/* we have a field 
		 * but from the SQLite point of view we do not care ! */
	}

	/* Now be sure of data access consistency : give the label the same order as the sqlite db */
	return sqlite_logsys_sync_label(log);
}

/*
 * Arrange label so that fields of same type are contiguous
 * in fields-array.
 * This is done to speed up the lookup of a field from it's name.
 *
 * Offsets are adjusted so that builtin fields stay in the beginning
 * and that user-fields of same type get next to each other (reduces slack).
 */

static int sqlite_sorter_f(const void *a,const void *b)
{
	return (strcmp(((LogField *)a)->name.string, ((LogField *)b)->name.string));
}

int sqlite_logsys_sortfieldnames(LogLabel *label)
{
	LogLabel *unsorted;
	LogField *f;
	int n, type, start;

	/* Make a copy of the field-array.
	 * Scan the field-array for each possible type. */
	unsorted = sqlite_loglabel_alloc(LABELSIZE(label));
	memcpy(unsorted, label, LABELSIZE(label));
	n = 0;

	for (type = 0; type < (int) SIZEOF(label->types); ++type)
    {
		start = -1;

		for (f = unsorted->fields; f->type; ++f)
        {
			if (f->type != type)
				continue;
			if (start == -1)
				start = n;
			label->fields[n++] = *f;
		}
		if (start == -1)
        {
			/* Not a single field with this type. */
			label->types[type].start = 0;
			label->types[type].end = 0;
			continue;
		}
		/* Sort the fieldnames. */
		label->types[type].start = start;
		label->types[type].end = n;
		qsort(&label->fields[start], n - start, sizeof(label->fields[0]), sqlite_sorter_f);
	}
	sqlite_loglabel_free(unsorted);
    return 0;
}

LogLabel * sqlite_logsys_arrangefields(LogField *fields, int onRebuild)
{
	LogLabel *label;
	LogField *f, *f2;
	unsigned int n;
	int nfields, offset, alignment;
	char *cp;

	/* Create label from field-array.
	 * First calculate the amount of memory needed
	 * and the number of fields. */
	nfields = 0;
	n = sizeof(*label);
	for (f = fields; f->type; ++f)
    {
		++nfields;
		n += sizeof(*f);
		n += 1 + strlen(f->name.string);
		n += 1 + strlen(f->format.string);
		n += 1 + strlen(f->header.string);
	}

	++nfields;
	n += sizeof(*f);

	/* Allocate label and copy fields */
	label = (void *) malloc(n);
	memset(label, 0, n);
	label->nfields = nfields;
	cp = (char *) &label->fields[nfields];
	f2 = label->fields;
	for (f = fields; f->type; ++f, ++f2)
    {
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

	/* Fix offsets.
	 * Fields have been marked as having no position.
	 * Insert builtin fields in correct order.
	 * User-defined fields get inserted last.
	 *
	 * Remember to keep built-in fields in the beginning. */
	offset = 0;

	for (n = 0; n < SIZEOF(builtin_fields); ++n)
    {
		f = &label->fields[builtin_fields[n].assigned];
		alignment = f->size;

		while (offset & (alignment - 1))
			++offset;

		f->offset = offset;
		offset += f->size;
	}

	/* do NOT sort when doing reconfig (aka logcreate -R) */
	if (onRebuild != 1)
		sqlite_logsys_sortfieldnames(label);

	for (f = label->fields; f->type; ++f)
    {
		if (f->offset != -1)
			continue;

		int sz = f->size;

		switch (f->type)
        {
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
			/*
 			 * Note : this is here that all the magic occurs for logsystem UNICODE adaptation
 			 * All string sizes allocations are multiplied by 4 (MB_CUR_MAX = 4 for UTF-8) 
 			 * to allow the storage of MB strings
 			 * f->size is NOT changed
 			 * the offset is the only part that is modified
 			 */
			sz = f->size * MB_CUR_MAX; /* a Unicode char in UTF-8 takes up to 4 bytes */
			break;
		}
		if (!alignment)
			alignment = 1;
		while (offset & (alignment - 1))
			++offset;

		f->offset = offset;
		offset += sz;
	}
	/* Field-array ends with type-zero field, */
	f->offset = offset;
	f->size = 0;

	/* Insert preferences into label. */
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

	/* If not specified, align recordsize to first big-enough
	 * power of two.
	 *
	 * Otherwise, use the specified size, but adjust to fit.
	 *
	 * Make sure no unaligned references happen with data
	 * in records. */
	if (label->recordsize <= 0)
    {
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

