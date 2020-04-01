#include "data-access/dao/pg/pgDao.h"
#include "data-access/dao/dao.h"
#include "data-access/commons/commons.h"

#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include <time.h>
#include <ctype.h>

static int ID = 1;
static int ROWS_PER_FETCH = 100; // TODO application parameter
static const char DECLARE_CURSOR_CMD[] = "DECLARE pgDaoCursor CURSOR FOR";
static const char FETCH_CMD_FORMAT[] = "FETCH %d IN pgDaoCursor";

static void PgDao_init(PgDao*);
// static void PgDao_setDefaultSchema(PgDao *This);
static int p_execQueryParams(PgDao *This, const char *command, const char *const*paramValues, int nParams);
static void PgDao_addResult(PgDao *This, PGresult *res);
static int PgDao_fetch(PgDao *This);
static int PgDao_doCommand(PgDao *This, char *command);
static void PgDao_closeCursor(PgDao *This);
static char* p_toPgStx(const char *str, int *nbValuesFound);

/******************************************************************************/

static void PgDao_init(PgDao *This) {
	This->openDB = PgDao_openDB;
	This->isDBOpen = PgDao_isDBOpen;
	This->closeDB = PgDao_closeDB;
	This->closeDBAndExit = PgDao_closeDBAndExit;
	This->execQuery = PgDao_execQuery;
	This->execQueryMultiResults = PgDao_execQueryMultiResults;
	This->execQueryParams = PgDao_execQueryParams;
	This->execQueryParamsMultiResults = PgDao_execQueryParamsMultiResults;
	This->newEntry = PgDao_newEntry;
	This->updateEntries = PgDao_updateEntries;
	This->getEntries = PgDao_getEntries;
	This->getNextEntry = PgDao_getNextEntry;
	This->hasNextEntry = PgDao_hasNextEntry;
	This->getFieldValue = PgDao_getFieldValue;
	This->getFieldValueAsInt = PgDao_getFieldValueAsInt;
	This->getResultNbFields = PgDao_getResultNbFields;
	This->getFieldValueByNum = PgDao_getFieldValueByNum;
	This->getFieldValueAsIntByNum = PgDao_getFieldValueAsIntByNum;
	This->getFieldValueAsDoubleByNum = PgDao_getFieldValueAsDoubleByNum;
	This->clearResult = PgDao_clearResult;
	This->beginTrans = PgDao_beginTrans;
	This->endTrans = PgDao_endTrans;

	// parent functions
	Dao_init((Dao*) This);
}

PgDao* PgDao_new() {
	PgDao *This = malloc(sizeof(PgDao));
	if (!This) {
		This->logError("PgDao_New", "malloc failed");
		return NULL;
	}
	PgDao_init(This);

	This->id = ID;
	This->conn = NULL;
	This->result = NULL;
	This->resultCurrentRow = -1;
	This->cursorMode = FALSE;
	// TODO This.Free = ?;
	return This;
}

int PgDao_openDB(PgDao *This, const char *conninfo) {
	This->logDebug("PgDao_openDB", "start");

	// Already open
	if (This->isDBOpen(This)) {
		return TRUE;
	}

	if (!conninfo) {
		// TODO a supprimer
		conninfo = "host=localhost port=5432 dbname=postgres user=postgres password=generix";
	}
	/* Cree une connexion e la base de donnees */
	This->conn = PQconnectdb(conninfo);

	/* Verifier que la connexion au backend a ete faite avec succes */
	if (PQstatus(This->conn) != CONNECTION_OK) {
		This->logError("Connection to database failed", PQerrorMessage(This->conn));
		This->closeDB(This);
		return FALSE;
	}

	// TODO exec_at_open (fichier .sqlite_startup: exécution des requêtes au démarrage)

	// PgDao_setDefaultSchema(This);
	return TRUE;
}

void PgDao_closeDB(PgDao *This) {
	This->logDebug("PgDao_closeDB", "start");
	PgDao_clearResult(This);
	if(This->conn != NULL) {
		PQfinish(This->conn);
		This->conn = NULL;
	}
}

void PgDao_closeDBAndExit(PgDao *This) {
	This->logDebug("PgDao_closeDBAndExit", "start");
	PQfinish(This->conn);
	exit(1);
}

