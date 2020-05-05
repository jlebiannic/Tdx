/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_readdir.c
	Programmer:	Mikko Valimaki
	Date:		Sun Nov  1 14:27:56 EET 1992

	Copyright (c) 1996-2011 GenerixGroup
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_readdir.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  3.01 16.05.94/JN	tr_FileAccess() takes into account supplementary-groups.
  3.02 17.06.94/JN	fix... fFILE.READ = TOSI didn't work out (4 != 1)
  3.03 20.02.95/JN	tr_CloseDir(DIR *) added, to complement tr_OpenDir()
  3.04 17.01.96/JN	FULLNAME.
  3.05 06.02.96/JN	DIR * changed to void *, /\/\/\/\/\/ too.
			statuschange was using mtime, now st_ctime
  3.06 27.11.96/JN	NT compatlib remove.
  3.07 11.06.98/JN	stat() of files in remote bases.
  4.00 02.11.98/KP      In NT lastsep was expanded incorrectly if path
                        contained bots /'s and \'s.
			tr_ReadDir used tr_wildmat() which treated \'s
			as escape characters... use tr_PathCompare() instead.
  4.01 03.02.99/KP      while fX in tDir failed if the tDir contained an
                        invalid or non-existing directory.
  Bug 1219 08.04.11/JFC ensure parameters given to tr_buildtext are not in the pool
						if they need to be used after the call.
  Jira TX-2580 23.12.14/YLI(CG) replace the function access by stat in the tr_FileAccess()
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
============================================================================*/
#ifdef MACHINE_HPUX
#define _LARGEFILE64_SOURCE
#else
#define __USE_LARGEFILE64
#endif
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef MACHINE_WNT
#define tr_lfs_stat _stati64
#define tr_mode_t unsigned short
#else
#define tr_lfs_stat stat64
#define tr_mode_t mode_t 
#endif
#ifdef MACHINE_WNT
#include <io.h>
#include <direct.h>
#else
#include <sys/errno.h>
#include <unistd.h>
#endif
#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

#ifdef MACHINE_WNT

#define F_OK 0

/*
 *  For NT \\ is preferred, but / also accepted.
 */

#define PSEPC '\\'
static int ISPSEPC(char c)
{
	return (c == PSEPC || c == '/');
}

/*
 *  Pointer to last \\ or last /
 *  If none such, then
 *  check for C: and return pointer to the :
 *
 * 4.00/KP Check correctly paths with both / and \ too.
 */
static char *LASTSEP(char *s)
{
	char *p, *p1, *p2;

	/*
	if (p = strrchr(s, PSEPC))
		return (p);
	if (p = strrchr(s, '/'))
		return (p);
	*/

        p1 = strrchr(s, PSEPC);
        p2 = strrchr(s, '/');

        if (p1 || p2)
                return (p1 > p2 ? p1 : p2);

	if (isalpha(s[0]) && s[1] == ':')
		return (s + 1);

	return (NULL);
}
#else

/*
 *  These are called with side-effecting parms !
 */
#define PSEPC '/'
#define ISPSEPC(c) ((c) == PSEPC)
#define LASTSEP(s) strrchr(s, PSEPC)
#endif

#ifndef MACHINE_LINUX
extern int errno;
#endif

#define TR_ACCESS_READ		4
#define TR_ACCESS_WRITE		2
#define TR_ACCESS_EXEC		1

/*======================================================================
  while fF in "/tmp/x*.tmp" do
	...
  endwhile
  This should be expanded to allow wildcards anywhere !
======================================================================*/
void *tr_OpenDir (char *original, char **path, char **search)
{
	DIR  *dirp;
	char *p;
	char *dirpath;

	if (!original)
		return ((void *) 0);

	/*
	**  Extract the last component to be used as the
	**  searching key.
	*/
	if ((p = LASTSEP(original)) == NULL) {
		/*
		**  No path specified, thus we use the
		**  current directory.
		*/
		*search = original;
		*path = tr_BuildText ("");
		dirpath = ".";
	} else {
		/*
		**  There is a path.
		**  and p points to the last separator.
		**  This used to bomb when we were given pure strings,
		**  lets not to poke into original, but use .* format
		**  to get start of original upto and including the sep.
		**  This has also the advantage of using the sep from user.
		**
		**  This also works for patters like "C:*.C" on NT, where
		**  original = "C:*.C"
		**  p        = ":*.C"
		**  search   = "*.C"
		**  path     = "C:"
		*/
		*search = tr_MemPool (p + 1);
		*path = tr_BuildText ("%.*s", p - original + 1, original);
		dirpath = *path;
	}
	if (!**search)
		return ((void *) 0);

#ifndef MACHINE_WNT
	if ((dirp = opendir (dirpath)) == NULL) {
		if (errno == ENOMEM) {
			tr_OutOfMemory();  /* log error and exit */
		}
		tr_Log (TR_WARN_DIR_OPEN_FAILED, dirpath, errno);
	}
#else
	dirp = tr_malloc(sizeof(*dirp));
	if (dirp == NULL) {
		tr_Log (TR_WARN_DIR_OPEN_FAILED, dirpath, GetLastError());
	} else {
		char *p = tr_malloc(strlen(dirpath) + sizeof("\\*"));
		if (!p) {
			tr_OutOfMemory();  /* exit on error */
		}
		strcpy(p, dirpath);
		strcat(p, "\\*");
		dirp->handle = FindFirstFile(p, &dirp->data);
		tr_free(p);
	}
#endif
	return (dirp);
}

