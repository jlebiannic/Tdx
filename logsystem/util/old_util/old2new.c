/*========================================================================
        E D I S E R V E R

        File:		old2new.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Reconstructing old-style logsystems.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: old2new.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 31.03.95/JN	Defaulting the xxx.cfg.
  3.02 12.04.95/JN	Repositioning entry 0 used data from last entry.
  3.03 02.04.96/JN	LOGSYS_EXT_IS_DIR hackery.
  3.04 06.08.96/JN	l, sp and rp files are munged to contain
			the new, changed index. Also XYZ.l is
			renamed to XYZ0.l if 3.1.1 compatibility is used.
  3.05 07.08.96/JN	FILE= needs also munging, in addition to INDEX=
			inside datafiles.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#ifdef MACHINE_WNT

logsys_convertsystem()
{
	return (0);
}

#else

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>

#include <malloc.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/param.h>
#include <unistd.h>

#include "old2new.h"

#include "logsystem/lib/logsystem.h"
#include "logsystem/lib/old_legacy/logsystem.old_legacy.h"
#include "logsystem/lib/old_legacy/private.old_legacy.h"
#include "logsystem/lib/lstool.h"
#include "logsystem/lib/port.h"

/*
 *  Which files are likely to contain the old index (INDEX=foo)
 *  or FILE=/foo/bar/IND.EXT
 */
#define NEEDS_IDX_FIX(x) ( \
	!strcmp((x), "l") || \
	!strcmp((x), "sp") || \
	!strcmp((x), "rp"))

void logsys_createlabel(LogSystem *log);
void logsys_createdata(LogSystem *log);
void logsys_createmap(LogSystem *log);
void logsys_createdirs(char *sysname);
int logsys_copyconfig(char *cfgfile, char *sysname);

static OLD_Limits limits;
static OLD_LOGSYS logsys;

static struct hinttmp {
	int idx;
	time_t stamp;
} created[128], modified[128];


int LoadSystemLimits(OLD_LOGSYS *ls);
int copydata(int fd, LogSystem *log);
int writehints(LogSystem *log);
int linkfiles(char *sysname);
int backlinkfiles(char *sysname);
int copyfields(char *old, LogEntry *entry);
int insert_hints(struct hinttmp *tab, int tabsize, int idx, int stamp);
int movezerofiles(char *sysname, int idx);
int linkfiles(char *sysname);
int fixindexes(char *filename, int oldindex, int newindex);
int GetPair (FILE *fp, char **name, char **value);


int
logsys_convertsystem(sysname, cfgfile)
	char *sysname;
	char *cfgfile;
{
	int fd, n;
	LogSystem *newlog;

	newlog = logsys_alloc(Sysname);

	newlog->open_mode = O_RDWR;

	newlog->label = logsys_readconfig(cfgfile,0);
	if (newlog->label == NULL)
		bail_out("Cannot open %s", cfgfile);

	if ((fd = logsys_openfile(newlog, "/.base", O_RDONLY, 0)) < 0)
		bail_out("Opening .base");

	if (read(fd, &logsys, sizeof(logsys)) != sizeof(logsys))
		bail_out("Reading header from .base");

	if (LoadSystemLimits (&logsys))
		bail_out("Reading limits");

	printf("\n");
	for (n = 0; n < logsys.numFields; ++n) {
		printf("%-16s  length %3d, pos %3d\n",
			logsys.field[n].name,
			logsys.field[n].length,
			logsys.field[n].pos);
	}
	printf("\n");
	printf("# %d numEntries\n", logsys.numEntries);
	printf("# %d insertPoint\n", logsys.insertPoint);
	printf("# %d freeSpot\n", logsys.freeSpot);
	printf("# %d sizeCheckCounter\n", logsys.sizeCheckCounter);
	printf("# %d agingCheckCounter\n", logsys.agingCheckCounter);
	printf("# %d entrysize\n", logsys.entrySize);
	printf("\n");
	printf("# %d maxNumEntries\n", limits.maxNumEntries);
	printf("# %d maxAging\n", limits.maxAging);
	printf("# %d maxSize\n", limits.maxSize);
	printf("# %d agingCheckInterval\n", limits.agingCheckInterval);
	printf("# %s agingUnit\n", limits.agingUnit);
	printf("# %d sizeCheckInterval\n", limits.sizeCheckInterval);
	printf("# %s sizeUnit\n", limits.sizeUnit);
	printf("# %d ownProgInterval\n", limits.ownProgInterval);
	printf("\n");

	if (Really) {
		int omask = umask(0);

		logsys_createlabel(newlog);
		logsys_createdata(newlog);
		logsys_createmap(newlog);

		/*
		 * Reopen and copy old stuff.
		 */
		newlog = logsys_open(sysname, LS_DONTFAIL);
		old_legacy_logsys_readmap(newlog);
		logsys_createdirs(newlog->sysname);
		logsys_copyconfig(cfgfile, newlog->sysname);
		copydata(fd, newlog);
		old_legacy_logsys_writemap(newlog);
		writehints(newlog);

		/*
		 *  Data in log itself now rearranged into
		 *  newstyle log.
		 *  Handle foo.b files, and
		 *  change INDEX parameters in .l, .sp and .rp files
		 *  to contain the right index. New index 
		 *  has a 0 appended, first "generation" of entry.
		 *
		 *  If compatibility with 3.1.1 logsystem is wanted,
		 *  the linked and index-fixed e/abc0 files are
		 *  later relinked as oldstyle abc0.e names.
		 */
		linkfiles(sysname);

		if (!LOGSYS_EXT_IS_DIR)
			backlinkfiles(sysname);

		umask(omask);
	}
	exit(0);
}

