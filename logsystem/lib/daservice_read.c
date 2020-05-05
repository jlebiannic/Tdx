#include <stdlib.h>
#include <string.h>
#include "data-access/commons/commons.h"
#include "logsystem.definitions.h"
#include "logsystem.sqlite.h"
#include "logsystem.h"

static void buildFilterAndValues(char **pSqlStatement, LogFilter *lf, char **values, int *nbValues);
static int calculateFilterLen(LogFilter *lf);
static int buildIndexes(LogSystem *log, LogIndex **pIndexes, LogFilter *lf);
static int getOffset(LogFilter *lf);
static int getLimit(LogFilter *lf);
static int isNoPageCount();
static const char* strPrefixColName(const char *name);
static const char* getOpString(int opValue);
static char* sqlite_wildcard(FilterRecord *f, char *p_text);
static void buildEntry(LogEntry *entry, int i);

static int MAXCOUNT = 100000;
#define MAX_OP_SIZE 8
/**
 * Jira TX-3199 DAO
 * */
int Service_findEntries(LogSystem *log, LogIndex **pIndexes, LogFilter *lf) {
	int nbResults = -1;

	char **values = (char**) malloc(sizeof(char**));
	char *filter = NULL;
	int nbValues = -1;
	buildFilterAndValues(&filter, lf, values, &nbValues);
	
	char *fields[] = { "TX_INDEX" };
	int resQuery = log->dao->getEntries(log->dao, log->table, (const char **)fields, 1, filter, (const char **)values);
	freeArray(values, nbValues);
	free(values);
	free(filter);

	if (resQuery) {
		nbResults = buildIndexes(log, pIndexes, lf);
	}

	return nbResults;
}

int Service_findEntry(LogEntry *entry) {
	/* SELECT data.* FROM data WHERE TX_INDEX=... */
	int result = -1;
	int i = 0;

	const char *table = entry->logsys->table;
	Dao *dao = entry->logsys->dao;

	if (!entry) {
		return result;
	}

	const char queryFormat[] = "SELECT %s.* FROM %s WHERE TX_INDEX=$1";
	char *query = allocStr(queryFormat, table, table);

	sqlite_debug_logsys_warning(entry->logsys, "SQL Statement : %s", query);

	char *idxAsStr = uitoa(entry->idx);
	const char *queryParamValues[1] = { idxAsStr };
	if (dao->execQueryParams(dao, query, queryParamValues, 1)) {
		/* statement OK (but still result == number of row/entry == 0) */
		result++;
		if (dao->hasNextEntry(dao)) {
			int nfields = dao->getResultNbFields(dao);
			for (i = 0; i < nfields; i++) {
				buildEntry(entry, i);
			}
		}
		result++;
	}
	free(query);
	dao->clearResult(dao);

	sqlite_debug_logsys_warning(entry->logsys, "Single Read Operation return : %d", result);
	return result;
}

////////////////////////////
// static functions
////////////////////////////
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
	if (lf && lf->max_count > 0) {
		max_count = lf->max_count;
	}
	return max_count;
}