void tr_CloseDir(void *_dirp)
{
	DIR *dirp = (DIR *)_dirp;
#ifdef MACHINE_WNT
	if (dirp) {
		if (dirp->handle != INVALID_HANDLE_VALUE)
			FindClose(dirp->handle);
		tr_free(dirp);
	}
#else
	if (dirp)
		closedir(dirp);
#endif
}

/*======================================================================
  Read entries from directory until one matches the search-pattern,
  or directory exhausts.
  Given path is prepended to selected ones.
  The path is either "foo/" or "" from OpenDir.

  This is {a,the only} place where fX variables receive
  non-absolute paths.
  (Could be changed by abspath(original) when opening the directory)

======================================================================*/
char *tr_ReadDir (DIR *dirp, char *path, char *search)
{
#ifdef MACHINE_WNT
	char *p = NULL;

	while (dirp && (dirp->handle != INVALID_HANDLE_VALUE)) {
		/*
		 * 4.00/KP
		 * Use tr_comparepaths() in WinNT to avoid problems
		 * with \'s
		 *
		 * if (tr_wildmat(dirp->data.cFileName, search)) {
		 */
		if (tr_comparepaths(dirp->data.cFileName, search)) {
			p = tr_BuildText("%s%s", path, dirp->data.cFileName);
		}
		if (!FindNextFile(dirp->handle, &dirp->data)) {
			FindClose(dirp->handle);
			dirp->handle = INVALID_HANDLE_VALUE;
		}
		if (p)
			return (p);
	}
#else
	struct dirent *dp;

	while (dirp && (dp = readdir (dirp)) != NULL) {
		if (tr_wildmat (dp->d_name, search)) {
			return tr_BuildText ("%s%s", path, dp->d_name);
		}
	}
#endif
	return NULL;
}

/*
 *  Assign next suitable filename into *found,
 *  or return zero.
 */
int tr_ReadDir2 (void *_dirp, char *path, char *search, char **found)
{
	char *p;
	char *lpath;
	int result;
	DIR *dirp = (DIR *)_dirp;

	/* in case path is allocated in mem pool we don't want it to be freed.
		This is the case in "RTE while statement", as local variable are
		allocated in the pool and must stay available until the endwhile.
	*/
	lpath = tr_strdup(path); 

	if ((p = tr_ReadDir (dirp, lpath, search)) != NULL) {
		tr_Assign(found, p);
		tr_MemPoolFree(p);
		result = 1;
	}
	else {
		tr_AssignEmpty(found);
		result = 0;
	}
	tr_free(lpath);
	return (result);
}

/*======================================================================
  Called for fX.NAME, like /bin/basename
======================================================================*/
char *tr_FileName (char *filename)
{
	char *p;

	if (!filename || !*filename)
		return TR_EMPTY;
	if (p = LASTSEP(filename))
		return tr_MemPool (p + 1);
	return tr_MemPool (filename);
}

/*======================================================================
  Add path and canonize ../ and ./ constructs.
  Called for fX := textvar, the absolute string is assigned to char *fX
======================================================================*/
char *tr_FileFullName (char *filename)
{
	char buf[1028];

	if (!filename || !*filename)
		return TR_EMPTY;
	if (tr_rlsRemoteName(filename))
		return tr_MemPool(filename);
	if (tr_abspath(filename, buf, sizeof(buf)))
		return TR_EMPTY;
	return tr_MemPool (buf);
}

