#ifndef __TR_PROTOTYPES_H__
#define __TR_PROTOTYPES_H__
/* "@(#)TradeXpress tr_prototypes.h" */
/*======================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:	tr_prototypes.h

========================================================================
 3.00 08.05.95/JN	tr_MemPoolPass().
 3.01 06.02.96/JN	dirent.h was unnecessary. gcounter protos.
 3.02 21.02.96/JN	arguments to setvalue were asking for trouble.
 3.03 09.02.98/HT	fix some predefines to match with actual functions.
 3.04 09.09.98/KP	Added tfGetUniq()
 4.00 21.09.99/JH	tr_amelemcopy() added.
 4.01 09.02.00/KP	nfXMLFetchElementValues()
 4.02 20.03.00/KP	4th argument for nfXMLFetchElementValues()
			(separator)
 4.03 08.11.05/LM	Added bfSetCounter
 4.04 05.03.07/AVD 	Add int pointer parameter in tr_strcpick() for gestion
			of text's position in case of analyse error
 5.00 05.02.09/LM	User's statistics functions BugZ 8688
 5.01 06.03.2009/LME	Exception stack printed on stderr BugZ 9057
 5.02 29.07.2009/LME	Exception bugZ 9252 fixed
 5.03 30.08.2012/SCh(CG) Add of tr_getcounterlimited()
 TX-3123 - 05.2019 - Olivier REMAUD - UNICODE adaptation
 TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
 Jira TX-3123 24.09.2019 - Olivier REMAUD - UNICODE adaptation
 Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
======================================================================*/

#include <stdio.h>
#include <sys/types.h>
#include <setjmp.h>
#include "tr_logsystem.h"

/*
 *	Error texts, indexed everywhere relative to TREB
 */
struct tr_errtable {
	char *text;
	int code;
};
extern struct tr_errtable tr_errs[];
#define TREB 0

#define TRE_UNDEFINED				0
#define TRE_EXCESS_COMPONENTS			1
#define TRE_TRUNCATED_SEGMENT			2
#define TRE_TOO_SHORT				3
#define TRE_TOO_MANY_GROUPS			4
#define TRE_TOO_MANY_SEGMENTS			5
#define TRE_TOO_LONG				6
#define TRE_TOO_FEW_GROUPS			7
#define TRE_TOO_FEW_SEGMENTS			8
#define TRE_CHARSET_NOT_FOUND			9
#define TRE_UNEXPECTED_EOF			10
#define TRE_MISSING_ELEMENT			11
#define TRE_MISSING_COMPONENT			12
#define TRE_MISSING_MANDATORY_ELEMENT		13
#define TRE_MISSING_MANDATORY_COMPONENT		14
#define TRE_REQUEST_UNDERRUN			15
#define TRE_NO_PLACE_FOR_SEGMENT		16
#define TRE_EXCESS_DATA_IN_SEGMENT		17
#define TRE_IO_ERROR				18
#define TRE_UNKNOWN_DATATYPE			19
#define TRE_CORRUPT_CHARSET			20
#define TRE_MESSAGE_WITH_ERROR			21
#define TRE_SEGMENT_WITH_ERROR			22
#define TRE_ELEMENT_WITH_ERROR			23
#define TRE_JUNK_OUTSIDE_MESSAGE		24
#define TRE_JUNK_INSIDE_MESSAGE			25
#define TRE_SEGMENT_OVERFLOW			26
#define TRE_GROUP_OVERFLOW			27
#define TRE_XERCES_MESSAGE			28
#define TRE_XERCES_SCHEMA			100


void tr_Startup (int argc, char **argv);
void tr_Exit (int exitcode);
void tr_GetProgInfo();
void tr_MessageDiag (int mode, FILE *fp);

int bfMktemp(char *data_file);
char *tfTempnam(char *psDir, char *psPfx);

void TXStartTimer(char *zemsg);
void TXSplitTime(char *zemsg);

int tr_FileRemove(char *path);
int tr_PrintFile (char *filename, FILE *fpOut);
int tr_CopyFile (char *source, char *destination);
int tr_MoveFile (char *source, char *destination);
int tr_LinkFile (char *source, char *destination);

int tr_wildmat(char *s, char *w);

void tr_Put (char *arrayname, double line, double pos, char *value);
void tr_Flush (char *arrayname, double minCol, double maxCol, char *separator, FILE *fp, int keep);

char *tr_GetNextLine(int);
char *	tr_Pick (double line, double pos, double len, int pickEOL);
char *	tr_Substr(char *s, double pos, double len, int pickEOT);
double tr_Split (char *text, char *array, char *separator, char *delimitor, char *escape);
double	tr_REsplit (char *, char *, void *);
void tr_EatUp ();

