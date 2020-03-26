/*========================================================================
        E D I S E R V E R

        File:		rmunref.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Remove unreferenced, dangling files, that have no record.

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: rmunref.c 55239 2019-11-19 13:50:31Z sodifrance $")
LIBRARY(liblogsystem_version)
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  4.01 08.08.02/CD      Recursivity was not working for files with same index
			in different subdirectory - needed to call rmunref
		   	more than once in order to have it working ... 
			It was just a question of using bitset instead of
			set_bit in the main loop and now it works fine.
  4.02 28.03.03/JT	lsunref command added
  4.03 19.05.05/CD	Extention of allocation not done correctly in set_bit.
                        resulting in a core dump when the INDEX 80000 is in 
			a valid entry	
  4.04 30.06.05/CD	The -q option will put the rmunref into quiet mode.
  4.05 05.12.06/LM  Ensure that bitfield is large enough in all cases
  4.06 29.12.14/YLI(CG) TX-2649 New entries in progress are not removed any more during the purge
  4.07 15.01.19/CD   TX-3109: prevent too many openfiles error in lstool fix, lsunref and rmunref
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef MACHINE_WNT
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include "logsystem/lib/logsystem.h"
#include "logsystem/lib/lstool.h"
#include "logsystem/lib/port.h"

int lsonly = 0;
static int bitsize;
static unsigned char *bitfield;

static int  set_bit(int);
static int  bitset(int);
static void dodir(LogSystem *,char *);

void other_options(int argc, char **argv);

int
do_rmunref(argc, argv)
	int argc;
	char **argv;
{
	LogSystem *log;
	LogEntry *entry;
	LogIndex idx;

	other_options(argc, argv);

	log = logsys_open(Sysname, LS_DONTFAIL | LS_READONLY);

	if (!Quiet) {
		printf("Looking up active entries...\n");
	}
	fflush(stdout);

	/*
	 *  See which records are in use.
	 */
	for (idx = 0; ; ++idx) {
		entry = logentry_readraw(log, idx);
		if (entry == NULL){
			break;
		}
		if (ACTIVE_ENTRY(entry->record)) {
			set_bit(entry->record_idx);
		}

		logentry_free(entry);
	}
	
	if (!Quiet){
		if (lsonly){
			printf("Listing stale files...\n");
		}
    }else{
		printf("Purging stale files...\n");
    }
	fflush(stdout);

	/*
	 *  Clean them recursively.
	 */
	dodir(log,logsys_filepath(log, LSFILE_FILES));

	if (!Quiet) { 
		printf("Done.\n");
	}
	exit(0);
}

int
do_lsunref(argc, argv)
	int argc;
	char **argv;
{
	lsonly = 1;
	do_rmunref(argc, argv);
}

/*
 *  Recursive function to remove every stale file under path.
 *  If a filename starts with a number (or is a number)
 *  and corresponding record is not in use, the file is removed.
 *  All directories are recursed into.
 *  TX-3109: in order to prevent too many openfiles error, log is only open before calling dodir and given as an attribute to dodir
 */

static void
dodir(log, path)
	LogSystem *log;
	char *path;
{
#ifdef MACHINE_WNT
	HANDLE dirp;
	WIN32_FIND_DATA dd;
#else
	DIR *dirp;
	DIRENT *dd;
	struct stat st;
#endif
	char *dname;
	char *slash;
	char *base;
	/*4.06 29.12.14/YLI(CG) TX-2649 declare the variables*/
	FILE *fp;
	LogEntry *entry;
	char * pathTmp;
	/*End 4.06 29.12.14/YLI(CG) TX-2649 */
	slash = path + strlen(path);
	base = slash + 1;

	/*
	 *  Look for both old- and newstyle filenames.
	 */
#ifdef MACHINE_WNT
	*slash = '\\';
	strcpy(base, "*");
	dirp = FindFirstFile(path, &dd);
	if (dirp != INVALID_HANDLE_VALUE)
#else
	dirp = opendir(path);
	*slash = '/';
	if (dirp)
#endif
	{
#ifdef MACHINE_WNT
		do
#else
		while (dd = readdir(dirp))
#endif
		{
			dname =
#ifdef MACHINE_WNT
				dd.cFileName
#else
				dd->d_name
#endif
			;
			/*
			 *  Use just the initial number, suffixing ignored.
			 */
			if (dname[0] == '.'){
				continue;
			}
			strcpy(base, dname);
#ifndef MACHINE_WNT
			if (stat(path, &st)){
				continue;
			}
			if ((st.st_mode & S_IFMT) == S_IFDIR)
#else
			if (dd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
#endif
			{
				dodir(log,path);
			}
			else
			{
				/* 4.01/CD use bitset instead of set_bit as we are consulting the set,
				           but not setting it !!!
				*/
				if (isdigit(dname[0])
				 && !bitset(atoi(dname))) {
				 /*4.06 29.12.14/YLI(CG)  TX-2649 save the variable path to remove it later if possible,
				 if bitset is not used for an entry and it does not exist in .data, remove it */
					pathTmp = strdup(path);
					entry = logentry_readindex(log, atoi(dname));
					if (entry == NULL) {
					/*End 4.06 29.12.14/YLI(CG)*/
						printf("%s\n", pathTmp);
						if (lsonly == 0){
							unlink(pathTmp);
						}
					}
					strcpy(path,pathTmp);
				}
			}

		}
#ifdef MACHINE_WNT
		while (FindNextFile(dirp, &dd));
		FindClose(dirp);
#else
		closedir(dirp);
#endif
	}
	*slash = 0;
}

/*
 * Mark index in bitmap.
 * Grow bitmap as needed.
 * Return previous state of index, marked or not.
 */
static int
set_bit(bit)
	int bit;
{
	int was;
	int hunk = 10000;

	if (bitfield == NULL) {
		bitsize = hunk * 8;
		bitfield = (void *) malloc(bitsize / 8);
		memset(bitfield, 0, hunk);
	}
	while (bit >= bitsize) { 
		/* 4.03/CD 
		   prevent core dump by replacing > by >= 
		   otherwise we have a core when the entry 8000 (index 80000)
		   is a valid entry 
		*/

		bitsize += hunk * 8;
		bitfield = (void *) realloc(bitfield, bitsize / 8);
		if (bitfield == NULL) {
			fprintf(stderr, "Out of memory, %d\n", bit);
			exit(1);
		}
		memset(bitfield + bitsize / 8 - hunk, 0, hunk);
	}
	was = (bitfield[bit / 8] & (1 << (bit % 8)));

	bitfield[bit / 8] |= (1 << (bit % 8));

	return (was);
}

/*
 * Is index marked ?
 */
static int
bitset(bit)
	int bit;
{
	if (bit >= bitsize){
		return (0);
	}
	return (bitfield[bit / 8] & (1 << (bit % 8)));
}