/*======================================================================
  Called for fX.PATH
  Last separator is cut away from path.
  Root directory is handled specially, "/tmp" -> "/" and "/" -> "/"
======================================================================*/
char *tr_FilePath (char *filename)
{
	char psav, *p;
	char *tmp;

	if (!filename || !*filename)
		return TR_EMPTY;

	if (p = LASTSEP(filename)) {
		/*
		 *  Here is another potential core from poking to
		 *  pure strings, but currently we never get
		 *  such here.
		 *  This can be only called after something is
		 *  assigned into fSomeThing and a copy is made there...
		 *
		 *  On Micro$oft world, C:foo gives C: as path
		 *  Also, /tmp gives /, not "" as it used to do.
		 *  Yet another special case for C:\ -> C:\ as well.
		 *
		 *  1:   /tmp   -> /
		 */
		if (p == filename) /* root */
			p++;
#ifdef MACHINE_WNT
		/*
		 *  2:   C:tmp  -> C:
		 *  3:   C:/tmp -> C:/
		 */
		else
		if (*p == ':'
		 || (p == filename + 2 && p[-1] == ':' && isalpha(p[-2])))
			p++;
#endif
		psav = *p;
		*p = '\0';
		tmp = tr_MemPool (filename);
		*p = psav;
		return tmp;
	} else
		return TR_EMPTY;
}

/*======================================================================
======================================================================*/
time_t tr_FileAccessTime (char *filename)
{
	struct stat   rls_buf;
	struct tr_lfs_stat buf;

	if (tr_lsTryStatFile(filename, &rls_buf))
		return (rls_buf.st_atime);

	if (tr_lfs_stat (filename, &buf)) {
		tr_Log (TR_WARN_STAT_FAILED, filename, errno);
		return 0;
	}
	return buf.st_atime;
}

/*======================================================================
======================================================================*/
time_t tr_FileModifiedTime (char *filename)
{
	struct stat   rls_buf;
	struct tr_lfs_stat buf;

	if (tr_lsTryStatFile(filename, &rls_buf))
		return (rls_buf.st_mtime);

	if (tr_lfs_stat (filename, &buf)) {
		tr_Log (TR_WARN_STAT_FAILED, filename, errno);
		return 0;
	}
	return buf.st_mtime;
}

/*======================================================================
======================================================================*/
time_t tr_FileStatusChangeTime (char *filename)
{
	struct stat   rls_buf;
	struct tr_lfs_stat buf;

	if (tr_lsTryStatFile(filename, &rls_buf))
		return (rls_buf.st_ctime);

	if (tr_lfs_stat (filename, &buf)) {
		tr_Log (TR_WARN_STAT_FAILED, filename, errno);
		return 0;
	}
	return buf.st_ctime; /* was mtime 3.05 */
}

/*======================================================================
======================================================================*/
double tr_FileSize (char *filename)
{
	struct stat   rls_buf;
	struct tr_lfs_stat buf;

	if (tr_lsTryStatFile(filename, &rls_buf))
		return (rls_buf.st_size);

	if (tr_lfs_stat (filename, &buf)) {
		tr_Log (TR_WARN_STAT_FAILED, filename, errno);
		return 0;
	}
	return (double)buf.st_size;
}

/*======================================================================
======================================================================*/
char * tr_FileType (char *filename)
{
	struct stat   rls_buf;
	struct tr_lfs_stat buf;
	int x;
    tr_mode_t st_mode;

	x = tr_lsTryStatFile(filename, &rls_buf);
	if (x == -1)
		return (TR_EMPTY);
	else 
        if (x == 0) {
            if (tr_lfs_stat (filename, &buf) == -1) {
		        tr_Log (TR_WARN_STAT_FAILED, filename, errno);
		        return TR_EMPTY;
            }
            else
            {
                st_mode = buf.st_mode;
            }
        } 
        else 
        {
            st_mode = rls_buf.st_mode;
        }

	switch (st_mode & S_IFMT) {
	case S_IFDIR:
		return tr_MemPool (TR_FILE_DIRECTORY);
	case S_IFREG:
		return tr_MemPool (TR_FILE_REGULAR);
	default:
		return tr_MemPool (TR_FILE_SPECIAL);
	}
}

