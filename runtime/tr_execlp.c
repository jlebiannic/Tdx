/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_execlp.c
	Programmer:	Mikko Valimaki
	Date:		Wed Sep 23 13:11:05 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_execlp.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 09.08.93/MV	Added a statement that closes all file descriptors
			from 3 to 100 before exec'ing a new program.
  3.02 06.02.96/JN	windog silliness.
  3.03 27.10.97/JN	More of it, dangerous arguments wrapped in "" on NT.
  3.04 30.03.99/JR	varargs->stdargs
  3.05 07.12.99/JR	Castings addded. Spawn returned 255 when it should
  			have returned -1.
  4.00 27.12.05/LM	Spawnlp return -1 if error like spawnvp bugZ 1532
============================================================================*/

#include <stdio.h>
/* #include <varargs.h> */
#ifdef MACHINE_LINUX
#include <errno.h>
#endif

#ifdef MACHINE_WNT
#undef _POSIX_
#include <process.h>
#define _POSIX_
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "tr_prototypes.h"
#include "tr_strings.h"

#define lowbyte(w)  ((w) & 0377)
#define highbyte(w) lowbyte ((w) >> 8)

extern char *TR_EMPTY;

/*
 *  NT executes can split command arguments from whitespace.
 *  There is no good solution for this, but with this one,
 *  at least embedded blanks work.
 */
#ifdef MACHINE_WNT

void tr_wrap_exec_argv(char **v)
{
	char *s;

	for ( ; v && *v; ++v) {
		for (s = *v; *s; ++s) {
			if (!isalnum(*s)
			 && !ispunct(*s))
				break;
		}
		if (*s) {
			s = malloc(1 + strlen(*v) + 1 + 1);
			strcpy(s, "\"");
			strcat(s, *v);
			strcat(s, "\"");
		} else {
			s = strdup(*v);
		}
		*v = s;
	}

}

void tr_free_exec_argv(char **v)
{
	while(v && *v)
		free(*v++);
}
#endif

/*============================================================================
============================================================================*/
void tr_Execlp ( char *args, ... )
{
	char    *argv[65536];
	int     i;
	va_list ap;
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	fflush (stderr);
	fflush (stdout);
	va_start (ap, args);
	argv[0] = args;
	
	for (i = 1; i < 65535; i++) {
		if (!(argv[i] = va_arg (ap, char *)))
			break;
	}
	if (i == 65535)
		tr_Log (TR_WARN_TOO_MANY_ARGS_IN_EXEC);
	argv[i] = NULL;
	va_end (ap);
	for (i = 3; i < 100; i++)
		close (i);

#ifdef MACHINE_WNT
	tr_wrap_exec_argv(argv);
	if ( _spawnvp ( _P_WAIT, argv[0], argv) != -1 )
		_exit(0); 			/* Don't do atexit procedures */

	if (errno < 1 || errno > 255)
		_exit(255);
	
	_exit(errno);
#else
	execvp (argv[0], argv);
#endif
	tr_Fatal (TR_ERR_EXEC_FAILED, argv[0], errno);
}

/*==========================================================================
==========================================================================*/
double tr_Spawnlp ( char *args, ... )
{
	va_list ap;
	int	status;
	int	code;
	char    *argv[256];
	int     i;
	int     pid;
	char    *tmp;
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	fflush (stderr);
	fflush (stdout);

#ifdef MACHINE_WNT
	va_start (ap, args);
	argv[0] = args;

	for (i = 1; i < 255; i++) {
		if (!(argv[i] = va_arg (ap, char *)))
			break;
	}
	if (i == 255)
		tr_Log (TR_WARN_TOO_MANY_ARGS_IN_EXEC);
	argv[i] = NULL;
	va_end (ap);

	tr_wrap_exec_argv(argv);

	/* 3.05/JR
	 * Someone might actually want to use negative return values
	 * from _spawnlp()
	 */
	errno = 0;
	status = (double) _spawnvp(_P_WAIT, argv[0], argv);

	/* 3.05/JR
	 * If _spawnlp fails, it sets errno != 0
	 * if (status == -1) {
	 */
	if ((status == -1)&&(errno != 0)) {
		tr_Log (TR_ERR_EXEC_FAILED, argv[0], errno);
		/*status = 1;*/
	}
	tr_free_exec_argv(argv);

	/* 3.05/JR */
	return ((double) status);

#else /* not WNT */

	argv[0] = args;
	switch (pid = fork ()) {
	case -1:
		tr_Log (TR_ERR_CANNOT_CREATE_PROCESS, errno);
		return -1;
	case 0:
		va_start (ap, args);
		/*
		**  This is the child process.
		*/
		for (i = 1; i < 255; i++) {
			if (!(argv[i] = va_arg (ap, char *)))
				break;
		}
		if (i == 255)
			tr_Log (TR_WARN_TOO_MANY_ARGS_IN_EXEC);
		argv[i] = NULL;
		va_end (ap);
		for (i = 3; i < 100; i++)
			close (i);
		execvp (argv[0], argv);
		tr_Fatal (TR_ERR_EXEC_FAILED, argv[0], errno);
		return -1; /* To avoid warning */
		break;
	default:
		while (wait (&status) != pid)
			;
		if (lowbyte (status) == 0) {
			/* 3.05/JR */
			return ((double) ((signed char) highbyte (status)));
		} else {
			tr_Log (TR_MSG_TERMINATED_BY_SIGNAL, argv[0], status & 0177);
			if ((lowbyte(status) & 0200) == 0200)
				tr_Log (TR_MSG_CORE_DUMPED);
			return -1;
		}
		break;
	}
#endif /* not WNT */
}

