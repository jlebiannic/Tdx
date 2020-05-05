/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		runtime/tr_logsysrem.c
	Programmer:	Juha Nurmela (JN)
	Date:		Mon Oct 28 12:27:22 EET 1996

	Copyright (c) 1997 Telecom Finland/EDI Operations


	Replacement routines to use remote bases with TCL.


	Structure members are reused from local usage.
	handle->logsys is the rls-structure, not LogSystem.
	handle->logEntry is the (virtual) index, not LogEntry.
	handle->entryList is a list of indices.
	They are all void *, int would suffice for the indices, but...

	This handles all direct fields and entries themselves, but
	I/O and, in fact, all manipulation, of "supplemental" files
	is handled inside tr_fopen() and other . Cannot do better.

	
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_logsysrem.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 25.09.96/JN	Created
  3.01 28.10.96/JN	Change to use rls.c routines.
					NOTE: strings returned from rls_xxx disappear
					in the next call with same *rls.
  3.02 17.09.97/JN	Touch here and there for changed rls_xxx stuff.
  3.03 09.06.98/JN	Rest of fileop remote stuff, open/copy/remove.
  3.04 30.11.98/KP	tr_rlsTryGetUniq now uses files/counter/<index>
					file, containing parameters like "FTP.COUNTER",
					"EDIFACT.COUNTER" etc., instead of directly
					manipulating the files/<ext>/<index> -file.
  3.05 12.03.99/JR	tr_free():ing bug fixed in tr_rlsFindFirst()
  3.06 24.03.99/KP	Renamed all tr_ls* functions to tr_rls*
					va_list version of tr_rlsFindFirst()
					tr_rlsFetchParms()
  3.07 25.05.99/HT	new: tr_rlsCloseFile
  3.08 04.06.99/HT	tr_rlsCloseFile returns value
  3.09 10.06.99/HT	Stream can be local file, even in remote mode.
					If we close for first time some of the standard
					streams ( stdout, stderr, stdin), then the stream
					file is a local file.
  3.10 18.08.99/JR	tr_rlsCloseFile fclose() disabled.
					real closing is done by calling function.
  3.11 07.09.99/KP	In tr_rlsCopyFile() if copying rls -> local open
					local file in BINARY mode in WNT, to prevent
					extraneous CR's and/or LF's at end of each line.
  3.12 29.09.99/KP	Argh.. _O_WRONLY missing from the _open()
  3.13 09.11.99/HT	Fixed Copying local to remote file.
  3.14 04.12.99/KP	Changed tr_rlsTryGetUniq() parameters.
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#include <sys/socket.h>
#endif
#ifdef MACHINE_LINUX
#include <errno.h>
#endif

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

#include "tr_logsystem.h"

#define TR_WARN_FILE_WRITE_FAILED "Failed writing to file \"%s\" (%d)."
#define TR_WARN_FILE_READ_FAILED  "Failed reading from file \"%s\" (%d)."
#define TR_WARN_FILE_COPY_FAILED  "Failed copying \"%s\" to \"%s\" (%d)."

/*
 *  Only for filter-comparison
 */
#define OPERATOR_STRINGS
#include "logsystem/lib/logsystem.h"

#include "rlslib/rls.h"

extern double atof();
#ifndef MACHINE_LINUX
extern int errno;
#endif

/*========================================================================
========================================================================*/

static LogSysHandle *ActiveLogSysHandles = (LogSysHandle *)NULL;


/*========================================================================
	rlsCloseFile
========================================================================*/
static rls	*tr_rlsFiles[1024];
static int	brlsFilesInited = 0;

static void _set_rlsFile( FILE *fp, rls *rls )
{
	int	i;

	if ( !brlsFilesInited ) {
		brlsFilesInited = 1;
		memset( tr_rlsFiles, 0x00, sizeof( tr_rlsFiles ) ); 
	}

	if (!fp) return;
#ifdef MACHINE_WNT
	i = _fileno(fp);
#else
	i = fileno(fp);
#endif
	if ( (i < 0) || (i >= 1024) ) 
		return;

	tr_rlsFiles[i] = rls;
	return;
}

void tr_rlsResetFile( FILE *fp )
{
	_set_rlsFile( fp, NULL );
	return;
}

void _dump_rls( rls *rls )
{
	printf("location: %s \n", rls->location );
	printf("sysname : %s \n", rls->sysname );
	printf("connfd  : %d \n", rls->connfd );
	printf("bufmax  : %d \n", rls->bufmax );
	if (rls->buffer)
		printf("buffer  : %s \n", rls->buffer );
	if (rls->pu_username)
		printf("user    : %s \n", rls->pu_username );
	fflush(stdout);
}


int tr_rlsCloseFile( FILE *fp )
{
#if 0
	long    h_osfhandle = -1;
#endif
	int i;
	rls	*rls;

	if ( !brlsFilesInited ) {
		brlsFilesInited = 1;
		memset( tr_rlsFiles, 0x00, sizeof( tr_rlsFiles ) ); 
	}

	if (!fp)
		return -1;
#ifdef MACHINE_WNT
	i = _fileno(fp);
#else
	i = fileno(fp);
#endif
	if ( (i < 0) || (i >= 1024) ) 
		return 1;

	fflush( fp );

	if ( tr_rlsFiles[i] ) {
		rls = tr_rlsFiles[i];
	 	rls_close( rls );
  
		/* 2.10/JR
		 * fclose( fp );
 		*/

/* #ifdef MACHINE_WNT */
#if 0
		h_osfhandle =  _get_osfhandle(i);
		shutdown( (SOCKET)h_osfhandle, 2 );
		closesocket( (SOCKET)h_osfhandle );
#endif
	 	tr_rlsFiles[i] = NULL;
	 	return 0;
	}

	tr_rlsFiles[i] = NULL;
	/*
	 ** 	3.09/HT
	 */

	/* 2.10/JR
	 * return (fclose(fp));
	 */
	 return 1;
}