/*======================================================================
  It would be nice if this used magic...
======================================================================*/
char * tr_FileContent (char *filename)
{
	FILE *fp;
	char *p;
	int x;
	struct stat   rls_buf;
	struct tr_lfs_stat buf;
    tr_mode_t st_mode;
	char buffer[32 + 1024];

	x = tr_lsTryStatFile(filename, &rls_buf);
	if (x == -1)
		return (TR_EMPTY);
	else
        if (x == 0)
        {
            if (tr_lfs_stat (filename, &buf) == -1) {
        		tr_Log (TR_WARN_STAT_FAILED, filename, errno);
        		return TR_EMPTY;
            }
            else
            {
                st_mode = buf.st_mode;
            }
    	}
        else
        {
            st_mode = rls_buf.st_mode;
        }

	switch (st_mode & S_IFMT) {
	case S_IFDIR:
		return tr_MemPool (TR_FILE_DIRECTORY);
	case S_IFREG:
		/*
		**  Execute a file that is called "filecontent".
		**  It is assumed to be in the path.
		*/
		sprintf (buffer, "content %s", filename);
		if (!(fp = tr_popen (buffer, "r")))
			return tr_MemPool (TR_FILE_REGULAR);
		if (!fgets (buffer, BUFSIZ, fp)) {
			tr_pclose (fp);
			return tr_MemPool (TR_FILE_REGULAR);
		}
		tr_pclose (fp);

		if (p = strchr (buffer, '\n'))
			*p = 0;

		return tr_MemPool (buffer);
	default:
		return tr_MemPool (TR_FILE_SPECIAL);
	}
}

/*======================================================================
  For a couple of microseconds, counter changed
  from double to u_long and fgetc to getc.

  "1\n2" gives 1 as linecount ! /bin/wc does the same, though.
======================================================================*/
double tr_FileLineCount (char *filename)
{
	FILE	*fp;
	int	c;
	unsigned long counter = 0;

	if ((fp = tr_fopen (filename, "r")) == NULL) {
		tr_Log (TR_WARN_STAT_FAILED, filename, errno);
		return (0);
	}
	while ((c = getc(fp)) != EOF) {
		if (c == '\n')
			counter++;
	}
	tr_fclose (fp, 0);

	return (counter);
}

/*======================================================================
======================================================================*/
int tr_FileExist (char *filename)
{
	int x;

	x = tr_lsTryStatFile(filename, NULL);
	if (x == 1)
		return (1); /* yes */
	if (x == -1)
		return (0); /* no file */
	/* else local */

	if (filename && *filename && access(filename, F_OK) == 0)
		return 1;
	return 0;
}

/*======================================================================
  Return 1 if the mode of the file has all the bits from type set, 0 if not.

  We depend on following defines:
  (which is exactly how the bits have been always)

  TR_ACCESS_READ		4
  TR_ACCESS_WRITE		2
  TR_ACCESS_EXEC		1

  Real user- and group-ids are used in testing,
  I wonder which would be the right one, real/efective ???
    juha

  Back here. If the real ids are what we mean, just use access !

  Always using access on NT.

  Should check uid/gid vs. euid/egid and if same, also use access.
  And eaccess on OSs which provide one.
======================================================================*/
int tr_FileAccess (char *filename, int type)
{
	struct stat   rls_st;
    struct tr_lfs_stat st, ls_st;
	int x;

#ifndef MACHINE_WNT
	int gid, ngrps;
#ifdef MACHINE_MIPS
	int
#else
	gid_t
#endif
	groups[128];	/* should really be NGROUPS, this has to be enough. */
#endif

	x = tr_lsTryStatFile(filename, &rls_st);
	if (x == -1)
		return (0); /* Error with remote file, so "no access" */

	if (x == 1) {
		/* ok stat of remote file, these are under .../files/.. thus our own */
		if (type == 4)
			return ((rls_st.st_mode & 0444) != 0);
		if (type == 2)
			return ((rls_st.st_mode & 0222) != 0);
		if (type == 1)
			return ((rls_st.st_mode & 0111) != 0);

		return (1); /* no test-bit, file is there anyway */
	}

	/* not remote, continue locally */

#ifdef MACHINE_WNT

/* TX-2580 23.12.14/YLI(CG) call stat for checking execute permission */
	 if (type == 1){
		if (tr_lfs_stat(filename, &ls_st) == -1){
		    tr_Log (TR_WARN_STAT_FAILED, filename, errno);
			return 0;
		}
		if (ls_st.st_mode & S_IEXEC){
			return 1;
		}else {
			return 0;
		}
		/*End TX_2580 23.12.14/YLI(CG)*/
	}else {
	return (access(filename, type) == 0);
	}
#else /* not WNT */

	if (tr_lfs_stat(filename, &st) != 0) {
		/*
		**  Probably means the file does not exist.
		**  It could be writable, if the parent directory is writable,
		**  should we check that here ???
		*/
		tr_Log(TR_WARN_STAT_FAILED, filename, errno);
		return (0);
	}
	if (getuid() == st.st_uid) {
		/*
		**  We are the proud owners of this file.
		**  Match type against the user-bits.
		*/
		type <<= 6;
		return ((st.st_mode & type) == type);
	}
	/*
	**  Not the owner, check group...
	*/
	if ((gid = getgid()) == (int) st.st_gid) {
		/*
		**  We belong to the group.
		**  Match type against the group-bits.
		*/
		type <<= 3;
		return ((st.st_mode & type) == type);
	}
	/*
	**  It could be one our other groups...
	*/
	if ((ngrps = getgroups(sizeof(groups)/sizeof(groups[0]), groups)) > 0) {
		while (--ngrps >= 0) {
			if (groups[ngrps] == st.st_gid) {
				/*
				**  We did belong to the group, after all.
				**  Match type against the group-bits.
				*/
				type <<= 3;
				return ((st.st_mode & type) == type);
			}
		}
	}
	/*
	**  "Others" is our only hope.
	**  Match type against the other-bits.
	*/
	return ((st.st_mode & type) == type);

#endif /* not WNT */
}

