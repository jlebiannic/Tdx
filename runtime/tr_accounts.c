/*========================================================================
        E D I S E R V E R

        File:		runtime/tr_accounts.c
        Author:		Juha Nurmela (JN(
        Date:		Fri Feb  9 12:03:34 EET 1996

        Copyright (c) 1996 Telecom Finland/EDI Operations
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: tr_accounts.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 09.02.96/JN	Created.
========================================================================*/
#ifdef MACHINE_HPUX
#define _LARGEFILE64_SOURCE
#else
#define __USE_LARGEFILE64
#endif
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <errno.h>
#endif

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

#ifdef MACHINE_WNT

static void *GetSecurity(char *filename, SECURITY_INFORMATION what)
{
	DWORD len = 1024;
	void *sec = malloc(len);

	while (sec) {
		DWORD needed = 0;

		if (GetFileSecurity(filename, what, sec, len, &needed))
			break;

		if (len >= needed) {
			free(sec);
			sec = NULL;
			break;
		}
		len = needed;
		sec = realloc(sec, len);
	}
	return (sec);
}

static char *Lookup(void *sid, SID_NAME_USE snu)
{
	char buf[256];
	char dom[256];
	int bsize = sizeof(buf);
	int dsize = sizeof(dom);

	if (LookupAccountSid(NULL, sid, buf, &bsize, dom, &dsize, &snu))
		return (tr_BuildText("%s\\%s", dom, buf));

	return (NULL);
}

char *tr_FileOwner(char *filename)
{
	char *ret = TR_EMPTY;
	void *sec;
	void *sid = 0;
	BOOL  dfl = 0;

	if (sec = GetSecurity(filename, OWNER_SECURITY_INFORMATION)) {
		if (GetSecurityDescriptorOwner(sec, &sid, &dfl))
			ret = Lookup(sid, SidTypeUser);
		free(sec);
	}
	return (ret);
}

char *tr_FileGroup(char *filename)
{
	char *ret = TR_EMPTY;
	void *sec;
	void *sid = 0;
	BOOL  dfl = 0;

	if (sec = GetSecurity(filename, GROUP_SECURITY_INFORMATION)) {
		if (GetSecurityDescriptorGroup(sec, &sid, &dfl))
			ret = Lookup(sid, SidTypeGroup);
		free(sec);
	}
	return (ret);
}

#else /* not WNT */

char *tr_FileOwner (char *filename)
{
	struct stat64 buf;
	struct passwd *pw, *getpwuid(uid_t);

	if (stat64 (filename, &buf)) {
		tr_Log (TR_WARN_STAT_FAILED, filename, errno);
		return TR_EMPTY;
	}
	if ((pw = getpwuid (buf.st_uid)) == NULL)
		return tr_BuildText ("%d",buf.st_uid);
	return tr_MemPool (pw->pw_name);
}

char * tr_FileGroup (char *filename)
{
	struct stat64 buf;
	struct group *gr, *getgrgid (gid_t);

	if (stat64 (filename, &buf)) {
		tr_Log (TR_WARN_STAT_FAILED, filename, errno);
		return TR_EMPTY;
	}
	if ((gr = getgrgid (buf.st_gid)) == NULL)
		return tr_BuildText ("%d",buf.st_gid);
	return tr_MemPool (gr->gr_name);
}

#endif /* not WNT */

