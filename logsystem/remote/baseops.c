/*========================================================================

	Check excess reply packet for local senddirectory.

	E D I S E R V E R

	File:		logsystem/baseops.c
	Author:		Juha Nurmela (JN)
	Date:		Fri Jan 13 13:13:13 EET 1994

	Copyright (c) 1997 Telecom Finland/EDI Operations

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: baseops.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 22.04.97/JN	Unix and NT code was so different, separated.
  3.01 19.08.97/JN	Garbage data slipped to nonexistent request members.
					Poking scanner can also give new config, "atomically".
					MOVE_FILE for general use.
  3.02 05.09.97/JN	General part.
					Separate general and NT/Unix parts
  3.08 06.10.97/JN	Nonspecific base ops here.
  3.10 09.01.98/JN	Basetype fishing now not so touchy about errors.
  4.00 12.01.98/HT	ODBSYS included.
  4.02 15.06.98/JN	remote stat(2) lookalike
  4.03 07.10.98/KP	Fixed append & truncate in do_really_open_file()
  4.04 08.10.98/HT	Synthetic_handler also for legacyops
  4.05
  4.06 24.11.98/KP	"local" support re-included
  4.07 02.02.99/KP	LOAD_EXTERNAL & SAVE_EXTERNAL
  4.08 10.02.99/HT	WNT/Synthetic handler did not opened file in binary
					mode on write/append modes. Corrected. Caused multiple
					CR's in text files.
  4.09 23.04.99/KP	REMOVE now does buck_expand to path if no index given.
  4.10 20.05.99/KP	STAT_FILE now accepts regular files (and buck_expands 
					their names etc.)
  4.11 26.05.99/HT	new init_client, with errno handling
  4.12 04.06.99/KP	REMOVE failed (crashed) with files outside logsystem.
  4.13 09.06.99/HT	Enhanced closing of synthetic_handler 
  4.14 10.08.99/KP	"Read only" responses were given even with requests
					that do not except any reply -> sync lost.
					Also many non-write operations were blocked in
					read-only mode...
  4.15 14.10.99/KP	Changes in do_synthetic_handle(), to allow it reading
					from pipes etc. also. Kind of a kludge but seems to
					work... i hope.
  4.16 30.05.2008/CRE Change DEBUG_PREFIX
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#ifdef MACHINE_WNT
#include <windows.h>
#endif
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#ifdef MACHINE_WNT
#include "io.h"
#endif

#include "bases.h"
#include "perr.h"
#include "rlserror.h"
#include "bases.h"
#include "runtime/tr_prototypes.h"

#define NOT_FOUND (off_t) -1
#define READ_SIZE 4096
#define LF "\x0a"

#ifndef DEBUG_PREFIX
#define DEBUG_PREFIX	" [unknown] "
#endif

char *buck_expand(char *pat);
extern int check_base_list(char *path, int *type, struct BASE_LIST **list, Client *pcli);
int do_stat_file(Client *p, int idx, char *ext);
int do_copy_file(Client *p, int sidx, char *sext, int didx, char *dext);
int do_get_uniq(Client *p, char *counter_name, int limit_low, int limit_high);
int do_open_directory(Client *p, char *directory);
int move_linking_or_copying(char *src, char *dst);

Base *bases;

baseops_t *baseops[128];

/*
 *  Add one type of base into known ones.
 *  *init returns the ops-struct of the base,
 *  which is added to baseops[]
 *  if it does not already exist there.
 */
void init_base(baseops_t *(*init)())
{
	baseops_t *ops;
	int i;

	if (init && (ops = (*init)(RLS_PROTOCOL_VERSION)))
	{
		for (i = 0; i < sizeof(baseops)/sizeof(*baseops); ++i)
		{
			if (baseops[i] == NULL
			 || baseops[i] == ops)
			 {
				baseops[i] = ops;
				break;
			}
		}
	}
}

/*
 *  Every now and then, let bases do whatever they desire.
 *  For example, as the name implies, for deferred writes.
 */
void global_periodic_sync(void)
{
	Base *bp;

	for (bp = bases; bp; bp = bp->next)
	{
		(*bp->ops->periodic_sync)(bp->internal);
	}
}

void release_base(Base **abase)
{
	Base *base, **pp;

	if ((base = *abase) != NULL)
	{
		*abase = NULL;
		if (--base->refcnt == 0)
		{
			for (pp = &bases; *pp; pp = &(*pp)->next)
			{
				if (*pp == base)
				{
					*pp = base->next;
					break;
				}
			}
			(*base->ops->close_base)(base->internal);
			free(base->name);
			memset( base, 0x00, sizeof(Base) );
			free(base);
		}
	}
}

/*
 *  Before exit()
 *
 *  Keep releasing references until everything gets away.
 */
void unplug_all_bases(void)
{
	Base *bp;

	while ((bp = bases) != NULL)
	{
		release_base(&bp);
	}
}

/*
 *  During endconn() just before client is scrubbed away.
 */
void cleanup_closing_client(Client *p)
{
	/* 4.13/HT file write added, moved outside p->base test */
	if (p->synthetic_handler)
	{
		(*p->synthetic_handler)(p, NULL, 0);
		p->done = 0;
	}
	if (p->base) {
		/*
		 *  Otherwise these make no sense, without a base...
		 *  Make sure basespec releases synthesized data
		 *  and then let basespec go,
		 *  and detach from common Base.
		 */
		(*p->base->ops->close_client)(p);
		release_base(&p->base);
	}
}

/*
 *  Close previous active base and open a new.
 *  First searched from and maybe linked into 'bases'.
 */
int open_base(Client *p, char *basename)
{
	Base *bp;
	void *base_internal;
	int i;

	/*
	 *  Should we call close_client here as well ?
	 *  Client is not closing,
	 *  it is just changing to use another base....
	 */
	release_base(&p->base);
	
	/*
	 * 1.3/KP 
	 * release_base() has free'd our internal active entry,
	 * so set it to NULL.
	 */
	p->internal = NULL;

	/*
	 *  Just closing current ?
	 */
	if (basename == NULL || *basename == 0)
	{
		return (0);
	}

	for (bp = bases; bp; bp = bp->next)
	{
		if (!strcmp(bp->name, basename))
		{

			++bp->refcnt;
			p->base = bp;

			if (DEBUG(10))
			{
				debug("%s%d Old base %s\n",DEBUG_PREFIX,p->key, NOTNUL(basename));
			}

			goto contin;
		}
	}
	if (DEBUG(10))
	{
		debug("%s%d New base %s\n",DEBUG_PREFIX, p->key, NOTNUL(basename));
	}

	/*
	 *  Try each way. Last is always badop returning ENOENT.
	 */
	for (i = 0; baseops[i]; ++i)
	{
		base_internal = (*baseops[i]->open_base)(basename);
		if (base_internal)
		{
			/* eek! */
			goto hit;
		}
		if (errno != EFTYPE   /* basetype unknown, keep searching */
		 && errno != ENOENT   /* accept these ... */
		 && errno != EINVAL)  /* ... also */
		{
			break;
		}
	}
	return (errno ? errno : -1);
hit:
	p->base = calloc(1, sizeof(*p->base));
	basename = strdup(basename);

	if (p->base == NULL || basename == NULL)
	{
		fatal("%salloc new base",DEBUG_PREFIX);
	}

	p->base->internal = base_internal;
	p->base->name     = basename;
	p->base->ops      = baseops[i];
	p->base->refcnt   = 1;
	p->base->next     = bases;

	bases = p->base;

contin:

	/*
	 *  Base now exists, link client and base together.
	 *
	 *  Notice for the underlying base
	 *  to do whatever necessary.
	 */


	/*
	**  4.11 26.05.99/HT
	**  errno can be EAGAIN, that means some resources are temporaly
	**  unavailable. Try again.
	*/
	errno = 0;
	(*p->base->ops->init_client)(p);
	if ( errno ) {
		release_base(&p->base);
		return( errno );
	}

	return (0);
}

/*
 *  Return 0 if IO- or protocol-error
 *  (client terminated).
 */