int PgDao_execQuery(PgDao *This, const char *query) {
	This->logDebug("PgDao_execQuery", "start");
	int res = FALSE;
	if (This->openDB(This, NULL)) {
		PgDao_addResult(This, PQexec(This->conn, query));

		if (PQresultStatus(This->result) != PGRES_TUPLES_OK) {
			This->logError("PgDao_exec failed", PQerrorMessage(This->conn));
			PgDao_clearResult(This);
		} else {
			res = TRUE;
		}
	}
	return res;
}

int PgDao_execQueryMultiResults(PgDao *This, const char *query) {
	This->logDebug("PgDao_execQueryMultiResults", "start");
	int res = FALSE;
	if (This->beginTrans(This)) {
		if (This->openDB(This, NULL)) {
			char *cursorQuery = allocStr("%s %s", DECLARE_CURSOR_CMD, query);
			PgDao_addResult(This, PQexec(This->conn, cursorQuery));
			free(cursorQuery);
			if (PQresultStatus(This->result) != PGRES_COMMAND_OK) {
				This->logError("PgDao_execQueryMultiResults declare cursor failed", PQerrorMessage(This->conn));
				PgDao_clearResult(This);
			} else {
				This->cursorMode = TRUE;
				if (PgDao_fetch(This) >= 0) {
					res = TRUE;
				}
			}
		}
	}
	return res;
}

int PgDao_execQueryParams(PgDao *This, const char *queryFormat, const char *paramValues[], int nbParams) {
	This->logDebug("PgDao_execQueryMultiResults", "start");
	int res = FALSE;
	if (This->openDB(This, NULL)) {
		return p_execQueryParams(This, queryFormat, paramValues, nbParams);
	}
	return res;
}

/**
 * Execute query with internal cursor
 * 	table
 * 	fields
 * 	nbFields
 * 	filter: can be c1=$ or c1=$1
 * 	values: values for "$" in filter
 * */
int PgDao_getEntries(PgDao *This, const char *table, const char *fields[], int nbFields, const char *filter, const char *values[]) {
	This->logDebug("PgDao_execQueryParamsMultiResults", "start");
	char *selectFields = arrayJoin(fields, nbFields, ",");
	int nbValues = 0;
	char *filterWithPgStx = filter != NULL && strlen(filter) > 0 ? p_toPgStx(filter, &nbValues) : "";
	char *query = allocStr("Select %s from %s where %s", selectFields, table, filterWithPgStx);
	This->logDebugFormat("[getEntries]select query = %s\n", query);
	int res = This->execQueryParamsMultiResults(This, query, values, nbValues);
	free(query);
	free(selectFields);
	return res;
}

int PgDao_execQueryParamsMultiResults(PgDao *This, const char *query, const char *values[], int nbValues) {
	int res = FALSE;
	if (This->beginTrans(This)) {
		if (This->openDB(This, NULL)) {
			char *cursorQuery = allocStr("%s %s", DECLARE_CURSOR_CMD, query);
			p_execQueryParams(This, cursorQuery, values, nbValues);
			free(cursorQuery);
			if (PQresultStatus(This->result) != PGRES_COMMAND_OK) {
				This->logError("PgDao_execQueryMultiResults declare cursor failed", PQerrorMessage(This->conn));
				PgDao_clearResult(This);
			} else {
				This->cursorMode = TRUE;
				if (PgDao_fetch(This) >= 0) {
					res = TRUE;
				}
			}
		}
	}
	return res;
}

int PgDao_hasNextEntry(PgDao *This) {
	This->logDebug("PgDao_hasNextEntry", "start");
	if ((This->resultCurrentRow != -1 && This->resultCurrentRow <= PQntuples(This->result) - 1) || PgDao_fetch(This) > 0) {
		return TRUE;
	} else {
		PgDao_closeCursor(This);
		return FALSE;
	}
}

void PgDao_getNextEntry(PgDao *This) {
	This->logDebug("PgDao_getNextEntry", "start");
	if (This->hasNextEntry(This)) {
		This->resultCurrentRow++;
	}
}

int PgDao_getFieldValueAsInt(PgDao *This, const char *fieldName) {
	This->logDebug("PgDao_getFieldValueAsInt", "start");
	return strtol(This->getFieldValue(This, fieldName), (char**) NULL, 10);
}

