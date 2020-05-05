/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_execvp.c
	Programmer:	Mikko Valimaki
	Date:		Wed Sep 23 13:11:05 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_execvp.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 09.08.93/MV      Added a statment that closes all file descriptors
			from 3 to 100 before exec'ing a new program.
  3.02 31.01.96/JN	Just added local_config...
  3.03 06.02.96/JN	nLastBackgroundPID. NT sillines.
       13.02.96/JN	tr_am_ change
       19.03.96/JN	ATT. NOFILE def was keyed with i386, changed to !NOFILE
  3.04 27.10.97/JN	Dangerous command arguments wrapped in "" on NT.
  3.05 05.02.98/JN	Middle process in background() was left as zombie.
  3.06 01.06.99/HT,KP	Win NT changes:
			"/dev/null" check in tr_Background
			Was: tr_Background spawned (with p_wait) tr_BackgroundExec
			which in turn spawned (with p_detach) the actual process.
			Now: tr_Background spawns (with p_detach) tr_BackgroundExec
			which then creates the actual process and waits for it to
			terminate.
			tr_BackgroundExec changed to use CreateProcess
			directly.
  3.07 07.12.99/JR	Castings addded. Spawn returned 255 when it should
  			have returned -1.
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
============================================================================*/

#include <stdio.h>
#include <signal.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>
#endif
#include <fcntl.h>

#ifdef MACHINE_WNT
#undef _POSIX_
#include <process.h>
#include <io.h>
#include <sys/stat.h>
#define _POSIX_
#endif

#ifndef MACHINE_WNT
#include <strings.h>
#endif

#include <string.h>
#include <errno.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"

#ifndef MACHINE_WNT
#ifndef NOFILE /* SCO */
#define NOFILE NOFILES_MIN
#endif
#endif

#ifndef MACHINE_LINUX
extern int errno;
#endif

/*============================================================================
============================================================================*/
void tr_Execvp (char *command, char *arrayname)
{
	static char *argv[65536];
	int  i = 0;
	struct am_node *walkpos;
	char *idx, *val;

	argv[i++] = command;

	walkpos = tr_am_rewind(arrayname);

	while (tr_amtext_getpair(&walkpos, &idx, &val)) {
		if (i > 65535) {
			tr_Log (TR_WARN_TOO_MANY_ARGS_IN_EXEC);
			break;
		}
		if (val && *val)
			argv[i++] = val;
	}
	argv[i] = NULL;
	fflush (stdout);
	fflush (stderr);
#ifdef MACHINE_WNT
	_fcloseall();
#else
	for (i = 3; i < 100; i++)
		close (i);
#endif

#ifdef MACHINE_WNT
	tr_wrap_exec_argv(argv);
	if ( _spawnvp ( _P_WAIT, command, argv) != -1 )
		_exit(0); 			/* Don't do atexit procedures */

	if (errno < 1 || errno > 255)
		_exit(255);
	
	_exit(errno);
#else
	execvp (command, argv);
#endif
	tr_Fatal (TR_ERR_EXEC_FAILED, command, errno);
}

/*============================================================================
============================================================================*/
#define lowbyte(w)	((w) & 0377)
#define highbyte(w)	lowbyte ((w) >> 8)

double tr_Spawnvp (char *command, char *arrayname)
{
	int  status;

#ifdef MACHINE_WNT

	static char *argv[256];
	int  i = 0;
	struct am_node *walkpos;
	char *idx, *val;

	argv[i++] = command;

	walkpos = tr_am_rewind(arrayname);

	while (tr_amtext_getpair(&walkpos, &idx, &val)) {
		if (i > 255) {
			tr_Log (TR_WARN_TOO_MANY_ARGS_IN_EXEC);
			break;
		}
		if (val && *val)
			argv[i++] = val;
	}
	argv[i] = NULL;
	fflush (stdout);
	fflush (stderr);

	tr_wrap_exec_argv(argv);
	status = _spawnvp (_P_WAIT, command, argv);
	if (status == -1) {
		tr_Log (TR_ERR_EXEC_FAILED, command, errno);
	}
	tr_free_exec_argv(argv);
	/* 3.07/JR */
	return ((double) status);

#else /* not WNT */

	int  pid;

	fflush (stdout);
	fflush (stderr);

	switch (pid = fork ()) {
	case -1:
		tr_Log (TR_ERR_CANNOT_CREATE_PROCESS, errno);
		return -1;
	case 0:
		/*
		**  This is the child process.
		*/
		tr_Execvp (command, arrayname);
		break;
	default:
		while (wait (&status) != pid)
			;
		if (lowbyte (status) == 0) {
			/* 3.07/JR */
			return ((double) ((signed char) highbyte (status)));
		} else {
			tr_Log (TR_MSG_TERMINATED_BY_SIGNAL, command, status & 0177);
			if ((lowbyte(status) & 0200) == 0200)
				tr_Log (TR_MSG_CORE_DUMPED);
			return -1;
		}
		break;
	}
	return 0;
#endif /* not WNT */
}

