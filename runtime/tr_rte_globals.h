/*========================================================================
        TradeXpress        

        File:		tr_rte_globals.h
        Author:		Christophe Mars (CMA)
        Date:		Mon Jan 10 2011

	This file declares or defines all RTE globals.
        Use this files instead using extern statement into C files.
        
	Created for BugZ_9980_OPEN_DB.

        Copyright (c) 2010 GenerixGroup
==========================================================================
        Record all changes here and update the above string accordingly.
        10.01.2011/CMA    Created.
==========================================================================*/
#ifndef _TR_RTE_GLOBALS_H
#define _TR_RTE_GLOBALS_H

#ifndef TR_RTE_GLOBALS_DECL
  extern char taARGUMENTS[];
  extern char taPARAMETERS[];
  extern char taFORMSEPS[];
#else
                      /* RTE array index */ 
  char taARGUMENTS[]  = "0";
  char taPARAMETERS[] = "1";
  char taFORMSEPS[]   = "2";
#endif
 

#endif /* _TR_RTE_GLOBALS__H */
