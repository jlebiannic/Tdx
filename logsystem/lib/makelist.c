/*========================================================================
        E D I S E R V E R

        File:		makelist.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Reading and writing logsystem stuff.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: makelist.c 47371 2013-10-21 13:58:37Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
       03.04.96/JN	Carved off the other list-maker to clusterlist.c
  4.00 15.12.05/CD    use lastindex instead of numused  
                      to prevent cases when numused is not uptodate 
========================================================================*/

#include <stdio.h>
#include <sys/types.h>

#include "logsystem.sqlite.h"

LogEntry **sqlite_logsys_makelist(LogSystem *log, LogFilter *lf)
{
	LogEntry **entries;
    LogIndex *indexes;
    int matchingIndexes = -1;

    /* retrieve the indexes comforming to the filter 
     * allocation of indexes is done here */
    matchingIndexes = log_sqlitereadbufsfiltered(log, &indexes, lf);

    if (matchingIndexes <= 0)
    {
        /* error while exec the request ... 
		 * or no matching entries */
        free(indexes);
        return NULL;
    }

    indexes[matchingIndexes] = 0;

	/* allocate just what needed */
	entries = sqlite_log_malloc((matchingIndexes + 1) * sizeof(*entries));

	/* populate */
	matchingIndexes = 0;
    while (indexes[matchingIndexes] != 0)
    {
        entries[matchingIndexes] = sqlite_logentry_readindex(log,indexes[matchingIndexes]);
        matchingIndexes++;
    }
    
	entries[matchingIndexes] = NULL;

	return entries;
}