/*======================================================================
  Canonize relative pathname into minimal absolute path.
  Redundant slashes are not removed, just in case the redundancy
  might be special (//spells/like/domain/os).
  Also, C:\\ or C:/ or C:foo.txt etc. are handled in windog.
  Triple sigh.
======================================================================*/

int tr_abspath(char *relpath, char *buffer, int bufmax)
{
	char *rp, *ap; /* relative and absolute working pointers */
	char *cp;      /* tmp for path component */
	int n;
	char *ep = buffer + bufmax - 4; /* end of space */
	int drv = 0;   /* drive letter if nonzero */

	rp = relpath;
	ap = buffer;

	if (*rp == 0) {
#ifndef MACHINE_WNT
		errno = EINVAL;
#endif
		return (-1);
	}
#ifdef MACHINE_WNT
	if (isalpha(rp[0]) && rp[1] == ':') {
		/*
		 *  Drive letter.
		 */
		drv = *rp;
		rp += 2;
	}
#endif
	if (*rp == PSEPC || *rp == '/') {
		/*
		 *  Was absolute.
		 */
		rp++;

		if (drv) {
		 	/*
			 *  Copy drive letter and
			 *  and skip over it, to protect against ../.. rumb.
			 */
			*ap++ = drv;
			*ap++ = ':';
			buffer = ap;
		}
	} else {
		/*
		 *  Relative. Root is handled specially.
		 *
		 *  Use supplied drive on Micro$oft OS.
		 *  get*cwd always return the drive-letter too,
		 *  Protect it from ../..
		 *
		 *  Note that 1=A: 2=B: with _getdcwd...
		 */
		char *rv;
#ifdef MACHINE_WNT
		buffer += 2;

		if (drv) {
			rv = _getdcwd(drv - "aA"[drv < 'a'] + 1, ap, ep - ap);
		} else
#endif
		rv = getcwd(ap, ep - ap);

		if (rv == NULL) {
			/* errno set */
			return (-1);
		}
		/*
		 *  Remove trailing pathseps.
		 */  
		while (*ap)
			ap++;
		while (ap > buffer && ISPSEPC(ap[-1]))
			ap--;
	}
	for ( ; *rp; rp = cp) {
		/*
		 *  Next path component, and its length.
		 *  End on top of terminating zero or slash.
		 */
		for (cp = rp; *cp && !ISPSEPC(*cp); ++cp)
			;
		n = cp - rp;

		if (n == 1 && rp[0] == '.') {
			/*
			 *  Nothing to do for .
			 */
			;
		} else if (n == 2 && rp[0] == '.' && rp[1] == '.') {
			/*
			 *  Strip one path component for ..
			 */
			while (ap > buffer && !ISPSEPC(*--ap))
				;
		} else {
			/*
			 *  Not special, just append / and the component.
			 */
			if (n > ep - ap) {
#ifndef MACHINE_WNT
				errno = ENAMETOOLONG;
#endif
				return (-1);
			}
			*ap++ = PSEPC;
			memcpy(ap, rp, n);
			ap += n;
		}
		/*
		 *  If not last component, bump position over /
		 */
		if (*cp)
			++cp;
	}
	/*
	 *  In case we landed on /.
	 */
	if (ap == buffer)
		*ap++ = PSEPC;
	*ap = 0;
	return (0);
}

