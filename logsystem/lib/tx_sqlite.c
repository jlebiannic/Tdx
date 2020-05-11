/* --------------------------------------------------------------------------------------------------------
!      19/05/2017  CGA TX-2973 [LOGVIEW] certains appels à logview, en mode multibases, provoquent des segfault.
!                         Ajout de l'allocation mémoire du caractère de fin de ligne 
! v1.1 10/05/2017 TCE(CG) EI-325 correct some memory leaks
! 10/04/2018 AAU (AMU) BG-198 Filter by env in MBA mode
! 22/11/2019 CDE TX-3158  use var to store len, and log->sysname
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 24.09.2019 - Olivier REMAUD - Passage au 64 bits
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
!---------------------------------------------------------------------------------------------------------*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tx_sqlite.c 55487 2020-05-06 08:56:27Z jlebiannic $")
#include "runtime/tr_constants.h"
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>

#include "logsystem.h"
#include "logsystem.dao.h"
#include "port.h"
#include "runtime/tr_prototypes.h"

#ifndef MACHINE_WNT
#include <unistd.h>
#include <sys/uio.h>
#endif

#define SELECTIDXFROM "SELECT TX_INDEX FROM data "
#define SELECTCOUNTFROM "SELECT COUNT(*) FROM data "

/*
	
 */

static int numberOfRowRead = 0;
static int maxcount = 100000;

int dao_logsys_opendata(LogSystem *log);
int dao_loglabel_free(LogLabel *lab);
bool check_filter_env(LogFilter *lf, char *env);

#define MAX_OP_SIZE 8

/* BugZ_9833 */
/*----------------------------------------------------------------------------------------------*/
/* cm 07/12/2010 */
/* some macros for new throwed exceptions throwed           */
#define SQLITE_CLASS_EXCEPTION          "internalDB"
#define SQLITE_MSG1_EXCEPTION           "SQLite database is busy"
#define SQLITE_MSG2_EXCEPTION           "SQLite database is locked"
#define SQLITE_BUSY_CODE_EXCEPTION      1.0
#define SQLITE_LOCKED_CODE_EXCEPTION    2.0


#define TYPE_LOGSYSTEM 'S'
#define TYPE_LOGENTRY  'E'
#define FROM_NEW       1
#define FROM_DELETE    2
#define FROM_WRITE     3

#define SQLITE_REQUEST_TIMEOUT 480000
/* EI-325 search SQLITE_TIMEOUT value from environment */
static int  getSyslogTimeout()
{
  int ret = SQLITE_REQUEST_TIMEOUT;
  int tmp_val;
  char *timeout_env = getenv("SQLITE_TIMEOUT");
  
  if (timeout_env)
  {
    ret = atoi(timeout_env);
  }

  return ret;
}

static void check_for_raising_exception(int returnCode, void* log_data, char log_type, char* __file__, int __line__, int from)
{
	int raise_busy_exception = 0;
	int raise_locked_exception = 0;
	char *fromMsg;

	switch(from)
	{
		case FROM_NEW   :
			fromMsg = "INSERT";
			break;
		case FROM_DELETE:
			fromMsg = "DELETE";
			break;
		case FROM_WRITE :
			fromMsg = "UPDATE";
			break;
		default         :
			fromMsg = "??????";
	}

	if (returnCode == SQLITE_BUSY)
	{
		raise_busy_exception = 1;
	}
	else if (returnCode == SQLITE_LOCKED)
	{
		raise_locked_exception = 1;
	}

	if (raise_busy_exception)
	{
		logsys_dump(log_data, log_type, SQLITE_MSG1_EXCEPTION, __file__, __line__, fromMsg);
		if (from == FROM_NEW)
		{
			raisexcept( SQLITE_CLASS_EXCEPTION, "warning",  SQLITE_MSG1_EXCEPTION, SQLITE_BUSY_CODE_EXCEPTION);
		}
	}
	else if (raise_locked_exception)
	{
		logsys_dump(log_data, log_type, SQLITE_MSG2_EXCEPTION, __file__, __line__, fromMsg);
		if (from == FROM_NEW)
		{
			raisexcept( SQLITE_CLASS_EXCEPTION, "warning",  SQLITE_MSG2_EXCEPTION,  SQLITE_LOCKED_CODE_EXCEPTION);
		}
	}
}
/*----------------------------------------------------------------------------------------------*/

static const char* getOpString(int opValue)
{
	/* the notation of operators used in filters is the one used in SQLite, cool */
	switch (opValue) {
		case OP_EQ:   return "=";
		case OP_NEQ:  return "!=";
		case OP_LT:   return "<";
		case OP_LTE:  return "<=";
		case OP_GTE:  return ">=";
		case OP_GT:   return ">";
		case OP_LEQ:  return "LIKE";
		case OP_LNEQ: return "NOT LIKE";
		default:      return NULL;
	}
}

static const char* getTypeAffinity(int typeValue)
{
	switch (typeValue)
	{
		case FIELD_TIME:    /* TIME must have INTEGER affinity for sorting but is always converted to text by sqlite3_column_text, and then to time by tr_parsetime */
		case FIELD_INDEX:
		case FIELD_INTEGER: return "INTEGER";
		case FIELD_NUMBER:  return "NUMERIC";
		/* assume text by default */
		case FIELD_TEXT:
		default:            return "TEXT";
	}
}
static const char* strPrefixColName(const char *name)
{
	if (strcmp(name,"INDEX") == 0) {
		return "TX_";
	} else { 
		return "";
	}
}

static int dao_singleRequest(LogSystem *log, const char* request)
{
	int returnCode = 0;
	char *errMsg = NULL;

	if (dao_logsys_opendata(log)) {
		return -1;
	}


	dao_debug_logsys_warning(log, "SQL Statement : %s", request);
	if (sqlite3_exec(log->datadb_handle,request,NULL,NULL,&errMsg) != SQLITE_OK)
	{
		dao_logsys_warning(log,"%s on database failed: %s", request, sqlite3_errmsg(log->datadb_handle));
		returnCode = -1;
	}
	free(errMsg);
	errMsg = NULL;
	return returnCode;
}

static int dao_singleRequest_opened(LogSystem *log, const char* request)
{
	int returnCode = 0;
	char *errMsg = NULL;

	dao_debug_logsys_warning(log, "SQL Statement : %s", request);
	if (sqlite3_exec(log->datadb_handle,request,NULL,NULL,&errMsg) != SQLITE_OK)
	{
		dao_logsys_warning(log,"%s on database failed: %s", request, sqlite3_errmsg(log->datadb_handle));
		returnCode = -1;
	}
	free(errMsg);
	errMsg = NULL;
	return returnCode;
}

static char* dao_wildcard(FilterRecord *f,char *p_text)
{
	/* Translate wildcard and joker */
	static char *workstr = NULL;
	int wildcard_ok=0;
	char *ptrcpy,*wildcard;

	/* only holds a small amount of memory at a moment */
	if (!workstr) {
		workstr=malloc(strlen(p_text) * 2);
	} else {
		workstr=realloc(workstr,strlen(p_text) * 2);
	}

	strcpy(workstr,p_text);
	wildcard=workstr;

	while (*wildcard!='\0')
	{
		if ((*wildcard=='*') && (*(wildcard-1) !='\\')) /* '*' present, not escaped */
		{
			*wildcard='%';
			wildcard_ok=1;
		}
		else
		{
			if ((*wildcard=='?') && (*(wildcard-1) !='\\')) /* '?' present, not escaped */
			{
				*wildcard='_';
				wildcard_ok=1;
			}
			else
			{
				if ((*wildcard=='_') || (*wildcard=='%')) /* must be escaped */
				{
					/* no realloc, only static buffers of 1kb are used */
					ptrcpy=wildcard+strlen(wildcard);
					while (ptrcpy>=wildcard)
					{
						*(ptrcpy+1)=*ptrcpy;
						ptrcpy--;
					}
					*wildcard='\\';
					wildcard_ok=1;
					wildcard++;
				}
			}
		}
		wildcard++;
	}

	if (wildcard_ok==1)
	{
		switch (f->op)
		{
			case OP_EQ : f->op=OP_LEQ;  break;
			case OP_NEQ: f->op=OP_LNEQ; break;
			default : break;
		}
	}
	return workstr;
}

/* side effects on pSqlStatement and pIoffset */
static void addSqlWhere(char **pSqlStatement, int *pIoffset, LogFilter *lf, int filter_count)
{
	FilterRecord *f;
	union FilterPrimary *p;
	char *p_text;

	if (!lf) {
		return;
	}


	if ((lf->filter_count > 0) && (filter_count != 0))
	{
		*pIoffset += sprintf(*pSqlStatement+*pIoffset,"WHERE ");

		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
		{
			if (f->field == NULL || f->field->type == PSEUDOFIELD_OWNER) {
				continue;
			}


			if (f != lf->filters) {
				*pIoffset += sprintf(*pSqlStatement+*pIoffset," AND ");
			}


			/* once the label found we write the corresponding primaries
			* e.g. : INDEX >= 12 AND INDEX <= 18 */
			for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p)
			{
				if (f->op != OP_NONE) {
					if (p != f->primaries)
					{
						*pIoffset += sprintf(*pSqlStatement+*pIoffset," OR ");
					}
					else
					{
						if (f->n_primaries > 1)
						{
							*pIoffset += sprintf(*pSqlStatement+*pIoffset,"( ");
						}
					}
	
					*pIoffset += sprintf(*pSqlStatement+*pIoffset,"\"%s%s\"",strPrefixColName(f->name),f->name);
	
					switch (f->field->type)
					{
						default:
							/* return (fprintf(fp, "TYPE%d", f->type)); */
							break;
						case FIELD_TIME:
							switch (f->op) {
								case OP_NEQ: *pIoffset += sprintf(*pSqlStatement+*pIoffset," NOT"); /* add NOT and go on as EQ case */
								case OP_EQ:  *pIoffset += sprintf(*pSqlStatement+*pIoffset," BETWEEN %d AND %d",p->times[0],p->times[1]); break;
								case OP_LT:  *pIoffset += sprintf(*pSqlStatement+*pIoffset,"< %d",p->times[0]); break;
								case OP_LTE: *pIoffset += sprintf(*pSqlStatement+*pIoffset,"<= %d",p->times[1]); break;
								case OP_GTE: *pIoffset += sprintf(*pSqlStatement+*pIoffset,">= %d",p->times[0]); break;
								case OP_GT:  *pIoffset += sprintf(*pSqlStatement+*pIoffset,">= %d",p->times[1]); break;
								default: break;
							}
							break;
						case FIELD_NUMBER:
							*pIoffset += sprintf(*pSqlStatement+*pIoffset,"%s %lf",getOpString(f->op),p->number);
							break;
						case FIELD_INDEX:
						case FIELD_INTEGER:
							*pIoffset += sprintf(*pSqlStatement+*pIoffset,"%s %d",getOpString(f->op),p->integer);
							break;
						case FIELD_TEXT:
							p_text = sqlite3_mprintf("%q",dao_wildcard(f,p->text));
							*pIoffset += sprintf(*pSqlStatement+*pIoffset,"%s '%s'",getOpString(f->op),p_text);

							sqlite3_free(p_text);
							p_text = NULL;
							if ((f->op == OP_LEQ) || (f->op == OP_LNEQ)) {
								*pIoffset += sprintf(*pSqlStatement+*pIoffset,"ESCAPE '\\'");
							}
							break;
					}
				}
			}
			if (f->n_primaries > 1)
			{
				*pIoffset += sprintf(*pSqlStatement+*pIoffset," )");
			}
		}
	}
}

