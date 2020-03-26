#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: external_trigger.c 53221 2017-06-22 13:20:01Z tcellier $")

/*
	v1.1 18/05/2017 TCE(CG) EI-325 free some memory for error message
*/

#include <stdio.h>
#include <limits.h>
#include "private.h"
#include "logsystem.sqlite.h"
/*
 * external program running of database actions are setted via trigger
 * with a when condition (trigger_check_*) in needed cases
 * and a subsequent action (trigger_prog_*) which call the method to run the actual user program
 */



void trigger_check_threshold(sqlite3_context* stmt,int nvalue,sqlite3_value** values)
{
    LogSystem *log = (LogSystem *) sqlite3_user_data(stmt);
	UNUSED_PARAM(nvalue);

    sqlite_debug_logsys_warning(log, "threshold trigger check");
    /* do we need threshold cleanup ? */
    /* if no maxcount defined use INT_MAX which is our limit for the indexes at the moment */
    if (((log->label->maxcount != 0) && (sqlite3_value_int(values[0]) > log->label->maxcount - log->label->cleanup_threshold))
     || (sqlite3_value_int(values[0]) > INT_MAX - log->label->cleanup_threshold)
       )
        sqlite3_result_int(stmt, 1);
    else
        sqlite3_result_null(stmt);
}
void trigger_check_cleanup(sqlite3_context* stmt,int nvalue,sqlite3_value** values)
{
    LogSystem *log = (LogSystem *) sqlite3_user_data(stmt);
	UNUSED_PARAM(nvalue);

    sqlite_debug_logsys_warning(log, "cleanup trigger check");
    /* do we need a cleanup ? */
    if (log->label->maxcount <= sqlite3_value_int(values[0]))
        sqlite3_result_int(stmt, 1);
    else
        sqlite3_result_null(stmt);
}
void trigger_prog_remove(sqlite3_context* stmt,int nvalue,sqlite3_value** values)
{
    LogSystem *log = (LogSystem *) sqlite3_user_data(stmt);
    int returnCode = 0;
	UNUSED_PARAM(nvalue);

    sqlite_debug_logsys_warning(log, "remove prog check index=%d", sqlite3_value_int(values[0]) );
    returnCode = sqlite_logsys_runprog(log,LSFILE_REMOVEPROG,(LogIndex) sqlite3_value_int(values[0]));
    if (returnCode != 0)
        sqlite_logsys_warning(log, "%s terminated with error code %d",LSFILE_REMOVEPROG,returnCode);
    sqlite3_result_int(stmt, returnCode);
}
void trigger_prog_add(sqlite3_context* stmt,int nvalue,sqlite3_value** values)
{
    LogSystem *log = (LogSystem *) sqlite3_user_data(stmt);
    int returnCode = 0;
	UNUSED_PARAM(nvalue);
    
    sqlite_debug_logsys_warning(log, "add prog check index=%d", sqlite3_value_int(values[0]) );
    returnCode = sqlite_logsys_runprog(log,LSFILE_ADDPROG,(LogIndex) sqlite3_value_int(values[0]));
    if (returnCode != 0)
        sqlite_logsys_warning(log, "%s terminated with error code %d",LSFILE_ADDPROG,returnCode);
    sqlite3_result_int(stmt, returnCode);
}
void trigger_prog_threshold(sqlite3_context* stmt,int nvalue,sqlite3_value** values)
{
    LogSystem *log = (LogSystem *) sqlite3_user_data(stmt);
    int returnCode = 0;
	UNUSED_PARAM(nvalue);
	UNUSED_PARAM(values);

    sqlite_debug_logsys_warning(log, "threshold prog check");
    returnCode = sqlite_logsys_runprog(log,LSFILE_THRESHOLDPROG,0);
    if (returnCode != 0)
        sqlite_logsys_warning(log, "%s terminated with error code %d",LSFILE_THRESHOLDPROG,returnCode);
    sqlite3_result_int(stmt, returnCode);
}
void trigger_prog_cleanup(sqlite3_context* stmt,int nvalue,sqlite3_value** values)
{
    LogSystem *log = (LogSystem *) sqlite3_user_data(stmt);
    int returnCode = 0;
	UNUSED_PARAM(nvalue);
	UNUSED_PARAM(values);

    sqlite_debug_logsys_warning(log, "cleanup prog check");
    /* we have a cleanup program : use it */
    /* LM : cleanupprog desactive car la base est lockee */
    /* if (log->label->run_cleanupprog)   */
    if (0)
    {
        returnCode = sqlite_logsys_runprog(log,LSFILE_CLEANUPPROG,0);
	/* there have been problem running cleanup prog, run default cleanup */
        if (returnCode != 0)
	{
            sqlite_logsys_warning(log, "%s terminated with error code %d",LSFILE_CLEANUPPROG,returnCode);
	    if (log->label->cleanup_count)
	    {
		if (returnCode = sqlite_logsys_defcleanup(log))
		{
		  sqlite3_result_null(stmt);
		  return;  
		}
	    }
	}else
	    /* if cleanup prog did n't clean any entry, run default one */
	    if ( log->label->cleanup_count && (sqlite_get_removed_entries_count(log)==0))
	    {
	      if (returnCode = sqlite_logsys_defcleanup(log))
	      {
		sqlite3_result_null(stmt);
		return;
	      }
	      
	    }
	    
    }
    else 
	if (log->label->cleanup_count)
	{
	  if (returnCode = sqlite_logsys_defcleanup(log))
	  {
	    sqlite3_result_null(stmt);
	    return;  
	  }
	}
	else{
	    sqlite3_result_null(stmt);
	    return;
	}
    sqlite3_result_int(stmt, returnCode);
}

