/*========================================================================
        E D I S E R V E R

        File:		eftpd/wildglob.c
        Author:		Juha Nurmela (JN)
        Date:		Thu Jan 11 01:50:50 EET 1996

        Copyright (c) 1996 Telecom Finland/EDI Operations
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: expand.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 10.01.96/JN	Created from parts of eftpd/email.
  3.01 17.09.98/VA	Windog kludges
  3.02 28.12.98/HT	user_name parameter added to setup_environment().
  3.03 12.02.99/KP	Replace \'s with /'s in WNT when processing
					.userenv etc., effectively allowing both /'s and \'s.
  3.04 23.04.99/KP	Allow spaces in environment variable values
			(Currently only in WinNT, caused problems in Unix)
  3.05 25.05.99/VA	Add PATH in .userenv to global PATH
  3.06 19.01.05/FH  set sql NULL string value from environnement variable
  3.07 21.07.08/CRE Read .userenv for Windows system
  3.08 27.01.16/TCE(CG) TXJ -1864 work buffer is now 1MB to allow more file
to be transmitted while a folder enumeration is asked
  3.09 18.05.17/TCE(CG) EI-325 correct some memory leak
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/
/*
 *  Construct a list of matching files from a pattern.
 *  Special characters are ~ { * ? and [, csh-like.
 *
 *  If no special characters are found on initial pattern,
 *  the pattern is returned in a list of one.
 *
 *  First {x,y} are separated into start of workspace,
 *  using curpath to indicate position where the next entry would go.
 *
 *  After that, each string is expanded with * ? and [
 *  with ~ in head of string being special, indicating
 *  the home-directory of either current user or someone else.
 *  ~/rest or ~user/rest.
 *
 *  curpath is used to point into current working-path,
 *  whenever a path is accepted, curpath is copied down in
 *  buffer. At the same time, curlimit, which marks end of workspace,
 *  is lifted to reserve space for the string-vector.
 */

#include <sys/types.h>
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "logsysrem.h"
#include "perr.h"

#undef PSEP
#ifdef MACHINE_WNT
#define PSEP  '\\'
#else
#define PSEP  '/'
#endif
#undef PSEP2
#define PSEP2 '/'

#define TILD '~'
#define LBRC '{'
#define RBRC '}'
#define STAR '*'
#define QUES '?'
#define LBRK '['
#define RANG '-'
#define RBRK ']'

#ifndef DEBUG_PREFIX
#define DEBUG_PREFIX	" [peruser]  [expand] "
#endif

static int  gargc;           /* total globbed strings */
static int  bargc;           /* number of brace-expanded strings */

static char *curpath;        /* path under construction */
static char *curlimit;       /* top of space reserved for string-vector */
static char *gbuf;           /* space for final strings */
static char buffer[1024 *1024]; /* TXJ-1864 TCE(CG) working space expanded to 1MB*/

#define buffer_end (buffer + sizeof(buffer))

#if 0
static int  expbraces(char *pat);
#endif
static void addc(int offset, int c);
static void addpath(int pathlen);
static int compar(void *a, void *b);

static int wildmat(unsigned char *pat, unsigned char *s);

/* 3.06 19.01.05/FH */
char *TR_EMPTY = "";
char *TR_SQLNULL; 

char *buck_expand(char *tmplt);

static char *
str2dup(char *a, char *b)
{
	char *p = malloc(strlen(a) + strlen(b) + 1);

	strcpy(p, a);
	strcat(p, b);
	return (p);
}

#ifdef MACHINE_WNT
static char *
str3dup(char *a, char *b, char *c)
{
	char *p = malloc(strlen(a) + strlen(b) + strlen(c) + 1);

	strcpy(p, a);
	strcat(p, b);
	strcat(p, c);
	return (p);
}
#endif

static char *HOME(void)
{
	char *cp;

	if ((cp = getenv("HOME")) != NULL)
	{
		return (cp);
	}
	return ("");
}