int log_daoreadbufsfiltered(LogSystem *log, LogIndex **pIndexes, LogFilter *lf)
{
	/* SELECT TX_INDEX FROM data WHERE ... */
	char *sqlStatement = NULL;
	int ioffset = 0;
	int returnCode = 0;
	sqlite3_stmt *sqlite3Statement = NULL;

	int sqlStatementSize = 0;
	int filter_count = 0;
	FilterRecord *f = NULL;
	union FilterPrimary *p = NULL;

	/* page handling */
	int offset = -1;
	int max_count = -1;

	int numberOfResults = 0;

	/* logsys_dao_entry_count(log,lf) is too costly
	 * use fixed first and realloc if necessary */
	maxcount = 100000;

	if (dao_logsys_opendata(log)) {
		return -1;
	}

	if (!lf) /* no filter, return all (???) */
	{
		sqlStatement = (char *)malloc(sizeof(SELECTIDXFROM)+1);
		ioffset += sprintf(sqlStatement+ioffset,SELECTIDXFROM);
		sqlStatementSize = sizeof(SELECTIDXFROM);
	}
	else
	{
		/* "SELECT TX_INDEX FROM data WHERE " + NL */
		/* + AND betwwen filters (filter_count - 1, always at least one filter) */
		sqlStatementSize = sizeof(SELECTIDXFROM) + sizeof("WHERE ") + sizeof (" AND ") * (lf->filter_count - 1);
		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
		{
			if (f->field != NULL && f->field->type != PSEUDOFIELD_OWNER) {
				/* OR at the beginning except for first primary */
				sqlStatementSize += sizeof("(  )") + sizeof(" OR ") * (f->n_primaries -1) + sizeof("\"\"") * f->n_primaries;

				for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p)
				{
					if (f->op == OP_NONE) {
						continue;
					}

					sqlStatementSize += strlen(f->name) + sizeof("TX_") + MAX_OP_SIZE;
					switch (f->field->type)
					{
						case FIELD_TEXT:
							/* we have at most f->field->size quote to escape ! + the quotes themselves */
							sqlStatementSize += strlen(p->text) * sizeof("\'") + sizeof("''") + sizeof("ESCAPE '\\'");
							break;
						default:
							sqlStatementSize += 2 * MAX_INT_DIGITS + sizeof(" NOT") + sizeof(" BETWEEN  AND ");
					}
				}
				filter_count++;
			}
		}

		if (lf->sort_key)
		{
			sqlStatementSize += sizeof(" ORDER BY ''") + strlen(lf->sort_key) + sizeof("TX_"); /* TX_ in case of INDEX */
		}
		else
		{
			/* if we have a sort order but no sort key, use (TX_)INDEX as a sort_key */
			sqlStatementSize += sizeof(" ORDER BY TX_INDEX");
		}

		/* lf->sort_order : either we have " DESC", " ASC" or nothing */
		sqlStatementSize += sizeof(" DESC");
		/* PSA(CG) : 24/10/2014 - WBA-333 : sqlStatementSize incrementation for LIMIT and OFFSET storage */
		sqlStatementSize += sizeof(" LIMIT NNNNNNN ") + sizeof(" OFFSET NNNNNNNNNNN ");
		/* fin WBA-333 */

		sqlStatement = (char *)malloc(sqlStatementSize+1);

		ioffset += sprintf(sqlStatement+ioffset,SELECTIDXFROM);

		addSqlWhere(&sqlStatement, &ioffset, lf, filter_count);

		if (lf->sort_key)
		{
			ioffset += sprintf(sqlStatement+ioffset," ORDER BY \"%s%s\"",strPrefixColName(lf->sort_key),lf->sort_key);
		}
		if ((!lf->sort_key) && (lf->sort_order != 0))
		{
			ioffset += sprintf(sqlStatement+ioffset," ORDER BY TX_INDEX");
		}
		switch (lf->sort_order)
		{
			case -1: ioffset += sprintf(sqlStatement+ioffset," DESC"); break;
			case  1: ioffset += sprintf(sqlStatement+ioffset," ASC");  break;
			default: /* no order for the filter, take default one */   break;
		}

        /* PSA(CG) : 24/10/2014 - WBA-333 : Use SQL offset if flag present */
		if (NULL == getenv("LOGVIEW_NO_PAGECOUNT") || strcmp(getenv("LOGVIEW_NO_PAGECOUNT"), "1") != 0) {
            if (lf->max_count > 0) {
                max_count = lf->max_count;
            }
            if (lf->offset > 0) {
                offset = lf->offset;
            }
		} else {
            if (lf->max_count > 0) {
                max_count = lf->max_count;
                ioffset += sprintf(sqlStatement+ioffset, " LIMIT %d", lf->max_count);
            }
            if (lf->offset > 0) {
                offset = 0;
                ioffset += sprintf(sqlStatement+ioffset, " OFFSET %d", lf->offset);
            }
		}
		/* fin WBA-333 */
	}

	dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);

	/* possibly all entries may match the filter
	 * but as we store only the indexes, it is not that big.
	 * besides we shrink it at the end to fit the resultset. */

	returnCode = sqlite3_prepare(log->datadb_handle,sqlStatement,-1,&sqlite3Statement,NULL);
	if (returnCode != SQLITE_OK) {
		dao_logsys_warning(log, "Multiple Index Read operation in sqlite database failed with error: %s", sqlite3_errmsg(log->datadb_handle));
	}
	free(sqlStatement);
	sqlStatement = NULL;

	numberOfRowRead = 0;
	numberOfResults = 0;

	if (max_count != -1) /* fix maximum of entries */
	{
		*pIndexes = dao_log_malloc((max_count + 1) * sizeof(**pIndexes));
	}
	else /* may vary accordingly to the number of entries */
	{
		*pIndexes = dao_log_malloc((maxcount + 1) * sizeof(**pIndexes));
	}

	if (returnCode == SQLITE_OK)
	{
		while ( sqlite3_step(sqlite3Statement) == SQLITE_ROW )
		{
			if (numberOfResults != max_count)
			{
				/* in case, we have a looot of entry matching and no max_count */
				if ((max_count == -1) && (numberOfResults > maxcount))
				{
					/* XXX should we check this against MAX_INT ??? */
					maxcount = maxcount * 2;
					*pIndexes = (LogIndex *) dao_log_realloc(*pIndexes,(maxcount + 1) * sizeof(**pIndexes));
				}
				if (numberOfRowRead >= offset)
				{
					(*pIndexes)[numberOfResults] = sqlite3_column_int(sqlite3Statement,0);
					numberOfResults++;
				}
			}
			numberOfRowRead++;
		}
		log->indexes_count = numberOfRowRead;
		sqlite3_finalize(sqlite3Statement);
		return numberOfResults;
	}
	else
	{
		log->indexes_count = 0;
		return -1;
	}
}

int log_daoreadbuf(LogEntry *entry)
{
	/* SELECT data.* FROM data WHERE TX_INDEX=... */
	char *sqlStatement = NULL;
	sqlite3_stmt *sqlite3Statement = NULL;
	int returnCode = -1;
	int result = -1;

	LogField *field;
	char *value = NULL;
	int i = 0;
	LogLabel *label;

	if (!entry) {
		return result;
	}


	label = entry->logsys->label;

	if (dao_logsys_opendata(entry->logsys)) {
		return result;
	}


	sqlStatement = sqlite3_mprintf("SELECT data.* FROM data WHERE TX_INDEX=%d", entry->idx);
	dao_debug_logsys_warning(entry->logsys, "SQL Statement : %s", sqlStatement);
	returnCode = sqlite3_prepare(entry->logsys->datadb_handle,sqlStatement,-1,&sqlite3Statement,NULL);
	sqlite3_free(sqlStatement);
	sqlStatement = NULL;

	if (returnCode == SQLITE_OK)
	{
		/* statement OK (but still result == number of row/entry == 0) */
		result++;
		/* try fetching the entry (row == 1 if OK) */
		if (sqlite3_step(sqlite3Statement) == SQLITE_ROW)
		{
			/* fieldsnames not used */
			/* write the right value at the right place in the record buffer */
			for(i=0; i<sqlite3_data_count(sqlite3Statement); i++)
			{
				field = &label->fields[i];

				/* buffer allready allocated in logentry_alloc_skipclear */
				switch (field->type)
				{
					default:
						/* XXX should not happen ??? */
						*(Integer *) (entry->record->buffer + field->offset) = sqlite3_column_int(sqlite3Statement,i);
						break;
					case FIELD_NUMBER:
						*(Number *) (entry->record->buffer + field->offset) = sqlite3_column_double(sqlite3Statement,i);
						break;
					case FIELD_TIME:
						{
							time_t tms[2];

							value = (char *) sqlite3_column_text(sqlite3Statement,i);
							/* This uses only the start-time of the range, i.e. 1.2.1999 means 1.2.1999 00:00:00 AM */
							if (value == NULL) {
								tr_parsetime("", tms);
							} else {
								tr_parsetime(value, tms);
							}

							*(TimeStamp *) (entry->record->buffer + field->offset) = tms[0];
							/* P.S.: do not free(value), it is garbaged by sqlite3 functions */
							value = NULL;
						}
						break;
					case FIELD_INDEX:
					case FIELD_INTEGER:
						*(Integer *) (entry->record->buffer + field->offset) = sqlite3_column_int(sqlite3Statement,i);
						break;
					case FIELD_TEXT:
						value = (char *) sqlite3_column_text(sqlite3Statement,i);
						mbsncpy(entry->record->buffer + field->offset,value?value:"",field->size - 1);
						/* P.S.: do not free(value), it is garbaged by sqlite3 functions */
						value = NULL;
						break;
				}
			}

			result++;
		}
	}
	/* EI-325 remove statement in every case */
	sqlite3_finalize(sqlite3Statement);
	dao_debug_logsys_warning(entry->logsys, "Single Read Operation return : %d", result);
	return result;
}

