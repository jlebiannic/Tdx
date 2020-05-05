
#include "translator.h"


/* PART 0 */

char *tr_versioncntrl="@(#)TradeXpress RTE-executable";
static char *tr_mtr_version="\nTradeXpress mtr version: 3.11\n";
static char *tr_mktr_argv="\nTradeXpress mktr argv: mktr -c TstPerf.rte\n";
static char *tr_compilation_host="\nTradeXpress compilation host: VRT1LAP3102\n";
char *TR_SQLNULL = "";
void bail_out (char* fmt,...) {return;}
void tr_Startup (int argc, char **argv);
int tr_UserStatements();
void tr_Exit (int exitcode);

char *tr_GetProgName(){
return "TstPerf.rte";}

void  tr_GetProgInfo(){
}



static LogFieldHandle lfh_1_3 = { "INDEX", 1, 0 };
static LogFieldHandle lfh_1_2 = { "USERNUM", 3, &lfh_1_3 };
static LogFieldHandle lfh_1_1 = { "INFO", 5, &lfh_1_2 };
LogSysHandle SYSLOG = { &lfh_1_1, 1, 0, 0, 0, 0, };

double nStep;
double nStepStart;
double nStart;
double nSeq;
double nI;
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
 "nStep", (void *)&nStep, 'N', 1,
 "nStepStart", (void *)&nStepStart, 'N', 1,
 "nStart", (void *)&nStart, 'N', 1,
 "nSeq", (void *)&nSeq, 'N', 1,
 "nI", (void *)&nI, 'N', 1,
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
debughook(2,1);
debughook(3,0);
nI= 1.0;debugvarchg(&nI);
debughook(6,0);
nSeq= 1.0;debugvarchg(&nSeq);
debughook(7,0);
nStart= tr_TextToNum(tr_GetTime(1,"now","%a"));debugvarchg(&nStart);
debughook(9,0);
nStepStart= nStart;debugvarchg(&nStepStart);
debughook(10,0);
tr_FileWrite(stdout,"%s%s%s","CREATION ",tr_GetTime(1,"now","%H:%M:%S"),TR_NEWLINE);
debughook(21,0);
while(nI <= 1.0){
debughook(12,0);
tr_lsCreate(&SYSLOG,tr_GetEnv("SYSLOG"));
debughook(13,0);
tr_lsSetTextVal(&SYSLOG,&lfh_1_1,tr_BuildText("%s%s","Test",tr_PrintNum(nI,"%05lf",0,0,0)));
debughook(14,0);
tr_lsSetNumVal(&SYSLOG,&lfh_1_2,nSeq);
debughook(15,0);
tr_lsWriteEntry(&SYSLOG);
debughook(16,0);
nSeq++;debugvarchg(&nSeq);
debughook(19,0);
if(nSeq > 10.0){
debughook(19,0);
nSeq= 1.0;debugvarchg(&nSeq);
}
debughook(21,0);
nI++;debugvarchg(&nI);
}
debughook(22,0);
nStep= tr_TextToNum(tr_GetTime(1,"now","%a"));debugvarchg(&nStep);
debughook(23,0);
tr_FileWrite(stdout,"%s%s%s%s","Durée:",tr_PrintNum(nStep - nStepStart,NULL,0,0,0)," s",TR_NEWLINE);
debughook(25,0);
nStepStart= nStep;debugvarchg(&nStepStart);
debughook(26,0);
tr_FileWrite(stdout,"%s%s%s","AFFICHAGE ",tr_GetTime(1,"now","%H:%M:%S"),TR_NEWLINE);
debughook(27,0);
tr_lsFindFirst(&SYSLOG,0,(void *)0,tr_GetEnv("SYSLOG"),NULL);
debughook(28,0);
tr_FileWrite(stdout,"%s%s","APRES FIND",TR_NEWLINE);
debughook(35,0);
while(tr_lsValidEntry(&SYSLOG)){
debughook(32,0);
tr_FileWrite(stdout,"%s%s","DANS WHILE",TR_NEWLINE);
debughook(33,0);
tr_Print("%s%s%s%s%s%s",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_3),NULL,0,0,0)," ",tr_lsGetTextVal(&SYSLOG,&lfh_1_1)," ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_2),NULL,0,0,0),TR_NEWLINE);
debughook(35,0);
tr_lsFindNext(&SYSLOG);
}
debughook(36,0);
tr_FileWrite(stdout,"%s%s","APRES ENDWHILE",TR_NEWLINE);
debughook(37,0);
nStep= tr_TextToNum(tr_GetTime(1,"now","%a"));debugvarchg(&nStep);
debughook(38,0);
tr_FileWrite(stdout,"%s%s%s%s","Durée:",tr_PrintNum(nStep - nStepStart,NULL,0,0,0)," s",TR_NEWLINE);
debughook(40,0);
nStepStart= nStep;debugvarchg(&nStepStart);
debughook(41,0);
tr_FileWrite(stdout,"%s%s%s","AFFICHAGE ",tr_GetTime(1,"now","%H:%M:%S"),TR_NEWLINE);
debughook(42,0);
tr_lsFindFirst(&SYSLOG,0,(void *)0,tr_GetEnv("SYSLOG"),&lfh_1_1,1,0,"Test0*14*",NULL);
debughook(48,0);
while(tr_lsValidEntry(&SYSLOG)){
debughook(44,0);
tr_FileWrite(tr_GetFp(tr_lsPath(&SYSLOG,"l"),"w"),"%s%s%s%s%s%s",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_3),NULL,0,0,0)," ",tr_lsGetTextVal(&SYSLOG,&lfh_1_1)," ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_2),NULL,0,0,0),TR_NEWLINE);
debughook(46,0);
tr_FileClose(tr_lsPath(&SYSLOG,"l"));
debughook(48,0);
tr_lsFindNext(&SYSLOG);
}
debughook(49,0);
nStep= tr_TextToNum(tr_GetTime(1,"now","%a"));debugvarchg(&nStep);
debughook(50,0);
tr_FileWrite(stdout,"%s%s%s%s","Durée:",tr_PrintNum(nStep - nStepStart,NULL,0,0,0)," s",TR_NEWLINE);
debughook(51,0);
tr_FileWrite(stdout,"%s%s%s","FIN ",tr_GetTime(1,"now","%H:%M:%S"),TR_NEWLINE);
debughook(53,0);
tr_FileWrite(stdout,"%s%s%s%s","Durée Totale:",tr_PrintNum(nStep - nStart,NULL,0,0,0)," s",TR_NEWLINE);
return 0;
} /* tr_UserStatements() */




