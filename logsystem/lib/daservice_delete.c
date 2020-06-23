/*
 * daservice_update.c
 *
 *  Created on: 19 may 2020
 *      Author: jlebiannic
 */

#include <stdlib.h>
#include <string.h>
#include "logsystem.definitions.h"
#include "data-access/commons/commons.h"

int Service_removeEntries(LogEntry *entry) {
    LogSystem *log = NULL;
	if (!entry) {
		return -1;
	}

 	log = entry->logsys;

    const char *values[1] = { inttoa(entry->idx) };
	int res = log->dao->removeEntries(log->dao, log->table, "tx_index = $", values);

    return res ? 0 : -1;
}