extern double tr_errorLevel;
void	tr_Fatal ( char *, ... ); /* varargs->stdarg */
void	tr_Log ( char *, ... ); /* varargs->stdarg */
void tr_OutOfMemory();  /* exit on error */

void tr_Execvp(char *command, char *arrayname);
double tr_Spawnvp(char *command, char *arrayname);
double tr_Background(char *command, char *arrayname, char *input, char *output, char *log);

double tr_Spawnlp ( char *, ... ); /* varargs->stdarg */
void tr_Execlp ( char *, ... ); /* varargs->stdarg */

char *	tr_strctok ();
int tr_strcmp (char *s, char *t);

char *	tr_textopt ();	/* varargs */
char *tr_strtoupr (char *string);
char *tr_strtolwr (char *string);
char *	tr_strtoken ();
char *	tr_strsplit ();
double tr_TextToNum (char *text);

void tr_Print ( char *args, ... );
char *tr_PrintNum (double num, char *format, int starcount, int w1, int w2);
char *	tr_PrintTime ();
/* 3.03/HT   time_t	tr_parsetime (); */
void	tr_parsetime ( char *, time_t * );
char *	tr_timestring ( char *, time_t );

double tr_Load (int loadMode, char *filename, char *arrayname, char *multiseps, char *seps);

int tr_Save(char *arrayname, FILE *fp);
int tr_SaveParameters ();

char *tr_am_textget(char *table, char *name);
void	tr_am_textset(char *table, char *name, char *txt);
double tr_am_numget(char *table, char *name);
void tr_am_numset(char *table, char *name, double num);
void tr_am_rewind2(char *table, void **handle);
int tr_am_getkey2(void **handle, char **idxp);
int tr_am_textremove(char *table);
int tr_am_numremove(char *table);

int tr_amtextcopy(char *to, char *from);
int tr_amnumcopy(char *to, char *from);
int tr_amelemcopy(char *to, char *firstelem, char **element, int index,
			double syntax, double composite, double component, void *segAddr);

char *	tr_am_firstkey ();
char *	tr_am_nextkey ();
int	tr_am_walk ();
void	tr_am_remove_completely (char *);
int tr_am_count(char *table);

char *	tr_TextOpt ( char *, ... ); /* varargs->stdarg */

char *	tr_strctok ();
char *	tr_BuildText ( char *, ... ); /* varargs->stdarg */

double tr_FileClose(char *filename);
char *	tr_FileRead ();
void tr_FileWrite ( FILE *, char *, ... ); /* varargs->stdarg */
FILE * tr_GetFp(char *filename, char *accmode);

char *	tr_CommRead ();

int tr_isempty (char *text);

char *tr_codeconversion(char *name, int keycol, char *key, int valuecol, char *ifs, int dummy);

char *tr_strtextget (char *string, double index);
int tr_strtextset (char *string, double index, char *value);

char *	tr_GetTime ( int args, ... );	/* varargs->stdarg */
char *tr_LastMemPoolValue ();
int 	tr_GetFunctionMemPtr();
void	tr_CollectFunctionGarbage(int);


/* proper declarations with parameters of memory functions */
void tr_berzerk(char *s, ... );
char *tr_MemPool(char *s);
char *tr_MemPoolPass(char *s);
int tr_MemPoolFree (char *s);
int tr_MemPoolRemove(char *s, int withFree);
void tr_GarbageCollection();
char *tr_ShowSep(unsigned char sepchar);
void tr_Assign(char **var, char *value);
void tr_AssignEmpty(char **var);
void tr_ElemAssign(char **var, char *value, int content);
char *tr_strvadup(char *s, ...);
char *tr_saprintf(char *fmt, ...);
char *tr_savprintf(char *fmt, va_list ap);
void tr_FreeElemList (char **list, int count);

#if !defined(DEBUG) || !defined(MEM_DEBUG) || defined(TR_MEMORY)

char *tr_strdup(char *s);
char *tr_strndup(char *s, int len);
void *tr_malloc(int size);
void *tr_zalloc(int size);
void *tr_calloc(int n, int size);
void *tr_realloc(void *p, int size);
void tr_free(void *p);
#endif

