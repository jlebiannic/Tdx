
#include "translator.h"


/* PART 0 */

char *tr_versioncntrl="@(#)TradeXpress RTE-executable";
static char *tr_mtr_version="\nTradeXpress mtr version: 3.11\n";
static char *tr_mktr_argv="\nTradeXpress mktr argv: mktr -c TstBaseAccess.rte\n";
static char *tr_compilation_host="\nTradeXpress compilation host: VRT1LAP3102\n";
char *TR_SQLNULL = "";
void bail_out (char* fmt,...) {return;}
void tr_Startup (int argc, char **argv);
int tr_UserStatements();
void tr_Exit (int exitcode);

char *tr_GetProgName(){
return "TstBaseAccess.rte";}

void  tr_GetProgInfo(){
}



static LogFieldHandle lfh_1_2 = { "NEW", 5, 0 };
static LogFieldHandle lfh_1_1 = { "PARTNER", 5, &lfh_1_2 };
LogSysHandle SYS = { &lfh_1_1, 1, 0, 0, 0, 0, };

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
debughook(3,1);
debughook(5,0);
tr_lsCreate(&SYS,tr_GetEnv("SYSLOG"));
debughook(6,0);
tr_lsFindFirst(&SYS,0,(void *)0,tr_GetEnv("SYSLOG"),&lfh_1_1,1,0,"ORDERS",&lfh_1_2,1,0,"BGM_1001",NULL);
debughook(8,0);
tr_FileReopen (tr_lsPath(&SYS,"l"),stderr);
debughook(11,0);
tr_Print("%s%s","PARTNERS ORDERS FOUND !",TR_NEWLINE);
return 0;
} /* tr_UserStatements() */




