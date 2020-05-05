#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_counter.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*==============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_counter.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 11:01:41 EET 1992
==============================================================================*/
#ifdef MACHINE_WNT

/* replace whole file */

#include "tr_counter.c.w.nt"

#else /* ! WNT */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"
#include <errno.h>

/*==============================================================================
  Record all changes here and update the above string accordingly.
  12.1.93/MV	v. 3.00
  05.08.94/MW	v. 3.01
	The tr_fclose added to avoid the "Too many files open" errno 24
	error with counters.
  08.11.05/LM   v. 4.00  Added bfSetCounter
	28.03.17/CD TX-2960 : rewrite the counters for eftp bad connections.
	                 add possibility to set the directory for the counters
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
==============================================================================*/

/*======================================================================
  XXX Remove directory search !
======================================================================*/
double tr_getcounter_with_dir(char *cptdir, char *name)
{
	/* TX-2960 CD: add the cptdir to specify a directory for the counters */

	FILE *fp;
	char buf[BUFSIZ];
	char *tmp;
	DIR  *dirp;
	struct dirent *dp;
	double value = 0;
	int  bufLen;
	int  found = 0;
	int errc;
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	bufLen = strlen(strcpy(buf,cptdir));
	if (!(dirp = opendir (buf))) {
		if (errno == ENOMEM) {
			tr_OutOfMemory();  /* log error and exit */
		}
		if (errno != ENOENT) {
			tr_Log (TR_WARN_DIR_OPEN_FAILED, buf, errno);
			return value;
		}
	}
	if (dirp) {
	    while ((dp = readdir (dirp)) != NULL ) {
		if (!strcmp (name, dp->d_name)) {
			found = 1;
			sprintf (buf + bufLen, "/%s", dp->d_name);
			if (!(fp = tr_fopen (buf, "r+"))) {
				tr_Log (TR_WARN_FILE_R_OPEN_FAILED, buf, errno);
				break;
			}
			if (lockf (fileno(fp), F_LOCK, 0)) {
				tr_Log (TR_WARN_LOCKING_FAILED, buf, errno);
				tr_fclose (fp, 0);
				break;
			}
			fscanf (fp, "%lf", &value);

			if (fseek (fp, 0L, 0)) {
				tr_Log (TR_WARN_SEEK_FAILED);
				tr_fclose (fp, 0);
				break;
			}
			tr_mbFprintf (fp, "%.0lf\n", value + 1);
			fflush (fp);
			if (fseek (fp, 0L, 0)) {
				tr_Log (TR_WARN_SEEK_FAILED);
				tr_fclose (fp, 0);
				break;
			}
			if (lockf (fileno(fp), F_ULOCK, 0)) {
				tr_Log (TR_WARN_LOCK_RELEASE_FAILED, buf, errno);
				tr_fclose (fp, 0);
				break;
			}
			/*
			** v. 3.01 (05.08.94/MW)
			** tr_fclose added.
			*/
			tr_fclose(fp, 0);
			break;
		}
	    }
	    closedir (dirp);
	}
	if (!found) {
		/*
		**  If the directory did not exist,
		**  let's create it.
		*/

		bufLen = strlen(strcpy(buf,cptdir));

		if (strcmp(buf,""))
		{
			errc=mkdir(buf, 0777);
			if (errc==-1 && errno!=EEXIST)
			{
				tr_Log("ERROR creating %s for counter %s (%d)",buf,name,errno);
			}
		}

		/*
		**  If the file did not exist,
		**  let's create it.
		*/
		sprintf (buf + bufLen, "/%s", name);
		if (fp = tr_fopen (buf, "w")) {
			tr_mbFprintf (fp, "1\n");	/* The next time we read the one */
			tr_fclose (fp, 0);
		} else {
			tr_Log (TR_WARN_FILE_W_OPEN_FAILED, buf, errno);
		}
	}
	return value;
}