/*========================================================================
  Nobody is supposed to know the pathnames, and it would not
  even be usefull with remote bases !!!
========================================================================*/

#if 0
char *tfBuild__LogSystemPath(LogSysHandle *log, char *ext)
{
	return (tr_lsPath(log, ext));
}
#endif

/*
 *  Utility to qsort the index-list from find()
 */
struct entryListTmp {
	int idx;
	union rlsvalue val;
};

static int tr_rlsFindFirst_keytype;
static int tr_rlsFindFirst_sortdir;

static int assert_time_t_is_64[(sizeof(time_t) != 8) ? -1 : 1]; 
/*
 * ORD :
 * Cette astuce pour faire un assert servait à tester que le time_t est bien en 32 bits ... 
 * Ce qu'on ne veut surtout plus après le passage en 64 bits
 * Test modifié pour tester qu'on est bien en 64 bits (8 octets)
 * Le RLS a été modifié en conséquence
 * */

static int tr_rlsFindFirst_sorter(struct entryListTmp *a, struct entryListTmp *b)
{
	switch (tr_rlsFindFirst_keytype) {
	default:
		return (0);
	case _Int:
	case _Index:
		return (tr_rlsFindFirst_sortdir > 0 ?
			a->val.t - b->val.t :
			b->val.t - a->val.t);
	case _Time:
		return (tr_rlsFindFirst_sortdir > 0 ?
			a->val.t - b->val.t :
			b->val.t - a->val.t);
	case _String:
		return (tr_rlsFindFirst_sortdir > 0 ?
			strcmp(a->val.s, b->val.s) :
			strcmp(b->val.s, a->val.s));
	case _Double:
		if (a->val.d == b->val.d)
			return (0);
		if (a->val.d > b->val.d)
			return (+tr_rlsFindFirst_sortdir);
		else
			return (-tr_rlsFindFirst_sortdir);
	}
}

/*========================================================================
  Variable argument list:
	LogSysHandle   * log handle,
	int              sorting order,
	LogFieldHandle * sorting key,
	char           * logsysname,
	filter ops
		LogFieldHandle *fh,
		int comparison,
		int valuetype;
		{char *, double, time_t, int} value,
	NULL terminates list
========================================================================*/

int tr_rlsFindFirst(
	LogSysHandle   *handle,
	int             order,
	LogFieldHandle *key,
	char           *path,
	...)
{
	int		result;
	va_list		ap;

	va_start(ap, path);

	result = tr_rlsVFindFirst(handle, order, key, path, ap);

	va_end(ap);

	return result;
}

/*
 * A va_list version of the tr_lsFindFirst. The one above is just a wrapper
 * for this. (KP)
 */
int tr_rlsVFindFirst(
	LogSysHandle   *handle,
	int             order,
	LogFieldHandle *key,
	char           *path,
	va_list		ap)
{
	LogFieldHandle	*field;

	char *keyname;   /* field name of sort key */
	int   keytype;   /* datatype of sort key */
	char *filp;
	int  nfilters;
	char *filters[256];
	char filterbuffer[4096];
#if 0
	va_list ap;
	
	va_start(ap, path);
#endif

	/*
	 *  Release anything hanging from the handle.
	 *  Make sure the system is open.
	 *  Previous find() entrylist is cleared there.
	 */
	tr_rlsOpen(handle, path);

	nfilters = 0;
	filp = filterbuffer;

	while (field = va_arg(ap, LogFieldHandle *)) {
		int cmp = va_arg(ap, int);
		char *comparsion = operator_strings[cmp];

		/*
		 * This is silly.
		 */
		switch (va_arg(ap, int)) {
		case 0:
			sprintf(filp, "%s%s%s",
				field->name,
				comparsion,
				va_arg(ap, char *));
			break;
		case 1:
			sprintf(filp, "%s%s%lf",
				field->name,
				comparsion,
				va_arg(ap, double));
			break;
		case 2:
			sprintf(filp, "%s%szero + %ld",
				field->name,
				comparsion,
				(long) va_arg(ap, time_t));
			break;
		default:
			sprintf(filp, "%s%s%d",
				field->name,
				comparsion,
				va_arg(ap, int));
			break;
		}
		filters[nfilters++] = filp;
		while (*filp++)
			;
	}
	va_end(ap);

	filters[nfilters] = NULL;

	if (order
	 && key
	 && key->name)
		keyname = key->name;
	else
		keyname = NULL;

	keytype = rls_vquery(handle->logsys, keyname, filters);
	if (keytype == -1)
		return (0);

	/*
	 *  If no keyname, order is not important, and
	 *  each index is picked separately by repeated findnexts.
	 */
	if (keyname) {
		/*
		 *  Collect all there is, and sort to index-list.
		 *  Keyfield value is temporarily kept in array.
		 *  Strings do not live long enough, copy is made,
		 *  and freed after sort is done.
		 */
		int idx, n, mx;
		struct entryListTmp *lst;
		union rlsvalue val;

		n = 0;
		mx = 1024;
		lst = tr_malloc(mx * sizeof(*lst));

		while ((idx = rls_getnext(handle->logsys, &val)) != 0) {
			if (n >= mx) {
				mx += 1024;
				lst = tr_realloc(lst, mx * sizeof(*lst));
			}
			if (keytype == _String)
				val.s = tr_strdup(val.s);
			lst[n].idx = idx;
			lst[n].val = val;
			n++;
		}
		tr_rlsFindFirst_keytype = keytype;
		tr_rlsFindFirst_sortdir = order;

		/* 040899/JR No more warnings, added that darn casting thing... */
		qsort(lst, n, sizeof(*lst), (int (*)(const void*, const void*))(tr_rlsFindFirst_sorter));

		/* One more to terminate the list (should use count instead). */
        if ( tr_SQLiteMode() == 0)
        {
            handle->entryList = tr_malloc((n + 1) * sizeof(*handle->entryList));
            handle->entryList[n] = 0;
        }
        else
        {
            handle->indexList = tr_malloc((n + 1) * sizeof(*handle->indexList));
            handle->indexList[n] = 0;
        }
		handle->listIndex = 0;

		while (n--) {
            if ( tr_SQLiteMode() == 0)
                handle->entryList[n] = (void *) lst[n].idx;
            else
                handle->indexList[n] = lst[n].idx;
			/* 3.05/JR bug! we kept freeing same string (val.s) */
			if (keytype == _String)
				tr_free(lst[n].val.s);
		}

		tr_free(lst);
	}
	return (tr_rlsFindNext(handle));
}

