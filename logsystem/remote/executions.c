/*========================================================================
:set ts=4 sw=4

	E D I S E R V E R

	File:		logsystem/executions.c
	Author:		Juha Nurmela (JN)
	Date:		Fri Jan 13 13:13:13 EET 1994

	Copyright (c) 1997 Telecom Finland/EDI Operations

	Client can request for execution of any command line,
	(one restriction: no "" arguments)
	but server decides if the given command is safe to execute.

	Commandline is fully expanded from environment and filesystem,
	like the shell does it, but simplified. First word (the program)
	is first replaced from a table of allowed programs.

	Unsafeties like `` and () are not expanded, but passes as is.
	Shell is not involved at all, so pipelines do not work.
	Use custom shell scripts instead.

	Some environment variables are set for the executed command:

	$LOGSYS is client->base if it is set.
	$INDEX is client->base->index if it is set. XXX fails remotely.

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: executions.c 47371 2013-10-21 13:58:37Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  4.00 02.03.98/JN	Created.
  4.01 09.09.98/VA	Kludge to make remote exec work... :P
  4.02 09.09.98/VA	Horrible kludges to make remote exec work in windog... :P
  4.03 22.04.99/KP	Some fiddling with i/o -handles in WNT
  4.04 14.10.99/KP	New synthetic handler stuff in do_rls_exec()
  4.05 11.01.00/KP,HT	When exec()'ing, do _exit() in the fork()ed listener.
  4.06 30.05.2008/CRE Change debug_prefix
========================================================================*/

#ifdef MACHINE_WNT
#include <windows.h>
#include <process.h>
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

#ifdef MACHINE_WNT
struct thr_info {
	Client *p;
	int	local;
	char *mode;
	char **cmd;
};

#endif

char *buck_expand(char *pat);

static void prep_env(Client *p)
{
	int x;
	static char envidx[256];
	static char envbase[1024];


	/*
	 *  Add a couple of e vars.
	 */
	if (p->base) {
		sprintf(envbase, "LOGSYS=%.1000s", p->base->name);
		putenv(envbase);
		x = (*p->base->ops->get_curidx)(p);
		if (x) {
			sprintf(envidx, "INDEX=%d", x);
			putenv(envidx);
		}
	}
}


#ifdef MACHINE_WNT
cdecl LPTHREAD_START_ROUTINE do_wnt_rls_exec(void * info)
{
	SECURITY_ATTRIBUTES sa;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	int n,x;
	HANDLE pipr, pipw, h;

	char 		** cmd,**v;
	Client	* p;
	char		* mode;
	struct thr_info	* thrinfo;

	thrinfo=(struct thr_info*)info;
	
	cmd=thrinfo->cmd;
	p=thrinfo->p;
	mode=thrinfo->mode;

	if (DEBUG(12))
	{
		debug("%s%d Executes ():",DEBUG_PREFIX, p->key);
		for (v = cmd; *v; ++v)
			debug(" %s", *v);

		debug("\n");
	}


	p->skip_client=1;

	/*
	 *  First assume no i/o wanted, then fix appropriate handles.
	 */
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	/*
	 * si.hStdInput  = INVALID_HANDLE_VALUE;
	 * si.hStdOutput = INVALID_HANDLE_VALUE;
	 * si.hStdError  = INVALID_HANDLE_VALUE;
	 */
	si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
	si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
	si.dwFlags = STARTF_USESTDHANDLES;

	if (mode) {
		memset(&sa, 0, sizeof(sa));
		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = 1;

		if (!CreatePipe(&pipr, &pipw, &sa, 0))
		{
			if (DEBUG(12)) 
				debug("%sdo_wnt_rls_exec::CreatePipe() failed.\n",DEBUG_PREFIX);

			vreply(p,RESPONSE,0,_Int,-1,_Int,0,_End);
			return (0);
		}
		/*
		 *  mode is wrt the rls client, _not_ this process...
		 * 4.03/KP Set all other handles to standard ones.
		 * (This avoids the "Redirect got x expected y" errors)
		 */
		if (*mode == 'r') {
			si.hStdOutput = pipw;
			si.hStdError  = pipw;
			/*
			 * si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
			 */
			si.hStdInput = CreateFile( "NUL", GENERIC_READ,
										FILE_SHARE_READ | FILE_SHARE_WRITE,
										&sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
										NULL );

		} else {
			si.hStdInput  = pipr;
			/*
			 * si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			 * si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
			 */
			si.hStdOutput = CreateFile( "NUL", GENERIC_WRITE,
										FILE_SHARE_READ | FILE_SHARE_WRITE,
										&sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
										NULL );
			si.hStdError = CreateFile( "NUL", GENERIC_WRITE,
										FILE_SHARE_READ | FILE_SHARE_WRITE,
										&sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
										NULL );
		}
	}

	x=rls_wnt_execute(cmd,&sa,&si,&pi);

	if (!x)
	{
		if (DEBUG(12)) 
			debug("%sdo_wnt_rls_exec::rls_wnt_execute() failed.\n",DEBUG_PREFIX);

		vreply(p, RESPONSE, 0, _Int, -1, _Int, 0, _End);

		if (mode)
		{
			CloseHandle(pipr);
			CloseHandle(pipw);
			if (*mode == 'r')
				CloseHandle(si.hStdInput);
			else
			{
				CloseHandle(si.hStdOutput);
				CloseHandle(si.hStdError);
			}
		}
		return (0);
	}

	p->child_pid = (int)pi.hProcess;


	if (mode) {
		if (*mode == 'r') {
			h = pipr;
			CloseHandle(pipw);
		} else {
			h = pipw;
			CloseHandle(pipr);
		}
	}

	WaitForSingleObject(pi.hThread,INFINITE);

	/*
	 *  XXX keep handle to process,
	 *  for picking and reporting the exit code later on.
	 */
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	if (mode) {
		if (*mode == 'r') {
			CloseHandle(si.hStdInput);
		} else {
			CloseHandle(si.hStdOutput);
			CloseHandle(si.hStdError);
		}
	}

	if (mode == NULL) {
		vreply(p, RESPONSE, 0, _Int, 0, _Int, 0, _End);
		x = 1;
	} else if (p->local) {
		x = reply_with_a_descriptor(p, h);
	} else {
		/*
		 * 4.04/KP Use synthetic handler for this, too.
		 */
		FileStruct *FS;

		FS = calloc( 1, sizeof(FileStruct) );
		FS->fnum = (int) h;
		FS->bReadonly = (*mode == 'r');
		FS->bAppend = (*mode == 'a');
		FS->name = strdup( "(pipe)" );
		p->synthetic_handler = do_synthetic_handle;
		x = transform_into_a_dataflow(p, (void *) FS, mode);
		/*
		 * x = transform_into_a_dataflow(p, (void *) h, mode);
		 */
	}
	free_stringarray(cmd);
	free(thrinfo->mode);
	free(thrinfo);
	p->skip_client=0;
    return (0);
}
#endif