int
copydata(fd, log)
	int fd;
	LogSystem *log;
{
	int n, idx, recsize;
	LogEntry *entry;
	int firstfree = 0;	/* first free entry found (above index 0) */
	int linkfromfirst = 0;	/* free pointer from entry 0) */
	int active_count = 0;
	int free_count = 0;

	OLD_Header *header, *oldfirst;

	recsize = logsys.entrySize;

	if ((oldfirst = (void *) malloc(recsize)) == NULL)
		bail_out("Mallocing record");
	if ((header = (void *) malloc(recsize)) == NULL)
		bail_out("Mallocing record");

	if (lseek(fd, LS_SUPERBLOCK_SIZE, 0) == -1)
		bail_out("Seeking to first entry");

	/*
	 * New system disallows index 0.
	 * If it was active, we have to find new place for it.
	 * Remember this also when copying freelist pointers.
	 */
	if ((n = read(fd, oldfirst, recsize)) == -1)
		bail_out("Reading entry 0");

	if (n != 0 && (n != recsize || oldfirst->creation == 0)) {
		if (n != recsize)
			fprintf(stderr, "Warning: Partial entry 0\n");
		else
			linkfromfirst = oldfirst->next;

		free(oldfirst);
		oldfirst = NULL;
	}
	for (idx = 1; ; ++idx) {
		if ((n = read(fd, header, recsize)) == 0)
			break;
		if (n == -1)
			bail_out("Reading entry %d", idx);

		if (n != recsize) {
			fprintf(stderr, "Warning: Partial entry %d\n", idx);
			break;
		}
		entry = old_legacy_logentry_alloc(log, idx);

		entry->record_idx = LS_VINDEX(idx, 0);

		if (header->creation) {
			copyfields(header, entry);

			insert_hints(created, SIZEOF(created), idx, header->creation);
			insert_hints(modified, SIZEOF(modified), idx, header->modification);

			++log->map->numused;
			++active_count;
		} else {
			if (!firstfree)
				firstfree = idx;

			/*
			 * Copying of freelist links is complicated,
			 * as entry zero disappears...
			 *
			 * next > 0 is the normal case.
			 * Freelist ends here if this is last or
			 *  links to zeroth entry, which in turn is last.
			 * If freelist goes thru zeroth entry, that
			 * has to be cut away.
			 * 
			 */
			if (header->next > 0)
				entry->record_nextfree = header->next;
			else
			if (header->next == END_OF_CHAIN
			 || linkfromfirst == END_OF_CHAIN)
				log->map->lastfree = idx;
			else
				entry->record_nextfree = linkfromfirst;

			++free_count;
		}
		log->map->lastindex = idx;

		logentry_writeraw(entry);
		logentry_free(entry);
	}
	/*
	 * See comments inside above loop.
	 */
	if (logsys.freeSpot > 0)
		log->map->firstfree = logsys.freeSpot;
	else
	if (logsys.freeSpot == END_OF_CHAIN
	 || linkfromfirst == END_OF_CHAIN)
		log->map->firstfree = 0;
	else
		log->map->firstfree = linkfromfirst;

	if (oldfirst) {
		/*
		 * First entry was active, it has to be moved.
		 * If we found a free entry, use that,
		 * otherwise last+1 index gets used.
		 */
		if (firstfree)
			idx = firstfree;
		else {
			log->map->lastindex = idx;
			++log->map->numused;
		}
		printf("0 moved to %d\n", idx);

		entry = old_legacy_logentry_alloc(log, idx);

		entry->record_idx = LS_VINDEX(idx, 0);

		++active_count;
		copyfields(oldfirst, entry);

		insert_hints(created,  SIZEOF(created),
				idx, oldfirst->creation);
		insert_hints(modified, SIZEOF(modified),
				idx, oldfirst->modification);

		logentry_writeraw(entry);
		logentry_free(entry);

		free(oldfirst);
		movezerofiles(log->sysname, idx);
	}
	free(header);

	printf("%d active, %d free\n", active_count, free_count);
}

