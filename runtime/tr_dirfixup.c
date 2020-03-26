/*===========================================================================
	E D I S E R V E R

	File:		tr_dirfixup.c
	Programmer:	Juha Nurmela (JN)
	Date:		Mon May  8 09:03:50 EET 1995

	Copyright (c) 1995 Telecom Finland/EDI Operations

	Sort of like mkdir -p

	tr_fopen uses this (of course only with "w or "a flags).

	Whenever opening a file for writing fails,
	this could be called to check if the creation failed
	because of nonexisting directories along the path.

	Currently only one level of path-sections is checked.
	If more levels are needed/wanted, just add a recursive call.

	Sample usage:

		char *path = "dir/fil";
		FILE *fp = fopen(path, "w");
		if (fp == NULL && tr_DirFixup(path))
			fp = fopen(path, "w");
		if (fp == NULL)
			error...

	global variable LOGSYS_EXT_IS_DIR determines
	if datafiles are located in .../files/IDX.EXT
	or in .../files/EXT/IDX

	This stuff was created because of the problem where
	new extensions failed (the /EXT/ directory did not exist).

	However, this is NOT disabled by the compability-mode,
	so lazy creation of files without first mkdirring
	is STILL POSSIBLE.

===========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: tr_dirfixup.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*===========================================================================
  Record all changes here and update the above string accordingly.
  3.00 08.05.95/JN	Created.
  3.01 08.02.96/JN	Micro$oft paths.
  3.02 28.01.97/JN	A:\nd\corrections/for/them to accept this kind.
===========================================================================*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#ifndef MACHINE_LINUX
extern int errno;
#endif

/*
 * Returns nonzero when successfully fixed the problem.
 */
int tr_DirFixup(char *problem_path)
{
	char *cp;
	int saverr = errno;
	int parent_len;
	char parent[1024];

	/*
	 * errno will be ENOENT
	 * if open failed due to
	 * nonexisting parts of path.
	 */
	if (saverr != ENOENT)
		return (0);

	/*
	 * No directory parts
	 * or ridiculously long path ?
	 */
#ifndef MACHINE_WNT
	if ((cp = strrchr(problem_path, '/')) == NULL)
		return (0);
#else
	{
		/*
		 *  Accept either / or \ or a mixture of them both.
		 *  Scan thru problem_path and remember last
		 *  separator seen.
		 */
		char *cp2;

		cp = NULL;
		for (cp2 = problem_path; *cp2; ++cp2)
			if (*cp2 == '\\'
			 || *cp2 == '/')
				cp = cp2;
		if (cp == NULL)
			return (0);
	}
#endif

	parent_len = cp - problem_path;

	if (parent_len <= 0 || parent_len >= sizeof(parent) - 1)
		return (0);

	memcpy(parent, problem_path, parent_len);
	parent[parent_len] = 0;

	/*
	 * If the parent exists, then we cannot help.
	 */
	if (mkdir(parent, 0777) != 0) {
		errno = saverr;
		return (0);
	}
	/*
	 * Hooray !
	 * Restore errno anyway,
	 * even a successful mkdir might have changed it.
	 */
	errno = saverr;

	return (1);
}