/*
 *  Pick one index from either a presorted list,
 *  or ask server for next match.
 *  The list is still checked (current record is set each time).
 */
int tr_rlsFindNext(LogSysHandle *handle)
{
	int idx;

	if (( tr_SQLiteMode() == 1) && (handle->indexList) || ( tr_SQLiteMode() == 0) && (handle->entryList)) {
		/* Silently skip records that have disappeared under us. */
oops:
		if ( tr_SQLiteMode() == 0)
			idx = (int) handle->entryList[handle->listIndex];
		else
			idx = * (int *) &handle->indexList[handle->listIndex];
		if (idx) {
			handle->listIndex++;
			if (rls_point(handle->logsys, idx))
				goto oops;
		}
		else
			tr_rlsFreeEntryList(handle);
	}
	else
		idx = rls_getnext(handle->logsys, NULL);

	return (!!(handle->logEntry = (void *) idx)); /* True if ok */
}

void tr_rlsFreeEntryList(LogSysHandle *handle)
{
	if ( tr_SQLiteMode() == 0)
	{
		if (handle->entryList) {
			tr_free(handle->entryList);
			handle->entryList = NULL;
			handle->listIndex = 0;
		}
	}
	else
	{
		if (handle->indexList) {
			tr_free(handle->indexList);
			handle->indexList = NULL;
			handle->listIndex = 0;
		}
	}
}

char *tr_rlsGetTextVal(LogSysHandle *sh, LogFieldHandle *fh)
{
	if (sh
	 && sh->logsys
	 && sh->logEntry) {
		char *s = rls_gettextvalue(sh->logsys, fh->name);

		if (s) {
			return (tr_MemPool(s));
		}
	}
	return (TR_EMPTY);
}

double tr_rlsGetNumVal(LogSysHandle *sh, LogFieldHandle *fh)
{
	if (sh
	 && sh->logsys
	 && sh->logEntry) {
		/*
		 *  Other such fields that cannot change
		 *  ever for given index ???
		 */
		if (!strcmp(fh->name, "INDEX"))
			return ((double) ((unsigned) sh->logEntry));

		return (rls_getnumvalue(sh->logsys, fh->name));
	}
	return (0);
}

time_t tr_rlsGetTimeVal(LogSysHandle *sh, LogFieldHandle *fh)
{
	if (sh
	 && sh->logsys
	 && sh->logEntry) {
		return (rls_gettimevalue(sh->logsys, fh->name));
	}
	return (0);
}

int tr_rlsSetTextVal(LogSysHandle *sh, LogFieldHandle *fh, char *value)
{
	if (sh
	 && sh->logsys
	 && sh->logEntry) {
		rls_settextvalue(sh->logsys, fh->name, value);
		return (1);
	}
	return (0);
}

int tr_rlsSetNumVal(LogSysHandle *sh, LogFieldHandle *fh, double value)
{
	if (sh
	 && sh->logsys
	 && sh->logEntry) {
		rls_setnumvalue(sh->logsys, fh->name, value);
		return (1);
	}
	return (0);
}

int tr_rlsSetTimeVal(LogSysHandle *sh, LogFieldHandle *fh, char *value)
{
	time_t tims[2];

	if (sh
	 && sh->logsys
	 && sh->logEntry) {
		/*
		 *  Doing the "text to time_t" parsing here (on client end)
		 *  keeps timestamps UTC (not so if we use settext).
		 */
		tr_parsetime(value, tims);
		rls_settimevalue(sh->logsys, fh->name, tims[0]);
		return (1);
	}
	return (0);
}

/*============================================================================
  Copy one entry into another.
  There is a copy-operation rls_copy(), but is does not support
  copies between different machines. This is much slower, but
  copying should not be done very often.
============================================================================*/

