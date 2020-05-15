#ifndef PGDAO_H
#define PGDAO_H

#include "libpq-fe.h"

typedef struct PgDao {
    int id;
    /* parent functions                               */
    int (*openDB)(struct PgDao *, const char *);
    int (*isDBOpen)(struct PgDao *);
    void (*closeDB)(struct PgDao *);
    void (*closeDBAndExit)(struct PgDao *);
    int (*execQuery)(struct PgDao *, const char *);
    int (*execQueryMultiResults)(struct PgDao *, const char *);
    int (*execQueryParams)(struct PgDao *, const char *, const char *[], int);
    int (*execQueryParamsMultiResults)(struct PgDao *, const char *, const char *[], int);
    unsigned int (*newEntry)(struct PgDao *, const char *);
    int (*updateEntries)(struct PgDao *, const char *, const char *[], const char *[], int, const char *);
    int (*getEntries)(struct PgDao *, const char *, const char *[], int, const char *, const char *[]);
    void (*getNextEntry)(struct PgDao *);
    int (*hasNextEntry)(struct PgDao *);
    char *(*getFieldValue)(struct PgDao *, const char *);
    int (*getFieldValueAsInt)(struct PgDao *, const char *);
    int (*getResultNbFields)(struct PgDao *);
    char *(*getFieldValueByNum)(struct PgDao *, int);
    int (*getFieldValueAsIntByNum)(struct PgDao *, int);
    double (*getFieldValueAsDoubleByNum)(struct PgDao *, int);
    int (*createTable)(struct PgDao *, const char *, const char *[], const char *[], int, int, int);
    int (*createIndex)(struct PgDao *, const char *, const char *, const char *[], int);
	int (*removeTable)(struct PgDao *, const char *, int);
    int (*createTriggersEntryCount)(struct PgDao *, const char *);
    void (*clearResult)(struct PgDao *);
    int (*beginTrans)(struct PgDao *);
    int (*endTrans)(struct PgDao *);
    void (*logError)(const char *, const char *);
    void (*logErrorFormat)(char *, ...);
    void (*logDebug)(const char *, const char *);
    void (*logDebugFormat)(const char *, const char *);

    /*  member functions                              */

    /*  member datas                                  */
    PGconn *conn;
    PGresult *result; // last result's query
    int resultCurrentRow;
    int cursorMode;
} PgDao;

/* Constructors  */
PgDao *PgDao_new(void);

/* overrided functions */
int PgDao_openDB(PgDao *, const char *);
int PgDao_isDBOpen(PgDao *);
void PgDao_closeDB(PgDao *);
void PgDao_closeDBAndExit(PgDao *);
int PgDao_execQuery(PgDao *This, const char *query);
int PgDao_execQueryMultiResults(PgDao *This, const char *query);
int PgDao_execQueryParams(PgDao *This, const char *queryFormat, const char *paramValues[], int nbParams);
int PgDao_execQueryParamsMultiResults(PgDao *This, const char *query, const char *values[], int nbValues);
unsigned int PgDao_newEntry(PgDao *This, const char *table);
int PgDao_updateEntries(PgDao *This, const char *table, const char *fields[], const char *values[], int nb, const char *filter);
int PgDao_getEntries(PgDao *This, const char *table, const char *fields[], int nbFields, const char *filter, const char *values[]);
void PgDao_getNextEntry(PgDao *);
int PgDao_hasNextEntry(PgDao *);
char *PgDao_getFieldValue(PgDao *This, const char *fieldName);
int PgDao_getFieldValueAsInt(PgDao *This, const char *fieldName);
int PgDao_getResultNbFields(PgDao *This);
char *PgDao_getFieldValueByNum(PgDao *This, int numField);
int PgDao_getFieldValueAsIntByNum(PgDao *This, int numField);
double PgDao_getFieldValueAsDoubleByNum(PgDao *This, int numField);
int PgDao_createTable(PgDao *This, const char *table, const char *fields[], const char *types[], int nb, int numSpecialField, int removeIfExists);
int PgDao_createIndex(PgDao *This, const char *table, const char *index, const char *fields[], int nb);
int PgDao_removeTable(PgDao *This, const char *table, int dropIfExists);
int PgDao_createTriggersEntryCount(PgDao *This, const char *tableName);
void PgDao_clearResult(PgDao *);
int PgDao_beginTrans(PgDao *);
int PgDao_endTrans(PgDao *);

#endif
