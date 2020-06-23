
#include "translator.h"


/* PART 0 */

char *tr_versioncntrl="@(#)TradeXpress RTE-executable";
static char *tr_mtr_version="\nTradeXpress mtr version: 4.0\n";
static char *tr_mktr_argv="\nTradeXpress mktr argv: mktr -g tus_dao.rte\n";
static char *tr_compilation_host="\nTradeXpress compilation host: VRT1LAP3102\n";
char *TR_SQLNULL = "";
void bail_out (char* fmt,...) {return;}
void tr_Startup (int argc, char **argv);
int tr_UserStatements();
void tr_Exit (int exitcode);

char *tr_GetProgName(){
return "tus_dao.rte";}

void  tr_GetProgInfo(){
}



static LogFieldHandle lfh_1_3 = { "VAL", 3, 0 };
static LogFieldHandle lfh_1_2 = { "STATUS", 5, &lfh_1_3 };
static LogFieldHandle lfh_1_1 = { "INDEX", 1, &lfh_1_2 };
LogSysHandle TUDAO = { &lfh_1_1, 1, 0, 0, 0, 0, };

double nfError();
int bfIsValid();
double nIndex;
char taARGUMENTS[] = "0";
char taFORMSEPS[] = "1";
char taWRITEBUFFER[] = "2";
char taPARAMETERS[] = "3";
char naGRP[] = "4";
static void tr_InitStrings(){
	static int initialized = 0;
	if (!initialized) {
		initialized = 1;
	}
}
struct {
			char *name;
			void *address;
			char type, editable;
		} tr_vars[] = {
	"RIVI", (void *)&TR_LINE, 'N', 0,
 "nIndex", (void *)&nIndex, 'N', 1,
 "taARGUMENTS", (void **)taARGUMENTS, 't', 1,
 "taFORMSEPS", (void **)taFORMSEPS, 't', 1,
 "taWRITEBUFFER", (void **)taWRITEBUFFER, 't', 1,
 "taPARAMETERS", (void **)taPARAMETERS, 't', 1,
 "naGRP", (void **)naGRP, 'n', 1,
 NULL, (void *)NULL, '0'
};

#ifndef debughook
#define debughook(l,w) tr_sourceLine = l
#endif
#ifndef debugvarchg
#define debugvarchg(x)  /* nothing */
#endif
#ifndef debugenter
#define debugenter(f,p) /* nothing */
#endif
#ifndef debugleave
#define debugleave()    /* nothing */
#endif


/* PART 1 */

