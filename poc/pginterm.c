
#include "translator.h"


/* PART 0 */

char *tr_versioncntrl="@(#)TradeXpress RTE-executable";
static char *tr_mtr_version="\nTradeXpress mtr version: 3.11\n";
static char *tr_mktr_argv="\nTradeXpress mktr argv: mktr -c pginterm.rte\n";
static char *tr_compilation_host="\nTradeXpress compilation host: VRT1LAP3102\n";
char *TR_SQLNULL = "";
void bail_out (char* fmt,...) {return;}
void tr_Startup (int argc, char **argv);
int tr_UserStatements();
void tr_Exit (int exitcode);

char *tr_GetProgName(){
return "pginterm.rte";}

void  tr_GetProgInfo(){
  printf(" pginterm INDEX=numindex STAGE=num\n");
  printf("   s'accroche à la syslog INDEX et ajoute à l'extension l de cette entrée plusieurs lignes dont la valeur STAGE\n");
  printf("   est appelé par pgedisend pour simuler des étappes de l'edisend\n");
}



static LogFieldHandle lfh_1_2 = { "STATUS", 5, 0 };
static LogFieldHandle lfh_1_1 = { "INDEX", 1, &lfh_1_2 };
LogSysHandle SYSLOG = { &lfh_1_1, 1, 0, 0, 0, 0, };

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
debughook(11,1);
debughook(12,0);
tr_lsCreate(&SYSLOG,tr_GetEnv("SYSLOG"));
debughook(13,0);
tr_lsRemove(&SYSLOG);
debughook(14,0);
tr_lsFindFirst(&SYSLOG,0,(void *)0,tr_GetEnv("SYSLOG"),&lfh_1_1,1,1,(double)tr_TextToNum(tr_am_textget(taPARAMETERS,"INDEX")),NULL);
debughook(28,0);
if(tr_lsValidEntry(&SYSLOG)){
debughook(16,0);
tr_lsSetTextVal(&SYSLOG,&lfh_1_2,tr_am_textget(taPARAMETERS,"STAGE"));
debughook(17,0);
tr_FileReopen (tr_lsPath(&SYSLOG,"l"),stderr);
debughook(18,0);
tr_FileWrite(stdout,"%s%s%s%s%s","Attaching INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE ",tr_am_textget(taPARAMETERS,"STAGE"),TR_NEWLINE);
debughook(19,0);
tr_Print("%s%s%s%s%s","Attaching INDEX ",tr_PrintNum(tr_lsGetNumVal(&SYSLOG,&lfh_1_1),NULL,0,0,0)," STAGE ",tr_am_textget(taPARAMETERS,"STAGE"),TR_NEWLINE);
debughook(20,0);
tr_Print("%s%s","Action One",TR_NEWLINE);
debughook(21,0);
tr_Print("%s%s","Action Two",TR_NEWLINE);
debughook(22,0);
tr_Print("%s%s","Action Three",TR_NEWLINE);
debughook(25,0);
if((tr_strcmp(tr_GetEnv("SLEEP"),TR_EMPTY))){
debughook(25,0);
tr_System(tr_BuildText("%s%s","sleep ",tr_GetEnv("SLEEP")));
}
debughook(27,0);
tr_FileClose(tr_lsPath(&SYSLOG,"l"));
}
return 0;
} /* tr_UserStatements() */




