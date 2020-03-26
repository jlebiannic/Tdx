/*========================================================================
:set ts=4 sw=4

	E D I S E R V E R

	File:		logsystem/paramfiles.c
	Author:		Juha Nurmela (JN)
	Date:		Fri Jan 13 13:13:13 EET 1994

	Copyright (c) 1997 Telecom Finland/EDI Operations

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: paramfiles.c 47944 2014-03-07 08:17:33Z dblanchet $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 17.12.97/JN	X=Y handling with regular files now here.
  3.01 25.02.98/JN	rls_setxxxvalue_file() did not work.
  3.02 26.03.98/JN	enum/fetch parameters for LOG.d
  3.03 26.11.98/KP	Totally reworked setaparm() to speed it up.
  3.04 30.05.2008/CRE Change debug_prefix
  3.05 10.02.2014/TCE(CG) TX-2497 improve code reliability when trying to open a file that doesn't exist
========================================================================*/

#ifdef MACHINE_WNT
#include <windows.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#include <sys/time.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "perr.h"
#include "logsysrem.h"

#ifndef DEBUG_PREFIX
#define DEBUG_PREFIX	" [unknown] "
#endif

/*
 *  Parameter-file is completely rewritten,
 *  updated value rises to 1st line in file.
 */
void setaparm(Client *p,
	int how,    /* to decode the type of rlsvalue arg */
	char *path, /* filename containing this and other parameter lines */
	char *name, /* this parameter name */
	void *value_arg)
{
	FILE *fp;
	union rlsvalue *value = value_arg;
	char c, *cp;
	char *valuestring;  /* value_arg converted to string format */
	char valuebuf[512]; /* buffer space used only for doubles and times */

	/* KP */
	char *buf;	/* Buffer to read the possibly existing paramfile */
	int bufsize;	/* Size of the above buffer */
	int fd;
	char oldparm[256];	/* Search string to find an existing paramvalue */
	stat_t statbuf;	/* To store information about an existing file */
	int test = -1; /* TX-2497 TCE(CG) var used for testing fstat return value */

	/*
	 *  XXX check multiline value and prefix each line with name=.
	 *  XXX we do not know the multiline fieldsep...
	 */
	 
	/*
	 *  Value in string format into s
	 */
	valuestring = valuebuf;
	switch (how) {
	case SET_NUM_FIELD:
	case SET_NUM_PARM:
		/*
		 *  XXX precision ???
		 *  Cut off trailing zeroes and the decimal point too
		 *  if fractional part was only zeroes.
		 */
		sprintf(valuestring, "%lf", value->d);

		cp = valuestring + strlen(valuestring);
		while (--cp > valuestring) {
			if ((c = *cp) == '0' || c == '.')
				*cp = 0;
			if (c != '0')
				break;
		}
		break;
	case SET_TIME_FIELD:
	case SET_TIME_PARM: {
			/*
			 *  Year stored in % 100 format.
			 */
			struct tm *tm = localtime(&value->t);

			sprintf(valuestring,
				"%02d.%02d.%02d %02d:%02d:%02d",
				tm->tm_mday,
				tm->tm_mon + 1,
				tm->tm_year % 100,
				tm->tm_hour,
				tm->tm_min,
				tm->tm_sec); /* dd.mm.yy HH:MM:SS */
		}
		break;
	case SET_TEXT_FIELD:
	case SET_TEXT_PARM:
		valuestring = value->s;
		break;
	}

	/*
	 * First try to stat file. If it exists, copy the whole file to memory
	 * for further processing.
	 */
	
	/* TX-2497 TCE(CG) improving stability to avoid crash if fd = -1 */ 	
	fd = open(path, O_RDONLY, 0);
	if (fd != -1) {
					test = fstat(fd, &statbuf);
	}
	
	if (fd != -1 && test == 0) {
	/*END TX-2497 */
		int n = -1;
		
		bufsize = statbuf.st_size + 8192; /* should be more than enough */
		buf = calloc(1, bufsize);

		if (buf) {
			if ((n = read(fd, buf, bufsize)) > 0) {
				/* 
				 * Now find all occurences of our param in buffer,
				 * and remove them.
				 */
				char *c1, *c2;
				buf[n] = 0;	/* Set the terminating null character to buf */
				n = n+1;
				sprintf(oldparm, "%s=", NOTNUL(name));
				c1 = buf;
				while ((c1 = strstr(c1, oldparm)) != NULL) {
					/* Check that c1 really points to beginning of line,
					 * that is, either c1=buf (first line) or c1-- == \n
					 */
					if ((c1 == buf) || ((c1 > buf) && (*(c1-1) == '\n'))) {
						/*
						 * Now c1 points to beginning of the old param,
						 * set c2 to the terminating newline of that...
						 */
						c2 = strchr(c1, '\n');
						if (c2) 
							memcpy(c1, c2+1, (n - (int)(c2 - buf)));
					}
					else
						c1++; 	/* Move forward, just in case we are stuck... */
				}
			}
			close (fd);
#ifdef MACHINE_WNT
			fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, _S_IREAD | _S_IWRITE);
#else
			fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
#endif
			if (fd) {				
				write(fd, buf, strlen(buf));
				close(fd);
			}
			free (buf);
			buf = NULL;
		}
	}

	{
		/*
		 *  Create file if it did not exist, permissions default inside fopen() XXX
		 *  No more content than the new namevalue.
		 */
		int rv, retryopen = 1;
		char *cp, csave;

		while ((fp = fopen(path, "a")) == NULL)
		{
			if ((errno == ENOENT && retryopen--) && ((cp = strrchr(path, '/' )) != NULL || (cp = strrchr(path, '\\')) != NULL))
			{
				csave = *cp;
				*cp = 0;
				rv = mkdir(path, 0755);
				*cp = csave;
				if (rv == 0)
					continue;
			}
			svclog('W', "%s%s",DEBUG_PREFIX, path);
			return;
		}

		if (fp)
		{
			fprintf(fp, "%s=%s\n", NOTNUL(name), NOTNUL(valuestring));
			fclose(fp);
		}
	}
}