/*============================================================================
============================================================================*/

double nLastBackgroundPID;

double tr_Background (char *command, char *arrayname, char *input, char *output, char *log)
{
	int status;

#ifdef MACHINE_WNT

	static char *argv[5 + 256];
	int  i = 0;
	struct am_node *walkpos;
	char *idx, *val;

	if ((input == NULL)
	 || (input[0] == 0)
	 /* 3.06/HT */
	 || (!strcmp(input, "/dev/null")))
		input = "NUL";

	if ((output == NULL)
	 || (output[0] == 0)
	 || (!strcmp(output, "/dev/null")))
		output = "NUL";

	if ((log == NULL)
	 || (log[0] == 0)
	 || (!strcmp(log, "/dev/null")))
		log = "NUL";

	argv[i++] = tr_progName;
	argv[i++] = "-background";
	argv[i++] = input;
	argv[i++] = output;
	argv[i++] = log;
	argv[i++] = command;

	walkpos = tr_am_rewind(arrayname);

	while (tr_amtext_getpair(&walkpos, &idx, &val)) {
		if (i >= sizeof(argv)/sizeof(argv[0]) - 1) {
			tr_Log (TR_WARN_TOO_MANY_ARGS_IN_EXEC);
			break;
		}
		if (val && *val)
			argv[i++] = val;
	}
	argv[i] = NULL;

	fflush (stdout);
	fflush (stderr);

	/*
	 *  spawn us to really _p_detach the command.
	 */
	status = _spawnvp(_P_NOWAIT, tr_progName, argv);

	if ((HANDLE)status == INVALID_HANDLE_VALUE) {
		errno = GetLastError();
		tr_Log (TR_ERR_EXEC_FAILED, command, errno);
	}
	/* 3.07/JR */
	return ((double) status);

#else /* not WNT */

	int  i;
	int tube[2];
	int err;
	char pidbuf[32];
	int tmpproc;

	if (input == NULL
	 || input[0] == 0)
		input = "/dev/null";

	if (output == NULL
	 || output[0] == 0)
		output = "/dev/null";

	if (log == NULL
	 || log[0] == 0)
		log = "/dev/null";

	fflush (stdout);
	fflush (stderr);

	if (pipe(tube))
		tube[0] = tube[1] = -1;

	/*
	 *  Create temporary process between us and the background process.
	 */
	switch (tmpproc = fork ()) {
	case -1:
		err = errno;
		tr_Log (TR_ERR_CANNOT_CREATE_PROCESS, errno);
		errno = err;
		return 1.0;
	case 0:
		/*
		 *  This is the temporary process.
		 *  Become process group leader and fork (again)
		 *  the final backgrounded process.
		 */
		setpgrp ();
		/*
		 *  This is unnecessary.
		 */
		signal (SIGHUP, SIG_IGN);

		switch (fork ()) {
		case -1:
			tr_Log (TR_ERR_CANNOT_CREATE_PROCESS, errno);
			_exit (1);
		case 0:
			/*
			 *  Final child, the backgrounded process.
			 *  We are NOT a process group NOR a session leader,
			 *  so we never get a controlling tty
			 *  even by opening as tty by accident.
			 *
			 *  Tell initial parent (the TCL prog) our pid.
			 *  exec might fail, but process was however created.
			 */
			sprintf(pidbuf, "%d", getpid());
			write(tube[1], pidbuf, strlen(pidbuf) + 1);

			tr_FileReopen (input, stdin);
			tr_FileReopen (output, stdout);
			tr_FileReopen (log, stderr);
			/*
			 *  This closes the tubes too,
			 *  but just in case...
			 */
			for (i = 3; i < NOFILE; i++)
				close (i);
			if (tube[0] >= i)
				close(tube[0]);
			if (tube[1] >= i)
				close(tube[1]);

			tr_Execvp (command, arrayname);
			_exit(0);
		default:
			/*
			 *  Temporary process does nothing more
			 */
			_exit (0);
		}
		break;
	default:
		/*
		 *  Initial parent (the TCL program).
		 *  Read in backgrounded pid.
		 */
		if (tube[1] != -1)
			close(tube[1]);
		if (tube[0] == -1)
			nLastBackgroundPID = 0.0;
		else {
			int n = 0;
			int nmax = sizeof(pidbuf) - 1;

			for (;;) {
				i = read(tube[0], &pidbuf[n], nmax - n);
				if (i <= 0)
					break;
				n += i;
				if  (n >= nmax)
					break;
			}
			close(tube[0]);

			pidbuf[n] = 0;
			nLastBackgroundPID = atoi(pidbuf);
		}
		if (nLastBackgroundPID <= 0.0 && tr_errorLevel >= 1.0) {
			tr_Log("Warning: PID of backgrounded process N/A");
		}
		/*
		 *  Clear out the zombie temporary process.
		 *  Keep waiting if interrupted.
		 *  This should never block for more than
		 *  few millisecs, the middle process did only
		 *  setpgrp(), signal(), fork() and exit().
		 */
		while ((i = waitpid(tmpproc, NULL, 0)) != tmpproc)
			if (errno != EINTR)
				break;

		return 0.0;
	}
	/*
	 *  Never gets here
	 */
	return 0.0;

#endif /* not WNT */
}

