/*========================================================================
        E D I S E R V E R

        File:		perr.c
        Author:		J. Nurmela (JN)
        Date:		Thu Aug 22 15:41:31 EET 1996

        Copyright (c) 1996 Telecom Finland/EDI Operations


	Error logging to eventlog.

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: perr.c 55164 2019-09-24 13:26:27Z sodifrance $")

/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 22.08.96/JN Created.
  3.01 10.01.97/JN ???
  3.02 09.09.97/JN Merge U*X and NT.
  3.03 21.07.14/TCE(CG) : allow to disable windows eventlog write
  3.04 18.05.17/TCE(CG) : EI-325 add robustness
  Jira TX-3143 24.09.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#ifdef MACHINE_WNT
#include <windows.h>
#include <process.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef MACHINE_WNT
#include <syslog.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#include "perr.h"
#include "translator/translator.h"
#include "runtime/tr_externals.h"

/*
 *  Brh different names for peruser and perhost linkage...
 */
int debugged;
unsigned debugbits = 0;
int nolog = 1;

#ifdef MACHINE_WNT

TOKEN_USER *token_usr;
SID *sid_usr;

char *
GetLastErrorString(void)
{
	DWORD err;
	static char errbuf[1024];

	err = GetLastError();

	if (!FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_ARGUMENT_ARRAY,
			NULL,
			err,
			LANG_NEUTRAL,
			errbuf,
			sizeof(errbuf),
			NULL))
		sprintf(errbuf, "Unknown error %d", err);

	return (errbuf);
}

/*
 *  Find out who we are.
 */
static void
who_am_I()
{
	HANDLE proc = 0;
	DWORD req;
	SID_NAME_USE snu = SidTypeUser;

	if (sid_usr)
		return; /* been here */

	/*
	 *  Lookup current credentials.
	 */
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &proc))
		fatal("OpenProcessToken: %s", syserr());

	req = 256;
	GetTokenInformation(proc, TokenUser, NULL, 0, &req);

	if ((token_usr = calloc(1, req)) == NULL)
		fatal("allocate user-token: %s", syserr());

	if (!GetTokenInformation(proc, TokenUser, token_usr, req, &req))
		fatal("GetTokenInformation: %s", syserr());

	sid_usr = token_usr->User.Sid;
}

#endif

char *
syserr()
{
#ifdef MACHINE_WNT
	return (GetLastErrorString());
#else
	return (strerror(errno));
#endif
}

void
vsvclog(char type, char *fmt, va_list ap)
{
	char msg[1024];

	if (islower(type)){
		type = toupper(type);
	}

	if (type == 'D'){
		type = 'I';
	}

	if (debugged || debugbits)
	{
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		fflush(stderr);
	}

	vsprintf(msg, fmt, ap);

	if (logging)
	{	
		FILE *fp = fopen(logging, "a");
		if (fp) {
			fputs(msg, fp);
			fputs("\n", fp);
			fclose(fp);
		}
	}
	
#ifdef MACHINE_WNT
{
	HANDLE hev;
	char *v[1];

	who_am_I();
		
	if (nolog != 0){
	hev = RegisterEventSource(NULL, EVENT_NAME);
	v[0] = msg;

	/*
	 *  If our identity has been found, use that,
	 *  otherwise put NULL identity.
	 */
	ReportEvent(hev,
		type == 'I' ? EVENTLOG_INFORMATION_TYPE :
		type == 'W' ? EVENTLOG_WARNING_TYPE :
			      EVENTLOG_ERROR_TYPE,
		EVENT_CATEGORY,
		0,
		sid_usr,
		1,
		0,
		v,
		NULL);
	DeregisterEventSource(hev);
	}
}
#else /* ! WNT */

	{
		static int beenhere = 0;

		if (!beenhere) {
			beenhere = 1;
			openlog(EVENT_NAME,
				LOG_CONS | LOG_NOWAIT,
				LOG_DAEMON);
		}
	}
	/* Jira EI-325 add robustness to avoid using a null pointer */
	if (nolog != 0 && type != NULL && msg != NULL) {
	syslog(
		type == 'I' ? LOG_NOTICE :
		type == 'W' ? LOG_WARNING :
			      LOG_ERR,
		"%s", msg);
	}
	
#endif /* ! WNT */
}


void
svclog(char type, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsvclog(type, fmt, ap);
	va_end(ap);
}

void
fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsvclog('E', fmt, ap);
	va_end(ap);
	exit(1);
}

void
warn(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsvclog('W', fmt, ap);
	va_end(ap);
}

void debug(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fflush(NULL);
	va_end(ap);
}
