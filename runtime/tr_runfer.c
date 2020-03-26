/*=============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:       tr_runfer.c
	Programmer: Juha Nurmela (JN)
	Date:       Tue Feb 16 08:51:40 EET 1993
=============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_runfer.c 47371 2013-10-21 13:58:37Z cdemory $")
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 ??.??.??/??	?
  3.00 17.02.93/??	The read lines were put into table in wrong order.
  3.00 23.02.93/JN	Change to command line of fer.
  3.02 11.03.93/MV	Changed the return status to be 0 (FALSE) for failures
					and 1 (TRUE) for success.
  3.03 13.02.96/JN	tr_am_ change.
  3.04 27.11.96/JN	Get rid of nt compatlib.
  3.05 19.08.97/JN	This cannot work on NT, dummy out to ld some progs.
============================================================================*/

#ifdef MACHINE_WNT

bfRunFer(resource, extras, args, table)
{
	return (0); /* Generally negative response from runfer */
}

#else /* ! NT, rest of file */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/param.h>

#include "tr_prototypes.h"
#include "tr_strings.h"

#define TR_2MANY_FILES "Too many files open (errno %d)"
#define TR_WRITE_ERROR "Write error (errno %d)"
#define TR_LINE_2LONG  "Line too long (line ignored)"

#ifndef MACHINE_LINUX
extern int errno;
#endif

/*
 *	Let's make sure 0 and 1 exist
 */
static
safefds()
{
	int i;
	if((i = open("/dev/null", 0)) == 0)
		i = dup(i);
	else if(i == 1)
		;
	else
		close(i);
}