int tr_rlsCopyEntry(LogSysHandle *dst, LogSysHandle *src)
{
	int i, type, status;
	char *name, *srcfile, *dstfile;
	char **files;
	char rlsname[256];

	if (!src
	 || !src->logsys
	 || !src->logEntry
	 || !dst
	 || !dst->logsys
	 || !dst->logEntry) {
		tr_Log (TR_WARN_LOGENTRY_NOT_SET);
		return 0;
	}
	for (i = 0; ; ++i) {
		if (rls_ask_nextfield(src->logsys, i,
				&name,
				&type,
				NULL,
				NULL,
				NULL))
			break;

		name = tr_strdup(name); /* src->buffer overwritten */

		switch (type) {
		case _Time:
			rls_settimevalue(dst->logsys, name,
				rls_gettimevalue(src->logsys, name));
			break;
		case _Double:
			rls_setnumvalue(dst->logsys, name,
				rls_getnumvalue(src->logsys, name));
			break;
		case _String:
			rls_settextvalue(dst->logsys, name,
				rls_gettextvalue(src->logsys, name));
			break;
		}
		tr_free(name);
	}

	/* 
	 * Extension files 
	 */
	files = rls_whatfiles(src->logsys, ((struct rls *)src->logsys)->curidx);
	for(i=0; files[i]; i++) {
		tr_Assign(&srcfile, tr_rlsPath(src, files[i]));
		tr_Assign(&dstfile, tr_rlsPath(dst, files[i]));
		if ((tr_rlsTryCopyFile(srcfile, dstfile, &status) == 0) || (status == 0)) {
			/*
			 * Copy failed -> probably a paramarray.
			 * Use tr_CopyArr instead.
			 */
			tr_CopyArr(0, src, files[i], dst, files[i], NULL, NULL);
		}
	}
	tr_AssignEmpty(&srcfile);
	tr_AssignEmpty(&dstfile);
	if (files)
		rls_free_stringarray(files);

	return 1;
}

int tr_rlsCreate(LogSysHandle *handle, char *path)
{
	int idx;

	tr_rlsOpen(handle, path);

	idx = rls_createnew(handle->logsys);
	if (idx == 0)
		tr_Fatal(TR_WARN_CANNOT_CREATE_NEW, handle->path);

	handle->logEntry = (void *) idx;
	return (1);
}

/*============================================================================
  Return the path of a specific file, extension.
  Returned string is temporarily saved in the string pool.
  Next GC will destroy it.
  XXX See comments above first function.
  Remove these and recode such TCL programs ???
  No, catch it inside tr_fopen ???
  BUT WE DO NOT HAVE THE HANDLE AVAILABLE THERE !!!
  Cheat. Return sOmEtHiNgReAlLyUglYfRoMhErE:rls_location:index:extension
  A totally separate handle is then created temporarily and released
  immediately, only the descriptor is kept open in a FILE *.
  brrr  rrrr rr rrrrrrrrhhh.
  Same whining about TCL copy() and TCL link() and such....
  Looks like best place for this would be inside namei(). sigh.
============================================================================*/

char *tr_rlsPath (LogSysHandle *handle, char *extension)
{
	if (!extension
	 || !handle
	 || !handle->logsys
	 || !handle->logEntry)
		return (TR_EMPTY);

	return (tr_BuildText("/_%d_@_%s_@_%s_@",
		(int) handle->logEntry,
		extension,
		rls_locationname(handle->logsys)));
}

#if 0

FILE *tr_rlsOpenFile(char *pseudoname, char *mode)
{
	rls *rls;
	FILE *fp = (FILE *) 1; /* not probably pseudoname */
	int   idx;
	char *ext = NULL;
	char *loc;
	char *cp = pseudoname;

	if (*cp++ != '/'
	 || *cp++ != '_')
		goto out;

	idx = strtol(cp, &cp, 10);
	if (idx == 0)
		goto out;

	if (*cp++ != '_'
	 || *cp++ != '@'
	 || *cp++ != '_')
		goto out;

	ext = tr_strdup(cp);

	if ((cp = strstr(ext, "_@_")) == NULL)
		goto out;

	*cp = 0;
	cp += 3;

	loc = cp;

	while (*cp)
		cp++;
	if (cp[-1] != '@'
	 || cp[-2] != '_')
		goto out;

	cp[-2] = 0;
	fp = NULL;   /* It really was pseudoname */

	/*
	 *  Make another quick connection,
	 *  to not upset existing connections.
	 */
	rls = rls_open(loc);
	if (rls) {
		fp = rls_fopen(rls, idx, ext, mode);
		rls_close(rls);
	}
out:
	if (ext)
		tr_free(ext);

	return (fp);
}
#endif

/*============================================================================
  Logsystem in handle->path is opened, and handle updated.
  If handle was already open, nothing is done.
  Errors are fatal, TCL TRUE returned.
  NULL path might be a way to "close" a base (a variable, in fact),
  but it cannot be given from TCL. It could be "close(LOG)" instead...
============================================================================*/

int tr_rlsOpen(LogSysHandle *handle, char *path)
{
	LogSysHandle **hpp;

	if (handle)
		tr_rlsFreeEntryList(handle);

	if (path == NULL)
		tr_Fatal(TR_FATAL_NO_LOGSYS);

	if (handle->path == NULL
	 || strcmp(handle->path, path) != 0) {

		char *pathcopy = tr_strdup(path);

		if (handle->logsys)
			rls_close(handle->logsys);
		if (handle->path)
			tr_free(handle->path);
		handle->path   = pathcopy;
		handle->logsys = rls_open(pathcopy);

		if (handle->logsys == NULL)
			tr_Fatal(TR_FATAL_LOGSYS_OPEN_FAILED, handle->path);

		/*
		 *  Keep a list of used loghandles.
		 *  After the first successful opening of a base
		 *  the base handle is appended to the list.
		 */
		hpp = &ActiveLogSysHandles;
		while (*hpp != handle) {
			if (*hpp == NULL) {
				if (handle->next_logsyshandle)
					*(int *) 0 = 0;
				*hpp = handle;
				break;
			}
			hpp = &(*hpp)->next_logsyshandle;
		}
	}
	return (1);
}

