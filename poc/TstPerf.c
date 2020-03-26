#include "translator.h"


/* PART 0 */

char *tr_versioncntrl = "@(#)TradeXpress RTE-executable";
static char *tr_mtr_version = "\nTradeXpress mtr version: 3.11\n";
static char *tr_mktr_argv = "\nTradeXpress mktr argv: mktr -c TstPerf.rte\n";
static char *tr_compilation_host = "\nTradeXpress compilation host: VRT1LAP3102\n";
char *TR_SQLNULL = "";
void bail_out(char *fmt, ...) {
	return;
}
void tr_Startup(int argc, char **argv);
int tr_UserStatements();
void tr_Exit(int exitcode);

char* tr_GetProgName() {
	return "TstPerf.rte";
}

void tr_GetProgInfo() {
}

static LogFieldHandle lfh_1_3 = { "INDEX", 1, 0 };
static LogFieldHandle lfh_1_2 = { "USERNUM", 3, &lfh_1_3 };
static LogFieldHandle lfh_1_1 = { "INFO", 5, &lfh_1_2 };
LogSysHandle SYSLOG = { &lfh_1_1, 1, 0, 0, 0, 0, };

double nSeq;
double nI;
char taARGUMENTS[] = "0";
char taFORMSEPS[] = "1";
char taWRITEBUFFER[] = "2";
char taPARAMETERS[] = "3";
char naGRP[] = "4";
static void tr_InitStrings() {
	static int initialized = 0;
	if (!initialized) {
		initialized = 1;
	}
}
struct {
	char *name;
	void *address;
	char type, editable;
} tr_vars[] = { "RIVI", (void*) &TR_LINE, 'N', 0, "nSeq", (void*) &nSeq, 'N', 1, "nI", (void*) &nI, 'N', 1, "taARGUMENTS", (void**) taARGUMENTS, 't', 1, "taFORMSEPS", (void**) taFORMSEPS, 't', 1,
		"taWRITEBUFFER", (void**) taWRITEBUFFER, 't', 1, "taPARAMETERS", (void**) taPARAMETERS, 't', 1, "naGRP", (void**) naGRP, 'n', 1, NULL, (void*) NULL, '0' };

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

int main(int argc, char **argv) {
	tr_Startup(argc, argv);
	tr_InitStrings();
	tr_UserStatements();
	tr_Exit(0);
}
int tr_UserStatements() {
	debughook(2, 1);
	debughook(3, 0);
	nI = 1.0;
	debugvarchg(&nI);
	debughook(4, 0);
	nSeq = 1.0;
	debugvarchg(&nSeq);
	debughook(5, 0);
	tr_FileWrite(stdout, "%s%s", "CREATION", TR_NEWLINE);
	debughook(16, 0);
	while (nI <= 5000.0) {
		debughook(7, 0);
		tr_lsCreate(&SYSLOG, "/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/databases/syslog");
		debughook(8, 0);
		tr_lsSetTextVal(&SYSLOG, &lfh_1_1, tr_BuildText("%s%s", "Test", tr_PrintNum(nI, "%05lf", 0, 0, 0)));
		debughook(9, 0);
		tr_lsSetNumVal(&SYSLOG, &lfh_1_2, nSeq);
		debughook(10, 0);
		tr_lsWriteEntry(&SYSLOG);
		debughook(11, 0);
		nSeq++;
		debugvarchg(&nSeq);
		debughook(14, 0);
		if (nSeq > 10.0) {
			debughook(14, 0);
			nSeq = 1.0;
			debugvarchg(&nSeq);
		}
		debughook(16, 0);
		nI++;
		debugvarchg(&nI);
	}
	debughook(17, 0);
	tr_FileWrite(stdout, "%s%s", "AFFICHAGE", TR_NEWLINE);
	debughook(18, 0);
	tr_lsFindFirst(&SYSLOG, 0, (void*) 0, "/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/databases/syslog", NULL);
	debughook(22, 0);
	while (tr_lsValidEntry(&SYSLOG)) {
		debughook(20, 0);
		tr_FileWrite(stdout, "%s%s%s%s%s%s", tr_PrintNum(tr_lsGetNumVal(&SYSLOG, &lfh_1_3), NULL, 0, 0, 0), " ", tr_lsGetTextVal(&SYSLOG, &lfh_1_1), " ",
				tr_PrintNum(tr_lsGetNumVal(&SYSLOG, &lfh_1_2), NULL, 0, 0, 0), TR_NEWLINE);
		debughook(22, 0);
		tr_lsFindNext(&SYSLOG);
	}
	debughook(23, 0);
	tr_FileWrite(stdout, "%s%s", "AFFICHAGE", TR_NEWLINE);
	debughook(24, 0);
	tr_lsFindFirst(&SYSLOG, 0, (void*) 0, "/data1/home/jlebiannic/DEVEL/TX5_2_UNICODE/Linux/databases/syslog", &lfh_1_1, 1, 0, "Test014*", NULL);
	debughook(29, 0);
	while (tr_lsValidEntry(&SYSLOG)) {
		debughook(26, 0);
		tr_FileWrite(stdout, "%s%s%s%s%s%s", tr_PrintNum(tr_lsGetNumVal(&SYSLOG, &lfh_1_3), NULL, 0, 0, 0), " ", tr_lsGetTextVal(&SYSLOG, &lfh_1_1), " ",
				tr_PrintNum(tr_lsGetNumVal(&SYSLOG, &lfh_1_2), NULL, 0, 0, 0), TR_NEWLINE);
		debughook(28, 0);
		tr_lsFindNext(&SYSLOG);
}
	return 0;
} /* tr_UserStatements() */