int log_daowritebuf(LogEntry *entry,int rawmode)
{
	/* UPDATE data SET ("..."='...',)* ("..."='...') WHERE TX_INDEX=entry->idx */
	/* if rawmode, we do specify the index in the base, so we must add it to the request */
	char *sqlStatement = NULL;
	int sqlStatementLength = 0;
	int ioffset = 0;
	LogSystem *log = NULL;
	char *value = NULL;
	int returnCode = -1;
	char *errMsg = NULL;
	int i = 0;
	int isTextField = 0;

 	if (!entry) {
		return -1;
	}

	log = entry->logsys;

	if (dao_logsys_opendata(log)) {
		return -1;
	}


	sqlStatementLength = sizeof("UPDATE data SET ") + log->label->recordsize * 2 ;
	sqlStatementLength +=  sizeof("\"\"=''[, ]") * (log->label->nfields - rawmode) ;
	sqlStatementLength += sizeof("WHERE TX_INDEX=") + MAX_INT_DIGITS;

	for (i=0;i<log->label->nfields -1;++i)
	{
		if (strcmp(log->label->fields[i].name.string,"INDEX") == 0)
		{
			if (rawmode) {
				sqlStatementLength += sizeof("TX_");
			} else {
				continue;
			}
		}
		sqlStatementLength += strlen((log->label->fields[i]).name.string);
	}

	sqlStatement = (char *)malloc(sqlStatementLength+1);

	ioffset += sprintf(sqlStatement+ioffset,"UPDATE data SET ");
	for (i=0;i<log->label->nfields -1;++i)
	{
		if (strcmp(log->label->fields[i].name.string,"INDEX") == 0)
		{
			if (!rawmode) {
				continue;
			}

			ioffset += sprintf(sqlStatement+ioffset,"\"TX_%s\"='",log->label->fields[i].name.string);
		}
		else
		{
			ioffset += sprintf(sqlStatement+ioffset,"\"%s\"='",log->label->fields[i].name.string);
		}

		isTextField = 0;
		switch ((log->label->fields[i]).type)
		{
			default:
				break;
			case FIELD_NUMBER:
				ioffset += sprintf(sqlStatement+ioffset,"%lf",*(Number *) (entry->record->buffer + log->label->fields[i].offset));
				break;
			case FIELD_TIME:
				ioffset += sprintf(sqlStatement+ioffset,"%s",tr_timestring("%a", *(TimeStamp *) (entry->record->buffer + log->label->fields[i].offset)));
				break;
			case FIELD_INDEX:
			case FIELD_INTEGER:
				ioffset += sprintf(sqlStatement+ioffset,"%d",*(int *) (entry->record->buffer + log->label->fields[i].offset));
				break;
			case FIELD_TEXT:
				value = sqlite3_mprintf("%q", (char *) (entry->record->buffer + log->label->fields[i].offset));
				ioffset += sprintf(sqlStatement+ioffset,"%s",value);
				sqlite3_free(value);
				value = NULL;
				break;
		}
		ioffset += sprintf(sqlStatement+ioffset,"'");

		if (i<log->label->nfields - 2) {
			ioffset += sprintf(sqlStatement+ioffset,",");
		} else {
			ioffset += sprintf(sqlStatement+ioffset," ");
		}
	}
	ioffset += sprintf(sqlStatement+ioffset,"WHERE TX_INDEX=%d",entry->idx);

	dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
	returnCode = sqlite3_exec(log->datadb_handle,sqlStatement,NULL,NULL,&errMsg);
	free(sqlStatement);
	/* EI-325 free error message memory allocation made by sqlite */
	sqlite3_free(errMsg);
	if (returnCode != SQLITE_OK) {
		dao_logsys_warning(log, "Write operation in SQLite database failed : %s", sqlite3_errmsg(log->datadb_handle));
	}

	/* cm 07/12/2010 */
	/* BugZ_9833: throws new exceptions for */
	/* SQlite Busy and Locked errors.       */
	check_for_raising_exception(returnCode, entry, TYPE_LOGENTRY, __FILE__, __LINE__, FROM_WRITE);
	/*--------------------------------------*/


	if (returnCode == SQLITE_OK) {
		return 0;
	} else {
		return -1;
	}
}

unsigned int logentry_dao_new(LogSystem *log)
{
	char *sqlStatement = NULL;
	int sqlStatementLength = 0;
	int ioffset = 0;
#ifdef MACHINE_WNT
	__int64 index = 0;
#else
	long long int index = 0; /* XXX long long int vs int ??? */
#endif
	int returnCode = -1;
	char *errMsg = NULL;
	unsigned int i = 0;
	/* INSERT INTO data VALUES() */
	/* here, we take the highest (last) created index and increments it
	* thus creating a well incremented list of entries */

	if (dao_logsys_opendata(log)) {
		return 0;
	}

	sqlStatementLength = sizeof("INSERT INTO data VALUES(NULL") + sizeof(",''") * (log->label->nfields) + 2 * MAX_INT_DIGITS;
	sqlStatement = (char *)malloc(sqlStatementLength+1);

	/* empty parameter for the rest
	 * specify NULL value to use autoincrement feature
	 * we do not specify dates here but the logentry does have both
	 * so next update we write the dates in the databse */
	ioffset += sprintf(sqlStatement+ioffset,"INSERT INTO data VALUES(NULL");
	for (i=1;i< (unsigned int) (log->label->nfields - 1);++i)
	{
		ioffset += sprintf(sqlStatement+ioffset,",");
		if ( (strcmp(log->label->fields[i].name.string,"CREATED") == 0)
		  || (strcmp(log->label->fields[i].name.string,"MODIFIED") == 0)
		   )
		{
			ioffset += sprintf(sqlStatement+ioffset,"%d",dao_log_curtime());
		}
		else
		{
			ioffset += sprintf(sqlStatement+ioffset,"''");
		}
	}
	ioffset += sprintf(sqlStatement+ioffset,")");

	dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
	returnCode = sqlite3_exec(log->datadb_handle,sqlStatement,NULL,NULL,&errMsg);
	free(sqlStatement);
	if (returnCode != SQLITE_OK) {
		dao_logsys_warning(log, "New entry in SQLite database failed : %s", sqlite3_errmsg(log->datadb_handle));
	}
	/* EI-325 free error message memory allocation */
	if (errMsg != NULL){ sqlite3_free(errMsg); }
	index = sqlite3_last_insert_rowid(log->datadb_handle);
	/* if we have effectively created a new entry, last index for insert should have been incremented */
	if (index > INT_MAX)
	{
		/* should be extremely rare and/or time spaced !!! */
		dao_logsys_warning(log, "You have exhausted all the unique indexes available, consider resetting the base");
		return 0;
	}

	if (returnCode != SQLITE_OK)
	{
		/* cm 07/12/2010 */
		/* BugZ_9833: throws new exceptions for */
		/* SQlite Busy and Locked errors.       */
		check_for_raising_exception(returnCode, log, TYPE_LOGSYSTEM, __FILE__, __LINE__,FROM_NEW);
		/*---------------------------------------*/
		return 0;
	}
	else
		return (unsigned int) index;

}

int logentry_dao_destroy(LogEntry *entry)
{
	/* DELETE FROM data WHERE TX_INDEX="" */
	char *sqlStatement = NULL;
	LogSystem *log = NULL;
	int returnCode = -1;
	char *errMsg = NULL;

	if (!entry) {
		return -1;
	}

 	log = entry->logsys;

	if (dao_logsys_opendata(log)) {
		return -1;
	}

	sqlStatement = sqlite3_mprintf("DELETE FROM data WHERE TX_INDEX='%d'",entry->idx);
	dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
	returnCode = sqlite3_exec(log->datadb_handle,sqlStatement,NULL,NULL,&errMsg);
	sqlite3_free(sqlStatement);
	sqlStatement = NULL;
	sqlite3_free(errMsg);
	if (returnCode != SQLITE_OK) {
		dao_logsys_warning(log, "Delete operation in SQLite database failed : %s", sqlite3_errmsg(log->datadb_handle));
	}

	/* cm 07/12/2010 */
	/* BugZ_9833: throws new exceptions for */
	/* SQlite Busy and Locked errors.       */
	check_for_raising_exception(returnCode, entry, TYPE_LOGENTRY, __FILE__, __LINE__,FROM_DELETE);
	/*--------------------------------------*/

	if (returnCode == SQLITE_OK) {
		return 0;
	} else {
		return -1;
	}
}

/* The request has a cost with sqlite at least
 * should be avoided if possiuble */
