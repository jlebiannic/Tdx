/*========================================================================
        E D I S E R V E R

        File:		makelist.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Reading and writing logsystem stuff.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: clusterlist.c 55487 2020-05-06 08:56:27Z jlebiannic $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.01 03.04 96/JN	Clustering reads.
========================================================================*/

#include <stdio.h>
#include <sys/types.h>

#include "logsystem.dao.h"
#include "daservice.h"

LogIndex *dao_logsys_list_indexed(LogSystem *log, LogFilter *lf)
{
    LogIndex *indexes;

    int matchingIndexes; /* number of matching entries */

    /* retrieve the indexes comforming to the filter 
     * allocation of indexes is done here */

	// Jira TX-3199 DAO
	// matchingIndexes = log_daoreadbufsfiltered(log, &indexes, lf);
	matchingIndexes = Service_findEntries(log, &indexes, lf);

    if (matchingIndexes <= 0)
    {
        /* error while exec the request ... 
		 * or no matching entries */
        free(indexes);
        return NULL;
    }

    indexes[matchingIndexes] = 0;

    /* Less than assumed amount should be returned, shrink.
     * libmalloc shouldn't do any copying, just bookkeep. */
    indexes = dao_log_realloc(indexes, (matchingIndexes + 1) * sizeof(*indexes));

	return indexes;
}

/*JRE 06.15 BG-81*/
LogIndexEnv *dao_logsys_list_indexed_env(LogSystem *log, LogFilter *lf)
{
    LogIndexEnv *indexes;
    LogIndexEnv *retour;

    int matchingIndexes; /* number of matching entries */

    /* retrieve the indexes comforming to the filter 
     * allocation of indexes is done here */
    matchingIndexes = log_daoreadbufsfiltermultenv(log, &indexes, lf);

    if (matchingIndexes <= 0)
    {
        /* error while exec the request ... 
		 * or no matching entries */
        free(indexes);
        retour = NULL;
    }
    else {
    	indexes[matchingIndexes] = "0";
    	
    	/* Less than assumed amount should be returned, shrink.
     	* libmalloc shouldn't do any copying, just bookkeep. */
    	indexes = dao_log_realloc(indexes, (matchingIndexes + 1) * sizeof(*indexes));
    	
    	retour = indexes;
    }
    	
	return retour;
}

int dao_logsys_entry_count(LogSystem* log, LogFilter* filter)
{
	if (log->listSysname == NULL) {
		log->indexes = dao_logsys_list_indexed(log,filter);
	}

	return log->indexes_count;
}
/*End JRE*/
