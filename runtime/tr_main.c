/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_main.c
	Programmer:	Mikko Valimaki
	Date:		Wed May 20 17:02:51 EET 1992

	Copyright (c) 1996 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_main.c 55495 2020-05-06 14:41:40Z jlebiannic $")
static char *uname = UNAME;
/* LIBRARY(libruntime_version) */
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 ??.??.??/MV	Removed the unused argument n and
			added the forgotten q argument to the list.
  3.02 ??.??.??/MV	Added the possibility to produce core files SIGBUS
			and SIGVEG
  3.03 ??.??.??/JN	produceCore now leaves QUIT, BUS and SEGV at
			default values.
  3.04 07.08.93/MV	The forgotten q arguement (see 3.01 above) had
			forgotten the colon sign to indicate it uses
			an option argument.  Thus it didnt work.
  3.05 06.07.94/JN	Adding argument to tr_InitMemoryManager.
  3.06 16.02.95/JN	main changed to be tr_Startup(argc, argv)
			and called from generated C.
  3.07 21.04.95/JN	MakeIndexes to IntIndexes.
  3.08 15.01.96/JN	version strings masked by missing end-of-comment above.
  3.09 06.02.96/JN	Alarm call added to catched signals.
  3.10 12.02.96/JN	background() on NT.
       13.02.96/JN	tr_am_ change
  3.11 14.03.96/JN	Added second argument to bfUserSignal(), char *signame
  3.12 02.04.96/JN	LOGSYS_EXT_IS_DIR
  3.13 27.11.96/JN	Getting rid of nt compatlib.
  3.14 19.11.98/HT	TradeXpress 4 changes.
  3.15 09.06.99/HT	tr_FileCloseAll() added to tr_Exit()
  3.16 06.07.99/HT	tr_sourceLine, tr_processErroneous, tr_writeSegStatus,
					tr_lastSegment defined as extern.
  3.17 04.08.99/JR	Flushing problems with Linux
  3.18 08.08.99/KP	Use '007' as multisep in WinNT when loading _PARAMETERS_
					(in local mode) and parsing them from cmdline.
  4.00 21.09.99/JH	Parsing for -eEDIFACT4 command line option
			and global variable tr_syntax added.
			Parsing for -e00402 command line option.
  4.01 26.10.99/KP	(NT) Set LogoffHandler after setting signal()'s, to avoid
					being killed at logoff if we are launched from a service
					(scanner or logd, for example)
  4.02 14.12.99/HT	Added initialization values to tr_syntax, tr_arrComposites,
					tr_arrComponets definitions.
					RTE-maincode refers to these variables as externals.
  4.03 29.03.2000/JR  freopen -> tr_FileReopen
  4.04 29.03.2000/JH    tr_FileReopen return value checked correcly now.
  4.05 07.07.2003/UK	If MULTI_SEPARATOR is defined in file $HOME/.tclrc,
  				multisep's default value (tr_multisep, defined
				in translator.h) is overrided
  4.06 16.07.2003/UK	Multisep is limited to : or \007
  5.00 01.07.2009/LME	Command line parameters must override those from parameter file bugZ 9200
  TX-3123 - 14.05.2019 - Olivier REMAUD - UNICODE adaptation

============================================================================*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif
#include <signal.h>
#ifdef MACHINE_LINUX
#include <errno.h>
#endif

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_bottom.h"
#include "tr_strings.h"

#ifndef MACHINE_LINUX
extern int  errno;
#endif

#if defined MACHINE_WNT || defined MULTI_SEP_007
/* Needed in tr_Load(_PARAMETRES_) later on... (3.18/KP) */
extern char taFORMSEPS[];
#endif

/*==========================================================================
==========================================================================*/
#define TCLRC_LINE_LENGHT	1024
#define TCLRC_MULTI_SEPARATOR	"MULTI_SEPARATOR="
#define TCLRC			"/.tclrc"
/*==========================================================================
==========================================================================*/

/*======================================================================
	Internally used variables.
======================================================================*/
static int produceCore = 0;
static int tr_charSetDirCount = 0;
static char *tr_charSetDirs[MAX_CHAR_DIRS+2];

static void tr_CatchSignals ();
static void tr_set_signal_handler(int sig, void (*handler)(int));

/*============================================================================
============================================================================*/
extern char *TR_EMPTY;
extern char taPARAMETERS[];
extern char taARGUMENTS[];
extern char *tr_multisep;
#ifdef DEBUG_CALLS
extern struct tagVarStruct tr_vars[];
#endif