/*
 * Insert idx and its time into sorted place in
 * table. Older entries drop from tail.
 */
int
insert_hints(tab, tabsize, idx, stamp)
	struct hinttmp *tab;
	int tabsize;
	int idx;
	int stamp;
{
	int j, i = 0;

	while (tab[i].idx && tab[i].stamp > stamp)
		if (++i == tabsize)
			return;

	for (j = tabsize - 1; j > i; --j)
		tab[j] = tab[j - 1];

	tab[i].idx = idx;
	tab[i].stamp = stamp;
}

int
writehints(log)
	LogSystem *log;
{
	int fd;
	int i;
	LogHints hint;

	fd = logsys_openfile(log, LSFILE_CREATED, O_WRONLY | O_CREAT, Access_mode);
	if (fd >= 0) {
		for (i = 0; i < SIZEOF(created); ++i) {
			if (created[i].idx == 0)
				break;
			hint.idx = created[i].idx;
			write(fd, &hint, sizeof(hint));
		}
		close(fd);
	}
	fd = logsys_openfile(log, LSFILE_MODIFIED, O_WRONLY | O_CREAT, Access_mode);
	if (fd >= 0) {
		for (i = 0; i < SIZEOF(modified); ++i) {
			if (modified[i].idx == 0)
				break;
			hint.idx = modified[i].idx;
			write(fd, &hint, sizeof(hint));
		}
		close(fd);
	}
}

int
copyfields(old, entry)
	char *old;
	LogEntry *entry;
{
	int n;

	for (n = 0; n < logsys.numFields; ++n) {
		
		/*
		 * positions are shifted by 8,
		 * oldstyle record has a pseudo-index in front of it.
		 */
		if (logsys.field[n].pos < 8)
			continue;

		switch (logsys.field[n].length) {
		case LS_TYPE_NONE:
			break;
		case LS_TYPE_TIME:
			old_legacy_logentry_settimebyname(entry,
				logsys.field[n].name,
				*(TimeStamp *) &old[logsys.field[n].pos - 8]);
			break;
		case LS_TYPE_NUMERIC:
			old_legacy_logentry_setnumberbyname(entry,
				logsys.field[n].name,
				*(Number *) &old[logsys.field[n].pos - 8]);
			break;
		default:
			if (logsys.field[n].length <= 0)
				continue;

			old_legacy_logentry_settextbyname(entry,
				logsys.field[n].name,
				(char *) &old[logsys.field[n].pos - 8]);
			break;
		}
	}
}

/*
 * Link oldstyle files to newstyle.
 *
 * .../loki/files/123.l -> .../loki/files/l/1230
 *
 * oldstyle file is unlinked if link ok.
 */