static int getOffset(LogFilter *lf) {
	int offset = -1;
	if (lf) {
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
	}
	return offset;
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

static void buildFilterAndValues(char **pSqlStatement, LogFilter *lf, char **values, int *nbValues) {
	FilterRecord *f;
	union FilterPrimary *p;
	char *p_text;
	int cpt = 0;
	int ioffset = 0;

	if (!lf) {
		return;
	}
	
	*pSqlStatement = malloc(calculateFilterLen(lf));

	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f) {
		if (f->field == NULL || f->field->type == PSEUDOFIELD_OWNER) {
			continue;
		}

		if (f != lf->filters) {
			ioffset += sprintf(*pSqlStatement + ioffset, " AND ");
		}

		/* once the label found we write the corresponding primaries
		 * e.g. : INDEX >= 12 AND INDEX <= 18 */
		for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p) {
			if (f->op != OP_NONE) {
				if (p != f->primaries) {
					ioffset += sprintf(*pSqlStatement + ioffset, " OR ");
				} else {
					if (f->n_primaries > 1) {
						ioffset += sprintf(*pSqlStatement + ioffset, "( ");
					}
				}

				ioffset += sprintf(*pSqlStatement + ioffset, "%s%s ", strPrefixColName(f->name), f->name);

				switch (f->field->type) {
				default:
					/* return (fprintf(fp, "TYPE%d", f->type)); */
					break;
				case FIELD_TIME:
					switch (f->op) {
					case OP_NEQ:
						ioffset += sprintf(*pSqlStatement + ioffset, " NOT"); /* add NOT and go on as EQ case */
					case OP_EQ:
						ioffset += sprintf(*pSqlStatement + ioffset, " BETWEEN $ AND $");
						arrayAddTimeElement(values, p->times[0], cpt, TRUE);
						cpt++;
						arrayAddTimeElement(values, p->times[1], cpt, TRUE);
						cpt++;
						break;
					case OP_LT:
						ioffset += sprintf(*pSqlStatement + ioffset, "< $");
						arrayAddTimeElement(values, p->times[0], cpt, TRUE);
						cpt++;
						break;
					case OP_LTE:
						ioffset += sprintf(*pSqlStatement + ioffset, "<= $");
						arrayAddTimeElement(values, p->times[1], cpt, TRUE);
						cpt++;
						break;
					case OP_GTE:
						ioffset += sprintf(*pSqlStatement + ioffset, ">= $");
						arrayAddTimeElement(values, p->times[0], cpt, TRUE);
						cpt++;
						break;
					case OP_GT:
						ioffset += sprintf(*pSqlStatement + ioffset, ">= $");
						arrayAddTimeElement(values, p->times[1], cpt, TRUE);
						cpt++;
						break;
					default:
						break;
					}
					break;
					case FIELD_NUMBER:
					ioffset += sprintf(*pSqlStatement + ioffset, "%s $", getOpString(f->op));
						arrayAddDoubleElement(values, p->number, cpt, TRUE);
						cpt++;
						break;
					case FIELD_INDEX:
					case FIELD_INTEGER:
					ioffset += sprintf(*pSqlStatement + ioffset, "%s $", getOpString(f->op));
						arrayAddIntElement(values, p->integer, cpt, TRUE);
						cpt++;
						break;
					case FIELD_TEXT:
						// p_text = sqlite3_mprintf("%q", sqlite_wildcard(f, p->text));
						p_text = sqlite_wildcard(f, p->text);
						// TODO use PQescapeLiteral insteadof sqlite_wildcard (=> tests)
					ioffset += sprintf(*pSqlStatement + ioffset, "%s $", getOpString(f->op));
						arrayAddElement(values, p_text, cpt, TRUE, TRUE);
						cpt++;
						if ((f->op == OP_LEQ) || (f->op == OP_LNEQ)) {
						ioffset += sprintf(*pSqlStatement + ioffset, " ESCAPE '\\'");
						}
						// FIXME: Second time crash !
						//						free(p_text);
						//						p_text = NULL;
						break;
				}
			}
		}
		if (f->n_primaries > 1) {
			ioffset += sprintf(*pSqlStatement + ioffset, " )");
		}
		*nbValues = cpt;
	}

	if (lf->sort_key) {
		ioffset += sprintf(*pSqlStatement + ioffset, " ORDER BY \"%s%s\"", strPrefixColName(lf->sort_key), lf->sort_key);
	}
	if ((!lf->sort_key) && (lf->sort_order != 0)) {
		ioffset += sprintf(*pSqlStatement + ioffset, " ORDER BY TX_INDEX");
	}
	switch (lf->sort_order) {
	case -1:
		ioffset += sprintf(*pSqlStatement + ioffset, " DESC");
		break;
	case 1:
		ioffset += sprintf(*pSqlStatement + ioffset, " ASC");
		break;
	default: /* no order for the filter, take default one */
		break;
	}

	/* PSA(CG) : 24/10/2014 - WBA-333 : Use SQL offset if flag present */
	if (isNoPageCount()) {
		if (lf->max_count > 0) {
			ioffset += sprintf(*pSqlStatement + ioffset, " LIMIT %d", lf->max_count);
		}
		if (lf->offset > 0) {
			ioffset += sprintf(*pSqlStatement + ioffset, " OFFSET %d", lf->offset);
		}
	}
	/* fin WBA-333 */
}