int logsys_dao_entry_count(LogSystem *log, LogFilter *lf)
{
	/* SELECT COUNT(TX_INDEX) FROM data WHERE ... */
	char *sqlStatement = NULL;
	int sqlStatementSize = 0;
	int ioffset = 0;
	sqlite3_stmt *sqlite3Statement = NULL;

	FilterRecord *f;
	int filter_count = 0;
	union FilterPrimary *p;

	int numberOfEntries = -1;
	int returnCode = 0;

	if (dao_logsys_opendata(log))
	{
		return -1;
	}

	if (!lf) /* no filter, return all */
	{
		dao_debug_logsys_warning(log, "SQL Statement : SELECT cnt FROM entry_count");
		returnCode = sqlite3_prepare(log->datadb_handle,"SELECT cnt FROM entry_count",-1,&sqlite3Statement,NULL);
	}
	else
	{
		/* "SELECT COUNT(*) FROM data WHERE " + NL */
		/* + f->op always between 0 and 2 */
		/* + AND betwwen filters (filter_count - 1, always at least one filter) */
		/* + 2 quotes for column name (in case we use non reserved keywords as column name) */
		sqlStatementSize = sizeof(SELECTCOUNTFROM) + sizeof("WHERE ") + sizeof (" AND ") * (lf->filter_count - 1);
		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
		{
			if (f->field == NULL || f->field->type == PSEUDOFIELD_OWNER) {
				continue;
			}


			/* OR at the beginning except for first primary */
			sqlStatementSize += sizeof("(  )") + sizeof(" OR ") * (f->n_primaries -1) + sizeof("\"\"") * f->n_primaries;

			for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p)
			{
				if (f->op == OP_NONE) {
					continue;
				}


				sqlStatementSize += strlen(f->name) + sizeof("TX_") + MAX_OP_SIZE;
				switch (f->field->type)
				{
					case FIELD_TEXT:
						/* we have at most f->field->size quote to escape ! + the quotes themselves */
						sqlStatementSize += strlen(p->text) * sizeof("\'") + sizeof("''") + sizeof("ESCAPE '\\'");
						break;
					default:
						sqlStatementSize += 2 * MAX_INT_DIGITS + sizeof(" NOT") + sizeof(" BETWEEN  AND ");
				}
			}
			filter_count++;
		}

		sqlStatement = (char *)malloc(sqlStatementSize+1);

		ioffset += sprintf(sqlStatement+ioffset,SELECTCOUNTFROM);

		addSqlWhere(&sqlStatement, &ioffset, lf, filter_count);

		/* contrary to sqlitereadbufs : */
		/* as we just count, we do not care about order */
		/* we do want all possibles entries
		 * so ignore here also max_count and offset
		 * else count would be useless ! */

		/* sql statement ready */
		returnCode = sqlite3_prepare(log->datadb_handle,sqlStatement,-1,&sqlite3Statement,NULL);
		free(sqlStatement);
		sqlStatement = NULL;
	}

	if ((returnCode == SQLITE_OK) && (sqlite3_step(sqlite3Statement) == SQLITE_ROW))
	{
		numberOfEntries = sqlite3_column_int(sqlite3Statement,0);
		sqlite3_finalize(sqlite3Statement);
	}
	else
	{
		/* numberOfEntries == -1 */
		dao_logsys_warning(log, "Count of entries operation in SQLite database failed : %s", sqlite3_errmsg(log->datadb_handle));
	}
	return numberOfEntries;
}

int logsys_createdata_dao(LogSystem *log,int mode)
{
	int fd = -1;
	char *errMsg = NULL;
	char *sqlStatement = NULL;
	int sqlStatementSize = 0;
	int ioffset = 0;
	LogLabel *label = NULL;
	LogSQLIndexes *sqlindexes = NULL;
	LogField *f = NULL;
	LogSQLIndex *sqlindex = NULL;
	int i = 0;
	int j = 0;
	int ret = 0;
	char* dbName ;
	int toFree = 0 ;
	int totLen = 0;
	
	/* create the .sqlite datafile */
	fd = dao_logsys_openfile(log, LSFILE_SQLITE, O_TRUNC | O_RDWR | O_CREAT | O_BINARY, mode);
	if (fd < 0) {
		dao_logsys_warning(log,"Cannot create sqlite datafile");
	}
	log->datafd = fd;
	close(log->datafd);

	/* EI-325 if size > 1024, manually allocate memory */
	/* TX-3158 use totLen to store len, and log->sysname */
	if ( (totLen=(strlen(log->sysname) + strlen(LSFILE_SQLITE) )) > 1024 )	{
		dbName = malloc(totLen +1);
		dbName = strcpy(dbName,log->sysname);	
		dbName = strcat(dbName, LSFILE_SQLITE);
		toFree = 1;
	} else {
		dbName = dao_logsys_filepath(log, LSFILE_SQLITE);
	}
	
	/* Let's open the new created file to write it down the database structure */
	if (sqlite3_open(dbName, &log->datadb_handle) != SQLITE_OK)
	{
		dao_logsys_warning(log,"Cannot open database: %s", sqlite3_errmsg(log->datadb_handle));
		/* EI-325 try to close correctly the database handler in case it's still used */
        ret = sqlite3_close(log->datadb_handle);
		if ( ret != SQLITE_OK ) 
		{
			int cpt = 0;
			sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
			while ( ret == SQLITE_BUSY && cpt < 4 ) {
				ret = sqlite3_close(log->datadb_handle);
				sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
				cpt++;			
			}
			if (cpt == 4 && ret != SQLITE_OK){
				dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
			} 		
		}
		log->datadb_handle = 0;
		/* EI-325 free memory allocation manually before exiting */
		if (toFree) { free(dbName); toFree = 0; }
		return -1;
	}
	/* EI-325 free memory allocation mannually */
	if (toFree) { free(dbName); toFree = 0; } ;

	/*
	 * The main table
	 */

	label = log->label;

	/* CREATE TABLE "data" ("TX_INDEX" INTEGER PRIMARY KEY AUTOINCREMENT ...//... ) */
	sqlStatementSize = sizeof("CREATE TABLE \"data\" (\"TX_INDEX\" INTEGER PRIMARY KEY AUTOINCREMENT)");
	for (f = label->fields; f->type; ++f)
	{
		/* (TX_)INDEX allready added, drop it */
		if (strcmp(f->name.string,"INDEX") != 0) {
			sqlStatementSize +=  strlen(f->name.string) + sizeof(",\"\" NUMERIC");
		}
	}
	sqlStatement = (char *)malloc(sqlStatementSize+1);
	ioffset += sprintf(sqlStatement+ioffset,"CREATE TABLE \"data\" (");
	/* ensure we have the (TX_)INDEX first */
	ioffset += sprintf(sqlStatement+ioffset,"\"TX_INDEX\" INTEGER PRIMARY KEY AUTOINCREMENT");
	for (f = label->fields; f->type; ++f)
	{
		/* (TX_)INDEX allready added, drop it */
		if (strcmp(f->name.string,"INDEX") == 0) {
			continue;
		}
		ioffset += sprintf(sqlStatement+ioffset,",\"%s\" %s", f->name.string, getTypeAffinity(f->type));
	}
	ioffset += sprintf(sqlStatement+ioffset,")");
	dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
	if (sqlite3_exec(log->datadb_handle,sqlStatement,NULL,NULL,&errMsg) != SQLITE_OK)
	{
		dao_logsys_warning(log, "Creation of the initial table for .sqlite datafile failed: %s", sqlite3_errmsg(log->datadb_handle));
		/* EI-325 try to close correctly the database handler in case it's still used */
        ret = sqlite3_close(log->datadb_handle);
		if ( ret != SQLITE_OK ) 
		{
			int cpt = 0;
			sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
			while ( ret == SQLITE_BUSY && cpt < 4 ) {
				ret = sqlite3_close(log->datadb_handle);
				sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
				cpt++;			
			}
			if (cpt == 4 && ret != SQLITE_OK){
				dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
			} 		
		}
		log->datadb_handle = 0;
		free(sqlStatement);
		free(errMsg);
		sqlStatement = NULL;
		errMsg = NULL;
		return -1;
	}
	free(sqlStatement);
	/* EI-325 remove memory allocated by sqlite */
	sqlite3_free(errMsg);
	sqlStatement = NULL;

	/*
	 * Secondary table and update triggers
	 */

#define TRIGGER_DEFINITION "\n\
CREATE TABLE entry_count AS SELECT COUNT(*) AS cnt FROM data; \n\
CREATE TRIGGER dec_count AFTER DELETE ON data \n\
BEGIN \n\
    UPDATE entry_count SET cnt=cnt-1; \n\
END; \n\
CREATE TRIGGER inc_count AFTER INSERT ON data \n\
BEGIN \n\
    UPDATE entry_count SET cnt=cnt+1; \n\
END\
"

	dao_debug_logsys_warning(log, TRIGGER_DEFINITION);
	if (sqlite3_exec(log->datadb_handle, TRIGGER_DEFINITION,NULL,NULL,&errMsg) != SQLITE_OK)
	{
		dao_logsys_warning(log, "Creation of the database update triggers failed : %s", sqlite3_errmsg(log->datadb_handle));
		/* EI-325 try to close correctly the database handler in case it's still used */
        ret = sqlite3_close(log->datadb_handle);
		if ( ret != SQLITE_OK ) 
		{
			int cpt = 0;
			sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
			while ( ret == SQLITE_BUSY && cpt < 4 ) {
				ret = sqlite3_close(log->datadb_handle);
				sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
				cpt++;			
			}
			if (cpt == 4 && ret != SQLITE_OK){
				dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
			} 		
		}
		log->datadb_handle = 0;
		free(errMsg);
		errMsg = NULL;
		return -1;
	}
	/* EI-325 free memory allocated by sqlite3 */
	sqlite3_free(errMsg);
#undef TRIGGER_DEFINITION


	/*
	 * Set the SQL indexes on the newly create database
	 */

	sqlindexes = log->sqlindexes;

	if (sqlindexes != NULL)
	{
		for (i = 0; i < sqlindexes->n_sqlindex; ++i)
		{
			sqlStatementSize = 0;
			ioffset = 0;
			sqlindex = sqlindexes->sqlindex[i];
			/* CREATE INDEX ... ON data ("<col1>"[,"<col2>"]*) */
			sqlStatementSize += sizeof("CREATE INDEX \"") + strlen(sqlindex->name) + sizeof("\" ON data (");
			for (j = 0; j < sqlindex->n_fields; ++j)
			{
				f = sqlindex->fields[j];
				sqlStatementSize += strlen(f->name.string) + sizeof("\"\","); /* ""[,)] */
				if (strcmp(f->name.string,"INDEX") == 0) {
					sqlStatementSize += 3; /* TX_ */
				}
			}
			sqlStatement = (char *)malloc(sqlStatementSize+1);
			ioffset += sprintf(sqlStatement+ioffset,"CREATE INDEX \"%s\" ON data (",sqlindex->name);
			for (j = 0; j < sqlindex->n_fields; ++j)
			{
				f = sqlindex->fields[j];
				ioffset += sprintf(sqlStatement+ioffset,"\"%s%s\"",strPrefixColName(f->name.string),f->name.string);
				if (j == sqlindex->n_fields - 1) {
					ioffset += sprintf(sqlStatement+ioffset,")");
				} else {
					ioffset += sprintf(sqlStatement+ioffset,",");
				}
			}
			dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
			if (sqlite3_exec(log->datadb_handle,sqlStatement,NULL,NULL,&errMsg) != SQLITE_OK)
			{
				dao_logsys_warning(log, "Creation of an SQL index failed: %s", sqlite3_errmsg(log->datadb_handle));
				/* EI-325 try to close correctly the database handler in case it's still used */
				ret = sqlite3_close(log->datadb_handle);
				if ( ret != SQLITE_OK ) 
				{
					int cpt = 0;
					sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
					while ( ret == SQLITE_BUSY && cpt < 4 ) {
						ret = sqlite3_close(log->datadb_handle);
						sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
						cpt++;			
					}
					if (cpt == 4 && ret != SQLITE_OK){
						dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
					} 		
				}
				log->datadb_handle = 0;
				free(sqlStatement);
				/* EI-325 free memory allocated by sqlite */
				sqlite3_free(errMsg);
				sqlStatement = NULL;
				errMsg = NULL;
				return -1;
			}
			free(sqlStatement);
			/* EI-325 free memory allocated by sqlite */
			sqlite3_free(errMsg);
			sqlStatement = NULL;
		}
	}
	/* EI-325 try to close correctly the database handler in case it's still used */
	ret = sqlite3_close(log->datadb_handle);
	if ( ret != SQLITE_OK ) 
	{
		int cpt = 0;
		sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
		while ( ret == SQLITE_BUSY && cpt < 4 ) {
			ret = sqlite3_close(log->datadb_handle);
			sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
			cpt++;			
		}
		if (cpt == 4 && ret != SQLITE_OK){
			dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
		} 		
	}
		
	log->datadb_handle = 0;
	free(errMsg);
	errMsg = NULL;
	return 0;
}