char* PgDao_getFieldValue(PgDao *This, const char *fieldName) {
	This->logDebug("PgDao_getFieldValue", "start");
	int nFields;
	int i;
	int numField = -1;
	char *name;
	char* fielValue = NULL;
	if (This->result != NULL && PQresultStatus(This->result) == PGRES_TUPLES_OK) {
		nFields = PQnfields(This->result);
		for (i = 0; i < nFields; i++) {
			name = PQfname(This->result, i);
			if (strcasecmp(name, fieldName) == 0) {
				numField = i;
				break;
			}
		}
		// numField = PQfnumber(This->result, fieldName);
		if (numField >= 0) {
			fielValue = PQgetvalue(This->result, This->resultCurrentRow, numField);
		} else {
			This->logError("PgDao_getFieldValue", "invalid numField");
		}
	} else {
		This->logError("PgDao_getFieldValue", "result is null");
	}
	return fielValue;
}

int PgDao_getResultNbFields(PgDao *This) {
	This->logDebug("PgDao_getResultNbFields", "start");
	if (This->result == NULL) {
		This->logError("PgDao_getResultNbFields", "result is null");
		return -1;
	}
	return PQnfields(This->result);
}

char* PgDao_getFieldValueByNum(PgDao *This, int numField) {
	This->logDebug("PgDao_getFieldValueByNum", "start");
	char *fielValue = NULL;
	if (numField >= 0) {
		fielValue = PQgetvalue(This->result, This->resultCurrentRow, numField);
	} else {
		This->logError("PgDao_getFieldValue", "invalid numField");
	}
	return fielValue;
}

int PgDao_getFieldValueAsIntByNum(PgDao *This, int numField) {
	This->logDebug("PgDao_getFieldValueAsIntByNum", "start");
	return strtol(This->getFieldValueByNum(This, numField), (char**) NULL, 10);
}

double PgDao_getFieldValueAsDoubleByNum(PgDao *This, int numField) {
	This->logDebug("PgDao_getFieldValueAsDoubleByNum", "start");
	return strtod(This->getFieldValueByNum(This, numField), (char**) NULL);
}

unsigned int PgDao_newEntry(PgDao *This, const char *table) {
	// FIXME Tous le champs ne sont pas initialises contrairement a la source
	This->logDebug("PgDao_addEntry", "start");

	const char insertInto[] = "insert into";
	char *query;
	unsigned int idx = 0;

	if (This->openDB(This, NULL)) {
		time_t time_ = time(NULL);
		const char values[] = "(TX_INDEX, CREATED, MODIFIED) values(default, $1, $2) returning TX_INDEX";
		//const int paramLengths[] = { sizeof(time_), sizeof(time_) };
		char strTime[21];
		sprintf(strTime, "%ld", time_);
		const char *const paramValues[] = { strTime, strTime };
		//const int paramFormats[] = { 0, 0 };
		query = allocStr("%s %s %s", insertInto, table, values);
		This->logDebugFormat("[newEntry]query: %s", query);
		p_execQueryParams(This, query, paramValues, 2);
		free(query);

		if (PQresultStatus(This->result) != PGRES_TUPLES_OK) {
			This->logError("PgDao_addEntry failed", PQerrorMessage(This->conn));
		} else {
			idx = strtol(This->getFieldValue(This, "TX_INDEX"), (char**) NULL, 10);
		}

		PgDao_clearResult(This);
	}
	return idx;
}

int PgDao_updateEntries(PgDao *This, const char *table, const char *fields[], const char *values[], int nb, const char *filter) {
	int res;

	char updateElemTpl[] = "%s=$%d";
	char updateElemWithCommaTpl[] = "%s=$%d,";
	char *updateSet = allocStr("UPDATE %s SET", table);
	char *updateElems = NULL;
	char *updateFilter = filter != NULL && strlen(filter) > 0 ? allocStr("WHERE %s", filter) : "";
        int i=0;
	for (i = 0; i < nb; i++) {
		char *tpl = NULL;
		if (i == nb - 1) {
			tpl = &updateElemTpl[0];
		} else {
			tpl = &updateElemWithCommaTpl[0];
		}

		if (updateElems == NULL) {
			updateElems = allocStr(tpl, fields[i], i + 1);
		} else {
			updateElems = reallocStr(updateElems, tpl, fields[i], i + 1);
		}
	}

	char *query = allocStr("%s %s %s", updateSet, updateElems, updateFilter);
	This->logDebugFormat("update query = %s\n", query);
	res = This->execQueryParams(This, query, values, nb);

	free(updateSet);
	free(updateElems);
	free(updateFilter);
	free(query);

	return res;
}

int PgDao_beginTrans(PgDao *This) {
	This->logDebug("PgDao_beginTrans", "start");
	return PgDao_doCommand(This, "BEGIN");
}