/*
 *  Get pARAMETER in any format,
 *  from named file.
 *  Conversions are made for any datatype.
 */
void
getaparm(Client *p,
	int how,
	char *path,
	char *name)
{
	char *cp;
	char *value;
	int namelen;
	time_t tims[2];
	FILE *fp;
	double n = 0.0;
	char *s = "";
	time_t t = 0;
	char line[8192];

	namelen = strlen(name);

	if ((fp = fopen(path, "r")) != NULL)
	{
		while (fgets(line, sizeof(line), fp))
		{
			if (!strncmp(line, name, namelen) && line[namelen] == '=')
			{
				value = line + namelen + 1;
				cp = value + strlen(value) - 1;
				while (*cp == '\r' || *cp == '\n')
					*cp-- = 0;

				switch (how)
				{
					case GET_NUM_FIELD:
					case GET_NUM_PARM: n = atof(value); break;
					case GET_TIME_FIELD:
					case GET_TIME_PARM:
						/*
						 *  Date was stored
						 *  with % 100 year.
						 *  tr_parsetime() is
						 *  supposedly handles
						 *  years 00...38 as
						 *  "being in the future".
						 */
						tr_parsetime(value, tims);
						t = tims[0];
						break;
					case GET_TEXT_FIELD:
					case GET_TEXT_PARM: s = value; break;
				}
				break;
			}
		}
		fclose(fp);
	}
	switch (how)
	{
		case GET_NUM_FIELD:
		case GET_NUM_PARM: vreply(p, RESPONSE, 0, _Double, n, _End); break;
		case GET_TIME_FIELD:
		case GET_TIME_PARM: vreply(p, RESPONSE, 0, _Time, t, _End); break;
		case GET_TEXT_FIELD:
		case GET_TEXT_PARM: vreply(p, RESPONSE, 0, _String, s ? s : "", _End); break;
	}
}

void listparms(Client *p, char *path, int how)
{
	char *cp;
	FILE *fp;
	char line[8192];
	int n = 0;

	if ((fp = fopen(path, "r")) != NULL)
	{
		while ((cp = fgets(line, sizeof(line), fp)) != NULL)
		{
			if (*cp == 0
			 || *cp == '#'
			 || *cp == '\r'
			 || *cp == '\n'
			 || (cp = strchr(cp, '=')) == NULL)
				continue;

			/*
			 *  Where to cut the line, only name
			 *  or both name and the value ?
			 */
			if (how == ENUM_PARMS)
				*cp = 0;
			else
			{
				while (*++cp)
					;
				while (*--cp == '\r' || *cp == '\n')
					*cp = 0;
			}
			append_line(&p->buf, line);
			n++;
		}
		fclose(fp);
		if (DEBUG(12))
			debug("%slistparams: read %d parameters from file %s.\n",DEBUG_PREFIX, n, NOTNUL(path));

	}
	else
		debug("%slistparms: failed to open param file %s for reading.\n",DEBUG_PREFIX, NOTNUL(path));

}