/* set up triggers for external actions on the database */
int logsys_trigger_setup(LogSystem *log,char **pErrMsg)
{
    if (log->label->run_removeprog)
    {
        if (sqlite3_create_function(log->datadb_handle,"trigger_prog_remove",1,SQLITE_ANY,(void*)log,trigger_prog_remove,NULL,NULL) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "remove trigger check function setup failed");
            return -1;
        }
        if (sqlite3_exec(log->datadb_handle,"\
CREATE TEMP TRIGGER delete_trigger BEFORE DELETE ON data \
BEGIN \
    SELECT trigger_prog_remove(OLD.TX_INDEX); \
END; \
",NULL,NULL,pErrMsg) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "remove trigger setup failed");
			/* TCE(CG) EI-325 free error message memory allocation made by sqlite3*/
			if (*pErrMsg != NULL){ sqlite3_free(*pErrMsg); *pErrMsg = NULL;}
            return -1;
        }
		/* TCE(CG) EI-325 free error message memory allocation made by sqlite3*/
		if (*pErrMsg != NULL){ sqlite3_free(*pErrMsg); *pErrMsg = NULL;}
    }
	

    if (log->label->run_addprog)
    {
        if (sqlite3_create_function(log->datadb_handle,"trigger_prog_add",1,SQLITE_ANY,(void*)log,trigger_prog_add,NULL,NULL) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "add trigger check function setup failed");
            return -1;
        }
        if (sqlite3_exec(log->datadb_handle,"\
CREATE TEMP TRIGGER insert_trigger AFTER INSERT ON data \
BEGIN \
    SELECT trigger_prog_add(NEW.TX_INDEX); \
END; \
",NULL,NULL,pErrMsg) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "add trigger setup failed");
			/* TCE(CG) EI-325 free error message memory allocation made by sqlite3*/
			if (*pErrMsg != NULL){ sqlite3_free(*pErrMsg); *pErrMsg = NULL;}
            return -1;
        }
		/* TCE(CG) EI-325 free error message memory allocation made by sqlite3*/
		if (*pErrMsg != NULL){ sqlite3_free(*pErrMsg); *pErrMsg = NULL;}
    }

    if ((log->label->cleanup_threshold != 0) && (log->label->run_thresholdprog))
    {
        if (sqlite3_create_function(log->datadb_handle,"trigger_check_threshold",1,SQLITE_ANY,(void*)log,trigger_check_threshold,NULL,NULL) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "threshold trigger check function setup failed");
            return -1;
        }
        if (sqlite3_create_function(log->datadb_handle,"trigger_prog_threshold" ,0,SQLITE_ANY,(void*)log,trigger_prog_threshold ,NULL,NULL) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "threshold trigger exec function setup failed");
            return -1;
        }
        if (sqlite3_exec(log->datadb_handle,"\
CREATE TEMP TRIGGER threshold_trigger BEFORE INSERT ON data \
WHEN (SELECT trigger_check_threshold(cnt) FROM entry_count) NOTNULL \
BEGIN \
    SELECT trigger_prog_threshold(); \
END; \
",NULL,NULL,pErrMsg) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "threshold trigger setup failed");
			/* TCE(CG) EI-325 free error message memory allocation made by sqlite3*/
			if (*pErrMsg != NULL){ sqlite3_free(*pErrMsg); *pErrMsg = NULL;}
            return -1;
        }
		/* TCE(CG) EI-325 free error message memory allocation made by sqlite3*/
		if (*pErrMsg != NULL){ sqlite3_free(*pErrMsg); *pErrMsg = NULL;}
    }
    if (log->label->maxcount != 0) 
    {
        if (sqlite3_create_function(log->datadb_handle,"trigger_check_cleanup",1,SQLITE_ANY,(void*)log,trigger_check_cleanup,NULL,NULL) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "cleanup trigger check function setup failed");
            return -1;
        }
        if (sqlite3_create_function(log->datadb_handle,"trigger_prog_cleanup" ,0,SQLITE_ANY,(void*)log,trigger_prog_cleanup ,NULL,NULL) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "cleanup trigger exec function setup failed");
            return -1;
        }
        if (sqlite3_exec(log->datadb_handle,"\
CREATE TEMP TRIGGER cleanup_trigger BEFORE INSERT ON data \
WHEN (SELECT trigger_check_cleanup(cnt) FROM entry_count) NOTNULL \
BEGIN \
  SELECT CASE \
    WHEN ((SELECT trigger_prog_cleanup()) IS NULL) \
	THEN RAISE(ABORT,\"insert failed because maxcount reached \") \
  END; \
END; \
",NULL,NULL,pErrMsg) != SQLITE_OK)
        {
            sqlite_debug_logsys_warning(log, "cleanup trigger setup failed");
			/* TCE(CG) EI-325 free error message memory allocation made by sqlite3*/
			if (*pErrMsg != NULL){ sqlite3_free(*pErrMsg); *pErrMsg = NULL;}
            return -1;
        }
		/* TCE(CG) EI-325 free error message memory allocation made by sqlite3*/
		if (*pErrMsg != NULL){ sqlite3_free(*pErrMsg); *pErrMsg = NULL;}
    }

    return 0;
}