int main(int argc, char **argv)
{
	tr_Startup(argc, argv);
	tr_InitStrings();
	tr_UserStatements();
	tr_Exit(0);
}
int tr_UserStatements()
{
if ( tr_am_textget(taPARAMETERS,"SQL_NULL") == TR_EMPTY )
{
 TR_SQLNULL = TR_EMPTY;
 }
else
 {
 TR_SQLNULL = tr_strdup(tr_am_textget(taPARAMETERS,"SQL_NULL"));
 }
debughook(8,1);
debughook(10,0);
tr_FileWrite(stdout,"%s%s%s","Create entry INDEX ",tr_PrintNum(nIndex,NULL,0,0,0),TR_NEWLINE);
debughook(11,0);
tr_lsCreate(&TUDAO,tr_GetEnv("TUDAO"));
debughook(12,0);
nIndex= tr_lsGetNumVal(&TUDAO,&lfh_1_1);debugvarchg(&nIndex);
debughook(14,0);
bfIsValid(tr_BuildText("%s%s","After new entry index ",tr_PrintNum(nIndex,NULL,0,0,0)));
debughook(15,0);
tr_FileWrite(stdout,"%s%s%s","Find entry INDEX ",tr_PrintNum(nIndex,NULL,0,0,0),TR_NEWLINE);
debughook(16,0);
tr_lsFindFirst(&TUDAO,0,(void *)0,tr_GetEnv("TUDAO"),&lfh_1_1,1,1,(double)nIndex,NULL);
debughook(18,0);
bfIsValid(tr_BuildText("%s%s","After find by index ",tr_PrintNum(nIndex,NULL,0,0,0)));
debughook(19,0);
tr_FileWrite(stdout,"%s%s%s","Update entry INDEX ",tr_PrintNum(nIndex,NULL,0,0,0),TR_NEWLINE);
debughook(20,0);
tr_lsSetTextVal(&TUDAO,&lfh_1_2,"Modified");
debughook(22,0);
tr_lsSetNumVal(&TUDAO,&lfh_1_3,123.456);
debughook(23,0);
tr_FileWrite(stdout,"%s%s%s","Update entry INDEX ",tr_PrintNum(nIndex,NULL,0,0,0),TR_NEWLINE);
debughook(24,0);
tr_lsSetTextVal(&TUDAO,&lfh_1_2,"Modified");
debughook(26,0);
tr_lsSetNumVal(&TUDAO,&lfh_1_3,123.456);
debughook(27,0);
tr_FileWrite(stdout,"%s%s%s","Find entry after update INDEX ",tr_PrintNum(nIndex,NULL,0,0,0),TR_NEWLINE);
debughook(28,0);
tr_lsFindFirst(&TUDAO,0,(void *)0,tr_GetEnv("TUDAO"),&lfh_1_1,1,1,(double)nIndex,NULL);
debughook(30,0);
bfIsValid(tr_BuildText("%s%s","After find by index ",tr_PrintNum(nIndex,NULL,0,0,0)));
debughook(36,0);
if((tr_strcmp(tr_lsGetTextVal(&TUDAO,&lfh_1_2),"Modified") || tr_lsGetNumVal(&TUDAO,&lfh_1_3) != tr_TextToNum("123.456"))){
debughook(32,0);
nfError("Update","values for STATUS and/or VAL are incorrect");
}else{
debughook(35,0);
tr_FileWrite(stdout,"%s%s","[OK] Update status and val",TR_NEWLINE);
}
debughook(37,0);
tr_FileWrite(stdout,"%s%s%s","Create 2nd entry INDEX ",tr_PrintNum(nIndex,NULL,0,0,0),TR_NEWLINE);
debughook(38,0);
tr_lsCreate(&TUDAO,tr_GetEnv("TUDAO"));
debughook(39,0);
nIndex= tr_lsGetNumVal(&TUDAO,&lfh_1_1);debugvarchg(&nIndex);
debughook(41,0);
bfIsValid(tr_BuildText("%s%s","After new entry index ",tr_PrintNum(nIndex,NULL,0,0,0)));
debughook(42,0);
tr_FileWrite(stdout,"%s%s%s","Update entry INDEX ",tr_PrintNum(nIndex,NULL,0,0,0),TR_NEWLINE);
debughook(43,0);
tr_lsSetTextVal(&TUDAO,&lfh_1_2,"Modified");
debughook(45,0);
tr_lsSetNumVal(&TUDAO,&lfh_1_3,tr_TextToNum("789.01"));
debughook(46,0);
tr_FileWrite(stdout,"%s%s%s","Find entry after update INDEX ",tr_PrintNum(nIndex,NULL,0,0,0),TR_NEWLINE);
debughook(47,0);
tr_lsFindFirst(&TUDAO,0,(void *)0,tr_GetEnv("TUDAO"),&lfh_1_1,1,1,(double)nIndex,NULL);
debughook(49,0);
bfIsValid(tr_BuildText("%s%s","After find by index ",tr_PrintNum(nIndex,NULL,0,0,0)));
debughook(55,0);
if((tr_strcmp(tr_lsGetTextVal(&TUDAO,&lfh_1_2),"Modified") || tr_lsGetNumVal(&TUDAO,&lfh_1_3) != tr_TextToNum("789.01"))){
debughook(51,0);
nfError("Update","values for STATUS and/or VAL are incorrect");
}else{
debughook(54,0);
tr_FileWrite(stdout,"%s%s","[OK] Update status and val",TR_NEWLINE);
}
debughook(56,0);
tr_FileWrite(stdout,"%s%s%s","Find entry after update INDEX ",tr_PrintNum(nIndex,NULL,0,0,0),TR_NEWLINE);
debughook(57,0);
tr_lsFindFirst(&TUDAO,0,(void *)0,tr_GetEnv("TUDAO"),&lfh_1_2,1,0,"Modified",&lfh_1_3,6,1,(double)tr_TextToNum("123"),NULL);
debughook(59,0);
bfIsValid(tr_BuildText("%s%s","After find with two parameters ",tr_PrintNum(nIndex,NULL,0,0,0)));
debughook(64,0);
if(tr_lsFindFirst(&TUDAO,0,(void *)0,tr_GetEnv("TUDAO"),NULL))do{
debughook(62,0);
tr_FileWrite(stdout,"%s%s%s%s%s%s%s","found INDEX ",tr_PrintNum(tr_lsGetNumVal(&TUDAO,&lfh_1_1),NULL,0,0,0)," STATUS=",tr_lsGetTextVal(&TUDAO,&lfh_1_2),"VAL=",tr_PrintNum(tr_lsGetNumVal(&TUDAO,&lfh_1_3),NULL,0,0,0),TR_NEWLINE);
}while(tr_lsFindNext(&TUDAO));
return 0;
} /* tr_UserStatements() */





/* PART 2 */

int bfIsValid(char *tAction){
int _tAction;
int _localretval;
int _localmemptr;


_tAction = tr_MemPoolRemove(tAction, 0);
_localmemptr = tr_GetFunctionMemPtr();
debugenter("bfIsValid",("tAction",&tAction,NULL));
debughook(65,1);
debughook(74,0);
if(tr_lsValidEntry(&TUDAO)){
debughook(68,0);
tr_FileWrite(stdout,"%s%s%s%s","[OK] ",tAction," Handle table valid",TR_NEWLINE);
debughook(69,0);
_localretval = 1;
if (_tAction) tr_free(tAction);
tr_CollectFunctionGarbage(_localmemptr);

debugleave();return _localretval;
}else{
debughook(71,0);
nfError(tAction," Handle table invalide");
debughook(73,0);
_localretval = 0;
if (_tAction) tr_free(tAction);
tr_CollectFunctionGarbage(_localmemptr);

debugleave();return _localretval;
}
if (_tAction) tr_free(tAction);
tr_CollectFunctionGarbage(_localmemptr);
return (int ) 0L;
debugleave();
}

double nfError(char *tAction,char *tMessage){
int _tMessage;
int _tAction;
double _localretval;
int _localmemptr;


_tMessage = tr_MemPoolRemove(tMessage, 0);
_tAction = tr_MemPoolRemove(tAction, 0);
_localmemptr = tr_GetFunctionMemPtr();
debugenter("nfError",("tAction",&tAction,"tMessage",&tMessage,NULL));
debughook(75,1);
debughook(77,0);
tr_FileWrite(stdout,"%s%s%s%s%s","[ERROR] ",tAction,": ",tMessage,TR_NEWLINE);
debughook(79,0);
tr_Exit((int)1.0);
if (_tMessage) tr_free(tMessage);
if (_tAction) tr_free(tAction);
tr_CollectFunctionGarbage(_localmemptr);
return (double ) 0L;
debugleave();
}