/* This function reset the sqlite sequence counter.
 * Its use should be extremely RARE and only needed when all the indexes have been exhausted
 * i.e you've got a index over MAX_INT
 * i.e. the counter use internally in the base to know the new index to be given to a new entry
 * which is given by the itself */
void dao_logsys_resetindex(char *basename)
{
	int ret = 0;
	LogSystem* log = dao_logsys_open(basename, 0644);

	if (dao_singleRequest(log,"UPDATE dao_sequence SET seq=1 WHERE name='data'") == -1) {
		dao_logsys_warning(log, "Reset of the database failed.");
	}

	/* EI-325 try to close correctly the database handler in case it's still used */
	ret = sqlite3_close(log->datadb_handle);
	if ( ret != SQLITE_OK ) 
	{
		int cpt = 0;
		sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
		while ( ret == SQLITE_BUSY && cpt < 4 ) {
			ret = sqlite3_close(log->datadb_handle);
			sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
			cpt++;			
		}
		if (cpt == 4 && ret != SQLITE_OK){
			dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
		} 		
	}
		
	log->datadb_handle = 0;
	dao_logsys_close(log);
}

/* alter to add a column to an already existing database */
int log_daocolumnadd(LogSystem *log,LogField* column)
{
	char *sqlStatement = NULL;
	char *errMsg = NULL;
	int returnCode = 0;

	if (dao_logsys_opendata(log)) {
		return -1;
	}

	if (!column) {
		return -1;
	}


	sqlStatement = sqlite3_mprintf("ALTER TABLE data ADD \"%q\" %s", column->name.string, getTypeAffinity(column->type));
	dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
	if (sqlite3_exec(log->datadb_handle,sqlStatement,NULL,NULL,&errMsg) != SQLITE_OK)
	{
		dao_logsys_warning(log, "Adding a column to the database failed: %s", sqlite3_errmsg(log->datadb_handle));
		returnCode = -1;
	}
	sqlite3_free(sqlStatement);
	/* EI-325 free memory allocated by sqlite */
	sqlite3_free(errMsg);
	sqlStatement = NULL;
	errMsg = NULL;
	return returnCode;
}

int dao_logsys_begin_transaction(LogSystem *log)
{
	return dao_singleRequest(log,"BEGIN");
}

int dao_logsys_end_transaction(LogSystem *log)
{
	return dao_singleRequest(log,"END");
}

int dao_logsys_rollback_transaction(LogSystem *log)
{
	return dao_singleRequest(log,"ROLLBACK");
}

/* this is where we check that label and sqlite db have the same fields/columns' order */
int dao_logsys_sync_label(LogSystem *log)
{
	char *errMsg = NULL;
	char** columnNames;
	int minCol = 1;
	int j = minCol;
	int nrow = 0;
	int ncol = 0;
	int colIndex = 0;
	LogLabel* oldlabel;
	LogField* fields;
	int fieldmax;
	int nfields;
	LogField* oldfield = NULL;
	LogField* newfield = NULL;

	if (dao_logsys_opendata(log)) {
		return -1;
	}


	dao_debug_logsys_warning(log, "SQL Statement : PRAGMA table_info(data)");
	if (sqlite3_get_table(log->datadb_handle,"PRAGMA table_info(data)",&columnNames,&nrow,&ncol,&errMsg) != SQLITE_OK)
	{
		dao_logsys_warning(log,"Column name extraction on database failed: %s", sqlite3_errmsg(log->datadb_handle));
		return -1;
	}
	free(errMsg);
	errMsg = NULL;

	/* to understand what's done here, look at sqlite3_get_table and PRAGMA table_info */
	nfields = 0;
	fieldmax = 64;
	fields = (void *) malloc(fieldmax * sizeof(*fields));
	if (fields == NULL) {
		dao_logsys_warning(log,"Out of memory.");
	}


 	oldlabel = log->label;

	for (j = minCol; j<nrow+1; j++)
	{
		colIndex = 1 + j * ncol;
		if (colIndex >= (nrow+1)*ncol) {
			break;
		}


		for (oldfield=oldlabel->fields; oldfield->type; ++oldfield)
		{
			if (columnNames[colIndex] && (!strcmp(oldfield->name.string,columnNames[colIndex]))
				   || ((!strcmp(oldfield->name.string,"INDEX")) && (!strcmp(columnNames[colIndex],"TX_INDEX")))) {
				
				if (nfields > fieldmax - 2)
				{
					fieldmax += 32;
					fields = (void *) realloc(fields, fieldmax * sizeof(*fields));
					if (fields == NULL) {
						dao_logsys_warning(log,"Out of memory.");
					}
				}
				newfield = &fields[nfields];
				newfield->name.string   = dao_log_strdup(oldfield->name.string);
				newfield->format.string = dao_log_strdup(oldfield->format.string);
				newfield->header.string = dao_log_strdup(oldfield->header.string);
				newfield->type = oldfield->type;
				newfield->size = oldfield->size;
				++nfields;
				columnNames[colIndex] = NULL;
				if (j == minCol) {
					minCol++;
				}
				break;
			}
		}
	}
	/* last field has a zero type */
	(&fields[nfields])->type = 0;
	dao_loglabel_free(oldlabel);
	sqlite3_free_table(columnNames);

	log->label=dao_logsys_arrangefields(fields,1);

	return 0;
}

/* check whether sqlite field affinities match label field type and correct them if necessary
 * this is the case for all bases created with logcreate from [TX5.0, TX5.0.5[ */
