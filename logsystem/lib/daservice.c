#include <stdlib.h>
#include <string.h>
#include "data-access/commons/commons.h"
#include "logsystem.definitions.h"
#include "logsystem.sqlite.h"

static char* buildSqlStatement(LogSystem *log, LogFilter *lf);
static void addSqlWhere(char **pSqlStatement, int *pIoffset, LogFilter *lf, int filter_count);
static int buildIndexes(LogSystem *log, LogIndex **pIndexes, LogFilter *lf);
static int getOffset(LogFilter *lf);
static int getLimit(LogFilter *lf);
static int isNoPageCount();
static const char* strPrefixColName(const char *name);
static const char* getOpString(int opValue);
static char* sqlite_wildcard(FilterRecord *f, char *p_text);

static int MAXCOUNT = 100000;
/**
 * Jira TX-3199 DAO
 * */
char* Service_buildQuery(Dao *dao, LogSystem *log, LogFilter *lf) {
	char *query = NULL;
	switch (dao->id) {
	case 1:
		// PostgreSQL DAO
		query = buildSqlStatement(log, lf);
		break;
	default:
		break;
	}
	return query;
}

int Service_findEntries(LogSystem *log, LogIndex **pIndexes, LogFilter *lf) {
	int nbResults = -1;
	char *query = Service_buildQuery(log->dao, log, lf);
	int resQuery = log->dao->execQuery(log->dao, query);
	free(query);
	query = NULL;
	if (resQuery) {
		nbResults = buildIndexes(log, pIndexes, lf);
	}

	return nbResults;
}

static int buildIndexes(LogSystem *log, LogIndex **pIndexes, LogFilter *lf) {
	Dao *dao = log->dao;
	int numberOfRowRead = 0;
	int numberOfResults = 0;

	int offset = getOffset(lf);
	int max_count = getLimit(lf);

	if (max_count != -1) /* fix maximum of entries */
	{
		*pIndexes = sqlite_log_malloc((max_count + 1) * sizeof(**pIndexes));
	} else /* may vary accordingly to the number of entries */
	{
		*pIndexes = sqlite_log_malloc((MAXCOUNT + 1) * sizeof(**pIndexes));
	}

	log->indexes_count = 0;

	while (dao->hasNextEntry(dao)) {
		if (numberOfResults != max_count) {
			/* in case, we have a looot of entry matching and no max_count */
			if ((max_count == -1) && (numberOfResults > MAXCOUNT)) {
				/* XXX should we check this against MAX_INT ??? */
				MAXCOUNT = MAXCOUNT * 2;
				*pIndexes = (LogIndex*) sqlite_log_realloc(*pIndexes, (MAXCOUNT + 1) * sizeof(**pIndexes));
			}
			if (numberOfRowRead >= offset) {
				(*pIndexes)[numberOfResults] = dao->getFieldValueAsInt(dao, "tx_index");
				numberOfResults++;
			}
		}
		numberOfRowRead++;
		dao->getNextEntry(dao);
	}

	log->indexes_count = numberOfRowRead;
	dao->clearResult(dao);

	return numberOfResults;
}

static int getLimit(LogFilter *lf) {
	int max_count = -1;
	if (lf->max_count > 0) {
		max_count = lf->max_count;
	}
	return max_count;
}

static int getOffset(LogFilter *lf) {
	int offset = -1;

	/* PSA(CG) : 24/10/2014 - WBA-333 : Use SQL offset if flag present */
	if (!isNoPageCount()) {
		if (lf->offset > 0) {
			offset = lf->offset;
		}
	} else {
		if (lf->offset > 0) {
			offset = 0;
		}
	}
	/* fin WBA-333 */
	return offset;
}