int
linkfiles(sysname)
	char *sysname;
{
	struct stat st;
	DIR *dirp;
	DIRENT *d;
	char path[1025];
	char newpath[1025];
	int ind;
	int dirmode = 0755;
	char *ext;
	struct dirs {
		struct dirs *next;
		char *dir;
	} *done = NULL, **dpp;

	sprintf(path, "%s/files", sysname);

	if ((dirp = opendir(path)) == NULL) {
		printf("No files-directory.\n");
		return;
	}
	if (stat(path, &st) == 0)
		dirmode = st.st_mode;

	printf("Linking files to newstyle directories.\n");

	while ((d = readdir(dirp)) != NULL) {
		/*
		 *  Ignore files which dont look like 123.ext
		 *  0.ext is special and will be handled
		 *  later; Index 0 is illegal in new logsystem.
		 */
		if ((ind = atoi(d->d_name)) <= 0)
			continue;
		if ((ext = strchr(d->d_name, '.')) == NULL || *++ext == 0)
			continue;

		sprintf(path,    "%s/files/%s", sysname, d->d_name);
		sprintf(newpath, "%s/files/%s", sysname, ext);

		for (dpp = &done; *dpp; dpp = &(*dpp)->next)
			if (!strcmp((*dpp)->dir, newpath))
				break;
		if (!*dpp) {
			*dpp = (void *) malloc(sizeof(**dpp));
			(*dpp)->next = NULL;
			(*dpp)->dir = strdup(newpath);

			printf("Making directory %s... ", newpath);
			if (mkdir(newpath, dirmode) == 0)
				printf("ok\n");
			else if (errno == EEXIST)
				printf("exists\n");
			else
				printf("%s\n", syserr_string(errno));
		}
		sprintf(newpath, "%s/files/%s/%d0", sysname, ext, ind);

		if (link(path, newpath)) {
			if (errno != EEXIST) {
				fprintf(stderr, "Linking %s to %s failed: %s\n",
					path, newpath, syserr_string(errno));
			}
		} else {
			/*
			 *  Data ok in new place.
			 *  Remove old name.
			 *
			 *  fix data inside l, rp and sp files.
			 */
			unlink(path);
			if (NEEDS_IDX_FIX(ext))
				fixindexes(newpath, ind, ind * 10);
		}
	}
	closedir(dirp);
}

/*
 *  Read thru file and change
 *  each "^INDEX=oldindex$" to be "INDEX=newindex"
 *  A tempfile is created as filename.tmp and
 *  then linked over the filename.
 *  Only result is, hopefully, that every INDEX= line was updated
 *  to contain the new index.
 *  Sigh, also FILE=123.e has to be munged.
 */
int
fixindexes(filename, oldindex, newindex)
	char *filename;
	int oldindex;
	int newindex;
{
	char newfile[512];
	char buf[256];
	char indline[80];
	char filestr[80];
	struct stat st;
	FILE *ofp, *ifp;
	int changed = 0;
	int x;
	static int said_munging = 0;

	if (said_munging++ == 0) {
		printf("\
Munging l, sp and rp files for new indexes in INDEX= and FILE= lines.\n");
	}

	sprintf(indline, "INDEX=%d\n", oldindex);
	sprintf(filestr, "/%d.", oldindex);

	if ((ifp = fopen(filename, "r")) == NULL) {
		fprintf(stderr, "Open of %s failed: %s\n",
			filename, syserr_string(errno));
		return (-1);
	}
	if (fstat(fileno(ifp), &st))
		st.st_mode = 0600;

	sprintf(newfile, "%s.tmp", filename);

	if ((ofp = fopen(newfile, "w")) == NULL) {
		fprintf(stderr, "Creation of %s failed: %s\n",
			newfile, syserr_string(errno));
		fclose(ifp);
		return (-1);
	}
	chmod(newfile, st.st_mode & 0777);

	while (fgets(buf, sizeof(buf), ifp)) {

		if (!strcmp(buf, indline)) {
			changed = 1;
			if (fprintf(ofp, "INDEX=%d\n", newindex) == EOF) {
				fprintf(stderr, "Write to %s failed: %s\n",
					newfile, syserr_string(errno));
				fclose(ifp);
				fclose(ofp);
				return (-1);
			}
			continue;
		}
		if (!strncmp(buf, "FILE=", sizeof("FILE"))
		 && strchr(buf, '\n')) {
		    char *cp;

		    /*
		     *  FILE=xxx/123.yyy\n
		     *          ^
		     *               ^
		     */
		    cp = strrchr(buf, '/');
		    if (cp && !strncmp(cp, filestr, strlen(filestr))) {

			*cp = 0;                /* cut from last / */
			cp += strlen(filestr);  /* then move into extension */
			cp[strlen(cp) - 1] = 0; /* cut newline, checked above */


			changed = 1;
			if (LOGSYS_EXT_IS_DIR)
			    x = fprintf(ofp, "%s/%s/%d\n", buf, cp, newindex);
			else
			    x = fprintf(ofp, "%s/%d.%s\n", buf, newindex, cp);
			if (x == EOF) {
				fprintf(stderr, "Write to %s failed: %s\n",
					newfile, syserr_string(errno));
				fclose(ifp);
				fclose(ofp);
				return (-1);
			}
			continue;
		    }
		}
		do {
			if (fputs(buf, ofp) == EOF) {
				fprintf(stderr, "Write to %s failed: %s\n",
					newfile, syserr_string(errno));
				fclose(ifp);
				fclose(ofp);
				return (-1);
			}
			if (strchr(buf, '\n'))
				break;
		} while (fgets(buf, sizeof(buf), ifp));
	}
	if (ferror(ifp)) {
		fprintf(stderr, "Read from %s failed: %s\n",
			filename, syserr_string(errno));
		fclose(ifp);
		fclose(ofp);
		return (-1);
	}
	fclose(ifp);

	if (fclose(ofp)) {
		fprintf(stderr, "Write to %s failed: %s\n",
			newfile, syserr_string(errno));
		return (-1);
	}

	if (!changed) {
		unlink(newfile);
	} else {
		unlink(filename);
		if (link(newfile, filename)) {
			fprintf(stderr, "Linking of %s to %s failed: %s\n",
				newfile, filename, syserr_string(errno));
			return (-1);
		}
		unlink(newfile);
	}
	return (0);
}

