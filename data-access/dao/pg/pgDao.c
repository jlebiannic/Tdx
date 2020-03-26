#include "data-access/dao/pg/pgDao.h"
#include "data-access/dao/dao.h"
#include "data-access/commons/commons.h"

#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include <time.h>


static int ID = 1;
static int ROWS_PER_FETCH = 100; // TODO application parameter
static const char DECLARE_CURSOR_CMD[] = "DECLARE pgDaoCursor CURSOR FOR";
static const char FETCH_CMD_FORMAT[] = "FETCH %d IN pgDaoCursor";
static const char SELECT[] = "SELECT";
static const char FROM[] = "FROM";
static const char WHERE[] = "WHERE";

static void PgDao_init(PgDao*);
// static void PgDao_setDefaultSchema(PgDao *This);
static void PgDao_execQueryParams(PgDao *This, const char *command, int nParams, const Oid *paramTypes, const char *const*paramValues, const int *paramLengths, const int *paramFormats,
		int resultFormat);
static void PgDao_addResult(PgDao *This, PGresult *res);
static int PgDao_fetch(PgDao *This);
static int PgDao_doCommand(PgDao *This, char *command);
static void PgDao_closeCursor(PgDao *This);

/******************************************************************************/

static void PgDao_init(PgDao *This) {
	This->openDB = PgDao_openDB;
	This->isDBOpen = PgDao_isDBOpen;
	This->closeDB = PgDao_closeDB;
	This->closeDBAndExit = PgDao_closeDBAndExit;
	This->execQuery = PgDao_execQuery;
	This->execQueryMultiResults = PgDao_execQueryMultiResults;
	This->execQueryParamsMultiResults = PgDao_execQueryParamsMultiResults;
	This->getEntry = PgDao_getEntry;
	This->getNextEntry = PgDao_getNextEntry;
	This->hasNextEntry = PgDao_hasNextEntry;
	This->getFieldValue = PgDao_getFieldValue;
	This->getFieldValueAsInt = PgDao_getFieldValueAsInt;
	This->newEntry = PgDao_newEntry;
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
		conninfo = "host=localhost port=5432 dbname=postgres user=postgres password=admin";
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
			sprintf(cursorQuery, "%s %s", DECLARE_CURSOR_CMD, query);
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

// FIXME For Test
int PgDao_execQueryParamsMultiResults(PgDao *This, const char *queryFormat, int value) {
	This->logDebug("PgDao_execQueryMultiResults", "start");
	int res = FALSE;
	if (This->beginTrans(This)) {
		if (This->openDB(This, NULL)) {

			const int paramLengths[] = { sizeof(int) };
			char valueStr[10];
			sprintf(valueStr, "%d", value);
			const char *const paramValues[] = { valueStr };
			const int paramFormats[] = { 0 };

			char *cursorQuery = allocStr("%s %s", DECLARE_CURSOR_CMD, queryFormat);
			sprintf(cursorQuery, "%s %s", DECLARE_CURSOR_CMD, queryFormat);
			PgDao_execQueryParams(This, cursorQuery, 1, NULL, paramValues, paramLengths, paramFormats, 0);
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

void PgDao_getEntry(PgDao *This, const char *table, const char *filter, const char *fieldnames) {
	This->logDebug("PgDao_getEntry", "start");

	char *query = malloc(sizeof(SELECT) + strlen(fieldnames) + 1 + sizeof(FROM) + strlen(table) + 1 + sizeof(WHERE) + strlen(filter) + 1 + 7);
	sprintf(query, "%s %s %s %s %s %s", SELECT, fieldnames, FROM, table, WHERE, filter);
	PgDao_addResult(This, PQexec(This->conn, query));
	free(query);

	if (PQresultStatus(This->result) != PGRES_TUPLES_OK) {
		This->logError("PgDao_getElement failed", PQerrorMessage(This->conn));
		PgDao_clearResult(This);
	}
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
			This->logError("PgDao_getFieldValue", "field not found");
		}
	} else {
		This->logError("PgDao_getFieldValue", "result is null");
	}
	return fielValue;
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
		const int paramLengths[] = { sizeof(time_), sizeof(time_) };
		char strTime[21];
		sprintf(strTime, "%I64d", time_);
		const char *const paramValues[] = { strTime, strTime };
		const int paramFormats[] = { 0, 0 };

		query = allocStr("%s %s %s", insertInto, table, values);
		sprintf(query, "%s %s %s", insertInto, table, values);

		This->logDebugFormat("query: %s", query);

		PgDao_execQueryParams(This, query, 2, NULL, paramValues, paramLengths, paramFormats, 0);
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

static void PgDao_execQueryParams(PgDao *This, const char *command, int nParams, const Oid *paramTypes, const char *const*paramValues, const int *paramLengths, const int *paramFormats,
		int resultFormat) {
	This->logDebug("PgDao_execQueryParams", "start");
	PgDao_addResult(This, PQexecParams(This->conn, command, nParams, paramTypes, paramValues, paramLengths, paramFormats, resultFormat));

	if (PQresultStatus(This->result) != PGRES_TUPLES_OK && PQresultStatus(This->result) != PGRES_COMMAND_OK) {
		This->logError("PgDao_execQueryParams failed", PQerrorMessage(This->conn));
		PgDao_clearResult(This);
	}
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
		sprintf(fetchQuery, FETCH_CMD_FORMAT, ROWS_PER_FETCH);
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