/*============================================================================
  Remove the entry and everything related to it.
  Possible entrylist is NOT released, so we can remove entries
  within a TCL filter-loop.
============================================================================*/

int tr_rlsRemove(LogSysHandle *handle)
{
	if (handle
	 && handle->logsys
	 && handle->logEntry) {
		int idx = (int) handle->logEntry;
		handle->logEntry = 0;

		if (rls_remove(handle->logsys, idx))
			return (0);
		return (1);
	}
	tr_Log (TR_WARN_LOGENTRY_NOT_SET);
	return (0);
}

/*========================================================================
  Read a specific entry.
  Current active and entrylist are released.
  XXX entrylist is NOT released, so we can use one TCL variable
  to walk thru a filter, and do other things inside the loop.
  Recursive loops do not work, however.
========================================================================*/

int tr_rlsReadEntry(LogSysHandle *handle, double d_idx)
{
	int idx = (int) (d_idx + 0.5);

	if (handle
	 && handle->logsys
	 && idx) {
		handle->logEntry = 0;
		if (rls_point(handle->logsys, idx))
			return (0);
		handle->logEntry = (void *) idx;
		return (1);
	}
	tr_Log (TR_WARN_LOGENTRY_NOT_SET);
	return (0);
}

/*============================================================================
  Flush out an entry, here this means to ask remote server
  to really write it to disk. Otherwise the writing
  is done by the server every now and then.
============================================================================*/

int tr_rlsWriteEntry(LogSysHandle *handle)
{
	if (handle
	 && handle->logsys
	 && handle->logEntry) {
		if (rls_synch(handle->logsys))
			return (0);
		return (1);
	}
	tr_Log (TR_WARN_LOGENTRY_NOT_SET);
	return (0);
}

/*
 *  Return TCL TRUE iff all looks right,
 *  check the connection too.
 */
int tr_rlsIsValidEntry(LogSysHandle *handle)
{
	if (handle
	 && handle->logsys
	 && handle->logEntry
	 && rls_noop(handle->logsys) == 0) {
		return (1);
	}
	return (0);
}

/*
 *  Called from rls_xxx for errors.
 *  Still needs filtering for repeated errors.
 */
void rls_message(rls *rls, int locus, char *fmt, ...)
{
	va_list ap;

	tr_mbFputs("rls error:\n", stderr);
	
	va_start(ap, fmt);
	tr_mbVfprintf(stderr, fmt, ap);
	va_end(ap);

	tr_mbFputs("\n\n", stderr);
}

/*
 *  These functions are called before all local file-operations.
 *
 *  If they return false, remote bases are not involved in the operation,
 *  and the caller should continue locally.
 *
 *  If they return true, *xxx_result is the return value of the operation.
 *
 *  Example:
 *    if (tr_rlsTryRemoveFile(path, &ok)) return (ok);
 *
 *  If appropriate, on errors, tr_Log() is done here.
 *
 *  Existing connections are not touched, a new connection is always
 *  created.
 */

struct rls_file_location {
	char *location;     /* e.g. foo@bar:baz */
	int   idx;
	char *ext;
	char *prettyname;   /* e.g. baz:42.rp */
	unsigned
		is_rls:1,   /* remote base involved */
		:0;
};

static void release_loc(struct rls_file_location *loc)
{
	if (loc->ext) {
		tr_free(loc->ext);
		loc->ext = NULL;
	}
	if (loc->prettyname) {
		tr_free(loc->prettyname);
		loc->prettyname = NULL;
	}
}

static int write_fd(int ofd, char *buf, int amt)
{
	int nw, did = 0;

	while (did < amt) {
		nw = write(ofd, buf + did, amt - did);
		if (nw == -1)
			return (-1);
		if (nw == 0)
			break;
		did += nw;
	}
	return (did);
}

static int writedata(int islocal, int ofd, char *buf, int amt)
{
	int nw, did = 0;

	while (did < amt) {
		if (islocal) {
#ifdef MACHINE_WNT
			if (!WriteFile((HANDLE) ofd, buf + did, amt - did, &nw, NULL))
				nw = -1;
#else
			nw = write(ofd, buf + did, amt - did);
#endif
		} else
			nw = send(ofd, buf + did, amt - did, 0);
		if (nw == -1)
			return (-1);
		if (nw == 0)
			break;
		did += nw;
	}
	return (did);
}

static int readdata(int islocal, int ifd, char *buf, int amt)
{
	int nr, did = 0;

	while (did < amt) {
		if (islocal) {
#ifdef MACHINE_WNT
			if (!ReadFile((HANDLE) ifd, buf + did, amt - did, &nr, NULL))
				nr = -1;
#else
			nr = read(ifd, buf + did, amt - did);
#endif
		} else
			nr = recv(ifd, buf + did, amt - did, 0);
		if (nr == -1) {
#ifdef MACHINE_WNT
			int err;
			FILE *fp;
			err = WSAGetLastError();
			if (fp = fopen("C:\\temp\\runtime.err", "a")) {
				fprintf(fp,
				"readdata(0x%X, 0x%X, %d) = %d\n",
				 ifd, buf+did, amt+did, err,
				 (err <= 0) ? " HEP" : "");
				 fclose(fp);
			}
#endif
			return (-1);
		}
		if (nr == 0)
			break;
		did += nr;
	}
	return (did);
}