bfRunFer(resource, extras, args, table)
char *resource;
char *extras; /* table of extra resources in addition to resource file */
char *args;   /* table of arguments */
char *table;  /* table of data to be edited */
{
	/*
	 *	Output of fer is read into this struct.
	 *	table is touched only if fer exists with 0.
	 */
	struct {
		void *next;
		char data[1];
	} **respp, *resp, *responses;
	void *walkpos;
	char *tab, *cp, *key, *value;
	int down[2], up[2];
	int i, pid, saverrno, status;
	FILE *rfp, *wfp;      /* from child and to child */
	char *argv[512];
	int argc = 0;
	char buf[2048];

	argv[argc++] = "fer";
	argv[argc++] = resource;

	walkpos = tr_am_rewind(extras);

	while (tr_amtext_getpair(&walkpos, &key, &value)) {
		if (value && *value) {
			if(argc >= sizeof(argv) / sizeof(argv[0]) - 5){
				tr_Log(TR_WARN_TOO_MANY_ARGS_IN_EXEC);
				return 0;
			}
			argv[argc++] = "-xrm";
			argv[argc++] = value;
		}
	}
	/*
	 * --- cuts the queue of toolkit-options,
	 * after this only our own arguments.
	 */
	argv[argc++] = "---";
	argv[argc++] = "-"; /* means stdin */

	walkpos = tr_am_rewind(args);

	while (tr_amtext_getpair(&walkpos, &key, &value)) {
		if (value && *value) {
			if(argc >= sizeof(argv) / sizeof(argv[0]) - 2){
				tr_Log(TR_WARN_TOO_MANY_ARGS_IN_EXEC);
				return 0;
			}
			argv[argc++] = value;
		}
	}
	argv[argc++] = NULL;

	safefds();

	if((i = pipe(down)) != 0
	|| pipe(up)){
		saverrno = errno;
		tr_Log(TR_2MANY_FILES, errno);
		if(i == 0){
			close(down[0]);
			close(down[1]);
		}
		errno = saverrno;
		return 0;
	}
	if((wfp = fdopen(down[1], "w")) == NULL
	|| (rfp = fdopen(up[0], "r")) == NULL){
		saverrno = errno;
		tr_Log(TR_2MANY_FILES, errno);
		close(down[0]);
		if(wfp)
			fclose(wfp);
		else
			close(down[1]);
		close(up[0]); close(up[1]);
		errno = saverrno;
		return 0;
	}
	fflush(stdout);
	fflush(stderr);

#undef fork
	pid = fork();
	if(pid == -1){
		saverrno = errno;
		tr_Log(TR_ERR_CANNOT_CREATE_PROCESS, errno);
		close(down[0]); fclose(wfp);
		fclose(rfp); close(up[1]);
		errno = saverrno;
		return 0;
	}
	if(pid == 0){
		close(down[1]);
		close(up[0]);
		close(0);
		if(dup(down[0]) != 0)
			_exit(1);
		close(1);
		if(dup(up[1]) != 1)
			_exit(1);
		close(down[0]);
		close(up[1]);

		execvp(*argv, argv);
		tr_Fatal(TR_ERR_EXEC_FAILED, *argv, errno);
		_exit(1); /* useless */
	}
	close(down[0]);
	close(up[1]);
	/*
	 *	Let's give the table contents to fer.
	 *	Fer first reads the table from stdin, executes and
	 *	then writes the table to stdout.
	 *
	 *	If the child gets killed before the whole table is
	 *	written, we'll get SIGPIPE.
	 */
	walkpos = tr_am_rewind(table);

	while (tr_amtext_getpair(&walkpos, &key, &value)) {
		if (value && *value) {
			if (fprintf(wfp, "%s=%s\n", key, value) < 0) {
				saverrno = errno;
				tr_Log(TR_WRITE_ERROR, errno);
				fclose(wfp);
				goto out;
			}
		}
	}
	if(fclose(wfp)){
		saverrno = errno;
		tr_Log(TR_WRITE_ERROR, errno);
out:
		fclose(rfp);
		kill(pid, SIGTERM);
		while((i = wait(&status)) != pid && (i != -1 || i == EINTR));
		errno = saverrno;
		return 0;
	}
	responses = NULL;
	respp = &responses;

	while(fgets(buf, sizeof(buf), rfp)){
		if((cp = strchr(buf, '\n')) == NULL){
			/*
			 *	Oops, too long line. Let's drop the rest of it.
			 */
			tr_Log(TR_LINE_2LONG);
			while(fgets(buf, sizeof(buf), rfp) && strchr(buf, '\n') == NULL);
		} else {
			*cp = 0;
			/*
			 *	We'll keep the list of lines read in numeric order.
			 *	This way the later lines overwrite the ones arrived earlier.
			 */
			resp = (void *) tr_malloc(sizeof(*resp) + strlen(buf));
			resp->next = NULL;
			strcpy(resp->data, buf);
			*respp = resp;
			respp = (void *) &resp->next;
		}
	}
	fclose(rfp);

	while((i = wait(&status)) != pid && (i != -1 || i == EINTR));
	/*
	 *	Three options:
	 *		Exit.
	 *		Died to signal !!!
	 *		Child was lost ???
	 */
	saverrno = errno;
	if(i == pid){
		if((status & 0xff) == 0)
			status = ((status >> 8) & 0xff);
		else {
			tr_Log(TR_MSG_TERMINATED_BY_SIGNAL, *argv, status & 0x7f);
			if(status & 0x80)
				tr_Log(TR_MSG_CORE_DUMPED);
			status = -1;
		}
	} else {
		tr_Log(TR_ERR_WAIT_FAILED, errno);
		status = 1;
	}

	/*
	 *	We'll touch the edited table only if everything went fine.
	 */
	if(status == 0)
		tr_am_textremove(table);
	while((resp = responses) != NULL){
		responses = resp->next;
		if(status == 0)
			tr_SetValue(0, table, resp->data, ':', '=');
		tr_free(resp);
	}
	/*
	**  MV	Thu Mar 11 09:53:01 EET 1993
	**
	**  Changed the return code to work "as expected" for
	**  boolean functions.
	**  FALSE means something went wrong or the user did
	**  not want the data to be saved.
	**  TRUE means that data is to saved.
	*/
	if (status)
		return 0;
	else
		return 1;
}

#endif /* ! NT */