/*============================================================================
============================================================================*/
void tr_Startup (int argc, char **argv)
{
	extern char *optarg;
	extern int  optind;
	extern int  opterr;

	int  i;
	int  option;
	char *cp;
	char *tr_separators = NULL;

#ifdef MACHINE_WNT
	/*
	 *  Call from background() ? (Windows kludgery)
	 *  If so, go continue the backrounding.
	 *  Does not return.
	 *  xxx -background path0 path1 path2 command args
	 */
	if (argc >= 5 && !strcmp(argv[1], "-background"))
		tr_BackgroundExec(argv);
#endif

	/*
	**  Set the logging level to one before a value is
	**  given either from the defaults file or from the
	**  command line.
	*/
	tr_errorLevel = 1.0;
	tr_progName = argv[0];

	if ((cp = getenv("TR_DEBUG")) && *cp > '0')
		fprintf(stderr, "%s invoked\n", tr_progName);

	/*
	**  Establish the heapbottom and heaptop to be used
	**  in deciding which memory to free in assignments.
	**  (see tr_assign.c)
	**  An address of a variable early in stack is given
	**  here, it is used in detection of dynamically
	**  allocated blocks.
	*/
	tr_InitMemoryManager ();
	tr_CatchSignals ();
	tr_LoadDefaults (NULL);

	/*
	**  Prepare for debugging.  This routine strips all
	**  motif arguments from the command line.
	*/
#ifdef DEBUG_CALLS
	debuginit (&argc, &argv, tr_programName, tr_vars);
#endif
	tr_am_textset (taARGUMENTS, tr_IntIndex(0), argv[0]);
	opterr = 0;
	while ((option = getopt (argc, argv,
	"ABCDEFGHIJKLMNOPQRSTUVXYZl:c:d:f:p:s:a:m:n:iq:bze:")) != -1) {
		switch (option) {
		case 'z':
			/*
			**  The user wats to have core files.
			*/
			produceCore = 1;
			break;
		case 'c':
			/*
			**  This defines the character set.
			*/
			tr_charSet = optarg;
			break;
		case 'e':
			/*
			**  4.00/JH EDI syntax.
			*/
			if(!strcmp(optarg, "EDIFACT4"))
				tr_syntax = 2.0;	/* from bottom/tr_edifact.h */
			else 
				tr_syntax = atol(optarg);
			break;
		case 'd':
			/*
			**  Directories for character sets.
			**  Check that there is room in the array
			**  (it has two seats reserved for $PWD and NULL)
			*/
			if (tr_charSetDirCount > MAX_CHAR_DIRS)
				tr_Fatal (TR_FATAL_TOO_MANY_CHAR_DIRS, MAX_CHAR_DIRS);
			tr_charSetDirs[tr_charSetDirCount++] = optarg;
			break;
		case 'f':
			/*
			**  There is an input file specified.
			**  4.03/JR
			*/
			if (tr_FileReopen (optarg, stdin))
				tr_Fatal (TR_WARN_FILE_R_OPEN_FAILED, optarg, errno);
			break;
		case 's':
			/*
			**  This defines the separators used if they
			**  differ from those defined in the character set.
			*/
			tr_separators = optarg;
			break;
		case 'p':
			tr_parameterFile = optarg;
			tr_updateParams = 1;
			break;
		case 'q':
			tr_parameterFile = optarg;
			tr_updateParams = 0;
			break;
		case 'l':
			tr_errorLevel = (double)atoi (optarg);
			/*
			**  Keep as an undocumented feature the possibilty
			**  to turn on the segment engine debugging.
			*/
			if (tr_errorLevel > 3.0)
				tr_verbose ();
			break;
		case 'b':
			setbuf (stdout, NULL);
			setbuf (stderr, NULL);
			tr_unBufferedOutput = 1;
			break;
		case 'a':
			/*
			**  Set the application decimal separator and
			**  make sure it is only one character long.
			*/
			tr_ADS = optarg;
			*(optarg + 1) = '\0';
			break;
		case 'm':
			/*
			**  Set the message decimal separator and
			**  make sure it is only one character long.
			*/
			tr_MDS = optarg;
			*(optarg + 1) = '\0';
			break;
		case 'n':
			/*
			**  Set the message decimal separator for received message
			**  make sure it is only one character long.
			*/
			tr_MDS_IN = optarg;
			*(optarg + 1) = '\0';
			break;
		case 'i':
			tr_GetProgInfo ();
			exit (0);
		case '?':
			tr_Fatal (TR_FATAL_INVALID_COMMANDLINE);
		default:
			if ((option >= 'A') && (option <= 'Z')) {
				/*
				**  Set the corresponding variable
				**  to value TRUE.
				*/
				tr_optArray[option - 'A'] = 1;
			}
			break;
		}
	}

	tr_UseLocaleUTF8();

	/*
	**  If there is a translator character set specified, then
	**  fix up the directory array.
	*/
	if (tr_charSet) {
		if (!(tr_charSetDirs[tr_charSetDirCount++] = getcwd (NULL, 1022)))
			tr_Fatal (TR_FATAL_GETCWD_FAILED);
		/*
		**  Terminate the list with NULL.
		*/
		tr_charSetDirs[tr_charSetDirCount] = NULL;
		tr_loadcharset (tr_charSet, tr_charSetDirs);
	}
	if (tr_separators)
		tr_setseps ((unsigned char *)tr_separators);

    /*
    **  Multi syntax initializations
    */
    tr_Outgoing_CharSet = NULL;
    tr_Incoming_CharSet = NULL;
    for (i = 0; i <= MAX_CHAR_DIRS+1; i++ ) {
        tr_Outgoing_CharSetDirs[i] = NULL;
        tr_Incoming_CharSetDirs[i] = NULL;
    }
    
	tr_returnBuffer = TR_EMPTY;

	if (tr_parameterFile && *tr_parameterFile)
	{
		/* local file */
		if ( (int) tr_Load(0,tr_parameterFile,taPARAMETERS,tr_multisep,"=") == 0)
		{
			tr_parameterFile = NULL;
		}
#if defined MACHINE_WNT || defined MULTI_SEP_007
		else
		{
			/*
			** Set the multisep to be "\007" hard-coded in NT
			** This way we ensure that the pathnames containing colons are loaded correctly
			** in all cases (hopefully) but still set the multisepc to '007' for later tr_Save()
			** so that it will not even create multivalues out of paths.
			** (Somewhat dirty trick but hopefully it works.)
			*/
			char buffer[3];

			buffer[0] = '=';
			buffer[1] = '\007';
			buffer[2] = '\0';
			tr_am_textset (taFORMSEPS, taPARAMETERS, buffer);
		}
#endif
		
	}

	while (optind < argc) {
		if (strchr (argv[optind], '='))
#ifdef MACHINE_WNT
			tr_SetValue (4, taPARAMETERS, argv[optind], 0, '=');
#else
			tr_SetValue (4, taPARAMETERS, argv[optind], 0, '=');
#endif
		else
			break;
		optind++;
	}

	for (i = 1; optind < argc; i++, optind++) {
		tr_am_textset (taARGUMENTS, tr_IntIndex(i), argv[optind]);
	}
	tr_ARGC = i - 1;
}

