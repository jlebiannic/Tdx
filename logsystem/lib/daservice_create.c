/*
 * daservice_update.c
 *
 *  Created on: 11 Mai 2020
 *
 */

#include "daservice_common.h"
#include "data-access/commons/commons.h"
#include "logsystem.dao.h"
#include "logsystem.definitions.h"
#include "logsystem.h"
#include <stdlib.h>
#include <string.h>

static int createIndex(LogSystem *log);
static const char *getTypeAffinity(int typeValue);
static int getNbFieldsAndIndexPosition(LogLabel *label, int *indexPosition);
int Service_createTable(LogSystem *log, int removeIfExists) {
    //char *errMsg = NULL;
    LogLabel *label = NULL;
    LogField *f = NULL;

    int ret = FALSE;
    char *tableName;

    tableName = log->table;
    label = log->label;

    int indexPosition = -1;
    int nbFields = getNbFieldsAndIndexPosition(label, &indexPosition);
    char *fields[nbFields];
    char *types[nbFields];

    int cpt = 0;
    for (f = label->fields; f->type; ++f) {
        arrayAddElement(fields, f->name.string, cpt, TRUE);
        arrayAddElement(types, (char *)getTypeAffinity(f->type), cpt, TRUE);
        cpt++;
    }
    ret = log->dao->createTable(log->dao, tableName, (const char **)fields, (const char **)types, cpt, indexPosition, removeIfExists);

    freeArray(fields, cpt);
    freeArray(types, cpt);

    if (ret) {
		/*
		 * Secondary table and update triggers
		*/
        ret = log->dao->createTriggersEntryCount(log->dao, tableName);
        if (ret) {
            /*
		 	* Set the SQL indexes on the newly create database
		 	*/
            ret = createIndex(log);
        }
    }

    return ret ? 0 : -1;
}

static int getNbFieldsAndIndexPosition(LogLabel *label, int *indexPosition) {
    int cpt = 0;
    LogField *f = NULL;
    for (f = label->fields; f->type; ++f) {
        if (strcmp(f->name.string, "INDEX") == 0) {
            *indexPosition = cpt;
        }
        cpt++;
    }
    return cpt;
}

static int createIndex(LogSystem *log) {
    LogSQLIndexes *sqlindexes = NULL;
    LogSQLIndex *sqlindex = NULL;
    int i = 0;
    int j = 0;
    LogField *f;
    char *fieldName;
    int ret = TRUE;

    sqlindexes = log->sqlindexes;

    if (sqlindexes != NULL) {
        for (i = 0; i < sqlindexes->n_sqlindex; ++i) {
            sqlindex = sqlindexes->sqlindex[i];
            int indexNbFields = sqlindex->n_fields;
            char *fields[indexNbFields];
            for (j = 0; j < indexNbFields; ++j) {
                f = sqlindex->fields[j];
                fieldName = allocStr("%s%s", strPrefixColName(f->name.string), f->name.string);
                arrayAddElement(fields, fieldName, j, FALSE);
            }
            ret = log->dao->createIndex(log->dao, log->table, sqlindex->name, (const char **)fields, indexNbFields);
            freeArray(fields, indexNbFields);
            if (!ret) {
                return ret;
            }
        }
    }
    return ret;
}

static const char *getTypeAffinity(int typeValue) {
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