static int nglobdir(char *pat, int pathlen, int start, int count, int current)
{
	char c, simple = 1;
	char *pat0 = pat;
	stat_t st;
	int index = current;
	while ((c = *pat++) != 0)
	{
		if (c == STAR || c == QUES || c == LBRK)
		{
			simple = 0;
		}
		if (c == PSEP || c == PSEP2)
		{
			if (!simple)
			{
				break;
			}
			while (pat0 != pat)
			{
				addc(pathlen++, *pat0++);
			}
			addc(pathlen, 0);
		}
	}
	if (simple)
	{
		while (*pat0)
		{
			addc(pathlen++, *pat0++);
		}
		addc(pathlen, 0);

		if (stat(curpath, &st) == 0)
		{
			addpath(pathlen);
		}
	}
	else
	{
#ifdef MACHINE_WNT
		HANDLE h;
		WIN32_FIND_DATA dd;
		char *cut;
#else
		struct dirent *d;
		DIR *dirp;
#endif
		char *dnam;
		int i;

#ifndef MACHINE_WNT
		dirp = NULL;
		if (stat(curpath, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR && (dirp = opendir(curpath)) != NULL)
		{
			while ((d = readdir(dirp)) != NULL)
#else
		cut = curpath + strlen(curpath);
		if (cut == curpath || (cut[-1] != PSEP && cut[-1] != PSEP2))
		{
			strcpy(cut, "\\*");
		}
		else
		{
			strcpy(cut, "*");
		}
		h = FindFirstFile(curpath, &dd);
		*cut = 0;
		if (h != INVALID_HANDLE_VALUE)
		{
			do
#endif
			{
#ifndef MACHINE_WNT
				dnam = d->d_name;
#else
				dnam = dd.cFileName;
#endif
				/* Consider dotfiles only if pat starts with a '.' */
				if (pat0[0] != '.' && dnam[0] == '.')
				{
					continue;
				}
				
				/* Apply our filter */
				if (!wildmat(pat0, dnam))
				{
					continue;
				}
				
				/* Return this file */
				index++;
				
				/* Enumerate "count" files starting at position "start" */
				if (index < start)
				{
					continue;
				}
				if ((count > 0) && (index >= start + count))
				{
					break;
				}
				
				/* Recursive enumeration */
				for (i = 0; dnam[i]; ++i)
				{
					addc(pathlen + i, dnam[i]);
				}
				if (c)
				{
					addc(pathlen + i++, PSEP);
					addc(pathlen + i, 0);
					index = nglobdir(pat, pathlen + i, start, count, index);
				}
				else
				{
					addc(pathlen + i, 0);
					addpath(pathlen + i);
				}
			}
#ifdef MACHINE_WNT
			while (FindNextFile(h, &dd));
		}
		if (h != INVALID_HANDLE_VALUE)
		{
			FindClose(h);
		}
#else
		}
		if (dirp)
		{
			closedir(dirp);
		}
#endif
	}
	return index;
}

static int ndoglob(char *pat, int start, int count)
{
	char *cp;

	/*
	 *  Any need for all this fuzz ?
	 */
	for (cp = pat; ; ++cp)
	{
		if (*cp == 0)
		{
			gargc = 1;
			gbuf = buffer;
			strcpy(gbuf, pat);
			curpath = gbuf + strlen(gbuf) + 1;
			return (0);
		}
		if (*cp == TILD || *cp == LBRC || *cp == STAR || *cp == QUES || *cp == LBRK)
		{
			break;
		}
	}

	/*
	 *  Set starting point in globals.
	 *  curlimit keeps track where the space reserved
	 *  for pointer vector resides.
	 *  Decrement one to get into safe are, one for NULL
	 *  and one for paranoia. 
	 *  Make sure it is aligned (overly conservative).
	 */
	curlimit = buffer_end - 3 * sizeof(char *);
	while ((unsigned long) curlimit & 0xF)
	{
		--curlimit;
	}

	bargc = 0;
	gargc = 0;

	/*
	 *  Expand braces first into zero-separated string at start of
	 *  buffer.
	 */
	curpath = buffer;

#if 0
	if (expbraces(pat))
		pat = buffer;
	else
#endif
		bargc = 1;

	gbuf = curpath;
	*curpath = 0;

	/*
	 *  Now glob each bracexpanded string separately.
	 */
	for (;;)
	{
		if (*pat == TILD)
		{
			/*
			 *  ~ expansion ~/ and ~username/
			 *  Seed with homedirectory.
			 */
			cp = curpath;

			while (*++pat != PSEP && *pat != PSEP2 && *pat != 0 && cp < curlimit)
			{
				*cp++ = *pat;
			}
			*cp = 0;
			if (*curpath == 0)
			{
				strcpy(curpath, HOME());
			}
			else
			{
#ifndef MACHINE_WNT
				struct passwd *pw;
				if ((pw = getpwnam(curpath)) != NULL)
				{
					strcpy(curpath, pw->pw_dir);
				}
				else
				{
					return (-1);
				}
#else
				return (-1);
#endif
			}
			nglobdir(pat, strlen(curpath), start, count, 0);
		}
		else
		{
			/*
			 *  Seed . in case the pattern is not absolute.
			 *  Note still starting with offset 0.
			 */
			strcpy(curpath, ".");
			nglobdir(pat, 0, start, count, 0);
		}
		/*
		 *  Next bracexpanded one.
		 */
		if (--bargc <= 0)
		{
			break;
		}
		while (*pat++)
		{
			/* there must be a better way to do this! */
			;
		}
	}
	*curpath = 0; /* zero terminate for sure */

	return (0);
}

char **nglob(char *pat, int start, int count)
{
	char *cp, **gv;
	int i;

	pat = buck_expand(pat);
	if (ndoglob(pat, start, count))
	{
		return (NULL);
	}
	/* (conservative) pointer aligning. */

	while ((unsigned long) curpath & 0xF)
	{
		++curpath;
	}
	gv = (char **) curpath;
	*gv++ = 0;

	cp = gbuf;
	for (i = 0; i < gargc; ++i)
	{
		gv[i] = cp;
		while (*cp++)
		{
			/* there must be a better way to do this! */
			;
		}
	}
	gv[i] = NULL;

	if (gargc > 1)
	{
		qsort(gv, gargc, sizeof(*gv), compar);
	}

	return (gv);
}

/* Kept for compatibility -- use nglob instead with start = 0 and count = 0 */
char **glob(char *pat)
{
	return nglob(pat, 0, 0);
}

static int compar(void *a, void *b)
{
	return (strcmp(*(char **)a, *(char **)b));
}

/*
 *  Current pathname is accepted.
 *  New template is copied after the current one. 
 */
static void addpath(int pathlen)
{
	/*
	 *  Zero terminated.
	 */
	addc(pathlen++, 0);

	if (curpath + 2 * pathlen < curlimit)
	{
		gargc++;
		memcpy(curpath + pathlen, curpath, pathlen);
		curpath  += pathlen;
		curlimit -= sizeof(char *);
	}
}

static void addc(int offset, int c)
{
	if (curpath + offset < curlimit) {
		curpath[offset] = c;
	}
}

static int wildmat(unsigned char *pat, unsigned char *s)
{
	for ( ; *pat && *pat != PSEP && *pat != PSEP2; ++pat, ++s)
	{
		switch (*pat)
		{
		default:
			if (*s != *pat)
			{
				return (0);
			}
			break;
		case QUES:
			if (*s == 0)
			{
				return (0);
			}
			break;
		case STAR:
			if (*++pat == 0 || *pat == PSEP || *pat == PSEP2)
			{
				return (1);
			}
			while (*s)
			{
				if (wildmat(pat, s++))
				{
					return (1);
				}
			}
			return (0);
		case LBRK: {
				unsigned char range_from = 0;
				char got_range = 0;
				char in_set = 0;

				while (*++pat != 0 && *pat != PSEP && *pat != PSEP2)
				{
					if (*pat == RBRK)
					{
						break;
					}
					if (*pat == RANG)
					{
						got_range = 1;
					}
					else
					{
						if (*pat == *s)
						{
							in_set = 1;
						}
						if (got_range)
						{
							got_range = 0;
							if (*s >= range_from && *s <= *pat)
							{
								in_set = 1;
							}
						}
						range_from = *pat;
					}
				}
				if (!in_set)
				{
					return (0);
				}
			}
			break;
		}
	}
	return (*s == 0);
}

#if 0
/*
 *  Expand brace-patterns.
 *  return zero if none was found.
 */
static int
expbraces(char *pat)
{
	int slen, elen;
	int nbrc, expmore;
	char *lbrc, *rbrc;
	char *cp;
	char *part, *epart;

	/*
	 *  Seek to first opening brace.
	 */
	for (lbrc = pat; *lbrc != LBRC; ++lbrc)
		if (*lbrc == 0)
			return (0);
	/*
	 *  Search the matching closing brace.
	 */
	nbrc = 0;
	for (rbrc = lbrc + 1; *rbrc != RBRC || nbrc; ++rbrc) {
		if (*rbrc == 0)
			return (0);
		if (*rbrc == LBRC)
			++nbrc;
		else if (*rbrc == RBRC)
			--nbrc;
	}
	/*
	 *  Lengths of starting and ending substrings.
	 *  elen includes the terminating zero.
	 */
	for (cp = rbrc; *cp; ++cp)
		;
	elen = cp - rbrc;
	slen = lbrc - pat;

	/*
	 *  Construct string from each part.
	 */
	for (part = lbrc; ++part < rbrc; part = epart) {
		/*
		 *  Need to watch subbraces again.
		 */
		expmore = 0;
		nbrc = 0;
		for (epart = part; epart < rbrc; ++epart) {
			if (*epart == ',' && nbrc == 0)
				break;
			else if (*epart == LBRC)
				--nbrc;
			else if (*epart == RBRC) {
				++nbrc; 
				expmore = 1;
			}
		}
		if (expmore) {
			/*
			 *  There are still more braces there.
			 */
			cp = allocate(slen + (epart - part) + elen);

			memcpy(cp, pat, slen);
			memcpy(cp + slen, part, (epart - part));
			memcpy(cp + slen + (epart - part), rbrc + 1, elen);

			/*
			 *  Recurse.
			 */
			expbraces(cp);
			free(cp);
		} else {
			/*
			 * Can save right away.
			 */
			cp = curpath;
			memcpy(cp, pat, slen);
			memcpy(cp + slen, part, (epart - part));
			memcpy(cp + slen + (epart - part), rbrc + 1, elen);

			curpath += slen + (epart - part) + elen;
			++bargc;
		}
	}
	/*
	 *  Did something. Tell it to caller.
	 */
	return (1);
}
#endif

/*
 *  Set user-environment and cwd into $HOME.
 *  We look from following places:
 *
 *   $HOME/.userenv
 *   $HOME/../../lib/userenv   ($EDIHOME/lib/userenv hopefully)
 *   $HOME/../default/.userenv ($EDIHOME/users/default/userenv hopefully)
 */

static int  source(char *filename);


static void process_userenv(int ac, char **av, int nesting);

#ifndef MACHINE_WNT
static void loadline(FILE *fp, int nesting, void (*handler)());
static void process_mapping(int ac, char **av, int nesting);
static int  source1(char *filename, int nesting, void (*handler)());
#endif

#define MAX_NESTING 32

static int source(char *filename)
{
	return (source1(filename, 0, process_userenv));
}

static int setup_environment_done;

void setup_environment(char *user_name)
{
	char *cp;
	char *path_userenv;
#ifdef MACHINE_WNT
	char *cp2;
	/* 3.05 */
	char *tmppath=NULL;
	char *oldpath=NULL;
#else
	struct passwd *pw;
#endif

	if (setup_environment_done) {
		return;
	}
	if (user_name) {
		debug("%ssetup_environment_done=1 (user_name=%s)\n", DEBUG_PREFIX, user_name);
		setup_environment_done = 1;
	}

#ifdef MACHINE_WNT
	/* 3.05 */
	if (tmppath = getenv("PATH"))
		oldpath=strdup(tmppath);

  /****
	Est-ce vraiment utile tout ce travail sur HOMEDRIVE/HOMEPATH puisque l'on utilise toujours HOME ?
	_chdrive attend le num�ro du disque en num�rique d'apr�s MSDN (msdn.microsoft.com/en-us/library/0d1409hb(v=vs.90).aspx)
	or jusqu'� pr�sent on donnait une chaine repr�sentant	un disque au format windows ce qui vient d'�tre chang� avec le calcul sur drive
	***/

	cp2 = "C:";
	if (cp = getenv("HOMEDRIVE")) {
		int drive;
		drive = (int) cp[0] - (int) 'A' + 1;
		cp2 = cp;
		_chdrive(drive);
	}

	if (cp = getenv("HOMEPATH"))
		_chdir(cp);
	else
		cp = "\\";
	
	if (!getenv("HOME"))
		putenv(str3dup("HOME=", cp2, cp));

	if (cp = getenv("USERNAME")) {
		putenv(str2dup("USER=",    cp));
		putenv(str2dup("LOGNAME=", cp));
	}

#else
	/* 28.12.98/HT    */
	if (!user_name && ((pw = getpwuid(getuid())) == NULL))
	{
		exit(1);
	}
	if (user_name && ((pw = getpwnam(user_name)) == NULL))
	{
		exit(1);
	}

	if (pw->pw_shell == NULL || *pw->pw_shell == 0)
	{
		pw->pw_shell = "/bin/sh";
	}

	/* Ignore error. */
	chdir(pw->pw_dir);

	/* First the constant ones... */
	putenv(str2dup("USER=",    pw->pw_name));
	putenv(str2dup("LOGNAME=", pw->pw_name));
	putenv(str2dup("HOME=",    pw->pw_dir));
	putenv(str2dup("SHELL=",   pw->pw_shell));
#endif

	/*
	 *  If it looks like we have already been here,
	 *  skip .user-env.
	 */
/* 3.05 */
#ifndef MACHINE_WNT
	if ((cp = getenv("USERINFO")) != NULL && *cp != 0)
	{
		return;
	}
#endif

	umask(027);

	/*
	 *  ... then customizable ones.
	 */
#ifndef MACHINE_WNT
	{
		path_userenv = malloc(strlen(HOME()) + sizeof("/../default/.userenv") );

		sprintf(path_userenv,"%s/.userenv",HOME());
		if (source(path_userenv) != 0) {
			sprintf(path_userenv,"%s/../../lib/userenv",HOME());
			if (source(path_userenv) != 0) {
				sprintf(path_userenv,"%s/../default/.userenv",HOME());
				if (source(path_userenv) != 0) {
					debug("%sWARNING : userenv file not sourced\n", DEBUG_PREFIX);
				}
			}
		}
	}
#else
	/* 3.05 */
	path_userenv = malloc(strlen(HOME()) + sizeof("/../../lib/.userenv") );
	sprintf(path_userenv,"%s/.userenv",HOME());

	if (source(path_userenv) != 0) {
		sprintf(path_userenv,"%s/../../lib/userenv",HOME());
		if (source(path_userenv) != 0) {
			debug("%sWARNING : userenv file not sourced\n", DEBUG_PREFIX);
		}
	}
	if (oldpath) {
                tmppath=malloc(5+strlen(oldpath)+1+strlen(getenv("PATH"))+1);
                strcpy(tmppath,"PATH=");
                strcat(tmppath,getenv("PATH"));
                strcat(tmppath,";");
                strcat(tmppath,oldpath);
                putenv(tmppath);
                free(tmppath);
                free(oldpath);
        }
#endif
	free(path_userenv);
	/* 3.06 19.01.05/FH */
	if ( !getenv("SQL_NULL") ) {
		TR_SQLNULL = TR_EMPTY;
	} else {
		TR_SQLNULL = getenv("SQL_NULL");
	}
}

static int source1(char *filename, int nesting, void (*handler)())
{
	FILE *fp;

	if (nesting >= MAX_NESTING)
	{
		return (-1);
	}

	if ((fp = fopen(filename, "r")) == NULL)
	{
		return (-1);
	}

	while (!feof(fp) && !ferror(fp))
	{
#ifndef MACHINE_WNT
		loadline(fp, nesting, handler);
#else
		loadline(fp, (void*)handler,NULL,NULL,NULL,NULL);
#endif
	}
	fclose(fp);

	return (0);
}

/* Command from .userenv */
static void process_userenv(int ac, char **av, int nesting)
{
	int i;
#ifdef MACHINE_WNT
	char buf[1024];
#endif

	/* Allow export X=Y by skipping export */
	if (ac > 1 && !strcmp(av[0], "export"))
	{
		--ac, ++av;
	}

	/*
	 *  X=Y
	 *  3.04/KP : Allow whitespace in value. (In WinNT only, currently)
	 *
	 *  XXX This converts all whitespace blocks into one <space>, e.g.
	 *  "One  Two\tThree" -> "One Two Three"
	 */
	 
/*
Jira EI-325 : add null checking to avoid to use a null pointer
*/
#ifndef MACHINE_WNT
	if (ac == 1 && strchr(av[0], '=') != NULL && av[0][0] != '=' && av[0][0] != NULL)
	{
		putenv(strdup(av[0]));
	}
#else
	if (ac >= 1 && strchr(av[0], '=') != NULL && av[0][0] != '=')
	{
		i = 0;
		strcpy(buf, av[i++]);
		while (i < ac)
		{
			strcat(buf, " ");
			strcat(buf, av[i++]);
		}
		putenv(buf);
	}
#endif

	/* umask 027 */
	if (ac == 2 && !strcmp(av[0], "umask") && sscanf(av[1], "%o", &i) == 1)
	{
		umask(i);
	}
	
	/* . filename */
	if (ac == 2 && !strcmp(av[0], "."))
	{
		source1(av[1], nesting + 1, process_userenv);
	}

	/* Debug */
	if (ac == 1 && !strcmp(av[0], "env"))
	{
		system("env");
	}
}

#ifndef MACHINE_WNT
static char **the_command;

void rls_execute(char **command)
{
	char *rlscmdfile;
	the_command = command;

	rlscmdfile = malloc(strlen(HOME()) + sizeof("/../../default/.rlscommands") );

	sprintf(rlscmdfile,"%s/.rlscommands",HOME());
	source1(rlscmdfile, 0, process_mapping);

	sprintf(rlscmdfile,"%s/../../lib/rlscommands",HOME());
	source1(rlscmdfile, 0, process_mapping);

	sprintf(rlscmdfile,"%s/../../default/.rlscommands",HOME());
	source1(rlscmdfile, 0, process_mapping);

	free(rlscmdfile);
	_exit(1);
}

#else /*MACHINE_WNT*/
int
rls_wnt_execute(char **command,
SECURITY_ATTRIBUTES *sa,
STARTUPINFO *si,
PROCESS_INFORMATION *pi)
{
	char		tmpcmd[512];
	int		x;

	if (getenv("HOME")) {
		strcpy(tmpcmd,getenv("HOME"));
		strcat(tmpcmd,"/.rlscommands");
	
		if ((x=source_wnt(tmpcmd, command,sa,si,pi))!=-1)
			return x;
	}

	if (getenv("EDIHOME")) {
		strcpy(tmpcmd,getenv("EDIHOME"));
		strcat(tmpcmd,"/lib/rlscommands");
	
		if ((x=source_wnt(tmpcmd, command,sa,si,pi))!=-1)
			return x;

		strcpy(tmpcmd,getenv("EDIHOME"));
		strcat(tmpcmd,"/default/.rlscommands");

		if ((x=source_wnt(tmpcmd, command,sa,si,pi))!=-1)
			return x;
	}
	return 0;
}

static int source_wnt(char *filename, char ** command, SECURITY_ATTRIBUTES *sa, STARTUPINFO *si, PROCESS_INFORMATION *pi)
{
	FILE *fp;
	int x;

	if ((fp = fopen(filename, "r")) == NULL)
		return (-1);

	while (!feof(fp) && !ferror(fp))
		if ((x=loadline(fp, NULL,command,sa,si,pi))!=-1) {
			fclose(fp);
			return x;
		}
		
	fclose(fp);

	return (-1);
}
#endif 

/*
 *  Mapping from .rlscommands
 *  Does not return when a match is found.
 *
 *  Each line of .rlscommands looks like:
 *  tag command_line
 */
#ifndef MACHINE_WNT
static void process_mapping(int ac, char **av, int nesting)
#else
static int process_mapping(int ac, 
char **av,
char ** the_command,
SECURITY_ATTRIBUTES *sa,
STARTUPINFO *si,
PROCESS_INFORMATION *pi)
#endif
{
	char *cp;
	char **v;
	int i;

	if (ac < 1 || strcmp(av[0], *the_command) != 0)
	{
#ifndef MACHINE_WNT
		return;
#else
		return -1;
#endif
	}

	/*
	 *  This is the line, step over tag
	 */
	ac--;
	av++;

	/*
	 *  X=Y ?
	 */

another_env:
	cp = *av;
	if (ac > 0 && strchr(cp, '=') && (isalpha(*cp) || *cp == '_'))
	{
		++cp;
		while (isalnum(*cp) || *cp == '_')
		{
			++cp;
		}
		if (*cp == '=')
		{
			/*
			 *  Environment setting before command.
			 */
			putenv(*av);
			ac--;
			av++;
			goto another_env;
		}
	}
	/*
	 *  No command left ??? Huh...
	 */
	if (ac < 1)
	{
#ifndef MACHINE_WNT
		_exit(0);
#else
		ExitThread(0);
#endif
	}

	/*
	 *  Concatenate configured fixed part (av)
	 *  and client supplied variable part (the_command).
	 *  Fs pathname for exec comes from configuration,
	 *  but argv[0] from client.
	 *  execv_p_ is used, however.
	 */
	i = 0;
	while (the_command[i])
	{
		++i;
	}

	v = malloc((ac + i + 1) * sizeof(*v));
	if (!v)
	{
		_exit(-1);
	}

	v[0] = *the_command++;     /* argv[0] */

	for (i = 1; i < ac; ++i)
	{
		v[i] = av[i];          /* middle args */
	}

	for ( ; *the_command; ++i)
	{
		v[i] = *the_command++; /* rest of args */
	}

	v[i] = NULL;

#ifndef MACHINE_WNT
	execvp(av[0], v);          /* program pathname */
	perror(av[0]);
	_exit(-1);
#else
	{
		int x;
		char *line;

		line = strdup(av[0]);
		for (v++; *v; ++v) {
			char *arg = buck_expand(*v);
			if (arg) {
				line = realloc(line, strlen(line) + 1 + strlen(arg) + 1);
				if (*line)
					strcat(line, " ");
				strcat(line, arg);
			}
		}
		x = CreateProcess(NULL, line,
                  NULL, NULL,
     	            TRUE,
           	      0,
           	      NULL, NULL,
			si, pi);
		free(line);
		return x;
	}
#endif

}

/*
 *  Read one line from file, and put it to environment
 *  if it was an assignment.
 */
#ifndef MACHINE_WNT
static void loadline(FILE *fp, int nesting, void (*handler)())
#else
static int loadline(FILE *fp, int (*handler)(), char ** command, SECURITY_ATTRIBUTES *sa, STARTUPINFO *si, PROCESS_INFORMATION *pi)
#endif
{
	enum { NONE, INWORD, INQUOTE } state = NONE;
	char *env, *copyer;
	int c, quote = 0;
	int substituting;
	int argc;
	char *argv[64];
	char buf[2048];
#define endbuffer (&buf[sizeof(buf) - 1])
#define maxargs   (sizeof(argv)/sizeof(argv[0]) - 1)

	copyer       = buf;
	argc         = 0;
	substituting = 0;

	while ((c = getc(fp)) != EOF)
	{
		if (copyer >= endbuffer)
		{
			goto out;
		}
		if (argc >= maxargs)
		{
			goto out;
		}
#ifdef MACHINE_WNT
		/*
		 * 3.03/KP : Replace \'s with /'s in WinNT
		 */
		if (c == '\\')
			c = '/';
#endif
		switch (c) {
		case '\\':
			if ((c = getc(fp)) == '\n')
			{
				/*
				 *	Continuing to next line.
				 */
				break;
			}
			if (c == EOF)
			{
				goto out;
			}

			if (state == NONE)
			{
				state = INWORD;
				argv[argc++] = copyer;
			}
			*copyer++ = c;
			break;
		case '\n':
			switch (state) {
			case INWORD:
			case NONE:
				goto out;
			default:; /* eek! */
			}
/*
 * 09.03.99/KP : Temporarily disabled (problems with variables with whitespace)
 * 23.04.99/KP : Then again, without this reading of .rlscommands fails. ARGH!
 *				 This is now done in process_userenv().
 */
		case ' ':
		case '\t':
			switch (state)
			{
				case NONE: break;
				case INWORD:
					state = NONE;
					*copyer++ = 0;
					break;
				default: *copyer++ = c; break;
			}
			break;
		case '\'':
		case '"':
			switch (state)
			{
				case INQUOTE:
					if (quote == c)
					{
						state = INWORD;
					}
					break;
				case NONE:
					quote = c;
					state = INQUOTE;
					argv[argc++] = copyer;
					break;
				case INWORD:
					quote = c;
					state = INQUOTE;
					break;
				default: break;
			}
			break;
		case '#':
			switch (state)
			{
				case NONE:
					do
					{
						c = getc(fp);
					}
					while (c != EOF && c != '\n');
					goto out;
				default:; /* eek! */
			}
		default:
			if (state == NONE)
			{
				state = INWORD;
				argv[argc++] = copyer;
			}
			if (c == '$' && (state != INQUOTE || quote != '\''))
			{
				/* Environment */
				substituting = 1;
				env = copyer;

				if ((c = getc(fp)) == '\n' || c == EOF)
				{
					goto out;
				}
				switch (c)
				{
					case LBRC:
						/* $HOME */
						while ((c = getc(fp)) != EOF && c != RBRC && c != '\n')
						{
							if (env >= endbuffer)
							{
								goto out;
							}
							*env++ = c;
						}
						if (c != RBRC)
						{
							goto out;
						}
						if (env >= endbuffer)
						{
							goto out;
						}
						*env = 0;
						env = getenv(copyer);
						break;
					case '$':
						if (copyer >= endbuffer - 5)
						{
							goto out;
						}
						sprintf(copyer, "%d", getpid());
						copyer += strlen(copyer);
						env = NULL;
						break;
					default:
						/* $HOME */
						while ( (c >= '0' && c <= '9') ||
							(c >= 'A' && c <= 'Z') ||
							(c >= 'a' && c <= 'z') ||
							(c == '_') )
						{
							if (env >= endbuffer)
							{
								goto out;
							}
							*env++ = c;
							if ((c = getc(fp)) == EOF)
							{
								goto out;
							}
						}
						ungetc(c, fp);
						if (env >= endbuffer)
						{
							goto out;
						}
						*env = 0;
						if (*copyer == 0)
						{
							/* Lone dollar */
							if (copyer >= endbuffer)
							{
								goto out;
							}
							*copyer++ = '$';
							env = NULL;
						}
						else
						{
							env = getenv(copyer);
						}
						break;
				}
				while (env && *env)
				{
					if (copyer >= endbuffer)
					{
						goto out;
					}
					*copyer++ = *env++;
				}
				substituting = 0;
				break;
			}
			else
			{
				/* Not environment */
				*copyer++ = c;
			}
		}
	}
out:
	/*
	 *  We come here with a complete line,
	 *  or when an overflow occurs.
	 *  Eat up the line if it overflowed,
	 *  and warn just once.
	 */
	if (c != '\n' && c != EOF)
	{
		while (c != '\n' && c != EOF)
		{
			c = getc(fp);
		}
	}
	*copyer = 0;

#ifndef MACHINE_WNT
	if (argc)
	{
		(*handler)(argc, argv, nesting);
	}
#else
	if (argc)
		if (handler)
			return (*handler)(argc,argv,0); 
		else
			return process_mapping(argc,argv,command,sa,si,pi); 
	return -1;
#endif
}

/*
 *  Expand variables from (path) template
 *  and return resulting string.
 *  Result is static data,
 *  but two successive calls work.
 *
 *  NULL input is returned as is.
 */
char * buck_expand(char *tmplt)
{
	static char buffers[2][1024];
	static int bufswap;
	char *buffer;
	char *buffer_tail;
	char *ev, *cp, *cpstart;
	int c;

	/*
	 *  Garbage in...
	 */
	if (!tmplt)
	{
		return (NULL);
	}

	/*
	 *  Two successive calls still do not
	 *  overwrite static buffers, but third
	 *  messes up the first one.
	 */
	bufswap ^= 1;
	buffer = buffers[bufswap];
	buffer_tail = buffer + sizeof(buffers[0]) - 2;

	cp = buffer;

#define put(c) if (cp < buffer_tail) *cp++ = (c); else /* SANS BLAGUE!!! */
#define get()    *tmplt++
#define unget(c) tmplt--

	while ((c = get()) != 0)
	{
		if (c != '$')
		{
			/*
			 *  Keep copying until $ encountered.
			 */
			put(c);
			continue;
		}
		if ((c = get()) == 0)
		{
			break;
		}

		if (c == '$')
		{
			/*
			 *  $$ replaced by single $
			 */
			put(c);
			continue;
		}
		/*
		 *  Use buffer to collect the name of the env. variable.
		 */
		cpstart = cp;

		if (c == LBRC)
		{
			/*
			 *  $HOME
			 */
			while ((c = get()) != 0 && c != RBRC)
				put(c);
			if (c != RBRC)
			{
				goto out;
			}
			put(0);
			ev = getenv(cpstart);
		} else {
			/*
			 *  $HOME
			 */
			while ((c >= '0' && c <= '9') ||
			       (c >= 'A' && c <= 'Z') ||
			       (c >= 'a' && c <= 'z') ||
			       (c == '_') ) {
				put(c);
				if ((c = get()) == 0)
				{
					break;
				}
			}
			/*
			 *  Put back next chr or \0 trailer.
			 */
			unget(c);
			if (*cpstart)
			{
				put(0);
				ev = getenv(cpstart);
			}
			else
			{
				/*
				 *  Lone dollar
				 */
				ev = "$";
			}
		}
		for (cp = cpstart; ev && *ev; ++ev)
			put(*ev);
	}
out:
	*cp = 0;

	return (buffer);
}

void __init_es_environment(void)
{
	setup_environment(NULL);
}

