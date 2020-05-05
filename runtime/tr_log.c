/*======================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_log.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 10:48:15 EET 1992
======================================================================*/
/* #include <varargs.h> */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

#ifdef MACHINE_WNT
#define vsnprintf _vsnprintf
#endif
/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_log.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	First version
  3.01 30.03.99/JR	varargs->stdargs
  Bug 6766: 11.04.11/JFC Avoid buffer overflow 
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits

==============================================================================*/

extern int    tr_sourceLine;
extern char * tr_progName;
extern double tr_errorLevel;
extern double TR_LINE;

int tracelevel = 0;      /* Debug level for traces */
static char *filetrace;  /* Traces filename */
#define TX_BUFLOGSIZE 256
#define TX_MAXTRACESIZE 100000000   /* file size limit in bytes */

#define MAX_INT_DIGITS 10

char * tr_basename (char *path)
{
	char *p;
	return (p = strrchr (path, '/')) ? p + 1 : path;
}

/*======================================================================
 tr_Print : log a string in stderr
======================================================================*/
void tr_Print ( char *args, ... )
{
    va_list ap;
    char *fmt;
    int sz;

    fmt = args;

    va_start( ap, args );
    sz = vsnprintf( NULL, 0, fmt, ap ) + 1;
    va_end(ap);

    char *tmp = (char*) tr_calloc( sz, sizeof(char) );

    va_start( ap, args );
    vsnprintf( tmp, sz, fmt, ap );
    va_end(ap);

    tr_mbFputs( tmp, stderr );
    tr_free( tmp );

    /* check in the list of arguments if they are in mempool
 *      * i.e. by use of call to build inside the build function */
    va_start( ap, args );
    tr_FreeMemPoolArgs(args, ap);
    va_end (ap);
}

/*======================================================================
 tr_LogHeader: Build Header for common error logging in tr_Log & tr_Fatal
======================================================================*/
int tr_LogHeader(char **msg, size_t *alloced, char *header, char *headerDataLine)
{
	char *basename;
	size_t size;

	basename = tr_basename(tr_progName);

	/* header */
	if (TR_LINE > 0)
	{
		*alloced = (size_t)(sizeof(TR_LINE)+strlen(basename)+MAX_INT_DIGITS+sizeof("\n")+TX_BUFLOGSIZE+1);
		if ((*msg = (char *)calloc(1, *alloced)) == NULL)
			raisexcept("general", "fatal", "out of memory", 2.0);

		size = sprintf(*msg, headerDataLine, basename, tr_sourceLine, TR_LINE);
	}
	else
	{
		*alloced = (size_t)(strlen(basename)+MAX_INT_DIGITS+sizeof("\n")+TX_BUFLOGSIZE+1);
		if ((*msg = (char *)calloc(1, *alloced)) == NULL)
			raisexcept("general", "fatal", "out of memory", 2.0);

		size = sprintf(*msg, header, basename, tr_sourceLine);
	}

	tr_Trace(1, 1, "tr_LogHeader: size = %d, alloced %d basename \"%s\" msg \"%s\"\n", size, *alloced, basename, *msg);
	return size;
}

/*======================================================================
 tr_LogError : Common error logging for tr_Log & tr_Fatal
======================================================================*/
int tr_LogError(char **msg, size_t *alloced, size_t size, char *args, va_list ap)
{
	char *fmt;
	int tmp_size;
	char *tmp_msg;

	/* body */
	fmt = args;
	tmp_size = vsnprintf( *msg + size, *alloced - size, fmt, ap);
	tr_Trace(1, 1, "tr_LogError : size = %d, alloced %d msg %s\n", tmp_size, *alloced, *msg);
	if ((tmp_size > -1) && (tmp_size < *alloced))
	{
		/* the buffer is ok */
		return 1;
	}

	if (tmp_size > -1) 
	{
		*alloced += tmp_size - TX_BUFLOGSIZE;  /* we allocate the size needed */
	}
	else  /* Windows return -1 if buffer is too small. This should not append on Linux with version > glic 2.1 */
	{
		*alloced *= 2;  /* We retry with a new buffer */
	}

	tr_Trace(1, 1, "tr_LogError : new alloced %d\n", *alloced);
	if ((tmp_msg = (void *)realloc((void *)*msg, *alloced)) == NULL) {
		free(*msg);
		raisexcept("general", "fatal", "out of memory", 2.0);
	}
	else
		*msg = tmp_msg;
	return 0;
}

/*======================================================================
 tr_Log : Log a warning and raise an exception if errorlevel= 3 or above
 should be called tr_Warning
======================================================================*/
void tr_Log ( char *args, ... )
{
	va_list ap;
	char *msg;
	size_t alloced, size;
	int result;

	/*
	**  If the user does not want to hear about
	**  the warnings, then do not continue.
	*/
	if (tr_errorLevel == 0.0)
		return;

	size = tr_LogHeader(&msg, &alloced, TR_WARN_HEADER, TR_WARN_HEADER_WITH_DATALINE);
	do {
		va_start(ap, args);
		result = tr_LogError(&msg, &alloced, size, args, ap);
		va_end(ap);
	} while (!result);
	/*
	**  If the user is completely allergic to any
	**  type of warning, then do the exit.
	*/
	if (tr_errorLevel > 2.0) {
		raisexcept("general", "warning", msg, 1.0);
	} else {
		tr_mbFputsNL( msg, stderr );
		free(msg);
	}
}

