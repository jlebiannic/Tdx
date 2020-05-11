/*========================================================================
        E D I S E R V E R

        File:		logsystem/build.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Building a new logsystem.
========================================================================*/
#include "conf/config.h"
LIBRARY(liblogsystem_version)
MODULE("@(#)TradeXpress $Id: build.c 55499 2020-05-07 16:25:38Z jlebiannic $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 19.09.97/JN	Fix for both \ and / pathseps on NT.
  3.02 29.06.05/CD  Remove all the .files if they already exist at build time 
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "logsystem/lib/logsystem.dao.h"
#include "logsystem/lib/logsystem.dao.h"

#define _BUILD_C
#include "logsystem/lib/lstool.h"
#include "build.h"
#undef _BUILD_C

static char *Sysdirs[] = {
	LSFILE_FILES,
	LSFILE_TRACE,
	LSFILE_TMP,
	NULL
};

extern char *logsys_Filesdirs[];
extern void bail_out(char *fmt, ...);

int get_confirmation(int def);

int logsys_buildsystem(char *sysname, char *cfgfile, struct opt options)
{
	LogSystem *log;
	LogLabel *old_label;
	int status = 0;

	FILE *fp = NULL;
	int statusBase;

	log = dao_logsys_alloc(sysname);

	if (options.rereadCfg == 1)
	{
		if (dao_logsys_readlabel(log) != 0)
			bail_out("Cannot reconfigure the specified base %s. Either it does not exist, or you mispelled the name, or it may be corrupted.", sysname);
		old_label = log->label;
	}

	
	/* label is expanded as needed by readconfig. */
	/* on first build, we reorder the cfgfile to stay at maximum compatible with pre-5.0.4 binaries
	 * on reconfig, order is broken if we had a column */
	log->label = dao_logsys_readconfig(cfgfile,options.rereadCfg);

	if (log->label == NULL)
		bail_out("Cannot open %s", cfgfile);

	/* only a few changes are allowed, do not execute the reread if comparison reports imcompatible changes.
	 * are allowed :
	 * - resizing a field (growing, better than shrinking)
	 * - adding a field
	 */
	if ((options.rereadCfg == 1) && (dao_loglabel_compareconfig(old_label,log->label) == -1))
		Really = 0;

	if (Really)
    {
		int omask = umask(0);

		fp = fopen(log->sysname, "rb");

		if (fp == NULL){
			statusBase = 1;
		} else {
			statusBase = 0;
			fclose(fp);
		}


		if ( (options.rereadCfg == 0) && (statusBase == 0) ){
			/* Print a warning and wait for confirmation */

			fprintf(stderr, "Warning: logcreate will remove all entries in %s.\nAre you sure you want to continue (y/n) ? ", sysname);

			if (get_confirmation('n') != 'y')
            		{
				fprintf(stderr, "Cancelled.\n");
				exit(2);
			}
			fprintf(stderr, "Continuing...\n");
		}
	
		if ((options.rereadCfg == 0) && (options.checkAffinities == 0))
		{
			logsys_createdirs(log->sysname);
		}
		logsys_copyconfig(cfgfile, log->sysname);
		logsys_createlabel(log);
		logsys_createsqlindexes(cfgfile, log);
		if ((options.rereadCfg == 0) && (options.checkAffinities == 0))
		{
			status = logsys_createdata_dao(log, Access_mode);
		}


		if (options.rereadCfg == 1)
		{
			/* we have compatible changes, lets apply them !
			 * are allowed :
			 * - resizing a field (growing, better than shrinking)
			 * - adding a field
			 */
			status = dao_logdata_reconfig(old_label, log);
			/* save (recreate) the updated label to the label file */
			logsys_createlabel(log);
		}

		if (options.checkAffinities == 1)
		{
			/* check whether sqlite field affinities match label field type and correct them if necessary */
			status = dao_loglabel_checkaffinities(log);
		}

		umask(omask);

		if (!Quiet && status == 0)
		{
			dao_logsys_dumplabelfields(log->label);
		}
	}

    dao_logsys_close(log);
    return 0;
}

void logsys_createlabel(LogSystem *log)
{
	int labelsize, fd;
	LogField *field;
	char *labelbase;

	log->label->magic = LSMAGIC_LABEL;
	log->label->version = LSVERSION;

	labelsize = LABELSIZE(log->label);
	labelbase = (char *) log->label;

	fd = dao_logsys_openfile(log, LSFILE_LABEL, O_TRUNC | O_RDWR | O_CREAT | O_BINARY, Access_mode);
	if (fd < 0)
		bail_out("Cannot create label");
	
	/* Adjust pointers. */
	for (field = log->label->fields; field->type; ++field)
    {
		field->name.offset   = field->name.string   - labelbase;
		field->format.offset = field->format.string - labelbase;
		field->header.offset = field->header.string - labelbase;
	}

	if (write(fd, log->label, labelsize) != labelsize)
		bail_out("Label write error");

	close(fd);

	/* And back to normal. */
	for (field = log->label->fields; field->type; ++field)
    {
		field->name.string   = labelbase + field->name.offset;
		field->format.string = labelbase + field->format.offset;
		field->header.string = labelbase + field->header.offset;
	}
}

void logsys_createdirs(char *sysname)
{
	char path[1024];
	int mode = Access_mode;
	int i;

	/* Add search permission, if either corresponding rw bit is on. */
	if (mode & 0600)
		mode |= 0100;
	if (mode & 0060)
		mode |= 0010;
	if (mode & 0006)
		mode |= 0001;

	if ((mkdir(sysname, mode) == -1) && (errno != EEXIST))
		bail_out("Cannot make directory");

	for (i = 0; Sysdirs[i]; ++i)
    {
		strcpy(path, sysname);
		strcat(path, Sysdirs[i]);

        if ((mkdir(path, mode) == -1) && (errno != EEXIST))
			bail_out("Cannot mkdir %s", path);
	}
	for (i = 0; logsys_Filesdirs[i]; ++i)
    {
		strcpy(path, sysname);
		strcat(path, LSFILE_FILES);
		strcat(path, "/");
		strcat(path, logsys_Filesdirs[i]);

        if ((mkdir(path, mode) == -1) && (errno != EEXIST))
			bail_out("Cannot mkdir %s", path);
	}
}

/* Copy the given configuration file to system directory,
 * if it does not already exist. */
int logsys_copyconfig(char *cfgfile, char *sysname)
{
	char filename[1024];
	char *basename, *cp;
	int c;
	FILE *from, *to;

	basename = cfgfile;

	for (cp = basename; *cp; ++cp)
    {
		switch (*cp)
        {
#ifdef MACHINE_WNT
		case '\\':
#endif
		case '/':
			basename = cp + 1;
		}
    }

	sprintf(filename, "%s/%s", sysname, basename);

	if ((to = fopen(filename, "r")) != NULL)
    {
		/* It's already there. */
		fclose(to);
		return (0);
	}
	if ((to = fopen(filename, "w")) == NULL)
    {
		/* Cannot copy the configuration, not a fatal error. */
		if (!Quiet)
			fprintf(stderr, "Warning: copy configuration file into system directory: %s\n", dao_syserr_string(errno));
		return (1);
	}
	if ((from = fopen(cfgfile, "r")) == NULL)
		bail_out("%s", cfgfile);

	c = getc(from);
	while (c != EOF)
    {
		if (putc(c, to) == EOF)
			bail_out("%s", filename);
		c = getc(from);
    }

	if (ferror(from))
		bail_out("%s", cfgfile);
	
	fclose(from); 
	fclose(to); 
	
	return (0);
}

/* This function let us create indexes from cfg file
 * without storing anything in the .label file
 * why ? cause this means toomuch changes at the moment
 * this may change if we change RTE syntax to dis/enable index
 * in WHILE/FIND requests.
 */
void logsys_createsqlindexes(char *cfgfile, LogSystem *log)
{
	FILE *fp;
	char *cp     = NULL;
	char buffer[8192];
	char *tag    = NULL;

	LogSQLIndexes *sqlindexes;
	int nsqlindex    = 0;	/* number of sqlindex added so far */
	int nsqlindexmax = 32;	/* number of sqlindex allocated */

	LogSQLIndex *sqlindex = NULL;
	char *sqlindexname = NULL;
	char *fieldname    = NULL;
	int nfields      = 0;	/* number of fields in a sqlindex added so far */
	int nfieldsmax   = 0;	/* number of fields in a sqlindex allocated */

	char *the_filename = cfgfile;
	int   the_lineno = 0;

	if ((fp = fopen(cfgfile, "r")) == NULL)
		return;

	sqlindexes = (LogSQLIndexes *) malloc(sizeof(LogSQLIndexes));
	sqlindexes->sqlindex = (LogSQLIndex **) malloc(nsqlindexmax * sizeof(LogSQLIndex *));

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
			bail_out("%s(%d): Malformed line: %s", the_filename, the_lineno, buffer);
		}
		*cp++ = 0;

		/* we are only interested in reading the SQL indexes */
		if (!strcmp(tag, "SQL_INDEX"))
		{
			/* Allocate space for sqlindex, 32 sqlindex max by default for a sqlindexes */
			if (nsqlindex >= nsqlindexmax - 2)
            {
				nsqlindexmax += 32;
				sqlindexes->sqlindex = (LogSQLIndex **) realloc(sqlindexes->sqlindex, nsqlindexmax * sizeof(LogSQLIndex *));
				if (sqlindexes == NULL)
					bail_out("Out of memory");
			}

			/* now create the sqlindex we are going to add to the sqlindexes */
			nfields = 0;
			nfieldsmax = 32;
			sqlindex = (LogSQLIndex *) malloc(sizeof(LogSQLIndex));
			sqlindex->fields = (LogField **) malloc(nfieldsmax * sizeof(LogField *));
			if (sqlindex == NULL)
				bail_out("Out of memory");

			sqlindexname = cp;
			/* extract the name of the sqlindex, followed by at least 1 fieldname, both mandatory */
			if ((cp=strchr(sqlindexname, ',')) == NULL)
			{
				errno = 0;
				bail_out("%s(%d): Malformed line: %s", the_filename, the_lineno, sqlindexname);
			}
			*cp = 0;
			sqlindex->name = dao_log_strdup(sqlindexname);

			/* split in fields names */
			fieldname = ++cp;
			while ((cp=strchr(fieldname, ',')) != NULL)
			{
				/* just in case ... */
				if (nfields >= nfieldsmax - 2)
				{
					nfieldsmax += 32;
					sqlindex->fields = (LogField **) realloc(sqlindex->fields, nfieldsmax * sizeof(LogField *));
					if (sqlindex == NULL)
						bail_out("Out of memory");
				}
				*cp = '\0';
				sqlindex->fields[nfields] = dao_logsys_getfield(log,fieldname);
				if (sqlindex->fields[nfields] == NULL)
				{
					errno = 0;
					bail_out("%s(%d): wrong field name for SQL index : %s", the_filename, the_lineno, fieldname);
				}
				nfields++;
				fieldname = ++cp;
			}
			/* either last or only field in this SQL index */
			sqlindex->fields[nfields] = dao_logsys_getfield(log,fieldname);
			if (sqlindex->fields[nfields] == NULL)
			{
				errno = 0;
				bail_out("%s(%d): wrong field name for SQL index : %s", the_filename, the_lineno, fieldname);
			}
			nfields++;

			/* we have nfields fields in this sqlindex */
			sqlindex->n_fields = nfields;

			/* add this logsqlindex to the logsqlindexes
			 * of the logsystem */
			sqlindexes->sqlindex[nsqlindex] = sqlindex;
			nsqlindex++;
			sqlindexes->n_sqlindex = nsqlindex;
		}
	}
	fclose(fp);
	if (nsqlindex == 0)
	{
		free(sqlindexes);
		log->sqlindexes=NULL;
	}
	else
	{
		log->sqlindexes=sqlindexes;
	}
}

int get_confirmation(int def)
{
	FILE *fp;
	char line[64];

	fprintf(stderr, "%c", def);
	fflush(stderr);

#ifdef MACHINE_WNT
	if ((fp = fopen("CON", "r")) == NULL)
		perror("\nCannot open CON");
#else
	if ((fp = fopen("/dev/tty", "r")) == NULL)
		perror("\nCannot open /dev/tty");
#endif

	if (!fp)
		return ('n');

	if (fgets(line, sizeof(line), fp) == NULL)
		line[0] = 'n';

	fclose(fp);

	if (line[0] == '\n' || line[0] == 0)
		line[0] = def;

	return (line[0]);
}

