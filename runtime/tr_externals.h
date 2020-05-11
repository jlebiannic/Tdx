#ifndef __TR_EXTERNALS_H__
#define __TR_EXTERNALS_H__
/* TradeXpress: tr_externals.h */
/*======================================================================
======================================================================*/
#include "tr_logsystem.h"
 
/* defined in tr_main.c */
extern char *tr_matchBuf;         /* Used for line matching */
extern char *tr_returnBuffer;     /* Contains possible "unread" */
extern int tr_winSize;            /* Window max size, 0 if not used */
extern int tr_curHighLine;        /* Highest line in current window */
extern char tr_optArray[];        /* Command line options A-Z */
extern int tr_lastCall;           /* Used in MOTIF-debugger */
extern char *tr_programName;      /* Prgram name */
extern char *tr_ADS;              /* Decimal separator used in application file */
extern char *tr_MDS;              /* Primary Decimal separator used in EDI-msg */
extern char *tr_MDS_IN;           /* Primary Decimal separator used in received EDI-msg */
extern char *tr_progName;         /* Name of the running program.  */
extern int tr_sourceLine;         /* Current source code line */
extern double tr_errorLevel;      /* To print or not, to exit or not */
extern double tr_ARGC;            /* Number of arguments (in addition to options and parameters) */
extern char *tr_parameterFile;    /* Filename for command line parameters */
extern int tr_updateParams;       /* Flag telling whether parameter file is updated */
extern int tr_unBufferedOutput;   /* Flag telling whether the output is buffered or not */
extern int tr_processErroneous;   /* A user modifiable variable to indicate where erroneous messages are directed  */
extern int tr_writeSegStatus;     /* The status from the last tr_writesegment() */
extern char *tr_lastSegment;      /* The segment data in string format for last read */
extern char *tr_charSet;          /* Filename for the compiled character set */
extern LogSysHandle paramArrayLog; /* rte parameters -p, -q */
#define MAX_CHAR_DIRS 16
extern char	*tr_Outgoing_CharSet;
extern char	*tr_Incoming_CharSet;
extern char	*tr_Outgoing_CharSetDirs[MAX_CHAR_DIRS+2];
extern char	*tr_Incoming_CharSetDirs[MAX_CHAR_DIRS+2];
extern int	tr_In;                /* Set to 1 when processing received message, 0 for building message. */

/* defined in generated .c from rte */
extern int  tr_validSchema;
extern char taFORMSEPS[];
extern char taPARAMETERS[];
extern int tr_elemCount;
extern int tr_segCounter;
extern short int tr_in_EDISyntax;
extern short int tr_out_EDISyntax;

/* defined in translator.h by the compiled rte */
extern char   *TR_EMPTY ;
extern char   *TR_EOF;
extern char   *TR_FILLCHAR ;
extern char   *TR_NEWLINE ;
extern char   *TR_IRS ;
extern double TR_LINE;
extern double TR_CARRIAGERETURN;
extern char   *tr_multisep;       /* Separator for repeating parameters default value ':' */
extern int tr_bufIndexWidth;      /* Width for the text representation */
extern int tr_codeConversionSize; /* Number of lines to be buffered */

// TODO remove these two variables and clean libs and tools which use them
// defined in tr_loaddef.c
extern int tr_useLocalLogsys;
extern int tr_useSQLiteLogsys;

/* defined in bottom\ckseg.c */
extern int tr_cleanTrailingSpaces;

#endif
