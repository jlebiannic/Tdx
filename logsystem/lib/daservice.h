/*
 * daservice.h
 *
 *  Created on: 2 mars 2020
 *      Author: jlebiannic
 */

#ifndef DA_SERVICE_H_
#define DA_SERVICE_H_

#include "logsystem.definitions.h"

int Service_findEntries(LogSystem *log, LogIndex **pIndexes, LogFilter *lf);
int Service_findEntry(LogEntry *entry);
int Service_updateEntry(LogEntry *entry, int rawmode);
int Service_createTable(LogSystem *log);

#endif /* DA_SERVICE_H_ */