/*============================================================================
============================================================================*/
void tr_Exit (int exitcode)
{
	char *cp;

	/* 3.17/JR Flushing problems with linux...
	 * Interesting feature! Linux won't flush stdout and stderr
	 * streams before it stops program execution. Maybe it has 
	 * something to do with our _brutal_ tr_FileReopen abroach.
	 *
	 * 18.08.99/JR obsoleted. New design for tr_FileReopen.
#ifdef MACHINE_LINUX
	fflush(stdout);
	fflush(stderr);
#endif
	*/

	tr_SaveParameters ();

	if ((cp = getenv("TR_DEBUG")) && *cp > '0')
		fprintf(stderr, "%s exiting\n", tr_progName);

	/* 3.15/HT */
	tr_FileCloseAll();

#if defined(DEBUG) && (MEM_DEBUG==1)
	TDXMemDebug_dump(1);
	TDXMemDebug_dump_Lines();
#endif
	exit (exitcode);
}

/*============================================================================
============================================================================*/
#ifdef SIGHUP
extern int bfUserSIGHUP();
#endif
#ifdef SIGQUIT
extern int bfUserSIGQUIT();
#endif
#ifdef SIGALRM
extern int bfUserSIGALRM();
#endif

static struct {
	int   signum;
	char *signame;
	int (*sighandler)();
	char *logfmt;
} user_sig_table[] = {
#ifdef SIGHUP
	{ SIGHUP,  "SIGHUP",   bfUserSIGHUP, NULL},
#endif
	{ SIGINT,  "SIGINT",   bfUserSIGINT, NULL},
	{ SIGTERM, "SIGTERM",  bfUserSIGTERM, NULL},
#ifdef SIGQUIT
	{ SIGQUIT, "SIGQUIT",  bfUserSIGQUIT, NULL},
#endif
#ifdef SIGALRM
	{ SIGALRM, "SIGALRM",  bfUserSIGALRM, NULL},
#endif
#ifdef SIGBREAK
	{ SIGBREAK,"SIGBREAK", NULL, },
#endif
	/*
	 *  Error conditions.
	 */
#ifdef SIGBUS
	{ SIGBUS,  "SIGBUS",   NULL, TR_ERR_BUSS_ERROR, },
#endif
	{ SIGFPE,  "SIGFPE",   NULL, TR_ERR_FLOATING_POINT_EXEPTION, },
	{ SIGSEGV, "SIGSEGV",  NULL, TR_ERR_SEGMENTATION_FAULT, },

	{ 0,        NULL,      NULL, NULL},
};
		