static int calculateFilterLen(LogFilter *lf) {
	FilterRecord *f = NULL;
	union FilterPrimary *p = NULL;
	int sqlStatementSize = sizeof(" AND ") * (lf->filter_count - 1);
	for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f) {
		if (f->field != NULL && f->field->type != PSEUDOFIELD_OWNER) {
			/* OR at the beginning except for first primary */
			sqlStatementSize += sizeof("(  )") + sizeof(" OR ") * (f->n_primaries - 1) + sizeof("\"\"") * f->n_primaries;

			for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p) {
				if (f->op == OP_NONE) {
					continue;
				}

				sqlStatementSize += strlen(f->name) + sizeof("TX_") + MAX_OP_SIZE;
				switch (f->field->type) {
				case FIELD_TEXT:
					/* we have at most f->field->size quote to escape ! + the quotes themselves */
					sqlStatementSize += strlen(p->text) * sizeof("\'") + sizeof("''") + sizeof("ESCAPE '\\'");
					break;
				default:
					sqlStatementSize += 2 * MAX_INT_DIGITS + sizeof(" NOT") + sizeof(" BETWEEN  AND ");
				}
			}
		}
	}

	if (lf->sort_key) {
		sqlStatementSize += sizeof(" ORDER BY ''") + strlen(lf->sort_key) + sizeof("TX_"); /* TX_ in case of INDEX */
	} else {
		/* if we have a sort order but no sort key, use (TX_)INDEX as a sort_key */
		sqlStatementSize += sizeof(" ORDER BY TX_INDEX");
	}

	/* lf->sort_order : either we have " DESC", " ASC" or nothing */
	sqlStatementSize += sizeof(" DESC");
	/* PSA(CG) : 24/10/2014 - WBA-333 : sqlStatementSize incrementation for LIMIT and OFFSET storage */
	sqlStatementSize += sizeof(" LIMIT NNNNNNN ") + sizeof(" OFFSET NNNNNNNNNNN ");
	/* fin WBA-333 */

	return sqlStatementSize;
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

static void buildEntry(LogEntry *entry, int i) {
	LogField *field;
	LogLabel *label;
	Dao *dao = entry->logsys->dao;

	label = entry->logsys->label;
	field = &label->fields[i];
	/* buffer allready allocated in logentry_alloc_skipclear */
	switch (field->type) {
	default:
		/* XXX should not happen ??? */
		*(Integer*) (entry->record->buffer + field->offset) = dao->getFieldValueAsIntByNum(dao, i);
		break;
	case FIELD_NUMBER:
		*(Number*) (entry->record->buffer + field->offset) = dao->getFieldValueAsDoubleByNum(dao, i);
		break;
	case FIELD_TIME: {
		time_t tms[2];
		tr_parsetime(dao->getFieldValueByNum(dao, i), tms);
		*(TimeStamp*) (entry->record->buffer + field->offset) = tms[0];
	}
	break;
	case FIELD_INDEX:
	case FIELD_INTEGER:
		*(Integer*) (entry->record->buffer + field->offset) = dao->getFieldValueAsIntByNum(dao, i);
		break;
	case FIELD_TEXT:
		mbsncpy(entry->record->buffer + field->offset, dao->getFieldValueByNum(dao, i), field->size - 1);
		break;
	}
}