static char* buildSqlStatement(LogSystem *log, LogFilter *lf) {
	/* SELECT TX_INDEX FROM data WHERE ... */
	char *sqlStatement = NULL;
	int ioffset = 0;
	FilterRecord *f = NULL;

	// FIXME calcul sqlStatementSize dynamique ?
	int sqlStatementSize = 2048 * sizeof(char);
	int filter_count = 0;

	char selectIdxFrom[100];
	sprintf(selectIdxFrom, "SELECT TX_INDEX FROM %s ", log->table);

	// FIXME calcul sqlStatementSize dynamique ?
	sqlStatement = (char*) malloc(sqlStatementSize + 1);

	if (!lf) /* no filter, return all (???) */
	{
		ioffset += sprintf(sqlStatement + ioffset, selectIdxFrom);
	} else {

		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f) {
			if (f->field != NULL && f->field->type != PSEUDOFIELD_OWNER) {
				// FIXME calcul sqlStatementSize dynamique ?
				filter_count++;
			}
		}

		ioffset += sprintf(sqlStatement + ioffset, selectIdxFrom);

		addSqlWhere(&sqlStatement, &ioffset, lf, filter_count);

		if (lf->sort_key) {
			ioffset += sprintf(sqlStatement + ioffset, " ORDER BY \"%s%s\"", strPrefixColName(lf->sort_key), lf->sort_key);
		}
		if ((!lf->sort_key) && (lf->sort_order != 0)) {
			ioffset += sprintf(sqlStatement + ioffset, " ORDER BY TX_INDEX");
		}
		switch (lf->sort_order) {
		case -1:
			ioffset += sprintf(sqlStatement + ioffset, " DESC");
			break;
		case 1:
			ioffset += sprintf(sqlStatement + ioffset, " ASC");
			break;
		default: /* no order for the filter, take default one */
			break;
		}

		/* PSA(CG) : 24/10/2014 - WBA-333 : Use SQL offset if flag present */
		if (isNoPageCount()) {
			if (lf->max_count > 0) {
				ioffset += sprintf(sqlStatement + ioffset, " LIMIT %d", lf->max_count);
			}
			if (lf->offset > 0) {
				ioffset += sprintf(sqlStatement + ioffset, " OFFSET %d", lf->offset);
			}
		}
		/* fin WBA-333 */
	}

	sqlite_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);

	return sqlStatement;
}

static const char* strPrefixColName(const char *name) {
	if (strcmp(name, "INDEX") == 0) {
		return "TX_";
	} else {
		return "";
	}
}

static int isNoPageCount() {
	if (NULL == getenv("LOGVIEW_NO_PAGECOUNT") || strcmp(getenv("LOGVIEW_NO_PAGECOUNT"), "1") != 0) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/* side effects on pSqlStatement and pIoffset */
static void addSqlWhere(char **pSqlStatement, int *pIoffset, LogFilter *lf, int filter_count) {
	FilterRecord *f;
	union FilterPrimary *p;
	char *p_text;

	if (!lf) {
		return;
	}

	if ((lf->filter_count > 0) && (filter_count != 0)) {
		*pIoffset += sprintf(*pSqlStatement + *pIoffset, "WHERE ");

		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f) {
			if (f->field == NULL || f->field->type == PSEUDOFIELD_OWNER) {
				continue;
			}

			if (f != lf->filters) {
				*pIoffset += sprintf(*pSqlStatement + *pIoffset, " AND ");
			}

			/* once the label found we write the corresponding primaries
			 * e.g. : INDEX >= 12 AND INDEX <= 18 */
			for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p) {
				if (f->op != OP_NONE) {
					if (p != f->primaries) {
						*pIoffset += sprintf(*pSqlStatement + *pIoffset, " OR ");
					} else {
						if (f->n_primaries > 1) {
							*pIoffset += sprintf(*pSqlStatement + *pIoffset, "( ");
						}
					}

					*pIoffset += sprintf(*pSqlStatement + *pIoffset, "%s%s ", strPrefixColName(f->name), f->name);

					switch (f->field->type) {
					default:
						/* return (fprintf(fp, "TYPE%d", f->type)); */
						break;
					case FIELD_TIME:
						switch (f->op) {
						case OP_NEQ:
							*pIoffset += sprintf(*pSqlStatement + *pIoffset, " NOT"); /* add NOT and go on as EQ case */
						case OP_EQ:
							*pIoffset += sprintf(*pSqlStatement + *pIoffset, " BETWEEN %ld AND %ld", p->times[0], p->times[1]);
							break;
						case OP_LT:
							*pIoffset += sprintf(*pSqlStatement + *pIoffset, "< %ld", p->times[0]);
							break;
						case OP_LTE:
							*pIoffset += sprintf(*pSqlStatement + *pIoffset, "<= %ld", p->times[1]);
							break;
						case OP_GTE:
							*pIoffset += sprintf(*pSqlStatement + *pIoffset, ">= %ld", p->times[0]);
							break;
						case OP_GT:
							*pIoffset += sprintf(*pSqlStatement + *pIoffset, ">= %ld", p->times[1]);
							break;
						default:
							break;
						}
						break;
					case FIELD_NUMBER:
						*pIoffset += sprintf(*pSqlStatement + *pIoffset, "%s %lf", getOpString(f->op), p->number);
						break;
					case FIELD_INDEX:
					case FIELD_INTEGER:
						*pIoffset += sprintf(*pSqlStatement + *pIoffset, "%s %d", getOpString(f->op), p->integer);
						break;
					case FIELD_TEXT:
						// p_text = sqlite3_mprintf("%q", sqlite_wildcard(f, p->text));
						p_text = sqlite_wildcard(f, p->text);
						// TODO use PQescapeLiteral insteadof sqlite_wildcard (=> tests)
						// TODO sanitize SQL replace sqlite3_mprintf by PQexecParams (=> refactor)
						*pIoffset += sprintf(*pSqlStatement + *pIoffset, "%s '%s'", getOpString(f->op), p_text);

						if ((f->op == OP_LEQ) || (f->op == OP_LNEQ)) {
							*pIoffset += sprintf(*pSqlStatement + *pIoffset, " ESCAPE '\\'");
						}
						// FIXME: Second time crash !
//						free(p_text);
//						p_text = NULL;
						break;
					}
				}
			}
			if (f->n_primaries > 1) {
				*pIoffset += sprintf(*pSqlStatement + *pIoffset, " )");
			}
		}
	}
}