/*======================================================================
  tr_Fatal : log an error and raise an exception
======================================================================*/
void tr_Fatal ( char *args, ... )
{
	va_list ap;
	char *msg;
	size_t alloced, size;
	int result;

	size = tr_LogHeader(&msg, &alloced, TR_FATAL_HEADER, TR_FATAL_HEADER_WITH_DATALINE);
	do {
		va_start(ap, args);
		result = tr_LogError(&msg, &alloced, size, args, ap);
		va_end(ap);
	} while (!result);

	raisexcept("general", "fatal", msg, 2.0);
}

/*======================================================================
  tr_OutOfMemory : log an error and exit
======================================================================*/
void tr_OutOfMemory()
{
	char *basename;

	basename = tr_basename(tr_progName);

	/* header */
	if (TR_LINE > 0)
	{
		fprintf(stderr, TR_FATAL_HEADER_WITH_DATALINE, basename, tr_sourceLine, TR_LINE);
	}
	else
	{
		fprintf(stderr, TR_FATAL_HEADER, basename, tr_sourceLine);
	}
	fprintf(stderr, TR_FATAL_NO_MEMORY);

	exit(1);
}

void tr_Inittrace() 
{
	if (getenv("TX_MEMDEBUG"))
		tracelevel = atoi(getenv("TX_MEMDEBUG"));
	filetrace = getenv("TX_MEMTRACE");
}

void tr_InittraceParams(char * iFiletrace, int iTraceLevel) 
{
	tracelevel = iTraceLevel;
	if (iFiletrace && strlen(iFiletrace) > 0)
		filetrace = strdup(iFiletrace);
}

void tr_Trace(int level, int withtime, const char *fmt, ...)
{
	static FILE *fdtrace = NULL;
	static int traceCounter = 1;
	static int filesize = 0;
	static int size, resultsize;
	static char buf[TX_BUFTRACESIZE];
	char* newbuf;
	va_list args;
	time_t timenow;
	struct tm *timestamp = NULL;

	if (tracelevel >= level) {
		if (!fdtrace) {
/*			filetrace = defaulttracefile;
			fprintf(stderr, "filetrace = %s\n", filetrace ? filetrace : "nul");*/
			if (filetrace) {
				sprintf(buf, "%s%d", filetrace, traceCounter);
				fdtrace = fopen(buf, "a");
				if (!fdtrace) {
					fprintf(stderr, "Could not open %s for traces, err %d\n", filetrace, errno);
					fdtrace = stderr;
				}
			}
			else
				fdtrace = stderr;
		}
		if (withtime) {
			timenow = time(NULL);
			timestamp = (localtime(&timenow));
			sprintf(buf, "%.2d/%.2d %.2d:%.2d:%.2d ", timestamp->tm_mday, timestamp->tm_mon+1,timestamp->tm_hour,timestamp->tm_min, timestamp->tm_sec);
			filesize += strlen(buf);
			tr_mbFputs( buf, fdtrace);
		}
		va_start(args, fmt);
		resultsize = vsnprintf(buf, TX_BUFTRACESIZE, fmt, args);
		va_end(args);
		if (resultsize == -1) { /* For Windows */
			va_start(args, fmt);
			resultsize = tr_getBufSize(TX_MAXTRACESIZE, fmt, args);
			va_end(args);
		}
		if (resultsize >= TX_BUFTRACESIZE) {
			if ((newbuf = (char *)malloc(resultsize+1)) == NULL) {
				filesize += TX_BUFTRACESIZE + sizeof("\n*** not enought memory to print the complete trace ***\n")-1;
				tr_mbFputs( buf, fdtrace );
				tr_mbFputs("\n*** not enought memory to print the complete trace ***\n", fdtrace);
			}
			else {
				size = resultsize;
				do {
					va_start(args, fmt);
					resultsize = vsnprintf(newbuf, size+1, fmt, args);
					va_end(args);
					if (resultsize == -1) {
						size = size + TX_BUFTRACESIZE;
						if ((newbuf = (char *)realloc(newbuf, size+1)) == NULL) {
							filesize += TX_BUFTRACESIZE + sizeof("\n*** not enought memory to print the complete trace ***\n")-1;
							tr_mbFputs(buf, fdtrace); 
							tr_mbFputs("\n*** not enought memory to print the complete trace ***\n", fdtrace);
							break;
						}
					}
				} while (resultsize == -1);
				if (newbuf) {
					filesize += size;
					tr_mbFputs(newbuf, fdtrace);
					free(newbuf);
				}
			}
		}
		else {
			filesize += strlen(buf);
			tr_mbFputs(buf, fdtrace);
		}
		fflush(fdtrace);
		if (filesize > TX_MAXTRACESIZE) {
			fclose(fdtrace);
			fdtrace = NULL;
			traceCounter++;
			filesize = 0;
		}
	}
}