#if defined(DEBUG) && (MEM_DEBUG==1)
char *TDXMemDebug_tr_strdup (char *s, int line_strdup, char *file_strdup);
char *TDXMemDebug_tr_strndup(char *s, int len, int line_strndup, char *file_strndup);
void *TDXMemDebug_tr_malloc (int size, int line_malloc, char *file_malloc);
void *TDXMemDebug_tr_zalloc (int size, int line_malloc, char *file_malloc);
void *TDXMemDebug_tr_calloc (int n, int size, int line_malloc, char *file_malloc);
void *TDXMemDebug_tr_realloc(void *p, int size, int line_malloc, char *file_malloc);
void  TDXMemDebug_tr_free   (void *p, int line_malloc, char *file_malloc);
#if !defined(TR_MEMORY)
#define tr_strdup(s)       TDXMemDebug_tr_strdup (s,__LINE__,__FILE__)
#define tr_strndup(s,l)    TDXMemDebug_tr_strdup (s,l,__LINE__,__FILE__)
#define tr_malloc(size)    TDXMemDebug_tr_malloc (size,__LINE__,__FILE__)
#define tr_zalloc(size)    TDXMemDebug_tr_zalloc (size,__LINE__,__FILE__)
#define tr_calloc(n,size)  TDXMemDebug_tr_calloc (n,size,__LINE__,__FILE__)
#define tr_realloc(p,size) TDXMemDebug_tr_realloc(p,size,__LINE__,__FILE__)
#define tr_free(p)         TDXMemDebug_tr_free   (p,__LINE__,__FILE__)
#endif
#include "localtools/tdxmemdebug.h"
#endif

char *	tr_IntIndex (int);
char *	tr_MakeIndex (double);
char *	tr_GetEnv ();
double	tr_Divide(double value, double divisor);
double	tr_Length (char *text);
double	tr_Index(char *s, char *t);
double	tr_IndexAfter (char *s, char *t, double d_offset);

double	tr_getcounter        (char *name);
double	tr_getcounter_with_dir  (char *cptdir, char *name);
double	tr_getcounterNoIncrement(char *name);
#ifdef MACHINE_WNT
double	tr_getcounterlimitedInc (char *name, double max, double increment);
double	tr_getcounterlimited (char *name, double max);
#endif
double	tr_set_gcounter      (char *name, double value);

double	tr_get_gcounter (char *);
double	tr_inc_gcounter (char *);
double	tr_dec_gcounter (char *);
double	tr_add_gcounter (char *, double value);

void *tr_OpenDir(char *original, char **path, char **search);
void tr_CloseDir(void *dirp);
int tr_ReadDir2(void *dirp, char *path, char *search, char **found);
char *tr_FileName(char *filename);
char *tr_FileFullName(char *filename);
char *tr_FilePath(char *filename);
time_t tr_FileAccessTime (char *filename);
time_t tr_FileModifiedTime (char *filename);
time_t tr_FileStatusChangeTime (char *filename);
double tr_FileSize(char *filename);
char *tr_FileOwner(char *filename);
char *tr_FileType(char *filename);
char *tr_FileContent(char *filename);
double tr_FileLineCount(char *filename);
int tr_FileExist(char *filename);
int tr_FileAccess(char *filename, int type);
int tr_FileReopen (char *filename, FILE *fp);

char *tr_FileGroup(char *filename);
char *tfTempnam(char *dir, char *prefix);

char *tr_AdjustDS(char *value);
char *tr_strip (char *s, char *garbage);
char *tr_replace (char *s, char *was, char *tobe);
char *tr_peel (char *s, char *garbage);

void tr_am_dbremove( LogSysHandle *, char * );
void tr_amdbparmcopy( LogSysHandle *, char *, LogSysHandle *, char * );
void tr_am_dbparmset( LogSysHandle *, char *, char *, char * );
char *tr_am_dbparmget( LogSysHandle *, char *, char * );
void tr_PrintParms( LogSysHandle *, char *, FILE * );
int tr_am_dbvalid( LogSysHandle *, char * );
double tr_CopyArr( int, LogSysHandle *, char *, LogSysHandle *, char *, char *, char * );

int bfComparePaths( char *, char * );

char *	tfGetUniq( char *, char *, char *, char *);
char *	tfGetUniqEx( LogSysHandle *, char *, char *, char *, char *);

int tr_Switch ( char *args, ... ); /* varargs->stdarg */

void tr_LocalTabName (const void *, char *); /* varargs->stdarg */

/* 4.01/KP */
double 	nfXMLFetchElementValues(char *, char *, char *, char *);

int bfSetCounter(char *name, double value);

int tr_MatchBufHit(register int indx, register char *text);
int tr_MatchBufHitEOL(char *text);
int tr_MatchBufRE(register void *re);