static char* sqlite_wildcard(FilterRecord *f, char *p_text) {
	/* Translate wildcard and joker */
	static char *workstr = NULL;
	int wildcard_ok = 0;
	char *ptrcpy, *wildcard;

	/* only holds a small amount of memory at a moment */
	if (!workstr) {
		workstr = malloc(strlen(p_text) * 2);
	} else {
		workstr = realloc(workstr, strlen(p_text) * 2);
	}

	strcpy(workstr, p_text);
	wildcard = workstr;

	while (*wildcard != '\0') {
		if ((*wildcard == '*') && (*(wildcard - 1) != '\\')) /* '*' present, not escaped */
		{
			*wildcard = '%';
			wildcard_ok = 1;
		} else {
			if ((*wildcard == '?') && (*(wildcard - 1) != '\\')) /* '?' present, not escaped */
			{
				*wildcard = '_';
				wildcard_ok = 1;
			} else {
				if ((*wildcard == '_') || (*wildcard == '%')) /* must be escaped */
				{
					/* no realloc, only static buffers of 1kb are used */
					ptrcpy = wildcard + strlen(wildcard);
					while (ptrcpy >= wildcard) {
						*(ptrcpy + 1) = *ptrcpy;
						ptrcpy--;
					}
					*wildcard = '\\';
					wildcard_ok = 1;
					wildcard++;
				}
			}
		}
		wildcard++;
	}

	if (wildcard_ok == 1) {
		switch (f->op) {
		case OP_EQ:
			f->op = OP_LEQ;
			break;
		case OP_NEQ:
			f->op = OP_LNEQ;
			break;
		default:
			break;
		}
	}
	return workstr;
}

static const char* getOpString(int opValue) {
	/* the notation of operators used in filters is the one used in SQLite, cool */
	switch (opValue) {
	case OP_EQ:
		return "=";
	case OP_NEQ:
		return "!=";
	case OP_LT:
		return "<";
	case OP_LTE:
		return "<=";
	case OP_GTE:
		return ">=";
	case OP_GT:
		return ">";
	case OP_LEQ:
		return "LIKE";
	case OP_LNEQ:
		return "NOT LIKE";
	default:
		return NULL;
	}
}

