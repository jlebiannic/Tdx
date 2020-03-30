/*
 * daservice_update.c
 *
 *  Created on: 27 mars 2020
 *      Author: jlebiannic
 */

#include <stdlib.h>
#include <string.h>
#include "data-access/commons/commons.h"
#include "logsystem.definitions.h"
#include "logsystem.sqlite.h"
#include "logsystem.h"

static int calculateQueryLenght(LogEntry *entry, int rawmode);
static char* buildUpdateQuery(LogEntry *entry, int rawmode);

int Service_updateEntry(LogEntry *entry, int rawmode) {
	/* UPDATE data SET ("..."='...',)* ("..."='...') WHERE TX_INDEX=entry->idx */
	/* if rawmode, we do specify the index in the base, so we must add it to the request */
	char *sqlStatement = NULL;
	LogSystem *log = entry->logsys;
	Dao *dao = entry->logsys->dao;

	if (!entry) {
		return -1;
	}

	sqlStatement = buildUpdateQuery(entry, rawmode);

	sqlite_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
	int returnCode = dao->execQuery(dao, sqlStatement);
	if (!returnCode) {
		sqlite_logsys_warning(log, "Write operation in SQLite database failed : %s", sqlStatement);
	}
	free(sqlStatement);
	if (returnCode == TRUE) {
		return 0;
	} else {
		return -1;
	}
}

static int calculateQueryLenght(LogEntry *entry, int rawmode) {
	int sqlStatementLength = 0;
	LogSystem *log = entry->logsys;
	int i = 0;
	const char *table = entry->logsys->table;

	sqlStatementLength = sizeof("UPDATE SET ") + log->label->recordsize * 2;
	sqlStatementLength += strlen(table) + 1;
	sqlStatementLength += sizeof("=''[, ]") * (log->label->nfields - rawmode);
	sqlStatementLength += sizeof("WHERE TX_INDEX=") + MAX_INT_DIGITS;

	for (i = 0; i < log->label->nfields - 1; ++i) {
		if (strcmp(log->label->fields[i].name.string, "INDEX") == 0) {
			if (rawmode) {
				sqlStatementLength += sizeof("TX_");
			} else {
				continue;
			}
		}
		sqlStatementLength += strlen((log->label->fields[i]).name.string);
	}

	return sqlStatementLength;
}

static char* buildUpdateQuery(LogEntry *entry, int rawmode) {
	int ioffset = 0;
	LogSystem *log = entry->logsys;
	int i = 0;
	const char *table = entry->logsys->table;

	int sqlStatementLength = calculateQueryLenght(entry, rawmode);
	char *sqlStatement = (char*) malloc(sqlStatementLength + 1);

	char *updateSetCmd = allocStr2("UPDATE %s SET ", table);
	ioffset += sprintf(sqlStatement + ioffset, updateSetCmd);
	for (i = 0; i < log->label->nfields - 1; ++i) {
		if (strcmp(log->label->fields[i].name.string, "INDEX") == 0) {
			if (!rawmode) {
				continue;
			}
			ioffset += sprintf(sqlStatement + ioffset, "TX_%s='", log->label->fields[i].name.string);
		} else {
			ioffset += sprintf(sqlStatement + ioffset, "%s='", log->label->fields[i].name.string);
		}

		switch ((log->label->fields[i]).type) {
		default:
			break;
		case FIELD_NUMBER:
			ioffset += sprintf(sqlStatement + ioffset, "%lf", *(Number*) (entry->record->buffer + log->label->fields[i].offset));
			break;
		case FIELD_TIME:
			ioffset += sprintf(sqlStatement + ioffset, "%s", tr_timestring("%a", *(TimeStamp*) (entry->record->buffer + log->label->fields[i].offset)));
			break;
		case FIELD_INDEX:
		case FIELD_INTEGER:
			ioffset += sprintf(sqlStatement + ioffset, "%d", *(int*) (entry->record->buffer + log->label->fields[i].offset));
			break;
		case FIELD_TEXT:
			// TODO sqlite3_mprintf: utiliser execQueryParam
			ioffset += sprintf(sqlStatement + ioffset, "%s", (char*) (entry->record->buffer + log->label->fields[i].offset));
			break;
		}
		ioffset += sprintf(sqlStatement + ioffset, "'");

		if (i < log->label->nfields - 2) {
			ioffset += sprintf(sqlStatement + ioffset, ",");
		} else {
			ioffset += sprintf(sqlStatement + ioffset, " ");
		}
	}
	ioffset += sprintf(sqlStatement + ioffset, "WHERE TX_INDEX=%d", entry->idx);

	return sqlStatement;
}
