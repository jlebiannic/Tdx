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
#include "logsystem.dao.h"
#include "logsystem.h"

static int execUpdateQuery(LogEntry *entry, int rawmode);
static void buildArraysForUpdateQuery(LogEntry *entry, int rawmode, char **fields, char **values, int *nb);

int Service_updateEntry(LogEntry *entry, int rawmode) {
	/* UPDATE data SET ("..."='...',)* ("..."='...') WHERE TX_INDEX=entry->idx */
	/* if rawmode, we do specify the index in the base, so we must add it to the request */
	LogSystem *log = entry->logsys;

	if (!entry) {
		return -1;
	}

	// TODO log debug dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
	int returnCode = execUpdateQuery(entry, rawmode);
	if (!returnCode) {
		dao_logsys_warning(log, "Write operation in SQLite database failed : Service_updateEntry");
	}
	if (returnCode == TRUE) {
		return 0;
	} else {
		return -1;
	}
}

static int execUpdateQuery(LogEntry *entry, int rawmode) {
	LogSystem *log = entry->logsys;
	Dao *dao = log->dao;
	const char *table = log->table;
	int nb = log->label->nfields;
	char *fields[nb];
	char *values[nb];
	int realNb;
	buildArraysForUpdateQuery(entry, rawmode, fields, values, &realNb);
	char *idxAsStr = uitoa(entry->idx);
	const char *filterValues[1] = { idxAsStr };
	int res = dao->updateEntries(dao, table, (const char**) fields, (const char**) values, realNb, "TX_INDEX=$", filterValues);
	free(filter);
	freeArray(fields, realNb);
	freeArray(values, realNb);
	return res;
}

static void buildArraysForUpdateQuery(LogEntry *entry, int rawmode, char *fields[], char *values[], int *nb) {
	LogSystem *log = entry->logsys;
	int i = 0;
	int cpt=0;
	char *fieldValue = NULL;
	char *fieldName = NULL;
	for (i = 0; i < log->label->nfields - 1; ++i) {
		if (strcmp(log->label->fields[i].name.string, "INDEX") == 0) {
			if (!rawmode) {
				continue;
			}
			fieldName = allocStr("TX_%s", log->label->fields[i].name.string);
			arrayAddElement(fields, fieldName, cpt, FALSE);
		} else {
			arrayAddElement(fields, log->label->fields[i].name.string, cpt, TRUE);
		}

		switch ((log->label->fields[i]).type) {
		default:
			break;
		case FIELD_NUMBER:
			fieldValue = allocStr("%lf", *(Number*) (entry->record->buffer + log->label->fields[i].offset));
			arrayAddElement(values, fieldValue, cpt, FALSE);
			break;
		case FIELD_TIME:
			fieldValue = allocStr("%s", tr_timestring("%a", *(TimeStamp*) (entry->record->buffer + log->label->fields[i].offset)));
			arrayAddElement(values, fieldValue, cpt, FALSE);
			break;
		case FIELD_INDEX:
		case FIELD_INTEGER:
			fieldValue = allocStr("%d", *(int*) (entry->record->buffer + log->label->fields[i].offset));
			arrayAddElement(values, fieldValue, cpt, FALSE);
			break;
		case FIELD_TEXT:
			arrayAddElement(values, (char*) (entry->record->buffer + log->label->fields[i].offset), cpt, TRUE);
			break;
		}
		cpt++;
	}
	*nb = cpt;
}