static int decode_loc(char *pseudoname, struct rls_file_location *loc)
{
	char *cp = pseudoname;

	loc->location = NULL;
	loc->idx = 0;
	loc->ext = NULL;
	loc->prettyname = NULL;
	loc->is_rls = 0;

	if (*cp++ != '/'
	 || *cp++ != '_')
		goto not_found;

	loc->idx = strtol(cp, &cp, 10);
	if (loc->idx == 0)
		goto not_found;

	if (*cp++ != '_'
	 || *cp++ != '@'
	 || *cp++ != '_')
		goto not_found;

	loc->ext = tr_strdup(cp);

	if ((cp = strstr(loc->ext, "_@_")) == NULL)
		goto not_found;

	*cp = 0;
	cp += 3;

	loc->location = cp;

	while (*cp)
		cp++;

	if (cp[-1] != '@'
	 || cp[-2] != '_')
		goto not_found;

	cp[-2] = 0;

	loc->prettyname = tr_malloc(strlen(pseudoname));
	sprintf(loc->prettyname, "%s:%d.%s",
		loc->location, loc->idx, loc->ext);

	loc->is_rls = 1;
	return (1);
not_found:
	release_loc(loc);
	return (0);
}

void tr_rlsReleaseRlsPath( PRLSPATH rlspath )
{
	if (!rlspath) return;
	if (rlspath->ext) tr_free(rlspath->ext);
	if (rlspath->location) tr_free(rlspath->location);
	memset( rlspath, 0x00, sizeof(RLSPATH) );
}

int tr_rlsDecodeLoc(char *pseudoname, PRLSPATH rlspath)
{
	char *cp = pseudoname;
	char *p;
	
	if (!rlspath)
		return 0;

	tr_rlsReleaseRlsPath( rlspath );

	if (*cp++ != '/'
	 || *cp++ != '_')
		return 0;

	rlspath->idx = strtol(cp, &cp, 10);
	if (rlspath->idx == 0)
		return 0;

	if (*cp++ != '_'
	 || *cp++ != '@'
	 || *cp++ != '_')
		return 0;

	p = cp;

	if ((cp = strstr(cp, "_@_")) == NULL)
		return 0;

	*cp = 0;
	rlspath->ext = tr_strdup(p);
	*cp = '_';
	cp += 3;

	p = cp;

	while (*cp)
		cp++;

	if (cp[-1] != '@'
	 || cp[-2] != '_')
		return 0;

	cp[-2] = 0;
	rlspath->location = tr_strdup(p);
	cp[-2] = '_';

	rlspath->is_rls = 1;

	return (1);
}

/*
 *  Is it, or is it not, encoded location of remote file.
 */
int tr_rlsRemoteName(char *pseudoname)
{
	struct rls_file_location loc;

	if (!decode_loc(pseudoname, &loc))
		return (0);

	release_loc(&loc);
	return (1);
}

/*
 *  Return 0 if fs-file, 1 if remote & exists and -1 if not.
 *  If result is NULL, this is only 'access()' call,
 *  and no error is logged.
 */
int tr_rlsTryStatFile(char *pseudoname, struct stat *st_result)
{
	struct rls_file_location loc;
	struct rls_file_attributes attr;
	rls *rls;
	int err = -1;

	if (!decode_loc(pseudoname, &loc))
		return (0);

	/*
	 *  Not all fields get sensible values, zero them.
	 */
	if (st_result)
		memset(st_result, 0, sizeof(*st_result));

	if (rls = rls_open(loc.location)) {
		memset(&attr, 0, sizeof(attr));
		err = rls_stat_file(rls, loc.idx, loc.ext, &attr);
		if (err) {
			err = -1;
			if (st_result)
				tr_Log(TR_WARN_STAT_FAILED,
					loc.prettyname, err);
		} else {
			err = 1;
			if (st_result) {
				st_result->st_size  = attr.size;
				st_result->st_atime = attr.atime;
				st_result->st_mtime = attr.mtime;
				st_result->st_ctime = attr.ctime;
				st_result->st_mode  = attr.perms;
				switch (attr.type) {
				case '-':
					st_result->st_mode |= S_IFREG;
					break;
				case 'd':
					st_result->st_mode |= S_IFDIR;
					break;
				default:
					st_result->st_mode |= S_IFCHR;
					break;
				}
			}
		}
		rls_close(rls);
	}
	release_loc(&loc);
	return (err);
}

int tr_rlsTryOpenFile(char *pseudoname, char *mode, FILE **fp_result)
{
	struct rls_file_location loc;
	rls	*nrls;
	rls *rls;
	int	fd;

	if (!decode_loc(pseudoname, &loc))
		return (0);

	*fp_result = NULL;

	fd = open( 		/* 15.09.98/HT Reserve first free
						file descriptor */
#ifdef MACHINE_WNT
		"NUL",
#else
		"/dev/null",
#endif
		O_RDONLY | O_CREAT );

	rls = rls_open(loc.location);
	if (rls) {
		if (fd != -1) close(fd); /* 15.09.98/HT */
		*fp_result = rls_fopen_withrls(rls, loc.idx, loc.ext, mode, &nrls, STREAMED);
		_set_rlsFile( *fp_result, nrls ); /* 25.05.99/HT */
		rls_close(rls);
	}
	else
		if (fd != -1) close(fd); /* 15.09.98/HT */

#if 0 /* caller (tr_fopen() probably) is supposed to log errors */
	if (*fp_result == NULL)
		tr_Log("", loc.prettyname)
#endif
	release_loc(&loc);
	return (1);
}