int dao_loglabel_checkaffinities(LogSystem* log)
{
	char *errMsg = NULL;
	char** tableInfo = NULL;
	int minCol = 1;
	int j = minCol;
	int nrow = 0;
	int ncol = 0;
	int nameIndex = 0;
	int typeIndex = 0;
	LogField* field = NULL;
	int shouldBeSynced = 0;
	char *sqlStatement = NULL;
	int sqlStatementSize = 0;
	int ioffset = 0;

	if (dao_logsys_opendata(log)) {
		return -1;
	}

	dao_debug_logsys_warning(log, "SQL Statement : PRAGMA table_info(data)");
	if (sqlite3_get_table(log->datadb_handle,"PRAGMA table_info(data)",&tableInfo,&nrow,&ncol,&errMsg) != SQLITE_OK)
	{
		dao_logsys_warning(log,"Column type extraction on database failed: %s", sqlite3_errmsg(log->datadb_handle));
		return -1;
	}
	free(errMsg);
	errMsg = NULL;

	/* to understand what's done here, look at sqlite3_get_table and PRAGMA table_info */
	for (j = minCol; j<nrow+1; j++)
	{
		nameIndex = 1 + j * ncol;
		typeIndex = nameIndex + 1;
		if ((nameIndex >= (nrow+1)*ncol) || (shouldBeSynced != 0)) {
			break;
		}


		for (field=log->label->fields; field->type; ++field)
		{
			if (!tableInfo[nameIndex]) {
				continue;
			}


			if (  (!strcmp(field->name.string,tableInfo[nameIndex]))
			   || ((!strcmp(field->name.string,"INDEX")) && (!strcmp(tableInfo[nameIndex],"TX_INDEX")))
			   )
			{
				if (strcmp(tableInfo[typeIndex],getTypeAffinity(field->type))) {
					shouldBeSynced = 1;
				}


				if (j == minCol) {
					minCol++;
				}
				break;
			}
		}
	}
	sqlite3_free_table(tableInfo);

#define RECREATE_HEADER "\n\
BEGIN;\n\
	DROP TRIGGER dec_count;\n\
	DROP TRIGGER inc_count;\n\
	ALTER TABLE \"data\" RENAME TO \"dataToBeDumped\";\n\
	CREATE TABLE \"data\" (\"TX_INDEX\" INTEGER PRIMARY KEY AUTOINCREMENT"

#define RECREATE_TRAILER ");\n\
	INSERT INTO \"data\" SELECT * FROM \"dataToBeDumped\";\n\
	CREATE TRIGGER dec_count AFTER DELETE ON data \n\
	BEGIN \n\
		UPDATE entry_count SET cnt=cnt-1; \n\
	END; \n\
	CREATE TRIGGER inc_count AFTER INSERT ON data \n\
	BEGIN \n\
		UPDATE entry_count SET cnt=cnt+1; \n\
	END;\
	DROP TABLE \"dataToBeDumped\";\n\
END;"

	if (shouldBeSynced != 0)
	{
		sqlStatementSize = sizeof(RECREATE_HEADER);
		for (field = log->label->fields; field->type; ++field)
		{
			/* (TX_)INDEX allready added, drop it */
			if (strcmp(field->name.string,"INDEX") == 0) {
				continue;
			}

			sqlStatementSize += strlen(field->name.string) + sizeof(",\"\" NUMERIC");
		}
		sqlStatementSize += sizeof(RECREATE_TRAILER);

		sqlStatement = (char *)malloc(sqlStatementSize+1);

		ioffset += sprintf(sqlStatement+ioffset,RECREATE_HEADER);
		for (field = log->label->fields; field->type; ++field)
		{
			/* (TX_)INDEX allready added, drop it */
			if (strcmp(field->name.string,"INDEX") == 0)
			{
				continue;
			}

			ioffset += sprintf(sqlStatement+ioffset,",\"%s\" %s", field->name.string, getTypeAffinity(field->type));
		}
		ioffset += sprintf(sqlStatement+ioffset,RECREATE_TRAILER);

#undef RECREATE_HEADER
#undef RECREATE_TRAILER

		dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
		if (sqlite3_exec(log->datadb_handle,sqlStatement,NULL,NULL,&errMsg) != SQLITE_OK)
		{
			int ret ; 
			dao_logsys_warning(log, "Synchronisation of the sqlite base affinities with the computed label failed (no modifications done): %s", sqlite3_errmsg(log->datadb_handle));
			/* EI-325 try to close correctly the database handler in case it's still used */
			ret = sqlite3_close(log->datadb_handle);
			if ( ret != SQLITE_OK ) 
			{
				int cpt = 0;
				sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
				while ( ret == SQLITE_BUSY && cpt < 4 ) {
					ret = sqlite3_close(log->datadb_handle);
					sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
					cpt++;			
				}
				if (cpt == 4 && ret != SQLITE_OK){
					dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
				} 		
			}
			log->datadb_handle = 0;
			free(sqlStatement);
			free(errMsg);
			sqlStatement = NULL;
			errMsg = NULL;
			return -1;
		}
		free(sqlStatement);
		/* EI-325 free memory allocated by sqlite */
		sqlite3_free(errMsg);
		sqlStatement = NULL;
	}

	return 0;
}

#define BUFSIZE 4096

static void read_line(LogSystem* log,FILE* sqlitestartupfd,char **pline/* address on data allocated by this function */)
{
	static size_t allocedsize = -1;
	char *buf  = NULL;
	char *line = NULL;

	if (pline == NULL) {
		return;
	}

	line = *pline;

	if (line == NULL)
	{
		allocedsize = BUFSIZE;
		line  = calloc(1,allocedsize);
		*pline = line;
	}

	strcpy(line,"");
	buf = malloc(BUFSIZE);
	do
	{
		buf = fgets(buf,BUFSIZE,sqlitestartupfd);
		if (buf != NULL)
		{
			if (strlen(line) + strlen(buf) > allocedsize)
			{
				allocedsize *= 2;
				line = realloc(buf,allocedsize);
			}
			strcat(line,buf);
		}
	} while ((buf !=NULL) && (strrchr(buf,'\n') == NULL));

	if (buf != NULL)
	{
		free(buf);
		buf = NULL;
	}
}

void dao_exec_at_open(LogSystem* log)
{
	FILE * sqlitestartupfd = NULL;
	char * line            = NULL;
	char * errMsg          = NULL;
	int    nstatements     = 0;
	int    returnCode      = 0;
	char* dbName;
	int toFree = 0 ;
	int totLen = 0 ;
	
	if (log == NULL) {
		return;
	}

	/* EI-325 if size > 1024, manually allocate memory */
	/* TX-3158 use totLen to store len */
	if ( ( totLen=(strlen(log->sysname) + strlen(LSFILE_SQLITE_STARTUP)) ) > 1024 )	{
		dbName = malloc(totLen +1);
		dbName = strcpy(dbName,log);	
		dbName = strcat(dbName, LSFILE_SQLITE_STARTUP);
		toFree = 1;
	} else {
		dbName = dao_logsys_filepath(log, LSFILE_SQLITE_STARTUP);
	}
	sqlitestartupfd = fopen(dbName, "r");
	if (toFree) { free(dbName); }
	if (sqlitestartupfd == NULL) {
		return;
	}	

	read_line(log,sqlitestartupfd,&line);
	while ((!feof(sqlitestartupfd)) && (line != NULL) && (returnCode == 0))
	{
		returnCode = dao_singleRequest_opened(log,line);
		nstatements++;
		read_line(log,sqlitestartupfd,&line);
	}

	fclose(sqlitestartupfd);

	if (line != NULL)
	{
		free(line);
		line = NULL;
	}
}