double tr_getcounter (char *name)
{
	/* TX-2960 CD: Use the default $HOME/.counters and call tr_getcounter_with_dir */

	char buf[BUFSIZ];
	char *tmp;
	int  bufLen;

	if (!(tmp = getenv ("HOME"))) {
		tr_Log (TR_WARN_HOME_NOT_SET);
		return 0;
	}
	bufLen = sprintf (buf, "%s/%s", tmp, TR_DIR_COUNTERS);
  return(tr_getcounter_with_dir(buf,name));
}

int tr_setcounter_with_dir(char *cptdir, char *name, double value)
{
	/* TX-2960 CD: add the cptdir to specify a directory for the counters */

	FILE *fp;
	char buf[BUFSIZ];
	DIR  *dirp;
	struct dirent *dp;
	int  bufLen;
	int  found = 0;
	int result = 0;
	int errc;
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	bufLen = strlen(strcpy(buf,cptdir));

	if (!(dirp = opendir (buf))) {
		if (errno == ENOMEM) {
			tr_OutOfMemory();  /* log error and exit */
		}
		if (errno != ENOENT) {
			/* other error than "does not exist" will produce an error */
			tr_Log (TR_WARN_DIR_OPEN_FAILED, buf, errno);
			return result;
		}
	}
	if (dirp) {
	   /* directory is open, so try to look for the counter file */
	   while ((dp = readdir (dirp)) != NULL ) {
		if (!strcmp (name, dp->d_name)) {
			found = 1;
			sprintf (buf + bufLen, "/%s", dp->d_name);
			if (!(fp = tr_fopen (buf, "r+"))) {
				tr_Log (TR_WARN_FILE_R_OPEN_FAILED, buf, errno);
				break;
			}
			if (lockf (fileno(fp), F_LOCK, 0)) {
				tr_Log (TR_WARN_LOCKING_FAILED, buf, errno);
				tr_fclose (fp, 0);
				break;
			}
			if (fseek (fp, 0L, 0)) {
				tr_Log (TR_WARN_SEEK_FAILED);
				tr_fclose (fp, 0);
				break;
			}
			tr_mbFprintf (fp, "%.0lf\n", value);
			fflush (fp);
			if (fseek (fp, 0L, 0)) {
				tr_Log (TR_WARN_SEEK_FAILED);
				tr_fclose (fp, 0);
				break;
			}
			if (lockf (fileno(fp), F_ULOCK, 0)) {
				tr_Log (TR_WARN_LOCK_RELEASE_FAILED, buf, errno);
				tr_fclose (fp, 0);
				break;
			}
			/*
			** v. 3.01 (05.08.94/MW)
			** tr_fclose added.
			*/
			tr_fclose(fp, 0);
			result = 1;
			break;
		}
	    }
	    closedir (dirp);
	}
	if (!found) {
		/*
		**  If the directory did not exist,
		**  let's create it.
		*/
		bufLen = strlen(strcpy(buf,cptdir));
		if (strcmp(buf,""))
		{
			errc=mkdir(buf, 0777);
			if (errc==-1 && errno!=EEXIST)
			{
				tr_Log("ERROR creating %s for counter %s (%d)",buf,name,errno);
			}
		}

		/*
		**  If the file did not exist,
		**  let's create it.
		*/
		sprintf (buf + bufLen, "/%s", name);
		if (fp = tr_fopen (buf, "w")) {
			tr_mbFprintf (fp, "%.0lf\n", value);

			fflush (fp);

			tr_fclose (fp, 0);
		} else
			tr_Log (TR_WARN_FILE_W_OPEN_FAILED, buf, errno);
	}
	return result;

}

int tr_setcounter(char *name, double value)
{
	/* TX-2960 CD: Use the default $HOME/.counters and call tr_setcounter_with_dir */

	char buf[BUFSIZ];
	char *tmp;
	int  bufLen;

	if (!(tmp = getenv ("HOME"))) {
		tr_Log (TR_WARN_HOME_NOT_SET);
		return 0;
	}
	bufLen = sprintf (buf, "%s/%s", tmp, TR_DIR_COUNTERS);
	return tr_setcounter_with_dir(buf,name,value);
}

#endif /* ! WNT */

int bfSetCounter(char *name, double value)
{
  return tr_setcounter(name, value);
}
