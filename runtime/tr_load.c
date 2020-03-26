/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		runtime/tr_load.c
	Programmer:	Mikko Valimaki
	Date:		Fri Oct  9 14:41:11 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_load.c 55062 2019-07-22 12:35:02Z sodifrance $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 17.03.93/JN	Removed tr_free for string that entered MemPool.
  3.02 20.04.94/KV	New load mode implemented (4)
  3.03 06.02.95/JN	Removed excess #includes.
  3.04 21.02.96/JN	fgets() kept last line on buf on eof,
			preventing close(). strtokking removed.
  3.05 27.03.97/JN	/dev/null vs. NUL swapping. brrrh.
  3.06 22.11.99/KP	Do not warn for mismatching separators if none are given
					(fixes unneccessary warnings in WinNT)
  TX-3123 - 28.05.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
============================================================================*/

#include <stdio.h>
#include <string.h>
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"
#ifdef MACHINE_LINUX
#include <errno.h>
#else
extern int errno;
#endif

enum {
    LOAD_FRESH    = 0, /* Create a fresh array */
    LOAD_OVERRIDE = 1, /* Override old values, leave old */
    LOAD_AUGMENT  = 2, /* Augment to existing values */
    LOAD_ADD      = 3, /* Add new entries */
    LOAD_4        = 4, /* Override existing, remove those with no new value */
};

/*============================================================================
  This procedure sets the value.
  If successfull, it returns 1, otherwise 0.

  multisepc, sepc : These are actually characters ... watch out for sign-extension
  
  Input buffer modified !
============================================================================*/
int tr_SetValue (int loadMode, char *arrayname, char *buffer, int multisepc,  int sepc)
{
	char *name;
	char *value;
	char *oldValue;
	char *cp;
	int _localmemptr;
	int result = 0;

	/*
	 *  buffer starts with the name,
	 *  no whitespace cleaning with it.
	 *
	 *  Skip lines without separator
	 *  and lines starting with the separator.
	 *  Clean end of value from crlfs, other whitespace left there.
	 */
	if ((name = buffer) == NULL
	 || (value = strchr(buffer, sepc)) == NULL
	 || name == value)
		return 0;

	_localmemptr = tr_GetFunctionMemPtr();

	*value++ = 0;

	cp = value + strlen(value);
	while (--cp >= value && (*cp == '\r' || *cp == '\n'))
		*cp = 0;

	if (*value == 0) {
		/*
		 * we enter here only when we have the NAME but no VALUE
		 */
		if (4 == loadMode) {
			/*
			** remove the existing value if it exists. 20.4.94/KV
			*/
			oldValue = tr_am_textget (arrayname, name);
			if (oldValue && *oldValue) {
				tr_am_textset (arrayname, name, TR_EMPTY);
			}
		}
		result = 0;
	} else {
		/*
		**  Now we have a NAME and a non-empty VALUE.
		**  Apply the value to the table depending on the loadMode.
		*/
		switch (loadMode) {
		default:
		case 0:
		case 2:
			if (multisepc
			 && ((oldValue = tr_am_textget (arrayname, name))
			 && *oldValue)) {
				/*
				**  If this index already has a value,
				**  then append the received value to it.
				**
				**  data returned by buildtext
				**  will be released in next GB.
				*/
				value = tr_BuildText ("%s%c%s", oldValue, multisepc, value);
			}
			tr_am_textset (arrayname, name, value);
			break;
		case 1:
		case 4:
			tr_am_textset (arrayname, name, value);
			break;
		case 3:
			if ((oldValue = tr_am_textget (arrayname, name))
			 && *oldValue) {
				/*
				**  Since this NAME already has a value,
				**  skip it.
				*/
				break;
			}
			tr_am_textset (arrayname, name, value);
			break;
		}
		result = 1;
	}

	tr_CollectFunctionGarbage(_localmemptr);  /* Remove unwanted mempool allocation */
	return result;
}

/*============================================================================
============================================================================*/
double tr_Load (int loadMode, char *filename, char *arrayname, char *multiseps, char *seps)
{
	char   *oldSeps;
	FILE   *fp;
	int    count;
	int    hit_eof;
	char   sepc, multisepc;
	char   buffer[8192];

	if (seps && mblen( seps, MB_CUR_MAX) > 1 ) {
		/* invalid seps : multibytes chars not allowed */
		/* TODO : add warning */
	}
	if (multiseps && mblen( multiseps, MB_CUR_MAX) > 1 ) {
		/* invalid multiseps : multibytes chars not allowed*/
		/* TODO : add warning */
	}


	sepc      = seps      ? *seps      : '=';
	multisepc = multiseps ? *multiseps : 0;

	if (filename == (char *)stdin) {
		/*
		**  This is stdin.
		*/
		fp = stdin;

	} else {
		/*
		 *  This is just plain silly.
		 *  If we know there is nothing there,
		 *  why to open it in the first place...
		 *  Note that we close() it below
		 *  using the same (changed) name.
		 *  Well, at least code can now just use EMPTY for filename.
		 */
#ifdef MACHINE_WNT
		if (!filename
		 || !filename[0]
		 || !strcmp(filename, "/dev/null"))
			filename = "NUL";
#else
		if (!filename
		 || !filename[0])
			filename = "/dev/null";
#endif
		if ((fp = tr_GetFp (filename, "r")) == NULL)
			return (0);
	}

	/*
	**  With the default loading mode we remove
	**  the possible previous table.
	*/
	if (loadMode == 0) {
		tr_am_textremove (arrayname);
	} else {
		/*
		**  For other options check the correspondence
		**  between the old and the new separators.
		*/
		oldSeps = tr_am_textget (taFORMSEPS, arrayname);

		if (oldSeps != NULL
		 && oldSeps != TR_EMPTY
		 && (oldSeps[0] != sepc
		  || oldSeps[1] != multisepc)) {
			/*
			 * 3.06/KP : If no seps (or default ones) are given for load() command, 
			 * do not issue a warning but use old seps silently.
			 */
			if ((sepc != '=') || (multisepc != ':'))
				tr_Log (TR_WARN_LOAD_SEP_MISMATCH);
			sepc      = oldSeps[0];
			multisepc = oldSeps[1];
		}
	}
	/*
	 *  Assume there will be no separating lines.
	 */
	hit_eof = 1;
	count   = 0;

	while (tr_mbFgets (buffer, sizeof(buffer), fp)) {
		if (*buffer == sepc) {
			/*
			 *  Premature exit when line starts with the sepchar.
			 */
			hit_eof = 0;
			break;
		}
		count += tr_SetValue (loadMode, arrayname, buffer,
			multisepc, sepc);
	}
	/*
	**  Do not close stdin or any file if we just
	**  got the terminating character instead of actual EOF.
	*/
	if (hit_eof && fp != stdin)
		tr_FileClose (filename);

	/*
	**  Save the used separators for later use by the
	**  tr_Save () routine.
	*/
	buffer[0] = sepc;
	buffer[1] = multisepc;
	buffer[2] = '\0';
	tr_am_textset (taFORMSEPS, arrayname, buffer);

	return count;
}