void tr_SignalTermination (int sig)
{
	int err = errno;
	int i;
	char *signame = "unnamed signal";
	char *logfmt  = TR_ERR_RECEIVED_SIGNAL;

	if (produceCore) {
#ifdef SIGBUS
		if (sig == SIGBUS)
			abort ();
#endif
#ifdef SIGSEGV
		if (sig == SIGSEGV)
			abort ();
#endif
	}
#ifdef MACHINE_LINUX
	signal(11, SIG_DFL);
#endif
	i = 0;
	do {
		if (user_sig_table[i].signum == sig)
			break;
	} while (user_sig_table[++i].signum);

	if (user_sig_table[i].signame)
		signame = user_sig_table[i].signame;
	
	if (user_sig_table[i].logfmt)
		logfmt = user_sig_table[i].logfmt;

	/*
	 *  If the specific handler returns 
	 *  TRUE, we ignore the signal.
	 */
	if (user_sig_table[i].sighandler)
		if ((*user_sig_table[i].sighandler)((double) sig, signame)) {
			errno = err;
			return;
		}

	/*
	 *  The return from generic handler is ignored,
	 *  for backwards compatibility
	 */
	bfUserSignal((double) sig, signame);

	tr_Fatal (logfmt, sig);

	/*
	 *  not reached. Fatal did exit(1)
	 */
	*(int*) 0 = 0;
}

#ifdef MACHINE_WNT
/*
 * 4.01/KP : LogoffHandler to catch CTRL_LOGOFF_EVENT's if we are
 * run as a service (this catches them in all cases but it does not
 * have any real effect if not launched by a service)
 */
BOOL WINAPI LogoffHandler(DWORD dwType)
{
	if (dwType == CTRL_LOGOFF_EVENT)
		return TRUE;

	return FALSE;
}
#endif

/*============================================================================
============================================================================*/
static void tr_CatchSignals ()
{
	/*
	 *	Leave some signals alone, in case
	 *	the user wants to get coredump.
	 *
	 *	Use sigaction to set handler,
	 *	so no need to set them again.
	 */
#ifdef SIGHUP
	tr_set_signal_handler (SIGHUP,   tr_SignalTermination);
#endif
	tr_set_signal_handler (SIGINT,   tr_SignalTermination);
	tr_set_signal_handler (SIGTERM,  tr_SignalTermination);
#ifdef SIGALRM
	tr_set_signal_handler (SIGALRM,  tr_SignalTermination);
#endif
#ifdef SIGBREAK
	tr_set_signal_handler (SIGBREAK, tr_SignalTermination);
#endif
#ifdef SIGQUIT
	if (!produceCore)
		tr_set_signal_handler (SIGQUIT, tr_SignalTermination);
#endif

	/*
	**  Error conditions.
	*/
	tr_set_signal_handler (SIGFPE, tr_SignalTermination);

#ifdef SIGBUS
	if (!produceCore)
		tr_set_signal_handler (SIGBUS, tr_SignalTermination);
#endif
	if (!produceCore)
		tr_set_signal_handler (SIGSEGV, tr_SignalTermination);

#ifdef MACHINE_WNT
    /*
	 * 4.01/KP : Set LogoffHandler to ignore logoff messages
	 * if we are launched from a service. Must be done after
	 * all signal() calls.
  	 */
	SetConsoleCtrlHandler(LogoffHandler, TRUE);
#endif
}

char *tr_GetEnv (char *p)
{
	char *tmp;

	if ((tmp = getenv (p)))
		return tr_MemPool (tmp);
	return TR_EMPTY;
}

static void tr_set_signal_handler(int sig, void (*handler)(int))
{
#ifdef MACHINE_MIPS
	signal(sig, handler);
#define XXX
#endif
#ifdef MACHINE_WNT
	signal(sig, handler);
#define XXX
#endif
#ifndef XXX
	struct sigaction sa, osa;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
#ifdef SA_INTERRUPT
	if (sig == SIGALRM)
		sa.sa_flags |= SA_INTERRUPT;
#endif
	sa.sa_handler = handler;
	sigaction(sig, &sa, &osa);
#endif
}