/* exception handling */
jmp_buf *pushExceptionAddress (void);
jmp_buf * popExceptionAddress(void);
void popExceptionInfosStack(void);
char * tr_getLastExceptionClass(void);
char * tr_getLastExceptionReason(void);
char * tr_getLastExceptionMessage(void);
double tr_getLastExceptionCode(void);
void raisexcept(char *classe, char *reason, char *message, double code);
int tr_matchException( char *args, ...);
int bfPrintExceptionStack();
int bfLogExceptionStack();

/* 4.05 /LM Statistics functions */
/* Public */
int bfStatEnveloppe(char *, char *, char *, char *, char *, char *, double, double, double, double, char *);
int bfStatMessage(char *,char *, char *, char *, char *, char *, char *, char *, double);
int bfStatMsgBusinessData(char *, char *, char *);
int bfStatMsgProcessInfos(char *, char *, char *, char *, char *, char *);
char * tfStatFlatMsgProcessInfos(char *, char *, char *, char *, char *, char *);
char * tfStatFlatMsgBusinessData(char *, char *, char *, char *);
int bfStatFlatNewEnveloppe(char *);

#ifdef MACHINE_WNT
#define DIRSEP '\\'
#else
#define DIRSEP '/'
#endif

/* tfSubstituteEnv
 * Return a string where all environment variables are substituted by their content.
 * string : the string to parse
 * mask : a bit mask to disable some type of var.
 *   ENV_VAR : allow default variable $envname $ENVNAME
 *   ENV_PAREN : allow parentheses $(envname)
 *   ENV_BRACE : allow parentheses ${envname}
 *   ENV_TIME :$% return time in 8 digits, hex
 *   ENV_PID : $$ return our PID, 5 digits
 */
#define ENV_VAR   1
#define ENV_PAREN 2
#define ENV_BRACE 4
#define ENV_TIME 8
#define ENV_PID 16
#define ENV_ALL (ENV_VAR | ENV_PAREN | ENV_BRACE | ENV_TIME | ENV_PID)

char *tfSubstituteEnv(char *string, double mask);
double nfExecCommand(char  *cmdline, double index, char  *filename, char  *parfile);

double tr_System (char *command);

int bfUserSignal (double signal, char *signame);
int bfUserSIGINT();
int bfUserSIGTERM();

int tr_SeparateString(char **sp, char *formseps, char **token);
int tr_SegRoutineDefault(char *segname, char **segdata);
int tr_out_SegRoutineDefault(char *segname, char **segdata);
int tr_in_SegRoutineDefault(char *segname, char **segdata);

void tr_DependencyError (char *name, char *dep, int type);
int bfDependencyError(char *name, char *dependency);
int tr_CheckMsgDep (char **data, char *segName, char *grpList);
int bfCheckmsgdep();
int tr_FindRunTimeSegment (char **data, char *segName, int segRepetition, char *grpList);
int bfIncomingError();
void tr_PackElems(char **elements, int start, int tail, int size, int count, int is_elem);

int tr_PrintMessage (FILE *fp);
int tr_SegmentDiagnostic(int mode, FILE *fp);
int tr_WriteSegmentDiagnostic(int mode, FILE *fp);

int bfSetCharset(char *tCharset, char *tCharPath);
int bfSetOutputCharset(char *tCharset, char *tCharPath);
int bfSetInputCharset(char *tCharset, char *tCharPath);

int bfSleep(double nsec);
int tr_LoadDefaultFile (char *filename);

/* Encoding related utilities */
int tr_UseLocaleUTF8();
int tr_mbLength (char *mbText);
char* tr_mbFgets(char *buffer, size_t buffsz, FILE* stream);
int tr_mbFnputs(char *string, FILE* stream, size_t maxCount);
int tr_mbFputs(char *string, FILE* stream);
int tr_mbFputsNL(char *string, FILE* stream);
void tr_SetIconvInfo( FILE* fh, int padding, char *encoding, void* converter );
void *tr_GetIconvInfo( FILE* fh, int *initd, int *padding, char **encoding );
void tr_FreeIconvInfo( void *converter );
int tr_mbSkipChars( size_t *n, char *ptr );
char* tr_mbStrChr( char *string, char *uniqueCharToFind );
char* tr_mbStrToLower( char *string );
char* tr_mbStrToUpper( char *string );
char* tr_mbToLower( char *string );
char* tr_mbToUpper( char *string );
int tr_mbFprintf( FILE *stream, char *format, ... );
int tr_mbVfprintf( FILE *stream, char *format, va_list ap );
int  tr_stripRC( char *text );

#endif