BOOL process(Client *p)
{
	int type;
	struct reqheader *req = p->request;

	switch (req->type) {

	default: {

			if (DEBUG(11))
			{
				dump_request(p);
			}

			reply(p, UNKNOWN, req->type);
		}
		break;

		/*
		 *  void
		 *
		 *  void
		 */
	case NOP: {

			if (DEBUG(11))
			{
				dump_request(p);
			}

			vreply(p, RESPONSE, 0, _Int, RLS_PROTOCOL_VERSION, _End);
		}
		break;

		/*
		 * void
		 *
		 * no ack (we quit)
		 */
	case FORCE_QUIT: {
			extern int signaled_to_exit;

			signaled_to_exit = 1;

			if (DEBUG(11))
			{
				dump_request(p);
			}
		}
		break;

		/*
		 *  void
		 *
		 *  no ack
		 */
	case SET_DEBUG: {

			lift_args(p,
				_Int, &debugbits,
				_End);

			readagain(p,"",0);
		}
		break;

		/*
		 *  void
		 *
		 *  no ack (not even readagain() !)
		 */
	case MONITOR: {

			if (DEBUG(11))
			{
				dump_request(p);
			}

			if (!p->monitoring) {
				p->monitoring = 1;

				if ((p->nextmon = monitors) != NULL)
				{
					p->prevmon = monitors->prevmon;
				}
				else
				{
					monitors = p;
				}
				monitors->prevmon = p;
				p->prevmon->nextmon = p;
				monitors = p;
			}
		}
		break;

		/*
		 *  string basepath
		 *
		 *  void
		 */
	case BASE: {
			char *path = NULL;

			int err;

			lift_args(p,
				_String, &path,
				_End);

			path = buck_expand(path);
			if (!path || !*path)
			{
				return (0);
			}

			if (( MATCH_FOUND == check_base_list ( path ,&type ,&(p->base_list), p)) && (type == 'N'))
			{
				svclog('I', "%sUser = %s , Authentication : Access to base %s denied in server. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(path) , ACCESS_TO_BASE_DENIED);
				vreply(p, RESPONSE, ACCESS_TO_BASE_DENIED, _Int, RLS_PROTOCOL_VERSION, _String, ACCESS_TO_BASE_DENIED_S ,_End);
				return (0);
			}

			if ((err = open_base(p, path)) != 0)
			{
				reply(p, RESPONSE, err);
			}
			else
			{
				reply(p, RESPONSE, 0);
			}

		}
		break;

		/*
		 *  void
		 *
		 *  int version
		 *  int maxcount
		 *  int numused
		 *  int 0
		 *  int 0
		 */
	case BASE_INFO:
		{
			if (DEBUG(11))
			{
				dump_request(p);
			}

			if (p->base == NULL)
			{
				return (0);
			}

			(*p->base->ops->get_base_info)(p);
		}
		break;

		/*
		 *  void
		 *
		 *  int count
		 */
	case ENTRY_COUNT:
		{
			if (DEBUG(11))
			{
				dump_request(p);
			}

			if (p->base == NULL)
			{
				return (0);
			}

			(*p->base->ops->entry_count)(p);
		}
		break;

		/*
		 *  int    index
		 *  string ext
		 *  string mode (fopen-wise r, w or a)
		 *  int    perms
		 *  int    pid  (specific to windows)
		 *  int    options (STREAMED vs PAGINATED atm)
		 *
		 *  int errno (changes to dataflow only, client deleted)
		 *
		 *  Local connections stay valid, inet connections
		 *  are removed from clients.
		 *  If index is 0, extension is full filename,
		 *  no logsystem-sillines at all.
		 */
	case OPEN: {
			uint32 idx   = 0;
			char  *ext   = NULL;
			uint32 perms = 0;
			char  *mode  = NULL;
			uint32 pid   = 0;
			uint32 options   = 0;

			lift_args(p,
				_Int, &idx,
				_String, &ext,
				_String, &mode,
				_Int, &perms,
				_Int, &pid,
				_Int, &options,
				_End);

			/*
			 *  ext might be part of, or a whole, pathname.
			 *  It is NOT expanded here, but in
			 *  do_open_file.
			 */
			if (!ext || !mode)
			{
				return (0);
			}

			p->pid = pid;
			if (p->base && ( MATCH_FOUND == check_base_list ( p->base->name ,&type ,&(p->base_list), p)) && (type == 'R') && (*mode != 'r'))
			{
				svclog('I', "%sUser = %s , Authentication : Only read access to base %s in server allowed. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(p->base->name), LIM_ACCESS_TO_BASE);
				vreply(p, RESPONSE, LIM_ACCESS_TO_BASE, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_BASE_S ,_End);
				return (0);
			}

			if (options == PAGINATED)
			{
				do_open_file_paginated(p, buck_expand(ext), perms);
			}
			else
			{
				return (do_open_file(p, 0, idx, ext, mode, perms));
			}
			break;
		}

		/*
		 *  int    index  from
		 *  string ext
		 *  int    index  to
		 *  string ext
		 *
		 *  int errno
		 *
		 */
	case COPY_FILE: {
			uint32 sidx = 0;
			char  *sext = NULL; /* source */
			uint32 didx = 0;
			char  *dext = NULL; /* destination */

			lift_args(p,
				_Int, &sidx,
				_String, &sext,
				_Int, &didx,
				_String, &dext,
				_End);
			if (p->base && ( MATCH_FOUND == check_base_list ( p->base->name ,&type ,&(p->base_list), p)) && (type == 'R'))
			{
				svclog('I', "%sUser = %s , Authentication : Only read access to base %s in server allowed. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(p->base->name), LIM_ACCESS_TO_BASE);
				vreply(p, RESPONSE, LIM_ACCESS_TO_BASE, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_BASE_S ,_End);
				return (0);
			}
			return (do_copy_file(p, sidx, sext, didx, dext));
		}

		/*
		 *  int    index
		 *  string ext
		 *
		 *  int errno
		 *
		 */
	case STAT_FILE: {
			stat_t st;
			uint32 idx = 0;
			char  *ext = NULL;
			int ok = 1;

			lift_args(p,
				_Int, &idx,
				_String, &ext,
				_End);

			if (!idx && ext)
			{
				/*
				 * idx=0 and ext != NULL -> regular file.
				 * stat here and do not bother baseops.
				 */
				if (stat(buck_expand(ext), &st) == -1)
				{
					memset(&st, 0, sizeof(st));
					ok = 0;
				}

				vreply(p, RESPONSE, 0,
					_Int, ok ? 0 : -1, /* err (ENOENT only meaninful) */

					_Int, (uint32) st.st_size,  /* size */
					_Int, (uint32) st.st_atime, /* atime */
					_Int, (uint32) st.st_mtime, /* mtime */
					_Int, (uint32) st.st_ctime, /* ctime */
					_Int, (uint32) st.st_mode,  /* perms */
					_Int, S_ISDIR(st.st_mode) ? 'd' :
						  S_ISREG(st.st_mode) ? '-' :
												'-', /* type (only files/dirs) */

					_Int, 0, /* reserved 0 */
					_Int, 0, /* 1 */
					_Int, 0, /* 2 */
					_Int, 0, /* 3 */
					_Int, 0, /* 4 */
					_Int, 0, /* 5 */
					_Int, 0, /* 6 */
					_Int, 0, /* 7 */
					_Int, 0, /* 8 */
					_Int, 0, /* reserved 9 */
					_End);

				return (1);
			}
			if (!idx || !ext || !p->base)
			{
				return (0);
			}

			return (do_stat_file(p, idx, ext));
		}

	case RLS_EXEC: {
			char *mode = NULL;
			char **cmd = NULL;
			uint32 pid = 0;
			int x;

			lift_args(p,
				_String, &mode,
				_StringArray, &cmd,
				_Int, &pid,
				_End);

			if (!cmd || !*cmd)
			{
				free_stringarray(cmd);
				return (0); /* Has to be at least argv[0], kick client */
			}
			if (mode && !*mode)
			{
				mode = NULL;    /* simplify "" to NULL */
			}

			if ( p->RunFlag == 'N')
			{
				free_stringarray(cmd);
				svclog('I', "%sUser = %s , Authentication : No permissions run programs in server. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), NO_RUN_RIGHTS_IN_SERVER);
				vreply(p, RESPONSE, NO_RUN_RIGHTS_IN_SERVER, _Int, RLS_PROTOCOL_VERSION, _String, NO_RUN_RIGHTS_IN_SERVER_S ,_End);
				return 0;
			}


			p->pid = pid;

			x = do_rls_exec(p, mode, cmd);
#ifndef MACHINE_WNT
			free_stringarray(cmd);
#endif
			return (x);
		}
		
		/*
		 *  string path
		 *  int    perms
		 *
		 *  int errno
		 */
	case MKDIR: {
			char  *path = NULL;
			uint32 perms = 0;

			lift_args(p,
				_String, &path,
				_Int, &perms,
				_End);

			path = buck_expand(path);
			if (!path)
			{
				return (0);
			}

			if (p->FileFlag == 'N')
			{
				svclog('I', "%sUser = %s , Authentication : No access rights to the file system in server. Auth Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), ACCESS_TO_FIL_SYS_DENIED);
				vreply(p, RESPONSE, ACCESS_TO_FIL_SYS_DENIED, _Int, RLS_PROTOCOL_VERSION, _String, ACCESS_TO_FIL_SYS_DENIED_S ,_End);
				return (0);
			}

			if (p->FileFlag == 'R')
			{
				svclog('I', "%sUser = %s , Authentication : Only read only access to the file system in server. Auth Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), LIM_ACCESS_TO_FIL_SYS);
				vreply(p, RESPONSE, LIM_ACCESS_TO_FIL_SYS, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_FIL_SYS_S ,_End);
				return (0);
			}
			return (do_really_make_directory(p, path, perms));
		}
		
		/*
		 *  int    file-designator
		 *  string dummy
		 *  string mode (fopen-wise)
		 *  int    perms
		 *
		 *  int errno (changes to dataflow only, client deleted)
		 *
		 *  Local connections stay valid, inet connections
		 *  are removed from clients.
		 *  If index is 0, extension is full filename,
		 *  no logsystem-sillines at all.
		 */
	case SPECIAL_OPEN:
	{
			uint32 kw    = 0;    /* designator */
			char  *dummy = NULL;
			uint32 perms = 0;
			char  *mode  = NULL;
			uint32 pid   = 0;

			lift_args(p,
				_Int, &kw,
				_String, &dummy,
				_String, &mode,
				_Int, &perms,
				_Int, &pid,
				_End);

			if (!mode)
			{
				return (0);
			}

			if (p->FileFlag == 'N')
			{
				svclog('I', "%sUser = %s , Authentication : No access rights to the file system in server. Auth Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), ACCESS_TO_FIL_SYS_DENIED);
				vreply(p, RESPONSE, ACCESS_TO_FIL_SYS_DENIED, _Int, RLS_PROTOCOL_VERSION, _String, ACCESS_TO_FIL_SYS_DENIED_S ,_End);
				return (0);
			}

			p->pid = pid;
			return (do_open_file(p, kw, 0, NULL, mode, perms));
		}

		/*
		 * int       offset bytes from specified position
		 * double    position aka SEEK_SET, SEEK_CUR, SEEK_END
		 *
		 * seek into the current opened file 
		 * return the new position we seeked (current with offset 0 and SEEK_CUR)
		 * or return -1 if no opened file
		 */
	case SEEK_FILE:
	{
			double offset = 0;
			uint32 position = 0;

			lift_args(p,
				_Double, &offset,
				_Int, &position,
				_End);

			if (p->FileFlag == 'N')
			{
				svclog('I', "%sUser = %s , Authentication : No access rights to the file system in server. Auth Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), ACCESS_TO_FIL_SYS_DENIED);
				vreply(p, RESPONSE, ACCESS_TO_FIL_SYS_DENIED, _Int, RLS_PROTOCOL_VERSION, _String, ACCESS_TO_FIL_SYS_DENIED_S ,_End);
				return (0);
			}

			do_seek_file(p, offset, position);
		}
		break;

		/*
		 * double    size : size of block we want to be returned from file 
		 * int       reserved1 : (not used reserved for futur use, could be omitted as the protocol is de facto extendable)
		 *
		 * seek into the current opened file 
		 * return the new position we seeked (current with offset 0 and SEEK_CUR)
		 * or return -1 if no opened file
		 */
	case READ_FILE:
	{
			double size = 0;
			uint32 reserved1 = 0;

			lift_args(p,
				_Double, &size,
				_Int, &reserved1,
				_End);

			if (p->FileFlag == 'N')
			{
				svclog('I', "%sUser = %s , Authentication : No access rights to the file system in server. Auth Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), ACCESS_TO_FIL_SYS_DENIED);
				vreply(p, RESPONSE, ACCESS_TO_FIL_SYS_DENIED, _Int, RLS_PROTOCOL_VERSION, _String, ACCESS_TO_FIL_SYS_DENIED_S ,_End);
				return (0);
			}

			do_read_file(p, size, reserved1);
		}
		break;

		/*
		 * double    size : size of block we want to be returned from file 
		 * int       reserved1 : (not used reserved for futur use, could be omitted as the protocol is de facto extendable)
		 *
		 * seek into the current opened file 
		 * return the new position we seeked (current with offset 0 and SEEK_CUR)
		 * or return -1 if no opened file
		 */
	case CLOSE_FILE:
	{
			if (DEBUG(11))
			{
				dump_request(p);
			}

			if (p->FileFlag == 'N')
			{
				svclog('I', "%sUser = %s , Authentication : No access rights to the file system in server. Auth Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), ACCESS_TO_FIL_SYS_DENIED);
				vreply(p, RESPONSE, ACCESS_TO_FIL_SYS_DENIED, _Int, RLS_PROTOCOL_VERSION, _String, ACCESS_TO_FIL_SYS_DENIED_S ,_End);
				return (0);
			}

			do_close_file(p);
		}
		break;

		/*
		 *  string field-name
		 *
		 *  fieldtype
		 *  fieldsize
		 */
	case TELL_THIS_FIELD: {
			char *fldname = NULL;

			lift_args(p,
				_String, &fldname,
				_End);

			if (p->base == NULL || !fldname || !*fldname)
			{
				return (0);
			}

			(*p->base->ops->tell_named_field)(p, fldname);
		}
		break;

		/*
		 *  int field-index (in label)
		 *
		 *  fieldtype
		 *  fieldsize
		 *  fieldname
		 *
		 *  OR
		 *
		 *  int field-index (in label)
		 *
		 *  fieldname or fieldtype or fieldsize
		 */
	case TELL_FIELD:
	case TELL_FIELD_NAME:
	case TELL_FIELD_TYPE:
	case TELL_FIELD_SIZE: {
			uint32 fnum;

			lift_args(p,
				_Int, &fnum,
				_End);

			if (p->base == NULL)
			{
				return (0);
			}

			(*p->base->ops->tell_next_field)(p, fnum, req->type);
		}
		break;

		/*
		 *  void
		 *
		 *  no ack.
		 */
	case CLEAR_FILTER: {

			if (DEBUG(11))
			{
				dump_request(p);
			}

			if (p->file != NULL)
			{
				do_clear_file_filter(p);
			}
			else if (p->base != NULL)
			{
				(*p->base->ops->clear_filter)(p);
			}
			else
			{
				return (0);
			}

			readagain(p,"",0);
		}
		break;

		/*
		 *  int order
		 *
		 *  no ack.
		 */
	case SET_FILTER_ORDER:
		{
			/* XXX default value ? */
			uint32 order;

			lift_args(p,
				_Int, &order,
				_End);

			if (p->file != NULL)
			{
				do_set_file_filter_order(p, order);
			}
			else if (p->base != NULL)
			{
				(*p->base->ops->set_filter_order)(p, order);
			}
			else
			{
				return (0);
			}

			readagain(p,"",0);
		}
		break;

		/*
		 *  string keyname
		 *
		 *  no ack.
		 */
	case SET_FILTER_KEY:
		{
			char *keyname = NULL;

			lift_args(p,
				_String, &keyname,
				_End);

			if (p->base == NULL || !keyname)
			{
				return (0);
			}

			(*p->base->ops->set_filter_key)(p, keyname);
			readagain(p,"",0);
		}
		break;

		/*
		 *  string filter
		 *
		 *  no ack.
		 */
	case ADD_FILTER:
		{
			char *filterop = NULL;

			lift_args(p,_String,&filterop,_End);

			if (p->base == NULL || !filterop)
			{
				return (0);
			}

			(*p->base->ops->add_filter)(p, filterop);
			readagain(p,"",0);
		}
		break;

		/* 
		 * string filter
		 *
		 * no ack.
		 */
	case ADD_FILTER_PARAM:
		{
			char *filterop = NULL;

			lift_args(p,_String,&filterop,_End);

			if (!filterop)
			{
				return (0);
			}

			if (p->file != NULL)
			{
				do_add_file_filterparam(p, filterop);
			}
			else if (p->base != NULL)
			{
				(*p->base->ops->add_filterparam)(p, filterop);
			}
			else
			{
				return (0);
			}

			readagain(p,"",0);
		}
		break;

		/*
		 *  void
		 *
		 *  int keytype
		 */
	case COMPILE_FILTER:
		{
			if (DEBUG(11))
			{
				dump_request(p);
			}

			if (p->file != NULL)
			{
				do_compile_file_filter(p);
				break;
			}

			if (p->base != NULL)
			{
				(*p->base->ops->compile_filter)(p);
				break;
			}
		}
		return (0);

		/*
		 *  void
		 *
		 *  int  index
		 */
	case GET_NEXT_MATCH:
		{
			if (DEBUG(11))
			{
				dump_request(p);
			}

			if (p->file != NULL)
			{
				do_get_next_file_match(p);
				break;
			}

			if (p->base != NULL)
			{
				(*p->base->ops->get_next_match)(p);
				break;
			}
		}
		return (0);

		/*
		 *  int index
		 *
		 *  void
		 */
	case READ_ENTRY: {
			uint32 idx;

			lift_args(p,
				_Int, &idx,
				_End);

			if (p->base == NULL)
			{
				return (0);
			}

			(*p->base->ops->read_entry)(p, idx);
		}
		break;

		/*
		 *  void
		 *
		 *  void
		 */
	case WRITE_ENTRY: {

			if (DEBUG(11))
			{
				dump_request(p);
			}

			if (p->base == NULL)
			{
				return (0);
			}

			if (( MATCH_FOUND == check_base_list ( p->base->name ,&type ,&(p->base_list), p)) && (type == 'R'))
			{
				svclog('I', "%sUser = %s , Authentication : Only read access to base %s in server allowed. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(p->base->name), LIM_ACCESS_TO_BASE);
				vreply(p, RESPONSE, LIM_ACCESS_TO_BASE, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_BASE_S ,_End);
				return (0);
			}


			(*p->base->ops->write_entry)(p);
		}
		break;

		/*
		 *  int fromidx
		 *
		 *  int error
		 *
		 *  Copy named record to current record.
		 *  If named (old) record is not found active,
		 *  read it in temporarily but free after copy.
		 *  Be careful if copying to itself.
		 */
	case COPY: {
			uint32 idx;

			lift_args(p,
				_Int, &idx,
				_End);

			if (p->base == NULL)
			{
				return (0);
			}

			if (( MATCH_FOUND == check_base_list ( p->base->name ,&type ,&(p->base_list), p)) && (type == 'R'))
			{
				svclog('I', "%sUser = %s , Authentication : Only read access to base %s in server allowed. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(p->base->name), LIM_ACCESS_TO_BASE);
				vreply(p, RESPONSE, LIM_ACCESS_TO_BASE, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_BASE_S ,_End);
				return (0);
			}

			(*p->base->ops->copy_entry)(p, idx);
		}
		break;

		/*
		 *  void
		 *
		 *  int   index
		 */
	case CREATE: {

			if (DEBUG(11))
			{
				dump_request(p);
			}

			if (p->base == NULL) {
				reply(p, RESPONSE, EINVAL);
				break;
			}

			if (( MATCH_FOUND == check_base_list ( p->base->name ,&type ,&(p->base_list), p)) && (type == 'R'))
			{
				svclog('I', "%sUser = %s , Authentication : Only read access to base %s in server allowed. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(p->base->name), LIM_ACCESS_TO_BASE);
				vreply(p, RESPONSE, LIM_ACCESS_TO_BASE, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_BASE_S ,_End);
				return (0);
			}

			(*p->base->ops->create_new)(p);
		}
		break;

		/*
		 *  int  index (0 for path in ext)
		 *  (string ext, can be missing)
		 *
		 *  void
		 */
	case REMOVE: {
			uint32 idx = 0;
			char *ext = NULL;

			int err = 1;

			lift_args(p,
				_Int, &idx,
				_String, &ext,
				_End);

			if (!idx && !ext)
			{
				if (DEBUG(12))
				{
					debug("%sREMOVE: Either idx or ext must be specified.\n",DEBUG_PREFIX);
				}

				return (0);  /* one or another is required */
			}

			if (idx)
			{
				if (p->base == NULL)
				{
					if (DEBUG(12))
					{
						debug("%sREMOVE: Base must be non-null.\n",DEBUG_PREFIX);
					}

					reply(p, RESPONSE, EINVAL);
					break;
				}
				if (( MATCH_FOUND == check_base_list ( p->base->name ,&type ,&(p->base_list), p)) && (type == 'R'))
				{
					svclog('I', "%sUser = %s , Authentication : Only read access to base %s in server allowed. Error: %d",
						DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(p->base->name), LIM_ACCESS_TO_BASE);
					if (DEBUG(12))
					{
						debug("%sREMOVE: Authentication error.\n",DEBUG_PREFIX);
					}

					vreply(p, RESPONSE, LIM_ACCESS_TO_BASE, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_BASE_S ,_End);
					return (0);
				}
			}

			if (!idx) {
				/*
				 *  File outside logsystem. path in ext.
				 *  Accept both files and directories.
				 *  At least NT4.0 did not return meaningfull
				 *  errors from using the wrong function for
				 *  file/directory, so try DF() blindly then RD()...
				 *
				 *  Avoiding unlinking directories on U*X
				 *
				 * 4.09/KP : buck_expand
				 */
				char *path = buck_expand(ext);

				if (DEBUG(12))
				{
					debug("%sREMOVE: Removing file %s.\n",DEBUG_PREFIX, NOTNUL(path));
				}
#ifdef MACHINE_WNT
				if (DeleteFile(path) || RemoveDirectory(path))
				{
					err = 0;
				}
				else if (DEBUG(12))
				{
					debug("%sREMOVE: Remove failed (path=%s, error %d).\n",DEBUG_PREFIX,NOTNUL(path), GetLastError());
				}
#else
				if (rmdir(path) == 0 || (errno == ENOTDIR && unlink(path) == 0))
				{
					err = 0;
				}
#endif
				vreply(p, RESPONSE, 0, _Int, err ? -1 : 0, _End);

			}
			else if (ext)
			{
				/*
				 *  Just files, "" extension means all files.
				 */
				if (*ext == 0)
				{
					ext = NULL;
				}

				(*p->base->ops->remove_subfiles)(p, idx, ext);
			}
			else
			{
				/*
				*  Currently active record ?
				*  Mark as removed,
				*  and if we hold it, release.
				*  Just remove if not active.
				*/
				(*p->base->ops->remove_entry)(p, idx);
			}
		}
		break;

		/*
		 *  string from
		 *  string to
		 *
		 *  int error
		 */
	case MOVE_FILE: {
			int junk;
			char *src;
			char *dst;

			int err;

			lift_args(p,
				_Int, &junk,
				_String, &src,
				_String, &dst,
				_End);

			src = buck_expand(src);
			dst = buck_expand(dst);

			if (!src || !dst)
			{
				return (0);  /* both are required */
			}

			if (p->FileFlag == 'N')
			{
				svclog('I', "%sUser = %s , Authentication : No access rights to the file system in server. Auth Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), ACCESS_TO_FIL_SYS_DENIED);
				vreply(p, RESPONSE, ACCESS_TO_FIL_SYS_DENIED, _Int, RLS_PROTOCOL_VERSION, _String, ACCESS_TO_FIL_SYS_DENIED_S ,_End);
				return (0);
			}

			if (p->FileFlag == 'R')
			{
				svclog('I', "%sUser = %s , Authentication : Only read only access to the file system in server. Auth Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), LIM_ACCESS_TO_FIL_SYS);
				vreply(p, RESPONSE, LIM_ACCESS_TO_FIL_SYS, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_FIL_SYS_S ,_End);
				return (0);
			}

			err = move_linking_or_copying(src, dst);

			vreply(p, RESPONSE, 0, _Int, err ? -1 : 0, _End);
		}
		break;

		/*
		 *  string name
		 *
		 *  string value
		 */
	case GET_ENV_VAR: {
			char *name = NULL;
			char *value;

			lift_args(p,
				_String, &name,
				_End);

			if (!name || !*name || !(value = getenv(name)))
			{
				value = "";
			}

			vreply(p, RESPONSE, 0, _String, value, _End);
		}
		break;
		
		/*
		 *  string fieldname
		 *
		 *  float/string/int  fieldvalue
		 */
	case GET_NUM_FIELD:
	case GET_TEXT_FIELD:
	case GET_TIME_FIELD: {
			char *fldname;

			lift_args(p,
				_String, &fldname,
				_End);

			if (p->base == NULL || !fldname) {
				reply(p, RESPONSE, EINVAL);
				break;
			}
			(*p->base->ops->getavalue)(p, req->type, fldname);
		}
		break;

		/*
		 *  string parmname
		 *  string filename
		 *
		 *  float/string/int  fieldvalue
		 */
	case GET_NUM_PARM:
	case GET_TEXT_PARM:
	case GET_TIME_PARM: {
			char *parmname = NULL;
			char *filename = NULL;

			lift_args(p,
				_String, &parmname,
				_String, &filename,
				_End);

			filename = buck_expand(filename);

			if (!parmname || !filename)
			{
				return (0);
			}

			if (p->FileFlag == 'N')
			{
				svclog('I', "%sUser = %s , Authentication : No access rights to the file system in server. Auth Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), ACCESS_TO_FIL_SYS_DENIED);
				vreply(p, RESPONSE, ACCESS_TO_FIL_SYS_DENIED, _Int, RLS_PROTOCOL_VERSION, _String, ACCESS_TO_FIL_SYS_DENIED_S ,_End);
				return (0);
			}

			getaparm(p, req->type, filename, parmname);
		}
		break;

		/*
		 *  string fieldname
		 *  float/string/int fieldvalue
		 *
		 *  no ack.
		 */

	case SET_NUM_FIELD:
	case SET_TEXT_FIELD:
	case SET_TIME_FIELD: {
			char *fldname = NULL;
			union rlsvalue avalue;

			switch (req->type)
			{
				default:
					if (DEBUG(11))
					{
						dump_request(p);
					}
					break;
				case SET_NUM_FIELD: lift_args(p,_String,&fldname,_Double,&avalue,_End); break;
				case SET_TEXT_FIELD: lift_args(p,_String,&fldname,_String,&avalue,_End);
					if (!avalue.s)
					{
						avalue.s = "";
					}
					break;
				case SET_TIME_FIELD: lift_args(p,_String,&fldname,_Int,&avalue,_End); break;
			}

			if (p->base && ( MATCH_FOUND == check_base_list ( p->base->name ,&type ,&(p->base_list), p)) && (type == 'R'))
			{
				svclog('I', "%sUser = %s , Authentication : Only read access to base %s in server allowed. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(p->base->name) , LIM_ACCESS_TO_BASE);
				vreply(p, RESPONSE, LIM_ACCESS_TO_BASE, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_BASE_S ,_End);
				return 0;
			}
			if (p->base && fldname)
			{
				(*p->base->ops->setavalue)(p, req->type, fldname, &avalue);
				vreply(p, RESPONSE, 0, _Int, 0, _End );
			}
			else
			{
				readagain(p,"",0);
			}
		}
		break;

		/*
		 *  string fieldname
		 *  float/string/int fieldvalue
		 *  string path
		 *
		 *  no ack.
		 */

	case SET_NUM_PARM:
	case SET_TEXT_PARM:
	case SET_TIME_PARM:
	{
			char *parmname = NULL;
			char *filename = NULL;
			union rlsvalue avalue;

			switch (req->type)
			{
				default:
					if (DEBUG(11))
					{
						dump_request(p);
					}
					break;
				case SET_NUM_PARM: lift_args(p,_String,&parmname,_Double,&avalue,_String,&filename,_End); break;
				case SET_TEXT_PARM: lift_args(p,_String,&parmname,_String,&avalue,_String,&filename,_End);
					if (!avalue.s)
					{
						avalue.s = "";
					}
					break;
				case SET_TIME_PARM: lift_args(p,_String,&parmname,_Int,&avalue,_String,&filename,_End); break;
			}
			filename = buck_expand(filename);

			if (filename && parmname)
			{
				if (p->FileFlag == 'N')
				{
					svclog('I', "%sUser = %s , Authentication : No access rights to the file system in server. Auth Error: %d",
						DEBUG_PREFIX,NOTNUL(getenv("USER")), ACCESS_TO_FIL_SYS_DENIED);
					/*	
					 * 4.14/KP : SET_XXX_PARM does not expect a reply, so fail silently...
					 */
				}
				else if (p->FileFlag == 'R')
				{
					svclog('I', "%sUser = %s , Authentication : Only read only access to the file system in server. Auth Error: %d",
						DEBUG_PREFIX,NOTNUL(getenv("USER")), LIM_ACCESS_TO_FIL_SYS);
					/*	
					 * 4.14/KP : SET_XXX_PARM does not expect a reply, so fail silently...
					 */
				}
				else
				{
					setaparm(p, req->type, filename, parmname, &avalue);
				}
			}
			readagain(p,"",0);
		}
		break;

		/*
		 *  void
		 *
		 *  int error
		 */
	case PEEK_SCANNER: {

			int err;

			if (DEBUG(11))
			{
				dump_request(p);
			}

			err = pokepeek_scanner(req->type, NULL);
			vreply(p, RESPONSE, 0, _Int, err, _End);
		}
		break;

		/*
		 *  string new_cfg_file (can be missing)
		 *
		 *  int error
		 */
	case POKE_SCANNER: {
			char *path;

			int err;

			lift_args(p,
				_String, &path,
				_End);

			path = buck_expand(path);

			err = pokepeek_scanner(req->type, path);
			vreply(p, RESPONSE, 0, _Int, err, _End);
		}
		break;

		/*
		 *  string keyword
		 *
		 *  string newline_separated_lines
		 */
	case ENUMERATE: {
			char *kw = NULL;
			int start = 0;
			int count = 0;

			lift_args(p,
				_String, &kw,
				_Int, &start,
				_Int, &count,
				_End);
				
			nenumerate_keyword(p, kw, start, count);
		}
		break;
	case WHAT_FILES: {
			int idx = 0;

			lift_args(p,
				_Int, &idx,
				_End);

			if (!p->base)
			{
				return (0);
			}
			(*p->base->ops->enumerate_extensions)(p, idx);
		}
		break;

		/*
		 *  int index
		 *  char *ext
		 *
		 *  no ack.
		 */
	case CLEAR_PARMS: {
			int idx = 0;
			char *ext = NULL;

			lift_args(p,
				_Int, &idx,
				_String, &ext,
				_End);

			if (!p->base || !idx || !ext)
			{
				return (0);
			}

			if (( MATCH_FOUND == check_base_list ( p->base->name ,&type ,&(p->base_list), p)) && (type == 'R'))
			{
				svclog('I', "%sUser = %s , Authentication : Only read access to base %s in server allowed. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), p->base->name , LIM_ACCESS_TO_BASE);
				vreply(p, RESPONSE, LIM_ACCESS_TO_BASE, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_BASE_S ,_End);
				return 0;
			}

			(*p->base->ops->clear_parms)(p, idx, ext);
			readagain(p,"",0);
		}
		break;

	case ENUM_PARMS: {
			int idx = 0;
			char *ext = NULL;

			lift_args(p,
				_Int, &idx,
				_String, &ext,
				_End);

			if (!p->base || !idx || !ext)
			{
				return (0);
			}
			(*p->base->ops->enum_parms)(p, idx, ext);
		}
		break;

	case FETCH_PARMS: {
			int idx = 0;
			char *ext = NULL;

			lift_args(p,
				_Int, &idx,
				_String, &ext,
				_End);

			if (!p->base || !idx || !ext)
			{
				return (0);
			}
			(*p->base->ops->fetch_parms)(p, idx, ext);
		}
		break;

		/*
		 * Save whole supplemental file, given in newline-separated
		 * string.
		 */
	case SAVE_PARMS: {
			int idx = 0;
			char *ext = NULL, *s = NULL;

			lift_args(p,
				_Int,		&idx,
				_String,	&ext,
				_String,	&s,
				_End);

			if (!p->base || !idx || !ext)
			{
				return 0;
			}

			if (( MATCH_FOUND == check_base_list ( p->base->name ,&type ,&(p->base_list), p)) && (type == 'R'))
			{
				svclog('I', "%sUser = %s , Authentication : Only read access to base %s in server allowed. Error: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(p->base->name) , LIM_ACCESS_TO_BASE);
				vreply(p, RESPONSE, LIM_ACCESS_TO_BASE, _Int, RLS_PROTOCOL_VERSION, _String, LIM_ACCESS_TO_BASE_S ,_End);
				return 0;
			}

			(*p->base->ops->save_parms)(p, idx, ext, s);
		}
		break;

		/*
		 *  counter-name (e.g. ovt.COUNTER)
		 *  min-counter-value (constant lower limit)
		 *  max-counter-value (constant upper limit)
		 *
		 *  counter-current-value (string)
		 *
		 *  side-effect: the counter-value is incremented.
		 */
	case GET_UNIQ: {
			char *counter = NULL;
			int limit_low = 0;
			int limit_high = 0;

			lift_args(p,
				_String, &counter,
				_Int,    &limit_low,
				_Int,    &limit_high,
				_End);

			if (!counter || !p->base)
			{
				return (0);
			}

			do_get_uniq(p, counter, limit_low, limit_high);
		}
		break;
	}

	return (1);
}

char * map_special(int num)
{
	switch (num)
	{
		case RLSFILE_SCANNER:
			return "scanner.cfg";
		default: break;
	}
	return (NULL);
}

/*
 *  Supplemental files under bases.
 *
 *  One level of nonexisting path is automatically created
 *  for writes and appends.
 */
BOOL do_open_file(Client *p,
	int spec,             /* if NZ, designator for "special" files */
	int idx,              /* index in logsystem */
	char *ext,            /* the supplemental extension */
	char *mode, int perms)
{
	char *filename;

	if (DEBUG(12))
	{
		debug("%sdo_open_file:  spec=%d  indx=%d  %s  [%s]\n",DEBUG_PREFIX, spec, idx, NOTNUL(ext), NOTNUL(mode));
	}

	switch (*mode) {
	default:
invalid:
		vreply(p, RESPONSE, 0, _Int, EINVAL, _Int, 0, _End);
			return (0);
	case 'r':
	case 'w':
	case 'a':
		break;
	}

	if (idx) {
		/*
		 *  ext is the supplemental extension.
		 *  Should we expand $E from extension too ?
		 */
		if (p->base == NULL)
		{
			/* eek! */
			goto invalid;
		}

		return ((*p->base->ops->open_file)(p, idx, ext, mode, perms));
	}
	if (spec) {
		/*
		 *  ext is meaningless
		 */
		filename = map_special(spec);
		if (filename == NULL)
		{
			/* eek! */
			goto invalid;
		}
	} else {
		/*
		 *  ext is the plain filename,
		 *  maybe with embedded environmental variables.
		 */
		filename = buck_expand(ext);
	}
	return (do_really_open_file(p, filename, mode, perms));
}

/*
 * open file in paginated mode
 * that's to say, open it, leave it opened, and store the handle to it
 * or return -1 if no file to open
 */
void do_open_file_paginated(Client *p, char *filename, int perms)
{
	FileStruct *FS;
	HANDLE h;

	if (DEBUG(12))
	{
		debug("%sdo_open_file_paginated.\n",DEBUG_PREFIX);
	}

#ifdef MACHINE_WNT
	h = _open(filename,
#else
	h = open(filename,
#endif
		O_RDONLY
#ifdef MACHINE_WNT
		| O_BINARY
#endif
		,
#ifdef MACHINE_WNT
		_S_IREAD | _S_IWRITE);
#else
		perms);
#endif

	if (h == -1)
	{
		if (DEBUG(12))
		{
			debug("%sopen_file_paginated: cannot open file %s : %s.\n",DEBUG_PREFIX,NOTNUL(filename),strerror(errno));
		}
		vreply(p, RESPONSE, 0, _Int, errno ? errno : -1, _Int, 0, _End);
	}
	else
	{
		FS = calloc( 1, sizeof(FileStruct) );
		FS->fnum = h;
		FS->bReadonly = 1;
		FS->bAppend = 0;
		FS->name = strdup( filename );
		p->file = FS;
		vreply(p, RESPONSE, 0, _Int, 0, _Int, 0, _End);
	}
}

/*
 * seek position into file opened in paginated mode
 * return the new position we seeked (current with offset 0 and SEEK_CUR)
 * or return -1 if no opened file
 */
void do_seek_file(Client *p,
	double offset,     /* offset bytes from specified position */
	int position)      /* SEEK_SET, SEEK_CUR, SEEK_END */
{
	FileStruct *FS = p->file;
	off_t newposition;
	int pos;

	if (DEBUG(12))
	{
		debug("%sdo_seek_file.\n",DEBUG_PREFIX);
	}

	if (FS == NULL)
	{
		if (DEBUG(12))
		{
			debug("%sseek_file: No file opened.\n",DEBUG_PREFIX);
		}
		vreply(p, RESPONSE, 0, _Double, -1.0, _End);
		return;
	}

	switch (position)
	{
		case 0: pos = SEEK_SET; break;
		case 1: pos = SEEK_CUR; break;
		case 2: pos = SEEK_END; break;
		default: pos = SEEK_CUR; break;
	}

#ifdef MACHINE_WNT
	if ((newposition = (double)_lseek(FS->fnum,offset,position)) == -1.0)
#else	
	if ((newposition = (double)lseek(FS->fnum,offset,position)) == -1.0)
#endif
	{
		if (DEBUG(12))
		{
			debug("%sseek_file: cannot seek/tell asked position : %s.\n",DEBUG_PREFIX,strerror(errno));
		}
		vreply(p, RESPONSE, 0, _Double, -1.0, _End);
		return;
	}

	/* error on ftell return -1.0 */
	vreply(p, RESPONSE, 0, _Double, (double)newposition, _End);
}

static void read_filtered_file(Client *p, const char *expression, char **pbuf)
{
	FileStruct* FS = p->file;
	char searched[READ_SIZE] = "*"; /* pattern "*expression*" */
	char s[READ_SIZE + 1] = "";
	char *line = NULL;
	char *nextline = NULL;
	char newline[READ_SIZE + 1] = "";
	char prevblockend[READ_SIZE + 1] = ""; 
#ifndef MACHINE_WNT
	char *saveptr = NULL;
#endif
	size_t chuncksize = 0;
	size_t inc = READ_SIZE;
	size_t size = 0;

	if (FS == NULL)
	{ 
		return;
	}

	if (FS->filter == NULL)
	{ 
		return;
	}

	strcpy(*pbuf,"");

	chuncksize = FS->filter->chuncksize;

	if (expression[0] == '*')
	{
		strcpy(searched, expression);
	}
	else
	{
		strcpy(searched+1, expression);
	}
	strcat(searched, "*");  /* the ...* in pattern */

	if (DEBUG(12))
	{
		debug("%ssearch filtered read\n",DEBUG_PREFIX);
		debug("%ssearch : chunksize=%ld, expression=%s, searched=%s\n",DEBUG_PREFIX, chuncksize, NOTNUL(expression), NOTNUL(searched)); 
	}

	do
	{
		if (size + READ_SIZE > chuncksize)
		{
			inc = chuncksize % READ_SIZE;
		}
		else
		{
			inc = READ_SIZE;
		}
		size += inc;

		if ((read(FS->fnum, s, inc)) > 0)
		{
			s[READ_SIZE + 1] = '\0';
#ifdef MACHINE_WNT
			line = strtok(s,LF);
#else
			line = strtok_r(s,LF,&saveptr);
#endif
			if (prevblockend != "")
			{
				strcpy(newline,prevblockend);
				strcat(newline,line);
				line = newline;
				strcpy(prevblockend,"");
			}	
#ifdef MACHINE_WNT
			while ((nextline = strtok(NULL,LF)) != NULL)
#else
			while ((nextline = strtok_r(NULL,LF,&saveptr)) != NULL)
#endif
			{
				if ((FS->filter->type == FILE_FILTER_TYPE_RTE) && (tr_wildmat(line, searched)))
				{
					strcat(*pbuf,line);
					strcat(*pbuf,LF);
				}
				strcpy(line,nextline);
			}
			if ((FS->filter->type == FILE_FILTER_TYPE_RTE) && (tr_wildmat(line, searched)))
			{
				strcat(*pbuf,line);
				strcat(*pbuf,LF);
			}
			else
			{
				strcpy(prevblockend,line);
			}
		}
	}
	while ((s) && (size != chuncksize));
}

/*
 * read a block into file opened in paginated mode
 */
void do_read_file(Client *p,
	double size,        /* offset bytes from specified position */
	int reserved1)      /* future use */
{
	char* buf;
	double retval;
	FileStruct *FS = p->file;
#ifdef MACHINE_WNT
	DWORD numread;
#endif
	int action = FILE_FILTER_ACTION_READ;

	if (DEBUG(12))
	{
		debug("%sdo_read_file.\n",DEBUG_PREFIX);
	}

	if (FS == NULL)
	{
		if (DEBUG(12))
		{
			debug("%sread_file: No file opened.\n",DEBUG_PREFIX);
		}
		vreply(p, RESPONSE, 0, _Int, -1, _String, "", _End);
		return;
	}

	if (size == 0.0)
	{
		if (DEBUG(12))
		{
			debug("%s%d Nothing to read from file : %s \n",DEBUG_PREFIX, p->key, NOTNUL(FS->name));
		}
		vreply(p, RESPONSE, 0, _Int, -1, _String, "", _End);
		return;
	}

	buf = malloc((size_t)(size + 0.5));

	if (DEBUG(12))
	{
		debug("%s%d Reading %lf/%d bytes from file : %s \n",DEBUG_PREFIX, p->key, size, (size_t)(size + 0.5), NOTNUL(FS->name));
	}

	if (FS->filter != NULL)
	{
		action = FS->filter->action;
	}

	switch (action)
	{
		case FILE_FILTER_ACTION_FILTER:
			read_filtered_file(p,FS->filter->filter,&buf);
			retval = (double) strlen(buf);
			break;
		case FILE_FILTER_ACTION_FIND:
		case FILE_FILTER_ACTION_READ:
		default:
#ifdef MACHINE_WNT
			if ((int)(retval = (double) _read( FS->fnum, buf, (DWORD)(size + 0.5))) < 0)
			{
				/* Try ReadFile() if _read() fails, in case the "file" is a pipe ... */
				ReadFile( (HANDLE) FS->fnum, buf, (DWORD)(size + 0.5), &numread, NULL );
				retval = (double)numread;
			}
#else
			retval = (double)read( FS->fnum, buf, (size_t)(size + 0.5));
#endif
	}

	if (DEBUG(12))
	{
		debug("%s%d Got %lf bytes from file %s \n",DEBUG_PREFIX, p->key, retval, NOTNUL(FS->name));
	}

	buf[(size_t)(retval + 0.5)] = '\0';
	vreply(p, RESPONSE, 0, _Int, retval == -1.0?errno:0, _String, buf, _End);
	free(buf);
}

/*
 * read a block into file opened in paginated mode
 */
void do_close_file(Client *p)
{
	FileStruct* FS = p->file;

	if (DEBUG(12))
	{
		debug("%sdo_close_file.\n",DEBUG_PREFIX);
	}

	if (p->file == NULL)
	{
		if (DEBUG(12))
		{
			debug("%sclose_file: no file to close ?\n",DEBUG_PREFIX);
		}
		vreply(p, RESPONSE, 0, _Int, -1, _End);
		return;
	}

	if (close(FS->fnum) == -1.0)
	{
		if (DEBUG(12))
		{
			debug("%sclose_file: fail to close file : %s.\n",DEBUG_PREFIX,strerror(errno));
		}
		vreply(p, RESPONSE, 0, _Int, errno, _End);
		return;
	}

	vreply(p, RESPONSE, 0, _Int, 0, _End);

	if (FS)
	{
		if (FS->name)
		{
			free(FS->name);
		}
		free(FS);
	}
	p->file = NULL;
}

/*
 * clear the filter set upon a file
 */
void do_clear_file_filter(Client *p)
{
	FileStruct* FS = p->file;

	if (DEBUG(12))
	{
		debug("%sdo_clear_file_filter.\n",DEBUG_PREFIX);
	}

	if ((p->file != NULL) && (FS->filter != NULL))
	{
		if (FS->filter->filter)
		{
			free(FS->filter->filter);
		} 
		free(FS->filter); 
		FS->filter = NULL;
	}
}


static int alloc_file_filter_order(FileStruct* FS)
{
	if (FS->filter != NULL)
	{
		return 1;
	}

	if ((FS->filter = malloc(sizeof(FileFilter))) != NULL)
	{
		FS->filter->direction = 1;
		FS->filter->type = FILE_FILTER_TYPE_RTE;
		FS->filter->action = FILE_FILTER_ACTION_FIND;
		FS->filter->chuncksize = READ_SIZE;
		FS->filter->filter = "";
		FS->filter->prevFound = -1;
		return 1;
	}
	
	return 0;
}


/*
 * set the direction we should search for in the file
 */
void do_set_file_filter_order(Client *p, int order)
{
	FileStruct* FS = p->file;

	if (DEBUG(12))
	{
		debug("%sdo_set_file_filter_order.\n",DEBUG_PREFIX);
	}

	if ((FS != NULL) && (alloc_file_filter_order(FS)))
	{
		FS->filter->direction = order;
	}
}

/*
 * set what we should search in the file and set a few other params like block size
 */
void do_add_file_filterparam(Client *p, char *filterop)
{
	FileStruct* FS = p->file;
	char *equalsep = 0;
	int filtopsize = 0;
	char *param = NULL;
	char *value = NULL;
	size_t maxchuncksize = 0;
	size_t chuncksize = 0;

	if (DEBUG(12))
	{
		debug("%sdo_add_file_filterparam.\n",DEBUG_PREFIX);
	}

	if ((p->file != NULL) && (alloc_file_filter_order(FS)))
	{
		equalsep = strchr(filterop,'=');
		filtopsize = strlen(filterop);
		param = calloc(1,equalsep - filterop);
		memcpy(param,filterop,equalsep - filterop);
		param[equalsep - filterop] = '\0';
		value = calloc(1,filterop + filtopsize - equalsep);
		memcpy(value,equalsep + 1,filterop + filtopsize - equalsep - 1);
		value[filterop + filtopsize - equalsep - 1] = '\0';
		if (!strcmp(param,"TYPE"))
		{
			FS->filter->type = atoi(value);
			if (DEBUG(12))
			{
				debug("%sFILE FILTER TYPE : %d\n",DEBUG_PREFIX,FS->filter->type);
			}
		}
		if (!strcmp(param,"CHUNKSIZE"))
		{
			maxchuncksize = atol(getenv("RLS_FILE_FILTER_MAXCHUNKSIZE"));
			chuncksize = atol(value);
			if ((maxchuncksize > 0) && (chuncksize > maxchuncksize))
			{
				FS->filter->chuncksize = maxchuncksize;
			}
			else
			{
				FS->filter->chuncksize = chuncksize;
			}
			if (DEBUG(12))
			{
				debug("%sFILE FILTER CHUNK SIZE : %d\n",DEBUG_PREFIX,FS->filter->chuncksize);
			}
		}
		if (!strcmp(param,"ORDER"))
		{
			FS->filter->direction = atoi(value);
			if (DEBUG(12))
			{
				debug("%sFILE FILTER DIRECTION : %d\n",DEBUG_PREFIX,FS->filter->direction);
			}
		}
		if (!strcmp(param,"FIND"))
		{
			FS->filter->action = FILE_FILTER_ACTION_FIND;
			FS->filter->filter = strdup(value);
			if (DEBUG(12))
			{
				debug("%sFILE FIND : %s\n",DEBUG_PREFIX,NOTNUL(FS->filter->filter));
			}
		}
		if (!strcmp(param,"FILTER"))
		{
			FS->filter->action = FILE_FILTER_ACTION_FILTER;
			FS->filter->filter = strdup(value);
			if (DEBUG(12))
			{
				debug("%sFILE FILTER : %s\n",DEBUG_PREFIX,NOTNUL(FS->filter->filter));
			}
		}
		free(param);
		free(value);
	}
}

/*
 * compile the whole search filter
 * needed to end filter request CLEAR _Continue ADDFILTER _Continue ... COMPILE _End
 */
void do_compile_file_filter(Client *p)
{
	FileStruct* FS = p->file;
	double maxchuncksize = -1;

	if (DEBUG(12))
	{
		debug("%sdo_compile_file_filter.\n",DEBUG_PREFIX);
	}

	if ((p->file != NULL) && (alloc_file_filter_order(FS)))
	{
		maxchuncksize = atol(getenv("RLS_FILE_FILTER_MAXCHUNKSIZE"));
		if ((maxchuncksize > 0) && (FS->filter->chuncksize > maxchuncksize))
		{
			FS->filter->chuncksize = maxchuncksize;
			vreply(p,RESPONSE,0,_Double,maxchuncksize,_End);
		}
		else
		{
			vreply(p,RESPONSE,0,_Double,0.0,_End);
		}
	}
	else
	{
		vreply(p,RESPONSE,0,_Double,-1.0,_End);
	}
}

static off_t search(Client *p, off_t top, off_t bottom, const char *expression)
{
	char searched[READ_SIZE] = "*"; /* pattern "*expression*" */
	char s[READ_SIZE] = "";
	char *line;
#ifndef MACHINE_WNT
	char *saveptr;
#endif
	off_t curpos;
	FileStruct* FS = p->file;

	if (FS == NULL)
	{ 
		return NOT_FOUND;
	}

	if (FS->filter == NULL)
	{ 
		return NOT_FOUND;
	}

#ifdef MACHINE_WNT
	_lseek(FS->fnum, top, SEEK_SET);
#else
	lseek(FS->fnum, top, SEEK_SET);
#endif
	curpos = top;

	if (expression[0] == '*')
	{
		strcpy(searched, expression);
	}
	else
	{
		strcpy(searched+1, expression);
	}

	strcat(searched, "*");  /* the ...* in pattern */

	if (DEBUG(12))
	{
		debug("%ssearch begin\n",DEBUG_PREFIX);
		debug("%ssearch : top=%ld, bottom=%ld, expression=%s, searched=%s\n",DEBUG_PREFIX, top, bottom, NOTNUL(expression), NOTNUL(searched)); 
	}

	do
	{
		if ((read(FS->fnum, s, READ_SIZE)) > 0)
		{
#ifdef MACHINE_WNT
			line = strtok(s,LF);
#else
			line = strtok_r(s,LF,&saveptr);
#endif
			do
			{
				if ((FS->filter->type == FILE_FILTER_TYPE_RTE) && (tr_wildmat(line, searched)))
				{
					return(curpos);
				}
				curpos += strlen(line) + 1;
			}
#ifdef MACHINE_WNT
			while ((line = strtok(NULL,LF)) != NULL);
#else
			while ((line = strtok_r(NULL,LF,&saveptr)) != NULL);
#endif
		}

#ifdef MACHINE_WNT
		curpos = _lseek(FS->fnum, 0, SEEK_CUR);
#else
		curpos = lseek(FS->fnum, 0, SEEK_CUR);
#endif
		if (DEBUG(12))
		{
			debug("%ssearched in \"%s\" failed \n",DEBUG_PREFIX, NOTNUL(s));
		} 
	}
	while ((s) && (curpos <= bottom));

	return NOT_FOUND;
}

static off_t searchExpression(Client *p, off_t top, off_t previousPos, const char *expression)
{
	off_t startPos, found, newTop;
	FileStruct* FS = p->file;
	off_t prevFound;
	
	if (FS == NULL)
	{ 
		return NOT_FOUND;
	}

	if (FS->filter == NULL)
	{ 
		return NOT_FOUND;
	}

	prevFound = FS->filter->prevFound;

	if (DEBUG(12))
	{
		debug("%ssearchExpression : begin expression=%s top=%ld previousPos=%ld\n",DEBUG_PREFIX,NOTNUL(expression),top,previousPos);
	}

	startPos = previousPos;
	newTop = (top + startPos) / 2;

	if (DEBUG(12))
	{
		debug("%ssearchExpression : begin newtop=%ld\n",DEBUG_PREFIX,newTop);
	}

	found = search(p,newTop,startPos,expression);

	if (found != NOT_FOUND)
	{
		prevFound = found;
		if ( (found == top) && (startPos == previousPos) )
		{
			return prevFound;
		}
		else
		{
			if (DEBUG(12))
			{
				debug("%ssearchExpression : exp found at %ld search for next\n",DEBUG_PREFIX,prevFound);
			} 

			FS->filter->prevFound = prevFound;
			return searchExpression(p,found,startPos,expression); 
		}
	}
	else
	{
		if (DEBUG(12))
		{
			debug("%ssearchExpression : exp not found\n",DEBUG_PREFIX,prevFound);
		} 

		if ( prevFound != NOT_FOUND )
		{
			if (DEBUG(12))
			{
				debug("%ssearchExpression : prev search succeed at %ld return it\n",DEBUG_PREFIX,prevFound);
			} 

			return prevFound;
		}
		else
		{
			if (DEBUG(12))
			{
				debug("%ssearchExpression : no search succeed , retry higher \n",DEBUG_PREFIX,prevFound);
			}

			startPos = (startPos + top) / 2;
			if (startPos < top + strlen(expression))
			{
				return ((prevFound != NOT_FOUND) ? prevFound : NOT_FOUND);
			}
			return searchExpression(p,top,startPos,expression);
		}
	}
}

/*
 * do the search ...
 */
static off_t get_next_pos(Client *p)
{
	FileStruct* FS = p->file;
	off_t position = 0;
	size_t chuncksize = READ_SIZE;

	if ((FS == NULL) && (!alloc_file_filter_order(FS)))
	{
		return -1;
	}

#ifdef MACHINE_WNT
	if ((position = _lseek(FS->fnum,0,SEEK_CUR)) == -1)
#else	
	if ((position = lseek(FS->fnum,0,SEEK_CUR)) == -1)
#endif
	{
		return -1;
	}

	chuncksize = FS->filter->chuncksize;

	if (FS->filter->direction >= 0)
	{
		return search(p,position,position + chuncksize,FS->filter->filter);
	}
	return searchExpression(p, (position >= chuncksize) ? (position - chuncksize) : 0, position, FS->filter->filter);
}

/*
 * ... and return the position found
 */
void do_get_next_file_match(Client *p)
{
	off_t matchedPos = -1;

	if (DEBUG(12))
	{
		debug("%sdo_get_next_file_match.\n",DEBUG_PREFIX);
	}

	if ((matchedPos = get_next_pos(p)) != -1)
	{
		vreply(p,RESPONSE,0,_Index,0,_Double,(double) matchedPos,_End);
	}
	else
	{
		vreply(p,RESPONSE,0,_Index,-1,_Double,-1.0,_End);
	}
}

int move_linking_or_copying(char *src, char *dst)
{
#ifdef MACHINE_WNT
	/*
	 *  Nice here.
	 */
	if (!MoveFileEx(src, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED))
	{
		return (-1);
	}
	
	return (0); /* no error */

#else
	int err, fi, fo, n;
	char buf[8192];

	/*
	 *  If it is a directory, it is either deleted,
	 *  or we fail. If we fail from "not a directory"
	 *  trying unlink blindly is safe.
	 *  If we fail from other reasons, unlink
	 *  is not tried, it could be harmfull
	 *  in case we held root power. Or,
	 *  errno is ENOENT and unlink thus unnecessary!
	 */
	if (rmdir(dst) && errno == ENOTDIR)
	{
		unlink(dst);
	}

	/*
	 *  rename(2) is almost as nice...
	 */
	if (rename(src, dst) == 0)
	{
		return (0);
	}

	if (errno != EXDEV)
	{
		return (errno ? errno : -1);
	}

	/*
	 *  They are both files. Try to copy over filesystems.
	 */
	fi = open(src, O_RDONLY);
	if (fi < 0)
	{
		return (errno ? errno : -1);
	}

	fo = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0620);
	if (fo < 0)
	{
		err = errno ? errno : -1;
		close(fi);
		return (err);
	}
	
	/*
	 *  After this point we have to remove dst on error.
	 */
	while ((n = read(fi, buf, sizeof(buf))) > 0)
	{
		if (write(fo, buf, n) != n)
		{
			break;
		}
	}

	if (n != 0)
	{
		err = errno ? errno : -1;
		close(fi);
		close(fo);
		unlink(dst);
		return (err);
	}
	close(fi); /* return ignored, as we read 0 bytes for good eof */

	if (close(fo))
	{
		err = errno ? errno : -1;
		unlink(dst);
		return (err);
	}

	if (unlink(src))
	{
		/*
		 *  Leaves both files on disk...
		 */
		return (errno ? errno : -1);
	}

	return (0);
#endif
}

/*
 *  Create normal no-logsystem directory.
 *
 *  One level of nonexisting path is automatically created.
 */
BOOL do_really_make_directory(Client *conn, char *path, int dperms)
{
	int twice = 1;

	if (DEBUG(12))
	{
		debug("%s%d Mkdirs %s 0%o\n",DEBUG_PREFIX, conn->key, NOTNUL(path), dperms);
	}

retry:
	if (mkdir(path, dperms))
	{
		if (twice--
#ifdef MACHINE_WNT
		 && (GetLastError() == ERROR_PATH_NOT_FOUND
		  || GetLastError() == ERROR_FILE_NOT_FOUND)
#else
		 && errno == ENOENT
#endif
		)
		{
			char *cp, savec;
			char *slash = NULL;
			int rv, oerr = errno;

			for (cp = path; *cp; ++cp)
			{
				if (cp != path && cp[1] && (*cp == '/' || *cp == '\\'))
				{
					slash = cp;
				}
			}
			if (slash)
			{
				savec = *slash;
				*slash = 0;
				rv = mkdir(path, dperms);
				*slash = savec;
				if (rv == 0)
				{
					/* eek! */
					goto retry;
				}
			}
			errno = oerr;
		}
		vreply(conn, RESPONSE, 0, _Int, errno ? errno : -1, _End);
		return (1);
	}
	vreply(conn, RESPONSE, 0, _Int, 0, _End);
	return (1);
}

int do_really_open_file(Client *p,
	char *filename,
	char *mode,
	int   perms)
{
	stat_t st;
	FileStruct *FS;
	HANDLE h;
	int retryopen = 1;

	if (DEBUG(12))
	{
		debug("%s%d Opens %s 0%o %c\n",DEBUG_PREFIX, p->key, NOTNUL(filename), perms, *mode);
	}

	if (*mode == 'r' && stat(filename, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR)
	{
		/*
		 *  Directory read operation.
		 */
		return (do_open_directory(p, filename));
	}
retry:

/* #ifdef MACHINE_WNT */
#if defined(ES32) && defined(MACHINE_WNT)
	/*
	 * 4.03/KP : If mode == 'w' and file exists, truncate it.
	 */
	if (*mode == 'w')
	{
		 h = CreateFile(filename,
			GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING | TRUNCATE_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);
		if (h != INVALID_HANDLE_VALUE)
		{
			CloseHandle(h);
		}
	}
	
	h = CreateFile(filename,
		(*mode == 'r') ? GENERIC_READ : GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		(*mode == 'w') ? CREATE_ALWAYS :
		(*mode == 'a') ? OPEN_ALWAYS : OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (h == INVALID_HANDLE_VALUE)
	{
		h = (void *) -1;
		if (GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			errno = ENOENT;
		}
	}
	else
	{
		/*
		 * 4.03/KP : 	Move file pointer to EOF if we are appending.
		 */
		if (*mode == 'a')
		{
			SetFilePointer(h,0,NULL,FILE_END);
		}
	}
#else /* Not ES32 && MACHINE_WNT */
#ifdef MACHINE_WNT
	h = _open(filename,
#else
	h = open(filename,
#endif
		(*mode == 'w' ? (O_WRONLY | O_CREAT | O_TRUNC) :
			(*mode == 'a' ? (O_WRONLY | O_CREAT | O_APPEND) : (O_RDONLY)))
#ifdef MACHINE_WNT
				| O_BINARY
#endif
	       ,
#ifdef MACHINE_WNT
		_S_IREAD | _S_IWRITE);
#else
		perms);
#endif
#endif
	if (h == -1)
	{
		if (retryopen && *mode != 'r' && errno == ENOENT)
		{
			char *cp, savec;
			char *slash;
			int oerr, rv, dperms;

			retryopen = 0;
			oerr = errno;

			/* TODO: utiliser les flags S_XXX à la place des valeurs numériques en dur */
			dperms = perms | 0700;
			if (perms & 0060)
			{
				dperms |= 0010;
			}
			if (perms & 0006)
			{
				dperms |= 0001;
			}
			slash = NULL;
			for (cp = filename; *cp; ++cp)
			{
				if (cp != filename && cp[1] && (*cp == '/' || *cp == '\\'))
				{
					slash = cp;
				}
			}
			if (slash)
			{
				savec = *slash;
				*slash = 0;
				rv = mkdir(filename, dperms);
				*slash = savec;
				if (rv == 0)
				{
					/* eek! */
					goto retry;
				}
			}
			errno = oerr;
		}
		vreply(p, RESPONSE, 0, _Int, errno ? errno : -1, _Int, 0, _End);
		return (1);
	}

	if (p->local)
	{
		return (reply_with_a_descriptor(p, h));
 	}
 	else
 	{ 
#ifdef ES32
		return (transform_into_a_dataflow(p, (void *) h, mode));
#else
		/* 4.04/HT */
		FS = calloc( 1, sizeof(FileStruct) );
		FS->fnum = h;
		FS->bReadonly = (*mode == 'r');
		FS->bAppend = (*mode == 'a');
		FS->name = strdup( filename );
		p->synthetic_handler = do_synthetic_handle; 
		return (transform_into_a_dataflow(p, (void *) FS, mode));
#endif
	}
}


/* 4.04/HT */
int do_synthetic_handle( Client *p, char *buf, int cc )
{
	FileStruct *FS;
	double retval = 0;
	int	bCloseFile = 0;

	FS = p->file;
	if ( !FS )
	{
		if (DEBUG(12))
		{
			debug("%sdo_synthetic_handle: Invalid FileStruct.\n",DEBUG_PREFIX);
		}
		*(int *) 0 = 0;
		exit(3);
	}

	if (!buf)
	{      /* CLOSE FILE */
		   /*
		   ** Ugly bug in peruser
		   ** File close does not remember to flush
		   ** buffers. We have to check from Client data
		   ** if there is unflushed buffer.
		   */
		if (DEBUG(12))
		{
			debug("%s%d Closing file %s \n",DEBUG_PREFIX, p->key, NOTNUL(FS->name));
		}

		buf = p->buf.data;
		bCloseFile = 1;
		cc = p->done;

		if ( FS->bReadonly )
		{
			/* sans blague? */
			goto close_file;
		}
	}

	/*
	**  And mode was ?
	*/
	if ( !FS->bReadonly )
	{  
	   if ( FS->bAppend )
	   {
#ifdef MACHINE_WNT
	      _lseek( FS->fnum, 0, SEEK_END );
#else
	      lseek( FS->fnum, 0, SEEK_END );
#endif
			if (DEBUG(12))
			{
				debug("%s%d Seeking to end of file %s \n",DEBUG_PREFIX, p->key, NOTNUL(FS->name));
			}
	   }
#ifdef MACHINE_WNT
	   retval = (double)_write( FS->fnum, buf, cc );
	   _commit( FS->fnum );
#else
	   retval = write( FS->fnum, buf, cc );
#endif
		if (DEBUG(12))
		{
			debug("%s%d Wrote %d bytes to file %s \n",DEBUG_PREFIX, p->key, cc, NOTNUL(FS->name));
		}
	}
	else
	{
		if (DEBUG(12))
		{
			debug("%s%d Reading %d bytes from file %s \n",DEBUG_PREFIX, p->key, cc, NOTNUL(FS->name));
		}

#ifdef MACHINE_WNT
		/*
		 * 4.15/KP (14.10.99)
		 * Try ReadFile() if _read() fails, in case the "file" is a pipe...
		 */
		if ((int)(retval = (double)_read( FS->fnum, buf, cc )) < 0)
		{
			DWORD numread;

			ReadFile( (HANDLE) FS->fnum, buf, cc, &numread, NULL );
			retval = numread;
		}
#else
		retval = read( FS->fnum, buf, cc );
#endif
		if (DEBUG(12))
		{
			debug("%s%d Got %d bytes from file %s.\n",DEBUG_PREFIX, p->key, (int)retval, NOTNUL(FS->name));
		}
	}

close_file:
	if (bCloseFile)
	{
#ifdef MACHINE_WNT
	   _close( FS->fnum );
#else
	   close( FS->fnum );
#endif
		if (DEBUG(12))
		{
			debug("%s%d Closed file %s \n",DEBUG_PREFIX, p->key, NOTNUL(FS->name));
		}

	   if (FS)
	   {
	      if (FS->name)
	      {
	      	free(FS->name);
	      }
	      free( FS );
	   }
	   p->file = NULL;
	}

	return (int)retval;
}



int do_open_directory(Client *p, char *directory)
{
	char *path;

#ifdef MACHINE_WNT
	char *msk;

	/*
	 *  Do not add slash if there is already one at end.
	 *  Special case with "", Find1stFile *.
	 */

	if (directory[0] == 0 || directory[strlen(directory) - 1] == '/' || directory[strlen(directory) - 1] == '\\')
	{
		msk = "*";
	}
	else
	{
		msk = "\\*";
	}

	path = malloc(strlen(directory) + strlen(msk) + 1);
	if (!path)
	{
		fatal("%smalloc",DEBUG_PREFIX);
	}

	strcpy(path, directory);
	strcat(path, msk);
#else
	if (directory == NULL || directory[0] == 0)
	{
		directory = ".";
	}

	path = strdup(directory);
#endif

	if (p->local)
	{
		/*
		 *  Local clients expect to get a direct handle to
		 *  the data, and there is no such file available here.
		 *  So we create a pipe, send the read-end to the local client,
		 *  and create another connection which just readdirs the
		 *  filenames and sends them as textual stream,
		 *  as if the directory was a regular file.
		 *
		 *  Notice that conn changes to point to another client !
		 */
#ifdef MACHINE_WNT
		HANDLE pipr, pipw;
		HANDLE hproc, newh;

		if (!CreatePipe(&pipr, &pipw, NULL, 0))
		{
			free(path);
			return (0);
		}
		hproc = OpenProcess(PROCESS_ALL_ACCESS, 0, p->pid);

		if (hproc == INVALID_HANDLE_VALUE)
		{
			free(path);
			CloseHandle(pipr);
			CloseHandle(pipw);
			return (0);
		}
		if (!DuplicateHandle(GetCurrentProcess(), pipr, hproc, &newh,
				0, 0, DUPLICATE_SAME_ACCESS))
		{
			free(path);
			CloseHandle(pipr);
			CloseHandle(pipw);
			CloseHandle(hproc);
			return (0);
		}

		CloseHandle(hproc);
		CloseHandle(pipr);

		/*
		 *  Reply goes to original client...
		 *  ... and we get another client for I/O to the pipe.
		 */
		vreply(p, RESPONSE, 0, _Int, 0, _Int, newh, _End);
#else
		int tube[2], pipw;

		if (pipe(tube))
		{
			free(path);
			return (0);
		}
		vreply(p, RESPONSE, 0, _Int, 0, _Descriptor, tube[0], _End);

		close(tube[0]);
		pipw = tube[1];
#endif

		p = newconn(pipw);

		p->local = 1;
		p->filemask = path;
		p->handler = inetsendfile;
		p->synthetic_handler = senddirectory;
		p->buf.len = senddirectory(p, p->buf.data, p->buf.size);

		client_write_blocked(p);
		return (1);
	}
	else
	{
		/*
		 *  Nonlocal client.
		 *  Set SO_LINGER to avoid dropping last odd segment.
		 *  Check first to see if it was by default greater
		 *  than what we want, if so, leave it as it was.
		 *  Errors are ignored.
		 */
		adjust_linger(p->net);

		p->filemask = path;

		/*
		 *  The reply is sent inside there.
		 */
		return (do_synthetic_file(p, senddirectory, "r"));
	}
}

/*
 *  We have a local client and a normal descriptor/handle.
 *  The fd is passed over for direct manipulation.
 */
int reply_with_a_descriptor(Client *p, HANDLE h)
{
	int err = 0;
#ifdef MACHINE_WNT
	HANDLE newh;
	HANDLE hproc;

	hproc = OpenProcess(PROCESS_ALL_ACCESS, 0, p->pid);

	if (hproc == INVALID_HANDLE_VALUE)
	{
		CloseHandle(h);
		return (0);
	}
	if (!DuplicateHandle(GetCurrentProcess(), h, hproc, &newh,
			0, 0, DUPLICATE_SAME_ACCESS))
	{
		err = GetLastError();
		CloseHandle(h);
		CloseHandle(hproc);
		return (0);
	}
	vreply(p, RESPONSE, 0, _Int, err, _Int, newh, _End);

	CloseHandle(hproc);
	CloseHandle(h);

#else /* U*X fdpass */

	/*
	 *  fildesc passing is completed inside vreply,
	 *  when the packet writing is first tried.
	 *  So we can close the descriptor here.
	 *  If sometimes desired, fd could be kept in the client-struct
	 *  and closed near and after the sendmsg() call.
	 */
	vreply(p, RESPONSE, 0, _Int, err, _Descriptor, (int) h, _End);

	/*
	 * Closed after sendmsg() in netstuff.c:dowrite()
	 */

#endif /* U*X fdpass */
	return (1);
}

/*
 *  Nonlocal client.
 *  After one last response is sent,
 *  the client connection is used to pass the filedata.
 *  In RX mode the received connection data is written into the file as is.
 *  In TX mode the file data is pumped into the connection as is,
 *  at least when normal flowctl allows.
 *  The connection and the client is terminated at EOF.
 */
int transform_into_a_dataflow(Client *p, void *handler_arg, char *mode)
{
	p->handler = ((*mode == 'r') ? inetsendfile : inetrecvfile_enter);
	p->file = handler_arg;

	vreply(p, RESPONSE, 0, _Int, 0, _Int, 0, _End);

	/*
	 *  Takes care about data in transit at connection close.
	 */
	adjust_linger(p->net);

	return (1);
}

int do_synthetic_file(Client *p, int (*handler_fcn)(Client *parm1, char *parm2, int parm3), char *open_mode)
{
	if (DEBUG(12))
	{
		debug("%s%d Opens synthesized datafile %c\n",DEBUG_PREFIX, p->key, *open_mode);
	}

	p->synthetic_handler = handler_fcn;

	return (transform_into_a_dataflow(p, NULL, open_mode));
}

int do_stat_file(Client *p, int idx, char *ext)
{
	int ok;
	stat_t st;

	memset(&st, 0, sizeof(st));

	if (p->base->ops->stat_file)
	{
		ok = (*p->base->ops->stat_file)(p, idx, ext, &st);
	}
	else
	{
		ok = 1;
		st.st_size = 1;
		st.st_mode = 0777 | S_IFREG;
	}
	vreply(p, RESPONSE, 0,
		_Int, ok ? 0 : -1, /* err (ENOENT only meaninful) */

		_Int, (uint32) st.st_size,  /* size */
		_Int, (uint32) st.st_atime, /* atime */
		_Int, (uint32) st.st_mtime, /* mtime */
		_Int, (uint32) st.st_ctime, /* ctime */
		_Int, (uint32) st.st_mode,  /* perms */
		_Int, S_ISDIR(st.st_mode) ? 'd' :
		      S_ISREG(st.st_mode) ? '-' :
		                            '-', /* type (only files/dirs) */

		_Int, 0, /* reserved 0 */
		_Int, 0, /* 1 */
		_Int, 0, /* 2 */
		_Int, 0, /* 3 */
		_Int, 0, /* 4 */
		_Int, 0, /* 5 */
		_Int, 0, /* 6 */
		_Int, 0, /* 7 */
		_Int, 0, /* 8 */
		_Int, 0, /* reserved 9 */
		_End);

	return (1);
}

int do_copy_file(Client *p, int sidx, char *sext, int didx, char *dext)
{
	/*
	 *  Require all arguments and open base,
	 *  or drop client.
	 */
	if (!sidx || !sext || !didx || !dext || !p->base)
	{
		return (0);
	}

#if 1
	vreply(p, RESPONSE, 0, _Int, -2, _End);
#else
	return (*p->base->ops->copy_suppfile(p, sidx, sext, didx, dext));
#endif
	return (1);
}

/*
 *  Companion for remote tfGetUniq()
 *
 *  See runtime/tr_getuniq.c for some clarification (hah).
 *
 * XXX NOTE! "Locking" here is peruser-level safe (that is, since
 * peruser is a single-threaded process, it only performs one function
 * at a time (this one), so the read-set-write can be seen as an atomic
 * operation.
 * However, should there be multiple peruser processes accessing the
 * same database, this must be fixed.
 *
 * XXX2 : The counter starts always from zero (0). This is due that the
 * getaparm() replies to client the _current_ value, which initially
 * does not exist, thus converts to zero...
 */
int do_get_uniq(Client *p, char *counter_name, int limit_low, int limit_high)
{
	union rlsvalue v;
	int count;
	char *the_counter_name = strdup(counter_name);

	/*
	 *  Get current count and reply it back.
	 *  (counter_name gets destroyed here, so it was copyed).
	 */
	p->sideways_num_result = 0;
	(*p->base->ops->getavalue)(p, GET_NUM_FIELD, counter_name);

	/*
	 *  See what was sent and bump. Then set value for next,
	 *  setavalue does not send replies.
	 */
	count = p->sideways_num_result;

	if (count < limit_low || (count > limit_high && limit_high > 0))
	{
		count = limit_low;
		/*
		 * 08.12.98/KP
		 * If the counter exceeds the limits, do not increment 
		 * the value, but save limit_low value into file.
		 * This, because the client notes that as the value does
		 * not match the limits, it tries to read it again.
		 * A somewhat dirty trick, but working. (hopefully)
		 */
		v.d = count;
	}
	else
	{
		v.d = count + 1;
	}

	(*p->base->ops->setavalue)(p, SET_NUM_FIELD, the_counter_name, &v);
	free(the_counter_name);
    return 0;
}
