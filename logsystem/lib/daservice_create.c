/*
 * daservice_update.c
 *
 *  Created on: 11 Mai 2020
 *
 */

#include <stdlib.h>
#include <string.h>
#include "data-access/commons/commons.h"
#include "logsystem.definitions.h"
#include "logsystem.sqlite.h"
#include "logsystem.h"
#include "daservice_common.h"

static int createIndex(LogSystem *log);
static const char* getTypeAffinity(int typeValue);

int Service_createTable(LogSystem *log) {
	//char *errMsg = NULL;
	LogLabel *label = NULL;
	LogField *f = NULL;

	int ret = 0;
	int retCreateTable = 0;
	int retCreateIndex = 0;
	char *tableName;
	char **fields;
	char **types;

	tableName = log->table;

	int cpt = 0;
	arrayAddElement(fields, f->name.string, cpt, TRUE, TRUE);
	for (f = label->fields; f->type; ++f) {
		arrayAddElement(fields, f->name.string, cpt, TRUE, TRUE);
		arrayAddElement(types, getTypeAffinity(f->type), cpt, TRUE, TRUE);
		cpt++;
	}
	retCreateTable = log->dao->createTable(tableName, fields, types, cpt, 0);

	if (retCreateTable) {
		/*
		 * Set the SQL indexes on the newly create database
		 */
		retCreateIndex = createIndex(log);
	}

	freeArray(fields, cpt);
	freeArray(types, cpt);

	return retCreateTable && retCreateIndex ? 0 : -1;
}

static int createIndex(LogSystem *log) {
	LogSQLIndexes *sqlindexes = NULL;
	LogSQLIndex *sqlindex = NULL;
	int i = 0;
	int j = 0;
	char *f;
	char *fieldName;
	int ret = 0;

	sqlindexes = log->sqlindexes;

	if (sqlindexes != NULL) {
		for (i = 0; i < sqlindexes->n_sqlindex; ++i) {
			char *fields[sqlindex->n_fields];
			for (j = 0; j < sqlindex->n_fields; ++j) {
				f = sqlindex->fields[j];
				fieldName = allocStr("%s%s", strPrefixColName(f->name.string), f->name.string);
				arrayAddElement(fields, fieldName, j, FALSE, FALSE);
			}
			ret = log->dao->createIndex(log->table, sqlindex->name, fields, sqlindex->n_fields);
			free(fields);
			if (!ret) {
				return ret;
			}
		}
	}
	return ret;
}

static const char* getTypeAffinity(int typeValue) {
	switch (typeValue) {
	case FIELD_TIME: /* TIME must have INTEGER affinity for sorting but is always converted to text by sqlite3_column_text, and then to time by tr_parsetime */
	case FIELD_INDEX:
	case FIELD_INTEGER:
		return "INTEGER";
	case FIELD_NUMBER:
		return "NUMERIC";
		/* assume text by default */
	case FIELD_TEXT:
	default:
		return "TEXT";
	}
}