/*JRE 06.15 BG-81*/
int log_daoreadbufsfiltermultenv(LogSystem *log, LogIndexEnv **pIndexes, LogFilter *lf)
{

	/* SELECT env.TX_INDEX FROM 'env'.data WHERE ... UNION SELECT ... WHERE ... */
	char * sqlStatement = NULL;
	char * sqlWhere = NULL;
	char * sqlOrder = NULL;
	char * base = NULL;
	char * env = NULL;
	char * url = NULL;
	char * selectTmp = NULL;
	char * listOrder = NULL;
	
	sqlite3_stmt * sqlite3Statement = NULL;
	
	int ioffsetWhere = 0;
	int ioffsetOrder = 0;
	int ioffsetStatement = 0;
	int returnCode = 0;
	int sqlStatementSize = 0;
	int sqlWhereSize = 0;
	int sqlOrderSize = 0;
	int selectTmpSize = 0;
	int filter_count = 0;
	int selectSize = 0;
	int retour;
	
	FilterRecord * f = NULL;
	union FilterPrimary * p = NULL;
	
   	int cpt=0;
			
	LogSystem * logsys;
	LogEnv log_env;

	/* page handling */
	int offset = -1;
	int max_count = -1;

	int numberOfResults = 0;

	/* logsys_dao_entry_count(log,lf) is too costly
	 * use fixed first and realloc if necessary */
	maxcount = 100000;
	
	logsys = log;
	log_env = logsys->listSysname;
	
	if (dao_logsys_opendata(log)) {
		retour = -1;
	}
	
	if (lf)
	{
		/* "SELECT env.TX_INDEX FROM 'env'.data WHERE " + NL */
		/* + AND betwwen filters (filter_count - 1, always at least one filter) */
		/* + UNION SELECT ....*/
		sqlWhereSize = sizeof(" WHERE ") + sizeof (" AND ") * (lf->filter_count - 1);
		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
		{
			if (f->field != NULL && f->field->type != PSEUDOFIELD_OWNER) {
				
				/* OR at the beginning except for first primary */
				sqlWhereSize += sizeof("(  )") + sizeof(" OR ") * (f->n_primaries -1) + sizeof("\"\"") * f->n_primaries;

				for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p)
				{
					if (f->op != OP_NONE) {
						
						sqlWhereSize += strlen(f->name) + sizeof("TX_") + MAX_OP_SIZE;
						switch (f->field->type)
						{
							case FIELD_TEXT:
								/* we have at most f->field->size quote to escape ! + the quotes themselves */
								sqlWhereSize += strlen(p->text) * sizeof("\'") + sizeof("''") + sizeof("ESCAPE '\\'");
								break;
							default:
								sqlWhereSize += 2 * MAX_INT_DIGITS + sizeof(" NOT") + sizeof(" BETWEEN  AND ");
								break;
						}
					}
					filter_count++;
				}
			}
		}
		sqlStatementSize = sqlWhereSize * filter_count;

		if (lf->sort_key)
		{
			sqlOrderSize += sizeof(" ORDER BY ''") + strlen(lf->sort_key) + sizeof("TX_"); /* TX_ in case of INDEX */
		}
		else
		{
			/* if we have a sort order but no sort key, use (TX_)INDEX as a sort_key */
			sqlOrderSize += sizeof(" ORDER BY TX_INDEX");
		}

		/* lf->sort_order : either we have " DESC", " ASC" or nothing */
		sqlOrderSize += sizeof(" DESC");
		sqlOrderSize += sizeof(" LIMIT NNNNNNN ") + sizeof(" OFFSET NNNNNNNNNNN ");
		
		if (lf->sort_key)
		{
			listOrder = (char*)malloc(strlen(strPrefixColName(lf->sort_key)) * sizeof(char) + strlen(lf->sort_key) * sizeof(char) + 3);
			sprintf(listOrder, ",\"%s%s\" ", strPrefixColName(lf->sort_key), lf->sort_key);

			sqlOrder = (char *)malloc(sqlOrderSize+1);
			
			ioffsetOrder += sprintf(sqlOrder+ioffsetOrder," ORDER BY \"%s%s\"",strPrefixColName(lf->sort_key),lf->sort_key);

		}

		if ((!lf->sort_key) && (lf->sort_order != 0))
		{
			ioffsetOrder += sprintf(sqlOrder+ioffsetOrder," ORDER BY TX_INDEX");
		}
		switch (lf->sort_order)
		{
			case -1: ioffsetOrder += sprintf(sqlOrder+ioffsetOrder," DESC"); break;
			case  1: ioffsetOrder += sprintf(sqlOrder+ioffsetOrder," ASC");  break;
			default: /* no order for the filter, take default one */   break;
		}

        if (NULL == getenv("LOGVIEW_NO_PAGECOUNT") || strcmp(getenv("LOGVIEW_NO_PAGECOUNT"), "1") != 0) {
            if (lf->max_count > 0) {
                max_count = lf->max_count;
                ioffsetOrder += sprintf(sqlOrder+ioffsetOrder, " LIMIT %d", lf->max_count);
            }
            if (lf->offset > 0) {
                offset = lf->offset;
                ioffsetOrder += sprintf(sqlOrder+ioffsetOrder, " OFFSET %d", lf->offset);
            }
		} else {
            if (lf->max_count > 0) {
                max_count = lf->max_count;
                ioffsetOrder += sprintf(sqlOrder+ioffsetOrder, " LIMIT %d", lf->max_count);
            }
            if (lf->offset > 0) {
                offset = 0;
                ioffsetOrder += sprintf(sqlOrder+ioffsetOrder, " OFFSET %d", lf->offset);
            }
		}
		sqlWhere = (char *)malloc(sqlWhereSize+1);
		strcpy(sqlWhere, "");
		addSqlWhere(&sqlWhere, &ioffsetWhere, lf, filter_count);

	}
		
	do {	/*JRE TX-2767*/
	
		env = log_env->env;
		base = log_env->base;
		
		/*BG-198 BEGIN*/
		if(!check_filter_env(lf, env)){
			log_env = log_env->pSuivant;
			continue;
		}
		/*BG-198 END*/
		
		selectSize = sizeof("SELECT \"") + strlen(env) * sizeof(char) + sizeof("\" || TX_INDEX") + sizeof(" FROM ") + strlen(env) * sizeof(char) + sizeof(".data") + 2 * sizeof("'");
		if (lf && listOrder != NULL) {
			selectSize += strlen(listOrder) * sizeof(char);
		}

		if (cpt == 0) {
      /* 19/05/2017 CGA ----TX-2973---- */
			sqlStatement = (char*)malloc(selectSize + sizeof("SELECT * FROM (")+1);
      /* ----Fin TX-2973---- */
			strcpy(sqlStatement, "");
		} else {
      /* 19/05/2017 CGA ----TX-2973---- */
			sqlStatement = (char*)realloc(sqlStatement, strlen(sqlStatement) * sizeof(char) + selectSize + sizeof(" UNION ")+1);
      /* ----Fin TX-2973---- */
		}
		
		/*ATTACH DATABASE '.....sqlite' as 'env'*/
		url = (char *)malloc(sizeof("ATTACH database '") + strlen(base) * sizeof(char) + sizeof("/.sqlite' as ") + strlen(env) * sizeof(char) + 2 * sizeof("\"") + 1);
		strcpy(url, "");
		sprintf(url, "ATTACH database '%s/.sqlite' as '%s'", base, env);
		writeLog(LOGLEVEL_BIGDEVEL, "requete sqlite: %s", url);
		dao_debug_logsys_warning(log, "SQL Statement : %s", url);
		sqlite3_exec(log->datadb_handle,url,NULL,NULL,NULL);
		
		if (cpt == 0) {
			strcat(sqlStatement, "SELECT * FROM (");
		} else {
			strcat(sqlStatement, " UNION ");
		}
		if (lf && listOrder != NULL) {
			sprintf(sqlStatement + strlen(sqlStatement) * sizeof(char), "SELECT \"%s.\" || TX_INDEX%s FROM '%s'.data ", env, listOrder, env);
		}
		else {
			sprintf(sqlStatement + strlen(sqlStatement) * sizeof(char), "SELECT \"%s.\" || TX_INDEX FROM '%s'.data ", env, env);
		}

		if (lf && strlen(sqlWhere) != 0) {
			sqlStatement = (char*)realloc(sqlStatement, strlen(sqlStatement) * sizeof(char) + strlen(sqlWhere) * sizeof(char) + 1);
			strcat(sqlStatement, sqlWhere);
		}
		
		cpt++;
		/*JRE TX-2767*/
		log_env = log_env->pSuivant;
		
	} while (log_env != NULL);
	/*End TX-2767*/
	
	if (sqlStatement != NULL) {
		strcat(sqlStatement, ")");
	}

	if (url) { free(url); }
	if (selectTmp) { free(selectTmp); }
	
	if (lf && sqlOrder != NULL) 
	{
		sqlStatementSize = strlen(sqlStatement) * sizeof(char) + sqlOrderSize;
		sqlStatement = (char *)realloc(sqlStatement, sqlStatementSize+1);

		strcat(sqlStatement,sqlOrder);
	}

	dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);

	/* possibly all entries may match the filter
	 * but as we store only the indexes, it is not that big.
	 * besides we shrink it at the end to fit the resultset. */
	
	writeLog(LOGLEVEL_BIGDEVEL, "requete sqlite: %s", sqlStatement);
	dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
	returnCode = sqlite3_prepare(log->datadb_handle,sqlStatement,-1,&sqlite3Statement,NULL);
	writeLog(LOGLEVEL_BIGDEVEL, "retour: %i", returnCode);

	if (returnCode != SQLITE_OK) {
		dao_logsys_warning(log, "Multiple Index Read operation in sqlite database failed with error: %s", sqlite3_errmsg(log->datadb_handle));
	}

	sqlStatement = NULL;  
	numberOfResults = 0;
	if (max_count != -1) /* fix maximum of entries */
	{
		*pIndexes = dao_log_malloc((max_count + 1) * sizeof(**pIndexes));
	}
	else /* may vary accordingly to the number of entries */
	{
		*pIndexes = dao_log_malloc((maxcount + 1) * sizeof(**pIndexes));
	}

	if (returnCode == SQLITE_OK)
	{
		while ( sqlite3_step(sqlite3Statement) == SQLITE_ROW )
		{
			
			if (numberOfResults != max_count)
			{
				/* in case, we have a looot of entry matching and no max_count */
				if ((max_count == -1) && (numberOfResults > maxcount))
				{
					/* XXX should we check this against MAX_INT ??? */
					maxcount = maxcount * 2;
					*pIndexes = (LogIndexEnv *) dao_log_realloc(*pIndexes,(maxcount + 1) * sizeof(**pIndexes));
				}

				(*pIndexes)[numberOfResults] = strdup((char *) sqlite3_column_text(sqlite3Statement,0));
				numberOfResults++;
			}
		}

		log->indexes_count = logsys_dao_entry_count_env(log, lf);
		sqlite3_finalize(sqlite3Statement);
		retour = numberOfResults;
	}
	else
	{
		log->indexes_count = 0;
		retour = -1;
	}
	
	if (sqlStatement) { free(sqlStatement); }
	if (sqlWhere) { free(sqlWhere); }
	if (sqlOrder) { free(sqlOrder); }
	
	return retour;
	
}

int dao_logsys_affect_sysname(LogSystem *log, LogEnv lsenv)
{
	
	log->listSysname = lsenv;	

	return(1);
	
}

int log_daoreadbuf_env(LogEntryEnv *entry)
{

	/* SELECT data.* FROM 'env'.data WHERE TX_INDEX=... */
	char *sqlStatement = NULL;
	sqlite3_stmt *sqlite3Statement = NULL;
	int returnCode = -1;
	int result = -1;
	char* env;
	char* cidx;
	char* idx;
	char* temp;

	LogField *field;
	char *value = NULL;
	int i = 0;
	LogLabel *label;
	int retour;

	if (!entry) {
		retour = result;
	}

	label = entry->logsys->label;

	if (dao_logsys_opendata(entry->logsys)) {
		retour = result;
	}
	temp = strtok(entry->idx, ".");
	env = strdup(temp);
	temp = strtok(NULL, ".");
	idx = strdup(temp);	
  
	sqlStatement = sqlite3_mprintf("SELECT data.* FROM '%s'.data WHERE TX_INDEX=%s", env, idx);	
	dao_debug_logsys_warning(entry->logsys, "SQL Statement : %s", sqlStatement);
	returnCode = sqlite3_prepare(entry->logsys->datadb_handle,sqlStatement,-1,&sqlite3Statement,NULL);
	sqlite3_free(sqlStatement);
	sqlStatement = NULL;
  
	if (returnCode == SQLITE_OK)
	{
		/* statement OK (but still result == number of row/entry == 0) */
		result++;
		/* try fetching the entry (row == 1 if OK) */
		if (sqlite3_step(sqlite3Statement) == SQLITE_ROW)
		{
			/* fieldsnames not used */
			/* write the right value at the right place in the record buffer */
			for(i=0; i<sqlite3_data_count(sqlite3Statement); i++)
			{
				field = &label->fields[i];

				/* buffer allready allocated in logentry_alloc_skipclear */
				switch (field->type)
				{
					default:
						/* XXX should not happen ??? */
						*(Integer *) (entry->record->buffer + field->offset) = sqlite3_column_int(sqlite3Statement,i);
						break;
					case FIELD_NUMBER:
						*(Number *) (entry->record->buffer + field->offset) = sqlite3_column_double(sqlite3Statement,i);
						break;
					case FIELD_TIME:
						{
							time_t tms[2];

							value = (char *) sqlite3_column_text(sqlite3Statement,i);
							/* This uses only the start-time of the range, i.e. 1.2.1999 means 1.2.1999 00:00:00 AM */
							if (value == NULL) {
								tr_parsetime("", tms);
							} else {
								tr_parsetime(value, tms);
							}

							*(TimeStamp *) (entry->record->buffer + field->offset) = tms[0];
							/* P.S.: do not free(value), it is garbaged by sqlite3 functions */
							value = NULL;
						}
						break;
					case FIELD_INDEX:
					case FIELD_INTEGER:
						*(Integer *) (entry->record->buffer + field->offset) = sqlite3_column_int(sqlite3Statement,i);
						break;
					case FIELD_TEXT:
						value = (char *) sqlite3_column_text(sqlite3Statement,i);
						strncpy(entry->record->buffer + field->offset,value?value:"",field->size);
						/* P.S.: do not free(value), it is garbaged by sqlite3 functions */
						value = NULL;
						break;
				}
			}

			result++;
			sqlite3_finalize(sqlite3Statement);
		}
	}
	
	dao_debug_logsys_warning(entry->logsys, "Single Read Operation return : %d", result);
	retour = result;
	
	return retour;
}