int tr_rlsTryRemoveFile(char *uglypath, int *ok_result)
{
	struct rls_file_location loc;
	rls *rls;

	if (!decode_loc(uglypath, &loc))
		return (0);

	*ok_result = 0; /* assume failure */

	if (rls = rls_open(loc.location)) {
		if (rls_removefile(rls, loc.idx, loc.ext) == 0)
			*ok_result = 1;
		rls_close(rls);
	}
	if (*ok_result == 0)
		tr_Log (TR_WARN_UNLINK_FAILED, loc.prettyname, errno);

	release_loc(&loc);
	return (1);
}

int tr_rlsTryCopyFile(char *source, char *destination, int *ok_result)
{
	struct rls_file_location src, dst;
	struct rls *rls1 = NULL, *rls2 = NULL;
	int nr = 0, nw = 0, ifd = -1, ofd = -1;
	int err = 0;

	char buf[8192]; /* used for massive data copy. */

	decode_loc(source, &src);
	decode_loc(destination, &dst);

	/*
	 *  Neither is remote base, caller can continue.
	 */
	if (src.is_rls == 0 && dst.is_rls == 0)
		return (0);

	*ok_result = 0;

	if (src.is_rls)
		source = src.prettyname;
	if (dst.is_rls)
		destination = dst.prettyname;

	if (src.is_rls && dst.is_rls) {
		if (strcmp(src.location, dst.location) == 0) {
			/*
			 *  (A) Copy inside one base, no i/o required.
			 */
			rls1 = rls_open(src.location);
			if (rls1) {
				err = rls_copy_file(rls1,
						src.idx, src.ext,
						dst.idx, dst.ext);
				if (err == -2) {
					/* server (or base) did not support */
					rls2 = rls1;
					goto hard_way;
				} else if (err) {
					/*
					 *  ERR - Cannot copy src to dst.
					 */
					tr_Log(TR_WARN_FILE_COPY_FAILED,
						source, destination, err);
				} else {
					*ok_result = 1;
				}
			} else {
				/*
				 *  ERR - Cannot connect to src (==dst).
				 */
				tr_Log(TR_FATAL_LOGSYS_OPEN_FAILED,
					src.location);
			}
		} else {
			/*
			 *  (B) Both are remote but DIFFERENT,
			 *      COPY is impossible, and
			 *      two connections are needed.
			 */
			if ((rls1 = rls_open(src.location)) != NULL
			 && (rls2 = rls_open(dst.location)) != NULL) {
	hard_way:
				ifd = rls_openf(rls1, src.idx, src.ext, "r", STREAMED);
				if (ifd >= 0) {
					rls		*nrls;
					ofd = rls_openf_withrls(rls2, dst.idx, dst.ext, "w", &nrls, STREAMED);
					if (ofd >= 0) {
						while ((nr = readdata(rls1->localconn, ifd, buf, sizeof(buf))) > 0)
							if ((nw = writedata(rls2->localconn, ofd, buf, nr)) != nr)
								break;
						if (nr == 0)
							*ok_result = 1;
						rls_closef(rls2, ofd);
						/* 3.13/HT */
						rls_close( nrls );
					} else {
						/*
						 *  ERR - Cannot open dst.prettyname.
						 */
						tr_Log(TR_WARN_FILE_W_OPEN_FAILED,
							destination, err);
					}
					rls_closef(rls1, ifd);
				} else {
					/*
					 *  ERR - Cannot open src.prettyname.
					 */
					tr_Log(TR_WARN_FILE_R_OPEN_FAILED,
						source, err);
				}
			} else {
				if (rls1 == NULL) {
					/*
					 *  ERR - Cannot connect to src.
					 */
					tr_Log(TR_FATAL_LOGSYS_OPEN_FAILED,
						src.location);
				} else {
					/*
					 *  ERR - Cannot connect to dst.
					 */
					tr_Log(TR_FATAL_LOGSYS_OPEN_FAILED,
						dst.location);
				}
			}
		}
	} else {
		/*
		 *  (C) Another one is a plain file,
		 *      only one connection is needed, but
		 *      explicit read/write.
		 *      Mode of local destination file
		 *      is the same as from fopen(xxx, "w").
		 */
		rls1 = rls_open(src.is_rls ? src.location : dst.location);
		if (rls1) {
			if (src.is_rls) {
				ifd = rls_openf(rls1, src.idx, src.ext, "r",STREAMED);
				if (ifd >= 0) {
					do {
#ifdef MACHINE_WNT
						/*
						 * 3.11/KP
						 * Replaced creat() with open() to allow opening
						 * the file in BINARY mode.
						 *
						 * ofd = creat(destination, 0666);
						 *
						 * 3.12/KP : _O_WRONLY was missing...
						 */
						ofd = _open(destination, _O_CREAT|_O_TRUNC|_O_BINARY|_O_WRONLY, 0666);
#else
						ofd = creat(destination, 0666);
#endif
					} while (ofd == -1 && tr_DirFixup(destination));
					if (ofd >= 0) {
						while ((nr = readdata(rls1->localconn, ifd, buf, sizeof(buf))) > 0)
							if ((nw = write_fd(ofd, buf, nr)) != nr)
								break;
						if (nr == 0)
							*ok_result = 1;
						close(ofd);
					} else {
						/*
						 *  ERR - Cannot create destination file
						 */
						tr_Log(TR_WARN_FILE_W_OPEN_FAILED,
							destination, errno);
					}
					rls_closef(rls1, ifd);
				} else {
					/*
					 *  ERR - Cannot open src.prettyname.
					 */
					tr_Log(TR_WARN_FILE_R_OPEN_FAILED,
						source, err);
				}
			} else {
#ifdef MACHINE_WNT
				ifd = _open(source, O_RDONLY|_O_BINARY);
#else
				ifd = open(source, O_RDONLY);
#endif
				if (ifd >= 0) {
				/*
				**	3.13/HT Trying too take care of rls-connection
				**          created by rls_openf. Using rls_openf_withrls
				**          to control rls-connection created by cloneconn.
				**	ofd = rls_openf(rls1, dst.idx, dst.ext, "w", STREAMED);
				*/
					rls		*nrls;
					ofd = rls_openf_withrls(rls1, dst.idx, dst.ext, "w", &nrls,STREAMED);
					if (ofd >= 0) {
						while ((nr = read(ifd, buf, sizeof(buf))) > 0)
							if ((nw = writedata(rls1->localconn, ofd, buf, nr)) != nr)
								break;
						if (nr == 0)
							*ok_result = 1;
						rls_closef(rls1, ofd);
						/* 3.13/HT */
						rls_close( nrls );
					} else {
						/*
						 *  ERR - Cannot create dst.prettyname
						 */
						tr_Log(TR_WARN_FILE_W_OPEN_FAILED,
							destination, err);
					}
					close(ifd);
				} else {
					/*
					 *  ERR - Cannot open source file
					 */
					tr_Log(TR_WARN_FILE_R_OPEN_FAILED,
						source, errno);
				}
			}
		} else {
			/*
			 *  ERR - Cannot connect to src.is_rls ? src : dst.
			 */
			tr_Log(TR_FATAL_LOGSYS_OPEN_FAILED,
				src.is_rls ? src.location : dst.location);
		}
	}
	/*
	 *  Check read/write errors here.
	 */
	if (nr < 0) {
		/*
		 *  ERR - Reading source
		 */
		tr_Log(TR_WARN_FILE_READ_FAILED,
			source, errno);
	} else if (nr > 0) {
		/*
		 *  ERR - Cannot write destination
		 */
		tr_Log(TR_WARN_FILE_WRITE_FAILED,
			destination, errno);
	}

	if (rls1)
		rls_close(rls1);
	if (rls2 && rls2 != rls1)
		rls_close(rls2);

	release_loc(&src);
	release_loc(&dst);

	return (1); /* we handled it, success or not */
}