#ifdef MACHINE_WNT

/*
 *  Kludgery for Windows.
 *  background() spawns the original program (itself),
 *  which then _P_DETACHes the wanted new program,
 *  after redirecting descriptors.
 *
 *  main branches here when it sees argv as
 *  xxx -background iii ooo lll rest
 */
void tr_BackgroundExec(char **argv)
{
	int i;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	char av[8192];
	DWORD retval;
	int nfd;
	

	argv += 2; /* skip tr_progName -background */

	tr_FileReopen( *argv++, stdin );
	tr_FileReopen( *argv++, stdout );
	tr_FileReopen( *argv++, stderr );

	_fcloseall();

	/*
	 * 3.06/KP : Replaced spawn with CreateProcess + Wait
	 *
	 */
#if 0

	/*
	 *  At least this works, but home made tastes better
	 * 02.06.99/HT
	 */
	 _spawnvp( _P_WAIT, *argv, argv);
	 
	tr_fclose( stdin, 0 );
	tr_fclose( stdout, 0 );
	tr_fclose( stderr, 0 );
	 
	_exit(0);
	 

#else

	/*
	 * Init neccessary structures
	 */
	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);

	/*
	 * Wrap argv around ""
	 */
	tr_wrap_exec_argv(argv);

	/*
	 * Expang arguments from argv to single line
	 */
	*av = 0;
	for (i=0; argv[i];  i++) {
		if ((strlen(av) + strlen(argv[i])) > sizeof(av)) {
			tr_Fatal("Too long arg list for command %s.\n", argv[0]);
		}
		strcat(av, argv[i]);
		strcat(av, " ");
	}
	av[strlen(av)-1] = 0;

	si.dwFlags |= STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOWMINNOACTIVE;

	si.dwFlags |= STARTF_USESTDHANDLES;
	nfd = _fileno(stdin);
	if ( nfd >= 0 ) 
		si.hStdInput = (HANDLE)_get_osfhandle(nfd);
	else
		si.hStdInput = GetStdHandle( STD_INPUT_HANDLE);
	nfd = _fileno(stdout);
	if ( nfd >= 0 ) 
		si.hStdOutput = (HANDLE)_get_osfhandle(nfd);
	else
		si.hStdOutput = GetStdHandle( STD_OUTPUT_HANDLE);
	nfd = _fileno(stderr);
	if ( nfd >= 0 ) 
		si.hStdError = (HANDLE)_get_osfhandle(nfd);
	else
		si.hStdError = GetStdHandle( STD_ERROR_HANDLE);

	retval = 0;
	if (!CreateProcess(NULL,
			av,
			NULL,
			NULL,
			TRUE,
			/* DETACHED_PROCESS, */
			CREATE_NO_WINDOW,
			NULL,
			NULL,
			&si,
			&pi)) {
		/*
		 * Failed to create new process
		 */
		tr_Log("Failed to create new process %s\n", argv[0]);
	}
	else {
		if (WaitForSingleObject(pi.hProcess, INFINITE) == WAIT_FAILED) {
			/*
			 * The Wait failed.
			 */
			tr_Log("Failed to wait for process %s (%d).\n", argv[0], GetLastError());
		}
		else
			GetExitCodeProcess( pi.hProcess, &retval );
		CloseHandle( pi.hProcess );

	}

	tr_fclose( stdin, 0 );
	tr_fclose( stdout, 0 );
	tr_fclose( stderr, 0 );

	_exit((int)retval);
#endif

}

#endif

