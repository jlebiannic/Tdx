/*
 * dastub.h
 *
 *  Created on: 20 févr. 2020
 *      Author:
 */

#ifndef LOGSYSTEM_LIB_DASTUB_H_
#define LOGSYSTEM_LIB_DASTUB_H_
// Jira TX-3199 DAO: stub dao/sqlite
unsigned int dao_logentry_new(LogSystem *log);
int dao_logentry_find(LogSystem *log, LogIndex **pIndexes, LogFilter *lf);
unsigned int dao_logentry_findOne(LogEntry *entry);

#endif /* LOGSYSTEM_LIB_DASTUB_H_ */