int logsys_dao_entry_count_env(LogSystem *log, LogFilter *lf)
{

	/* SELECT sum(nb) FROM (SELECT COUNT(TX_INDEX) as nb FROM data WHERE ... UNION ALL SELECT ...) */
	char *sqlStatement = NULL;
	char *sqlWhere = NULL;
	int sqlStatementSize = 0;
	int sqlWhereSize = 0;
	int ioffset = 0;
	sqlite3_stmt *sqlite3Statement = NULL;
	LogEnv log_env;
	char * base, *env;
	LogSystem * logsys;
	int cpt=0;
	char * url;

	FilterRecord *f;
	int filter_count = 0;
	union FilterPrimary *p;

	int numberOfEntries = -1;
	int returnCode = 0;
	int retour;

	if (dao_logsys_opendata(log))
	{
		retour = -1;
	}
		
	if (lf) 
	{
		sqlWhereSize = sizeof("WHERE ") + sizeof (" AND ") * (lf->filter_count - 1);
		for (f = lf->filters; f < &lf->filters[lf->filter_count]; ++f)
		{
			if (f->field != NULL && f->field->type != PSEUDOFIELD_OWNER) {
			
				/* OR at the beginning except for first primary */
				sqlWhereSize += sizeof("(  )") + sizeof(" OR ") * (f->n_primaries -1) + sizeof("\"\"") * f->n_primaries;

				for (p = f->primaries; p < &f->primaries[f->n_primaries]; ++p)
				{
					if (f->op != OP_NONE) {
					
						sqlWhereSize += strlen(f->name) + sizeof("TX_") + MAX_OP_SIZE;
						switch (f->field->type)
						{
							case FIELD_TEXT:
								/* we have at most f->field->size quote to escape ! + the quotes themselves */
								sqlWhereSize += strlen(p->text) * sizeof("\'") + sizeof("''") + sizeof("ESCAPE '\\'");
								break;
							default:
								sqlWhereSize += 2 * MAX_INT_DIGITS + sizeof(" NOT") + sizeof(" BETWEEN  AND ");
								break;
						}
					}
				}
				filter_count++;
			}
		}
	
		if (filter_count != 0) {
			sqlStatementSize = sqlWhereSize * filter_count;

			sqlWhere = (char*)malloc(sqlWhereSize+1);
			
			strcpy(sqlWhere, "");

			addSqlWhere(&sqlWhere, &ioffset, lf, filter_count);
		}
		
	}
	
	sqlStatement = "";
	logsys = log;
	log_env = logsys->listSysname;
	do {	/*JRE TXJ-2767*/
		env = log_env->env;
		base = log_env->base;
	
		/*BG-198 BEGIN*/
		if(!check_filter_env(lf, env)){
			log_env = log_env->pSuivant;
			continue;
		}
		/*BG-198 END*/
		
		url = (char *)malloc(sizeof("ATTACH database '") + strlen(base) * sizeof(char) + sizeof("/.sqlite' as ") + strlen(env) * sizeof(char) + 2 * sizeof("\"") + 1);
		strcpy(url, "");
		sprintf(url, "ATTACH database '%s/.sqlite' as '%s'", base, env);
		writeLog(LOGLEVEL_BIGDEVEL, "requete sqlite: %s", url);
		dao_debug_logsys_warning(log, "SQL Statement : %s", url);
		sqlite3_exec(log->datadb_handle,url,NULL,NULL,NULL);
	
		if (cpt == 0) {
		
			if (lf) {
				sqlStatement = (char *)malloc(sizeof("SELECT SUM(nb) FROM (SELECT COUNT(*) as nb FROM ") + strlen(env) * sizeof(char) + sizeof(".data ") + 2 * sizeof("'") + 1);
				strcpy(sqlStatement, "");
				sprintf(sqlStatement, "SELECT SUM(nb) FROM (SELECT COUNT(*) as nb FROM '%s'.data ", env);
			} else {
				sqlStatement = (char *)malloc(sizeof("SELECT SUM(nb) FROM (SELECT cnt as nb FROM ") + strlen(env) * sizeof(char) + sizeof(".entry_count ") + 2 * sizeof("'") + 1);
				strcpy(sqlStatement, "");
				sprintf(sqlStatement, "SELECT SUM(nb) FROM (SELECT cnt as nb FROM '%s'.entry_count ", env);
			}
		}
		else {
			sqlStatement = (char *)realloc(sqlStatement, strlen(sqlStatement) * sizeof(char) + sizeof(" UNION ALL SELECT cnt as nb FROM ") + strlen(env) * sizeof(char) + sizeof(".entry_count ") + 2 * sizeof("'") + 1);
			if (lf) {
				sprintf(sqlStatement + strlen(sqlStatement) * sizeof(char), " UNION ALL SELECT COUNT(*) as nb FROM '%s'.data ", env);
			} else {
				sprintf(sqlStatement + strlen(sqlStatement) * sizeof(char), " UNION ALL SELECT cnt as nb FROM '%s'.entry_count ", env);
			}
		}

		if (sqlWhere != NULL) {
			sqlStatement = (char*)realloc(sqlStatement, strlen(sqlStatement) * sizeof(char) + strlen(sqlWhere) * sizeof(char) + 1);
			strcat(sqlStatement, sqlWhere);
		}
		
		cpt++;
		/*JRE TX-2767*/
		log_env = log_env->pSuivant;
		
	} while (log_env != NULL);
	/*Fin TX-2767*/
	
	if (sqlStatement != NULL) {
		sqlStatement = (char *)realloc(sqlStatement, strlen(sqlStatement) * sizeof(char) + sizeof(")") + 1);
		strcat(sqlStatement, ")");
	}
	free(url);
	
	if (!lf) /* no filter, return all */
	{
		dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
		returnCode = sqlite3_prepare(log->datadb_handle,sqlStatement,-1,&sqlite3Statement,NULL);
	}
	else {
			
		/* contrary to sqlitereadbufs : */
		/* as we just count, we do not care about order */
		/* we do want all possibles entries
		 * so ignore here also max_count and offset
		 * else count would be useless ! */

		/* sql statement ready */
		writeLog(LOGLEVEL_BIGDEVEL, "requete sqlite: %s", sqlStatement);
		dao_debug_logsys_warning(log, "SQL Statement : %s", sqlStatement);
		returnCode = sqlite3_prepare(log->datadb_handle,sqlStatement,-1,&sqlite3Statement,NULL);
		writeLog(LOGLEVEL_BIGDEVEL, "retour requete: %i", returnCode);
		free(sqlStatement);
		sqlStatement = NULL;
	}

	if ((returnCode == SQLITE_OK) && (sqlite3_step(sqlite3Statement) == SQLITE_ROW))
	{
		numberOfEntries = sqlite3_column_int(sqlite3Statement,0);
		sqlite3_finalize(sqlite3Statement);
	}
	else
	{
		/* numberOfEntries == -1 */
		dao_logsys_warning(log, "Count of entries operation in SQLite database failed : %s", sqlite3_errmsg(log->datadb_handle));
	}
	
	free(sqlStatement);
	
	retour = numberOfEntries;
	
	return retour;
}

/* Format of input file:
 * FIELDNAME=VALUE */
LogEnv dao_logenv_read(FILE *fp)
{
	char buf[1024];
	char *compteur;
	char *namedata, *data;
	int cptenv = 0;
	int cptbase = 0;
	
	LogEnv log_env = (LogEnv)malloc(sizeof(struct logenv));
	LogEnv nouvelElement;
	log_env = NULL;
	

	while (fp && fgets(buf, sizeof(buf), fp)) {
	
		if (*buf != '\n' && *buf != '#') {	
		
			namedata = strtok(buf, "=");
			data = strtok(NULL, "\n");
			if (strstr(namedata,"ENV") != NULL) {
				cptenv = strtol(namedata + strlen("ENV"), (char**)NULL, 10);		
				if (cptenv == 0) {
					fprintf(stderr, "Malformed file of environments\n");
					exit(1);
				}
				nouvelElement = (LogEnv)malloc(sizeof(struct logenv));
				nouvelElement->env = dao_log_strdup(data);
			} else if (strstr(namedata,"BASE") != NULL) {
				cptbase = strtol(namedata + strlen("BASE"), (char**)NULL, 10);
				if (cptbase == 0 || cptenv != cptbase) {
					fprintf(stderr, "Malformed file of environments\n");
					exit(1);
				}
				nouvelElement->base = dao_log_strdup(data);
				nouvelElement->pSuivant = NULL;
			
				if(log_env == NULL) {
					log_env = nouvelElement;
				}
				else {
					logenv * temp = log_env;
					while (temp->pSuivant != NULL) {
						temp = temp->pSuivant;
					}
					temp->pSuivant = nouvelElement;
				}

			
			}
		}
	}
	
	return log_env;
}

/* TX-3025 function to attach the databases in a multibase mode */
int log_dao_attach_database_env(LogSystem *log){
	int returnCode = 0;
	
	LogEnv log_env;
	char * base, *env;
	LogSystem * logsys;
	char * url;
	
	if (dao_logsys_opendata(log))
	{
		returnCode = -1;
	}
	logsys = log;
	log_env = logsys->listSysname;
	do {	
		env = log_env->env;
		base = log_env->base;
		url = (char *)malloc(sizeof("ATTACH database '") + strlen(base) * sizeof(char) + sizeof("/.sqlite' as ") + strlen(env) * sizeof(char) + 2 * sizeof("\"") + 1);
		strcpy(url, "");
		sprintf(url, "ATTACH database '%s/.sqlite' as '%s'", base, env);
		writeLog(LOGLEVEL_BIGDEVEL, "requete sqlite: %s", url);
		dao_debug_logsys_warning(log, "SQL Statement : %s", url);
		sqlite3_exec(log->datadb_handle,url,NULL,NULL,NULL);
		log_env = log_env->pSuivant;
		
	} while (log_env != NULL);
	return returnCode;
}
/*End JRE*/

/*BG-198 BEGIN*/
bool check_filter_env(LogFilter *lf, char *env){
	bool env_ok = true;
	
	if (lf->env_val){
		switch(lf->env_op){
			case OP_EQ:
				if (strcmp(lf->env_val, env) != 0){
					env_ok = false;
				}
				break;
			case OP_NEQ:
				if (strcmp(lf->env_val, env) == 0){
					env_ok = false;
				}
				break;
		}
	}
	return env_ok;
}
/*BG-198 END*/