int do_rls_exec(Client *p, char *mode, char **cmd)
{
#ifdef MACHINE_WNT
	/*
	 *   PUUUUUUUUUUUH... 
	 */

	struct thr_info	*thr;

	thr=malloc(sizeof(struct thr_info));

	/*
	 *  Expand args and cat them into one command-line.
	 *  As usual on NT, this fails miserably when args
	 *  contain whitespace or quotes or ...
	 *  Note environment touchup _before_ expansion of the
	 *  commandline.
	 */
	prep_env(p);
	
	thr->mode=strdup(mode);
	thr->p=p;
	thr->cmd=cmd;

	p->skip_client=1;
	p->child_pid = 0;/*pi.hProcess;*/
	_beginthread(do_wnt_rls_exec,10240,(void*)thr);

	return (1);
#else
	int x;
	char **v;
	HANDLE h = 0; /* if mode is set this gets the descriptor we send to client */
	int pid;
	int tube[2];
	FileStruct *FS;

	/*
	 *  Code ASSUMES 0, 1 and 2 are open ! which they are.
	 */
	if (mode && pipe(tube)) {
		vreply(p, RESPONSE, 0, _Int, errno ? errno : -1, _Int, 0, _End);
		return (0);
	}
	if ((pid = fork()) == 0) {

		if (!mode) {
			close(0);
			close(1);
			close(2);
			if (open("/dev/null", 0) != 0
			 || open("/dev/null", 2) != 1
			 || open("/dev/null", 2) != 2)
				_exit(-1);
		} else if (*mode == 'r') {
			/*
			 *  child output wanted by client.
			 *  stdout and stderr get intermixed,
			 *  both buffered by stdio, might cause problems...
			 */
			close(1);
			close(2);
			if (dup(tube[1]) != 1
			 || dup(tube[1]) != 2)
				_exit(-1);
			close(tube[0]);
			close(tube[1]);

			close(0);
			if (open("/dev/null", 0) != 0)
				_exit(-1);
		} else {
			/*
			 *  input for child written by client.
			 */
			close(0);
			if (dup(tube[0]) != 0)
				_exit(-1);
			close(tube[0]);
			close(tube[1]);

			close(1);
			close(2);
			if (open("/dev/null", 2) != 1
			 || open("/dev/null", 2) != 2)
				_exit(-1);
		}
		/*
		 *  Expand now, in child, no need to call free(),
		 *  also parent cmd[] is preserved (for no reason).
		 *  Environment is touched before expanding command,
		 *  so $LOGSYS can be used in .rlscommands.
		 */
		prep_env(p);

		for (v = cmd; *v; ++v)
			*v = strdup(buck_expand(*v));

		/*
		 *  Following never returns, _exit() is paranoia.
		 */
		rls_execute(cmd);
		_exit(-1);
	}

	/*
	 *  Parent continuing...
	 */

	if (mode) {
		if (*mode == 'r') {
			h = tube[0];
			close(tube[1]);
		} else {
			h = tube[1];
			close(tube[0]);
		}
	}
	/*
	 *  XXX remember the pid for reporting the exit code later on.
	 *  Just a single pid per Client would suffice.
	 *  If the result is important for the Client,
	 *  executes cannot be concurrent.
	 */

	p->child_pid = pid;

	switch (fork()) {
	case 0:
		if (mode == NULL) {
			vreply(p, RESPONSE, 0, _Int, 0, _Int, 0, _End);
			x = 1;
		} else if (p->local) {
			x = reply_with_a_descriptor(p, h);
		} else {
			FS = calloc( 1, sizeof(FileStruct) );
			FS->fnum = h;
			FS->bReadonly = (*mode == 'r');
			FS->bAppend = (*mode == 'a');
			FS->name = strdup( "(pipe)" );
			p->synthetic_handler = do_synthetic_handle;
			x = transform_into_a_dataflow(p, (void *) FS, mode);
			endconn(p,"",0);
			/*
			 * 4.05/KP,HT : Use _exit() instead of exit(), because
			 * Sybase API uses atexit() to shut down the database
			 * connection.
			 * exit(0);
			 */
			_exit(0);
		}
	case -1:
		return(0);
	default:
		close(h);
		return (0);
	}
#endif
}

