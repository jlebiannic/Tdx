
#include "translator.h"


/* PART 0 */

char *tr_versioncntrl="@(#)TradeXpress RTE-executable";
static char *tr_mtr_version="\nTradeXpress mtr version: 4.0\n";
static char *tr_mktr_argv="\nTradeXpress mktr argv: mktr -g pgedisend.rte\n";
static char *tr_compilation_host="\nTradeXpress compilation host: VRT1LAP3102\n";
char *TR_SQLNULL = "";
void bail_out (char* fmt,...) {return;}
void tr_Startup (int argc, char **argv);
int tr_UserStatements();
void tr_Exit (int exitcode);

char *tr_GetProgName(){
return "pgedisend.rte";}

void  tr_GetProgInfo(){
}



static LogFieldHandle lfh_1_3 = { "INFO", 5, 0 };
static LogFieldHandle lfh_1_2 = { "STATUS", 5, &lfh_1_3 };
static LogFieldHandle lfh_1_1 = { "INDEX", 1, &lfh_1_2 };
LogSysHandle SYSLOG = { &lfh_1_1, 1, 0, 0, 0, 0, };

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
debughook(19,1);
debughook(20,0);
tr_lsCreate(&SYSLOG,tr_GetEnv("SYSLOG"));
debughook(21,0);
nIndex= tr_lsGetNumVal(&SYSLOG,&lfh_1_1);debugvarchg(&nIndex);
debughook(22,0);
tr_lsSetTextVal(&SYSLOG,&lfh_1_2,"Starting...");
debughook(23,0);
tr_FileReopen (tr_lsPath(&SYSLOG,"l"),stderr);
debughook(24,0);
tr_FileWrite(stdout,"%s%s%s","Creating INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0),TR_NEWLINE);
debughook(25,0);
tr_Print("%s%s%s","Creating INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0),TR_NEWLINE);
debughook(26,0);
tr_FileClose(tr_lsPath(&SYSLOG,"l"));
debughook(27,0);
tr_System(tr_BuildText("%s%s%s%s%s","./pginterm INDEX=",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE=\"","Pre processing...","\""));
debughook(28,0);
tr_FileReopen (tr_lsPath(&SYSLOG,"l"),stderr);
debughook(29,0);
tr_FileWrite(stdout,"%s%s","Apres appel pgintem",TR_NEWLINE);
debughook(30,0);
tr_Print("%s%s","Apres appel pgintem",TR_NEWLINE);
debughook(31,0);
tr_FileWrite(stdout,"%s%s%s%s","INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE 1",TR_NEWLINE);
debughook(32,0);
tr_Print("%s%s%s%s","INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE 1",TR_NEWLINE);
debughook(33,0);
tr_FileClose(tr_lsPath(&SYSLOG,"l"));
debughook(34,0);
tr_System(tr_BuildText("%s%s%s%s%s","./pginterm INDEX=",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE=\"","Translating...","\""));
debughook(35,0);
tr_FileReopen (tr_lsPath(&SYSLOG,"l"),stderr);
debughook(36,0);
tr_FileWrite(stdout,"%s%s","Apres appel pgintem",TR_NEWLINE);
debughook(37,0);
tr_Print("%s%s","Apres appel pgintem",TR_NEWLINE);
debughook(38,0);
tr_FileWrite(stdout,"%s%s%s%s","INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE 2",TR_NEWLINE);
debughook(39,0);
tr_Print("%s%s%s%s","INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE 2",TR_NEWLINE);
debughook(40,0);
tr_FileClose(tr_lsPath(&SYSLOG,"l"));
debughook(41,0);
tr_System(tr_BuildText("%s%s%s%s%s","./pginterm INDEX=",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE=\"","Making interchg...","\""));
debughook(42,0);
tr_FileReopen (tr_lsPath(&SYSLOG,"l"),stderr);
debughook(43,0);
tr_FileWrite(stdout,"%s%s","Apres appel pgintem",TR_NEWLINE);
debughook(44,0);
tr_Print("%s%s","Apres appel pgintem",TR_NEWLINE);
debughook(45,0);
tr_FileWrite(stdout,"%s%s%s%s","INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE 3",TR_NEWLINE);
debughook(46,0);
tr_Print("%s%s%s%s","INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE 3",TR_NEWLINE);
debughook(47,0);
tr_FileClose(tr_lsPath(&SYSLOG,"l"));
debughook(48,0);
tr_System(tr_BuildText("%s%s%s%s%s","./pginterm INDEX=",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE=\"","Transporting...","\""));
debughook(49,0);
tr_FileReopen (tr_lsPath(&SYSLOG,"l"),stderr);
debughook(50,0);
tr_FileWrite(stdout,"%s%s","Apres appel pgintem",TR_NEWLINE);
debughook(51,0);
tr_Print("%s%s","Apres appel pgintem",TR_NEWLINE);
debughook(52,0);
tr_lsReadEntry(&SYSLOG,nIndex);

debughook(53,0);
tr_lsSetTextVal(&SYSLOG,&lfh_1_3,"finished");
debughook(55,0);
tr_lsSetTextVal(&SYSLOG,&lfh_1_2,"OK");
return 0;
} /* tr_UserStatements() */




