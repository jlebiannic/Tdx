/*========================================================================
        E D I S E R V E R

        File:		logsystem.h
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1997 Telecom Finland/EDI Operations
==========================================================================
  @(#)TradeXpress $Id: logsystem.h 55487 2020-05-06 08:56:27Z jlebiannic $
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 02.05.95/JN	Pseudo field _OWNER for managerlog.
  3.02 15.09.97/JN	Couple of pointers added to LS struct.
   .   20.03.00/KP	Added logoperatr_*() functions
   .   06.04.00/KP	logoperator_write() had incorrect arguments. Sorry.
  4.00 05.07.02/CD      added column separator in LogFilter and PrintFormat.
  4.0? 22.09.04/JMH     suppress the logd cache (Xentries) from the logsystem struct
                        thus cache is not shared anymore by rls client
       22.04.2011/CMA   add new interface (logentry_getbyfield)  for eSignature CGI script
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation

========================================================================*/
#ifndef _LOGSYSTEM_LOGSYSTEM_H
#define _LOGSYSTEM_LOGSYSTEM_H

#include "logsystem.definitions.h"
#include "runtime/tr_constants.h"

/* BugZ_9833 */
/*----------------------------------------------------------------------------------------------*/
/* cm 07/13/2010 */
#define TYPE_LOGSYSTEM 'S'
#define TYPE_LOGENTRY  'E'

/* data is either logsystem* or logentry* */
extern void logsys_dump(void* log_data, char log_type, char* msg, char* __file__, int __line__, char* from); /* defined in io.c */
/*----------------------------------------------------------------------------------------------*/

void 				writeLog					(int level, char *fmt, ...);

int					mbsncpy						(char* to, char* from, size_t sz);
int					mbsncpypad					(char* to, char* from, size_t size, char padding);
int 				log_UseLocaleUTF8			();
#endif
