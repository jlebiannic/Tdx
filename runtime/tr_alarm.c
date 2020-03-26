/*========================================================================
        E D I S E R V E R

        File:		runtime/tr_alarm.c
        Author:		J. Random Hacker (JH)
        Date:		Fri Jan 13 13:13:13 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: tr_alarm.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 11.03.96/JN	Created.
  3.01 20.03.98/JN	alarm() wannabefor NT added.
========================================================================*/

#include "tr_prototypes.h"

#ifndef MACHINE_WNT

#include <sys/types.h>
#include <signal.h>

double nWatchDogAlarmSeconds;

int tr_alarmHeld;
int bfSetupAlarm();

static int beenhere;
static void ringgg(int);

int bfHoldAlarm()
{
	int was;
	sigset_t mask, omask;

	if (!beenhere)
		bfSetupAlarm();

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigprocmask(SIG_BLOCK, &mask, &omask);

	was = tr_alarmHeld;
	tr_alarmHeld = 1;

	return (was);
}

int bfSetupAlarm()
{
	struct sigaction sa, osa;

	beenhere = 1;

	bfHoldAlarm();

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = ringgg;

	sigaction(SIGALRM, &sa, &osa);

	return (0);
}

int bfReleaseAlarm()
{
	int was;
	sigset_t mask, omask;

	if (!beenhere)
		bfSetupAlarm();

	was = tr_alarmHeld;
	tr_alarmHeld = 0;

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &mask, &omask);

	return (was);
}

static void ringgg(int sig)
{
	if (bfUserSignal((double) sig, "SIGALRM")) {
		tr_Fatal("bfUserSignal returned TRUE for alarm signal.");
		*(int*) 0 = 0;
	}
}

#endif /* WNT */

#ifdef MACHINE_WNT

/*
 *  Very simple U*X alarm() wannabe for NT.
 *  Multiple "main" threads cannot use this concurrently.
 */

#include <windows.h>

/*
 *  SIGALRM is 14 on most U*X, mimic.
 */
#define SIGALRM 14

static HANDLE mainthread;

/*
 *  Cannot let the main thread suddenly pop up,
 *  user might continue within handlers....
 *  So the main thread is temporarily suspended.
 *
 *  If the user keeps executing within handlers,
 *  the "main thread" is left suspended
 *  until program exits. It is ok for simple progs,
 *  but is very ugly, suddenly the executing thread
 *  freezes and execution continues in another thread...
 *  Cannot terminate the main thread, however, someone
 *  might be watching for its existence.
 *
 *  Reason for this silliness is missing longjump
 *  between threads.
 *  Reason for not Sleeping the time in one long nap, is
 *  to allow over 49+ days sleeps, and to keep track of unslept
 *  time conveniently, unfortunately this way time skews somewhat.
 *
 *  "Flag" is cleared here, another alarms
 *  can be added after this point.
 *
 *  Try to debug this next year...
 */
static DWORD WINAPI watchdog(void *arg)
{
	int *secs = arg;

	for ( ; *secs > 0; --*secs)
		Sleep(1000);

	SuspendThread(mainthread);

	if (!bfUserSIGALRM((double) SIGALRM, "SIGALRM")) {
		bfUserSignal((double) SIGALRM, "SIGALRM");
		tr_Fatal("Alarm call.");
		/* NOTREACHED */
	}
	/*
	 *  User says it was ok after all (and returned).
	 *  Let the main thread continue.
	 */
	ResumeThread(mainthread);
	return (0);
}

int alarm(int secs)
{
	static HANDLE dogthread;
	static int sleeping;
	int residue = sleeping;

	if (residue)
		TerminateThread(dogthread, 0);

	if (secs > 0) {
		DWORD id;

		DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
				GetCurrentProcess(), &mainthread,
				0, FALSE, DUPLICATE_SAME_ACCESS);
		sleeping = secs;
		dogthread = CreateThread(NULL, 0, watchdog, &sleeping, 0, &id);
	}
	return (residue < 0 ? 0 : residue);
}

#endif /* of WNT */