/*
 * Link newstyle files back to oldstyle names, but with generation
 * added.
 * This shuffling is done to overcome situations
 * where both 888.l and 8880.l did exist on oldstyle log.
 * Now all files are first put into .../files/l/,
 * then back into .../files/.
 *
 * .../loki/files/l/1230 -> .../loki/files/1230.l
 */
int
backlinkfiles(sysname)
	char *sysname;
{
	struct stat st;
	DIR *dirp, *extdirp;
	DIRENT *extd, *d;
	char path[1025];
	char newpath[1025];
	int ind;
	int dirmode = 0755;
	char *ext;

	sprintf(path, "%s/files", sysname);

	if ((dirp = opendir(path)) == NULL) {
		printf("No files-directory.\n");
		return;
	}
	printf("Linking files back to oldstyle for compatibility.\n");

	while ((extd = readdir(dirp)) != NULL) {

		sprintf(path, "%s/files/%s", sysname, extd->d_name);

		if (stat(path, &st)) {
			fprintf(stderr, "Cannot stat %s: %s\n",
				path, syserr_string(errno));
			continue;
		}
		if ((st.st_mode & S_IFMT) != S_IFDIR)
			continue;

		if ((extdirp = opendir(path)) == NULL) {
			fprintf(stderr, "Cannot open %s: %s\n",
				path, syserr_string(errno));
			continue;
		}
		while ((d = readdir(extdirp)) != NULL) {
			/*
			 *  Ignore files which dont look like 1230
			 */
			if ((ind = atoi(d->d_name)) <= 0
			 || d->d_name[strlen(d->d_name) - 1] != '0')
				continue;

			sprintf(path,    "%s/files/%s/%s",
				sysname, extd->d_name, d->d_name);
			sprintf(newpath, "%s/files/%d.%s",
				sysname, ind, extd->d_name);

			if (link(path, newpath)) {
				fprintf(stderr, "Linking %s to %s failed: %s\n",
					path, newpath, syserr_string(errno));
			}
		}
		closedir(extdirp);
	}
	closedir(dirp);
}

/*
 * Moves files under index 0 to another index.
 * (both filenames still oldstyle)
 *
 * XYZ.l, XYZ.sp and XYZ.rp are fixed to contain the changed index.
 */
int
movezerofiles(sysname, idx)
	char *sysname;
	int idx;
{
	DIR *dirp;
	DIRENT *d;
	char *ext;
	char path[1025];
	char newpath[1025];

	sprintf(path, "%s/files", sysname);

	if ((dirp = opendir(path)) == NULL)
		return;

	while ((d = readdir(dirp)) != NULL) {

		if (strncmp(d->d_name, "0.", 2))
			continue;

		ext = d->d_name + 2;
		sprintf(path,    "%s/files/%s", sysname, d->d_name);
		sprintf(newpath, "%s/files/%d.%s", sysname, idx, ext);

		if (link(path, newpath)) {
		    if (errno != EEXIST)
			fprintf(stderr, "Linking %s to %d.%s failed: %s\n",
				d->d_name,
				idx, d->d_name + 2,
				syserr_string(errno));
		} else {
			printf("Note: %s now %d.%s\n",
				d->d_name,
				idx, d->d_name + 2);
			unlink(path);
			if (NEEDS_IDX_FIX(ext))
				fixindexes(newpath, 0, idx);
		}
	}
	closedir(dirp);
}

