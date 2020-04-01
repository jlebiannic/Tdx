#ifndef DAO_H
#define DAO_H

typedef struct Dao {
	/*  functions member                              */
	int id;
	int (*openDB)(struct Dao*, const char*);
	int (*isDBOpen)(struct Dao*);
	void (*closeDB)(struct Dao*);
	void (*closeDBAndExit)(struct Dao*);
	int (*execQuery)(struct Dao*, const char*);
	int (*execQueryMultiResults)(struct Dao*, const char*);
	int (*execQueryParams)(struct Dao*, const char*, const char*[], int);
	int (*execQueryParamsMultiResults)(struct Dao*, const char*, const char*[], int);
	unsigned int (*newEntry)(struct Dao*, const char *table);
	int (*updateEntries)(struct Dao*, const char*, const char*[], const char*[], int nb, const char*);
	int (*getEntries)(struct Dao*, const char*, const char*[], int, const char*, const char*[]);
	void (*getNextEntry)(struct Dao*);
	int (*hasNextEntry)(struct Dao*);
	char* (*getFieldValue)(struct Dao*, const char*);
	int (*getFieldValueAsInt)(struct Dao*, const char*);
	int (*getResultNbFields)(struct Dao*);
	char* (*getFieldValueByNum)(struct Dao*, int);
	int (*getFieldValueAsIntByNum)(struct Dao*, int);
	double (*getFieldValueAsDoubleByNum)(struct Dao*, int);
	void (*clearResult)(struct Dao*);
	int (*beginTrans)(struct Dao*);
	int (*endTrans)(struct Dao*);
	void (*logError)(const char*, const char*);
	void (*logErrorFormat)(char *formatAndParams, ...);
	void (*logDebug)(const char*, const char*);
	void (*logDebugFormat)(char *formatAndParams, ...);

/*  datas member                                  */
} Dao;


void Dao_init(Dao*);
void Dao_logError(const char *fctName, const char *msg);
void Dao_logErrorFormat(char *formatAndParams, ...);
void Dao_logDebug(const char *fctName, const char *msg);
void Dao_logDebugFormat(char *formatAndParams, ...);

#endif
