/*========================================================================
 E D I S E R V E R

 File:		logsystem/dastub.c
 Author:
 Date:

 Copyright (c)

 redirection layer between Sqlite and Dao (postgreSQL)
 ========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: dastub.c  $")
#include <stdio.h>
#include "logsystem.sqlite.h"
#include "tr_externals.h"
#include "daservice.h"
/**
 * Jira TX-3199 DAO: Stub dao/sqlite
 * */

int daoMode() {
	if (tr_errorLevel > 5.0)
		fprintf(stderr, "%s : %s mode.\n", tr_programName, tr_useDao ? "dao" : "sqlite");
	return (tr_useDao);
}
unsigned int dao_logentry_new(LogSystem *log) {
	if (daoMode()) {
		return log->dao->newEntry(log->dao, log->table);
	} else {
		return logentry_sqlite_new(log);
	}
}

int dao_logentry_find(LogSystem *log, LogIndex **pIndexes, LogFilter *lf) {
	if (daoMode()) {
		return Service_findEntries(log, pIndexes, lf);
	} else {
		return log_sqlitereadbufsfiltered(log, pIndexes, lf);
	}
}