#define VALUE 0
#define UNIT  1

int
LoadSystemLimits (ls)
	OLD_LOGSYS *ls;
{
	int  i;
	FILE *fp;
	char *name;
	char *value;
	static struct {
		char *name;
		void  *address;
		int type;
	} limitTable[] = {
		MKLS_TEXT_MAX_COUNT,	&limits.maxNumEntries,		VALUE,
		MKLS_TEXT_MAX_AGING,	&limits.maxAging,		VALUE,
		MKLS_TEXT_MAX_SIZE,	&limits.maxSize,		VALUE,
		MKLS_TEXT_AGING_CHECK,	&limits.agingCheckInterval,	VALUE,
		MKLS_TEXT_AGING_UNIT,	limits.agingUnit,		UNIT,
		MKLS_TEXT_SIZE_CHECK,	&limits.sizeCheckInterval,	VALUE,
		MKLS_TEXT_SIZE_UNIT,	limits.sizeUnit,		UNIT,
		MKLS_TEXT_OWN_PROG,	&limits.ownProgInterval,	VALUE,
		(char *)0
	};

	{
		char filename[1025];

		sprintf(filename, "%s/.limits", Sysname);

		if (!(fp = fopen (filename, "r")))
			return 1;
	}
	while (GetPair (fp, &name, &value) != EOF) {
		if (!name || !value)
			continue;
		for (i = 0; limitTable[i].name; i++) {
			if (!strcmp (limitTable[i].name, name)) {
				switch (limitTable[i].type) {
				case VALUE:
					*((int *)limitTable[i].address) = atoi (value);
					break;
				case UNIT:
					while (isspace (*value))
						value++;
					*((char *)limitTable[i].address) = toupper(*value);
					break;
				}
			}
		}
	}
	/*
	**  Calculate actual values based on the units.
	**  Aging is presented in seconds.
	*/
	if (*limits.agingUnit == (int)toupper(LS_TIME_DAY))
		limits.maxAging *= 24 * 60 * 60;
	else if (*limits.agingUnit == (int)toupper(LS_TIME_HOUR))
		limits.maxAging *= 60 * 60;
	else if (*limits.agingUnit == (int)toupper(LS_TIME_MIN))
		limits.maxAging *= 60;
	else
		/*
		**  Use weeks as the default unit.
		*/
		limits.maxAging *= 7 * 24 * 60 * 60;

	/*
	**  Size.
	**  Use kilobytes as the default unit.  If the size unit
	**  is kilobytes we do not have to do anything since the
	**  size is presented as kilobytes.
	*/
	if (*limits.sizeUnit == (int)toupper(LS_SIZE_BYTE))
		limits.maxSize /= 1024;
	else if (*limits.sizeUnit == (int)toupper(LS_SIZE_MEGABYTE))
		limits.maxSize *= 1024;
	fclose (fp);
	return 0;
}

int
GetPair (fp, name, value)
	FILE *fp;
	char **name;
	char **value;
{
	static char buffer[BUFSIZ];
	static int  lineno = 0;
	char *p;
	char *q;

	while (fgets (p = buffer, BUFSIZ, fp)) {
		lineno++;
		while (isspace (*p))
			p++;
		if (!*p || *p == '\n' || *p == '#')
			continue;
		/*
		**  Extract the NAME-VALUE pair
		*/
		if (!strchr (p, '=') || !(*name = strtok (p, "=\n#"))) {
			fprintf (stderr, "No name-value pair on line %d\n", lineno);
			continue;
		}
		if (strcspn (*name , " \t") != strlen (*name)) {
			fprintf (stderr, "A name cannot contain any white spaces (line %d)\n", lineno);
			continue;
		}
		*value = strtok (NULL, "\n#");
		return 0;
	}
	return EOF;
}

#endif /* ! WNT */