int PgDao_endTrans(PgDao *This) {
	This->logDebug("PgDao_endTransaction", "start");
	/* termine la transaction */
	return PgDao_doCommand(This, "END");
}

int PgDao_isDBOpen(PgDao *This) {
	This->logDebug("PgDao_isDBOpen", "start");
	return This->conn != NULL ? TRUE : FALSE;
}

void PgDao_clearResult(PgDao *This) {
	This->logDebug("PgDao_clearResult", "start");
	if (This->result != NULL) {
		PQclear(This->result);
		This->result = NULL;
	}
	This->resultCurrentRow = -1;
}

/******************************************************************************/

// static void PgDao_setDefaultSchema(PgDao *This) {
// 	This->logDebug("PgDao_setDefaultSchema", "start");
// 	This->execQuery(This, "SELECT pg_catalog.set_config('search_path', '\"Tdx\"', false)");
// 	if (PQresultStatus(This->result) != PGRES_TUPLES_OK) {
// 		This->logError("PgDao_setDefaultSchema", PQerrorMessage(This->conn));
// 		PgDao_clearResult(This);
// 		This->closeDBAndExit(This);
// 	}
// 	PgDao_clearResult(This);
// }

static int p_execQueryParams(PgDao *This, const char *command, const char *const*paramValues, int nParams) {
	int res = TRUE;
	This->logDebug("p_execQueryParams", "start");
	PgDao_addResult(This, PQexecParams(This->conn, command, nParams, NULL, paramValues, NULL, NULL, 0));

	if (PQresultStatus(This->result) != PGRES_TUPLES_OK && PQresultStatus(This->result) != PGRES_COMMAND_OK) {
		This->logError("p_execQueryParams failed", PQerrorMessage(This->conn));
		PgDao_clearResult(This);
		res = FALSE;
	}

	return res;
}

static void PgDao_addResult(PgDao *This, PGresult *res) {
	This->logDebug("PgDao_addResult", "start");
	PgDao_clearResult(This);
	This->result = res;
	This->resultCurrentRow = 0;
}


static int PgDao_fetch(PgDao *This) {
	This->logDebug("PgDao_fetch", "start");
	int res = -1;
	if (This->cursorMode) {
		PgDao_clearResult(This);
		char *fetchQuery = allocStr(FETCH_CMD_FORMAT, ROWS_PER_FETCH);
		PgDao_addResult(This, PQexec(This->conn, fetchQuery));
		free(fetchQuery);
		if (PQresultStatus(This->result) != PGRES_TUPLES_OK) {
			This->logError("PgDao_execQueryMultiResults fetch failed", PQerrorMessage(This->conn));
			PgDao_clearResult(This);
		} else {
			res = PQntuples(This->result);
		}
	}
	return res;
}

static int PgDao_doCommand(PgDao *This, char *command) {
	This->logDebug("PgDao_doCommand", "start");
	PGresult *res;
	res = PQexec(This->conn, command);
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		This->logErrorFormat("doCommand: %s failed", command, PQerrorMessage(This->conn));
		PQclear(res);
		return FALSE;
	}
	PQclear(res);
	return TRUE;
}

static void PgDao_closeCursor(PgDao *This) {
	if (This->cursorMode) {
		This->logDebug("PgDao_closeCursor", "start");
		PgDao_clearResult(This);
		PgDao_doCommand(This, "CLOSE pgDaoCursor");
		This->endTrans(This);
		This->cursorMode = FALSE;
	}
}

/**
 * Transform str to PostgreSQL synthax.
 * Ex: select * from t where c1=$ and c2>$
 *     become
 *     select * from t where c1=$1 and c2>$2
 * */
static char* p_toPgStx(const char *str, int *nbValuesFound) {
	char *strRes = NULL;
	int len = strlen(str) * 2;
	int currentResLen = len;
	strRes = (char*) malloc(len + 1);
	strcpy(strRes, "");
	int cpt = 1;
	char *varNum = NULL;
	while (*str != '\0') {
		strncat(strRes, str, 1);
		if (*str == '$') {
			// verify not $1 (for example) but $ only
			if (!isdigit((int) *(str + 1))) {
				varNum = inttoa(cpt);
				if (strlen(strRes) + strlen(varNum) > currentResLen) {
					currentResLen = currentResLen + len;
					strRes = (char*) realloc(strRes, currentResLen + 1);
				}
				strcat(strRes, varNum);
				free(varNum);

			}
			cpt++;
		}
		str++;
	}
	*nbValuesFound = cpt - 1;
	return strRes;
}