char *tr_PrettyPrintFileName(char *filename)
{
	static struct rls_file_location loc;

	release_loc(&loc); /* clear previous data */

	if (!decode_loc(filename, &loc))
		return (filename);

	return (loc.prettyname); /* not released until next pp() */
}

int tr_rlsTryGetUniq(LogSysHandle *pLog, char *tFile, char *tCounter, int MinCounter, int MaxCounter, int *Counter_result)
{
	struct rls_file_location loc;
	rls *rls;
	char parm[1024], file[1024];
	int _localmemptr;
	int result;

	_localmemptr = tr_GetFunctionMemPtr();

	if (pLog)
		strcpy(file, tr_rlsPath(pLog, tFile));
	else
		strcpy(file, tFile);

	/*
	 *  Not ours ?
	 */
	if (!decode_loc(file, &loc)) {
		result = 0;
		goto end_tr_rlsTryGetUniq;
	}

	/*
	 *  Ours.
	 */
	*Counter_result = 0; /* might use random value ?? */

	if (rls = rls_open(loc.location)) {
		if (rls_point(rls, loc.idx) == 0) {
			/*
			 * 3.04/KP : Changed (see comments at top)
			 *
			 * sprintf(parm, "%s.%s", loc.ext, tCounter);
			 */
			sprintf(parm, "COUNTERS.%s.%s", loc.ext, tCounter);
			*Counter_result = rls_get_uniq(rls,
				parm, MinCounter, MaxCounter);
		}
		rls_close(rls);
	}
	release_loc(&loc);
	result = 1;

end_tr_rlsTryGetUniq:
	tr_CollectFunctionGarbage(_localmemptr);  /* Remove unwanted mempool allocation */
	return result;
}

char **tr_rlsFetchParms(LogSysHandle *ls, char *ext)
{
	return (rls_fetch_parms(ls->logsys, (int)ls->logEntry, ext));
}

int tr_rlsSetTextParm(LogSysHandle *pLog,
                char *tExt,
                char *tName,
                char *tValue)
{
    char    *buf;

	buf = tr_malloc( strlen(tName)+strlen(tExt)+2 );
	sprintf(buf,"%s.%s",tExt,tName);
	rls_settextvalue(pLog->logsys, buf, tValue ? tValue : "");
	tr_free(buf);

	return (1);
}

char *tr_rlsGetTextParm( LogSysHandle *pLog,
               char     *tExt,
               char     *tName )
{
    char    *buf, *retval, *p;

    buf =  tr_malloc( strlen(tName)+strlen(tExt)+2 );
    sprintf( buf, "%s.%s", tExt, tName );
    retval = rls_gettextvalue(pLog->logsys, buf);
    tr_free(buf);
    p = tr_MemPool(retval);

    return p;
}

void tr_rlsClearParms(LogSysHandle *pLog, char *tExt)
{
	rls_clear_parms(pLog->logsys, (int)pLog->logEntry, tExt);
}

int tr_rlsSaveParms(LogSysHandle *pLog, char *tExt, char **ta)
{
	return (rls_save_parms(pLog->logsys, (int)pLog->logEntry, tExt, ta));
}
