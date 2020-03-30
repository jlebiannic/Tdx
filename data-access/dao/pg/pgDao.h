#ifndef PGDAO_H
#define PGDAO_H

#include "libpq-fe.h"

typedef struct PgDao {
	int id;
	/* parent functions                               */
	int (*openDB)(struct PgDao*, const char*);
	int (*isDBOpen)(struct PgDao*);
	void (*closeDB)(struct PgDao*);
	void (*closeDBAndExit)(struct PgDao*);
	int (*execQuery)(struct PgDao*, const char*);
	int (*execQueryMultiResults)(struct PgDao*, const char*);
	int (*execQueryParams)(struct PgDao*, const char*, const char*[], int);
	int (*execQueryParamsMultiResults)(struct PgDao*, const char*, int);
	void (*getEntry)(struct PgDao*, const char*, const char*, const char*);
	void (*getNextEntry)(struct PgDao*);
	int (*hasNextEntry)(struct PgDao*);
	char* (*getFieldValue)(struct PgDao*, const char*);
	int (*getFieldValueAsInt)(struct PgDao*, const char*);
	int (*getResultNbFields)(struct PgDao*);
	char* (*getFieldValueByNum)(struct PgDao*, int);
	int (*getFieldValueAsIntByNum)(struct PgDao*, int);
	double (*getFieldValueAsDoubleByNum)(struct PgDao*, int);
	unsigned int (*newEntry)(struct PgDao*, const char*);
	int (*updateEntries)(struct PgDao*, const char*, const char*[], const char*[], int nb, const char*);
	void (*clearResult)(struct PgDao*);
	int (*beginTrans)(struct PgDao*);
	int (*endTrans)(struct PgDao*);
	void (*logError)(const char*, const char*);
	void (*logErrorFormat)(char *formatAndParams, ...);
	void (*logDebug)(const char*, const char*);
	void (*logDebugFormat)(const char*, const char*);

	/*  member functions                              */

	/*  member datas                                  */
	PGconn *conn;
	PGresult *result; // last result's query
	int resultCurrentRow;
	int cursorMode;
} PgDao;


/* Constructors  */
PgDao* PgDao_new(void);

/* overrided functions */
int PgDao_openDB(PgDao*, const char*);
int PgDao_isDBOpen(PgDao*);
void PgDao_closeDB(PgDao*);
void PgDao_closeDBAndExit(PgDao*);
int PgDao_execQuery(PgDao *This, const char *query);
int PgDao_execQueryMultiResults(PgDao *This, const char *query);
int PgDao_execQueryParams(PgDao *This, const char *queryFormat, const char *paramValues[], int nbParams);
int PgDao_execQueryParamsMultiResults(PgDao *This, const char *queryFormat, int value);
void PgDao_getEntry(PgDao*, const char*, const char*, const char*);
void PgDao_getNextEntry(PgDao*);
int PgDao_hasNextEntry(PgDao*);
char* PgDao_getFieldValue(PgDao *This, const char *fieldName);
int PgDao_getFieldValueAsInt(PgDao *This, const char *fieldName);
int PgDao_getResultNbFields(PgDao *This);
char* PgDao_getFieldValueByNum(PgDao *This, int numField);
int PgDao_getFieldValueAsIntByNum(PgDao *This, int numField);
double PgDao_getFieldValueAsDoubleByNum(PgDao *This, int numField);
unsigned int PgDao_newEntry(PgDao *This, const char *table);
int PgDao_updateEntries(PgDao *This, const char *table, const char *fields[], const char *values[], int nb, const char *filter);
void PgDao_clearResult(PgDao*);
int PgDao_beginTrans(PgDao*);
int PgDao_endTrans(PgDao*);


#endif
