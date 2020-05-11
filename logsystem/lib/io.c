/*========================================================================
        E D I S E R V E R

        File:		io.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Reading and writing logsystem stuff.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: io.c 55495 2020-05-06 14:41:40Z jlebiannic $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 27.04.95/JN	mmap added to readraw.
  3.02 24.01.96/JN	Solaris. MAP_FILE undef.
  3.04 03.04.96/JN	Clustering reads.
  3.05 09.11.05/CD  Set correct errorno when index not found in dao_logentry_readindex
  3.06 18.05.17/TCE(CG) EI-325 correct many memory leak	
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "private.h"
#include "logsystem.dao.h"
#include "logsystem.h" /* adding for BugZ_9833 */

#ifndef MACHINE_WNT
#include <sys/uio.h>
#include <unistd.h>
#endif

#include "port.h"
#include "data-access/commons/daoFactory.h"
#include "tr_externals.h"
#include "daservice.h"


#ifdef MAXIOV
#define CLUSTER_MAX MAXIOV
#endif
#ifdef UIO_MAX
#define CLUSTER_MAX UIO_MAX
#endif
#ifndef CLUSTER_MAX
#define CLUSTER_MAX 16
#endif

int dao_loglabel_free(LogLabel *lab);
int old_legacy_logsys_warning(LogSystem *ls, char *fmt, ...);

TimeStamp dao_log_curtime()
{
	return ((TimeStamp) time(NULL));
}

/* Value for sqlite busy handler timeout
 * which is used when a request because the database is locked */
/* define dao_REQUEST_TIMEOUT 60000 */
/* BugZ_9833 : set to 8 mn */
#define dao_REQUEST_TIMEOUT 480000

/* BugZ_9833: add timeout setting                          */ 
/*---------------------------------------------------------------------------------------------*/
/* Gets timeout value either from dao_TIMEOUT env value */
/* or dao_REQUEST_TIMEOUT default value                 */
static int  getSyslogTimeout()
{
  int ret = dao_REQUEST_TIMEOUT;
  int tmp_val;
  char *timeout_env = getenv("dao_TIMEOUT");
  
  if (timeout_env)
  {
    ret = atoi(timeout_env);
  }

  return ret;
}

static char* getDumpFileName()
{
  static char s_buf[1000];
  time_t      now;
  struct tm  *ts;
  char       *dumpErrFlag = getenv("dao_DUMP_ERR");
  char       *ret = NULL;

  if (dumpErrFlag)
  {
    now = time(NULL);
    ts = localtime(&now);
    strftime(s_buf, sizeof(s_buf), "/trace/%y%m%d_logsys_dump.log", ts);
    ret = s_buf;
  }
  return ret;
}

/* dump error during logentry access or logentry creation */
void logsys_dump(void* log_data, char log_type, char* msg, char* __file__, int __line__, char* from)
{
  LogSystem    *log = NULL;
  LogEntry     *entry = NULL;
  FILE*         fp;
  time_t        now;
  struct tm    *ts;
  char          buf[80];
  PrintFormat  *printFormat = NULL;
  char         *tmp =  getDumpFileName();

  /* get syslog object addr */
  switch (log_type)
  {
    case  TYPE_LOGSYSTEM :
			  log = (LogSystem*)log_data;
                          break;
    case  TYPE_LOGENTRY :
                          entry = (LogEntry*)log_data;
        		  if (entry)
			  {
                            log = entry->logsys;
                          }
			  break;
          break;
  }

  if (log && tmp)
  {
	char *dbName;
	int toFree = 0;	 
	/* EI-325 if size > 1024, manually allocate memory */
	if ( (strlen(log->sysname) + strlen(tmp) ) > 1024 )	{
		dbName = malloc(strlen(log->sysname) + strlen(tmp) +1 );
		dbName = strcpy(dbName,log->sysname);	
		dbName = strcat(dbName, tmp);
		toFree = 1;
	} else {
		dbName = dao_logsys_filepath(log, tmp);
	}  
    /* always open dump file in create and append mode */
    if (fp = fopen(dbName, "a+w+")) /* TODO ORD : CHECK THIS ! (was sqlite3_open) */
    {
      /* Get the current time */
      now = time(NULL);
 
      /* Default format */
      printFormat =  dao_logsys_defaultformat(log);
      printFormat->separator = ";";    

      /* Format time, "dd..mm.yyyy HH:MM:SS */
      ts = localtime(&now);
      strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", ts);

      /* dump timestamp,  file, line, msg*/
      fwrite(buf, sizeof(char), strlen(buf), fp);
      fwrite(";", sizeof(char), 1, fp);


      /* Exception info msg */
      fwrite(msg, sizeof(char), strlen(msg), fp);
      fwrite(";", sizeof(char), 1, fp);

      /* SQL statement source */
      fwrite(from, sizeof(char), strlen(from), fp);
      fwrite(";", sizeof(char), 1, fp);

      /* dump log entry */
      if (entry)
      {
        /* print log entry index */
        sprintf(buf, "%d", entry->idx); /* fix bug for Windows which does'nt support snprintf */
        fwrite(buf, sizeof(char), strlen(buf), fp);
        fwrite(";", sizeof(char), 1, fp);
  
        /* print log entry data */
        dao_logentry_printbyformat(fp, entry, printFormat);
      }
      else
      {
        fwrite("\n", sizeof(char), 1, fp);
      }
	  /* EI-325 free manually the string */
	  if (toFree) { free(dbName); };
      /* always close file */
      fclose(fp); 
    }
  }
}

/*---------------------------------------------------------------------------------------------*/

int dao_logsys_opendata(LogSystem *log)
{
//     char *errMsg = NULL;
// 	char *dbName; 
// 	int toFree = 0;
// 	int ret;
// 
//     /* already opened */
 	if ((log->datafd > 0) && (log->datadb_handle != 0))
 		return (0);
	log->datafd=1;
	log->datadb_handle=1;
	
// 
//     /* checks if database file exist */
// 	if (log->datafd <= 0) 
//     {
// 		/* EI-325 if size > 1024, manually allocate memory */
// 		if ( (strlen(log->sysname) + strlen(LSFILE_SQLITE) ) > 1024 )	{
// 			dbName = malloc(strlen(log->sysname) + strlen(LSFILE_SQLITE) +1 );
// 			dbName = strcpy(dbName,log->sysname);	
// 			dbName = strcat(dbName, LSFILE_SQLITE);
// 			toFree = 1;
// 		} else {
// 			dbName = dao_logsys_filepath(log, LSFILE_SQLITE);
// 		}
// 		
//         log->datafd = open(dbName, log->open_mode | O_BINARY, 0644);
//         if (log->datafd <= 0)
//         {
//             dao_logsys_warning(log, "Cannot open datafile: %s", dao_syserr_string(errno));
//             if (errno == ENOENT) /* do not even exist */
//             {
//                 /* try to see if LSFILE_DATA do exist in case we have an old legacy base */
//                 int testfd = open(old_legacy_logsys_filepath(log, LSFILE_DATA), O_RDONLY);
//                 if ((testfd >= 0) || (errno != ENOENT))
//                 {
//                     if (testfd >= 0)
//                         close(testfd);
//                     old_legacy_logsys_warning(log, "You have a %s file present : maybe you should try and use old legacy mode", LSFILE_DATA);
//                 }    
//             }
// 			/* EI-325 desallocate memory manually for string > 1024 */
// 			if (toFree) { free(dbName); toFree = 0 ;}
//             return (-1);
//         }
// 		/* EI-325 desallocate memory mannually for string > 1024 */
// 		if (toFree) { free(dbName); toFree = 0 ;}
//         close(log->datafd);
//     }
//     
//     /* if file exists then open database itself
//      * this avoids sqlite3_open create an unwilling empty database */
//     /*EI-325 : we have to check return code to see if resources have really been released! */
// 	ret = sqlite3_close(log->datadb_handle);
// 	if ( ret != SQLITE_OK ) 
// 	{
// 		int cpt = 0;
// 		sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
// 		while ( ret == SQLITE_BUSY && cpt < 4 ) {
// 			ret = sqlite3_close(log->datadb_handle);
// 			sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
// 			cpt++;			
// 		}
// 		if (cpt == 4 && ret != SQLITE_OK){
// 			dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
// 		} 		
// 	}	
// 	
//     log->datadb_handle = 0;
// 	
// 	/* EI-325 if size > 1024, manually allocate memory */
// 	if ( (strlen(log->sysname) + strlen(LSFILE_SQLITE) ) > 1024 )	{
// 		dbName = malloc(strlen(log->sysname) + strlen(LSFILE_SQLITE) +1 );
// 		dbName = strcpy(dbName,log->sysname);	
// 		dbName = strcat(dbName, LSFILE_SQLITE);
// 		toFree = 1;
// 	} else {
// 		dbName = dao_logsys_filepath(log, LSFILE_SQLITE);
// 	}
//     if (sqlite3_open(dbName, &log->datadb_handle) != SQLITE_OK)
//     {
// 		/* EI-325 desallocate memory mannually for string > 1024 */
// 		if (toFree) { free(dbName); toFree = 0 ;}
//         dao_logsys_warning(log, "Cannot open database: %s", sqlite3_errmsg(log->datadb_handle));
// 		/* EI-325 try to close correctly th database handler in case it's still used */	
//         ret = sqlite3_close(log->datadb_handle);
// 		sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
// 		if ( ret != SQLITE_OK ) 
// 		{
// 			int cpt = 0;
// 			while ( ret == SQLITE_BUSY && cpt < 4 ) {
// 				ret = sqlite3_close(log->datadb_handle);
// 				sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());	
// 				cpt++;				
// 			}
// 			if (cpt == 4 && ret != SQLITE_OK){
// 				dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
// 			} 		
// 		}	
//         log->datadb_handle = 0;
//         return (-1);
//     }
// 	/* EI-325 desallocate memory mannually for string > 1024 */
// 	if (toFree) { free(dbName); toFree = 0 ;}
//     /* process must wait before returning BUSY without looping forever. */
//     sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout()); /* BugZ_9833: getSyslogTimeout call adding */
// 
//     /* external program trigger setup : cleanup, remove, add, threshold */
//     if (logsys_trigger_setup(log,&errMsg) != 0)
//     {
//         dao_logsys_warning(log, "Cannot setup triggers: %s", sqlite3_errmsg(log->datadb_handle));
//         /* EI-325 try to close correctly th database handler in case it's still used */
//         ret = sqlite3_close(log->datadb_handle);
// 		if ( ret != SQLITE_OK ) 
// 		{
// 			int cpt = 0;
// 			sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
// 			while ( ret == SQLITE_BUSY && cpt < 4 ) {
// 				ret = sqlite3_close(log->datadb_handle);
// 				sqlite3_busy_timeout(log->datadb_handle, getSyslogTimeout());
// 				cpt++;			
// 			}
// 			if (cpt == 4 && ret != SQLITE_OK){
// 				dao_logsys_warning(log, "Cannot close link to database : %s", sqlite3_errmsg(log->datadb_handle));
// 			} 		
// 		}
//         log->datadb_handle = 0;
//         return (-1);
//     }
//    
// 	sqlite3_free(errMsg);
   
	dao_exec_at_open(log);

    dao_debug_logsys_warning(log, "OPEN BASE : %s", log->sysname);

    /* everything is ok */
    return 0;
}

/* Read a record from datafile.
 * Returned entry is allocated here, caller must free when done.
 * Nothing locked. */ 
LogEntry * dao_logentry_readindex(LogSystem *log, LogIndex idx)
{
	LogEntry *entry;

	entry = dao_logentry_alloc_skipclear(log, idx);

	// Jira TX-3199 DAO
	// if (log_daoreadbuf(entry) < 1)
	if (Service_findEntry(entry) < 1)
    {
        /*  0 no entries
         * -1 an error appened */
		errno = ENOENT; /* 3.05/CD added correct errno */
		dao_logentry_free(entry);
		return (NULL);
	}

	return (entry);
}

/*JRE 06.15 BG-81*/
LogEntryEnv * dao_logentry_readindex_env(LogSystem *log, LogIndexEnv idx)
{
	LogEntryEnv *entry;

	entry = dao_logentry_alloc_env(log, idx);

	if (log_daoreadbuf_env(entry) < 1)
    {
        /*  0 no entries
         * -1 an error appened */
		errno = ENOENT; /* 3.05/CD added correct errno */
		dao_logentryenv_free(entry);
		entry = NULL;
	}

	return (entry);
}
/*End JRE*/

/* Now we have only one index so method is duplicate */
LogEntry * dao_logentry_readraw(LogSystem *log, LogIndex idx)
{
	return dao_logentry_readindex(log,idx);
}

/* Write a record to database.
 * Entry is NOT free'ed.
 * Nothing locked.
 * Record updated before write. */
int dao_logentry_write(LogEntry *entry)
{
	/* overridden every time and modifications from user get silently lost. */
	dao_logentry_settimebyname(entry,"MODIFIED",dao_log_curtime());

	// Jira TX-3199 DAO: stub
	//if (log_daowritebuf(entry,0))
	if (Service_updateEntry(entry, 0))
		return (-2);

	return (0);
}

/* Write a record to datafile.
 * Entry is NOT free'ed.
 * Nothing locked.
 * Record is NOT updated before write. */
int dao_logentry_writeraw(LogEntry *entry)
{
	// Jira TX-3199 DAO: stub
	// if (log_daowritebuf(entry,1))
	if (Service_updateEntry(entry, 1))
		return (-2);

	return (0);
}

LogSystem * dao_logsys_open(char *sysname, int flags)
{
	LogSystem *log;

	dao_logsys_compability_setup(); 
	/* The LOGSYS_EXT_IS_DIR was set there... */

	log = dao_logsys_alloc(sysname);
	// Jira TX-3199 DAO: création du dao correspondant à la valeur de 1
	log->dao = daoFactory_create(1);
	

	log->walkidx = 1;
	log->open_mode = (flags & LS_READONLY) ? O_RDONLY : O_RDWR;

	if (dao_logsys_readlabel(log))
    {
		if (flags & LS_FAILOK)
        {
			dao_logsys_free(log);
			return (NULL);
		}
		fprintf(stderr, "Cannot open logsystem %s: %s\n", sysname, dao_syserr_string(errno));
		exit(1);
	}

	return (log);
}

void dao_logsys_close(LogSystem *log)
{
	int ret = 0;
	if (log->datadb_handle != 0)
	{
		/* EI-325 try to close correctly th database handler in case it's still used */
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
	}
	if (log->labelfd >= 0)
		close(log->labelfd);
	if (log->label)
		dao_loglabel_free(log->label);
	dao_logsys_free(log);
}

int dao_logsys_readlabel(LogSystem *log)
{
	struct stat st;
	LogLabel *label;
	LogField *field;
	int fd;

	if ((fd = dao_logsys_openfile(log, LSFILE_LABEL, O_RDONLY | O_BINARY, 0)) < 0)
		return (-1);

	if (fstat(fd, &st))
    {
		dao_logsys_voidclose(fd);
		return (-2);
	}
	if ((unsigned int) st.st_size < sizeof(*label))
    {
		/* It has to be at least this big. */
		dao_logsys_voidclose(fd);
		errno = EINVAL;		/* XXX */
		return (-3);
	}
	if ((label = dao_loglabel_alloc(st.st_size)) == NULL)
    {
		dao_logsys_voidclose(fd);
		return (-4);
	}
	if (read(fd, label, st.st_size) != st.st_size)
    {
		dao_logsys_voidclose(fd);
		dao_loglabel_free(label);
		return (-5);
	}
	if (label->magic != LSMAGIC_LABEL)
    {
		dao_logsys_voidclose(fd);
		dao_loglabel_free(label);
		dao_logsys_warning(log, "Label is not valid");
		errno = EINVAL;
		return (-6);
	}
	close(fd);

	/* Adjust fieldname offsets into strings. */
	for (field = label->fields; field->type; ++field)
    {
		field->name.string   = (char *) label + field->name.offset;
		field->header.string = (char *) label + field->header.offset;
		field->format.string = (char *) label + field->format.offset;
	}
	log->label = label;
	return (0);
}

/* Return path into named file in logsystem.
 * ALERT : Returned buffer is static and is overwritten on each call. */
char * dao_logsys_filepath(LogSystem *log, char *basename)
{
	static char filename[1024];

	strcpy(filename, log->sysname);
	strcat(filename, basename);

	return (filename);
}

/* Open specified file and make sure it does not clash with stdio. */
int dao_logsys_openfile(LogSystem *log, char *basename, int flags, int mode)
{
	int fd;

	for (;;)
    {
		char* dbName;
		int toFree = 0;
		
		/* EI-325 if size > 1024, manually allocate memory */
		if ( (strlen(log->sysname) + strlen(basename) ) > 1024 )	{
			dbName = malloc(strlen(log->sysname) + strlen(basename) +1 );
			dbName = strcpy(dbName,log->sysname);	
			dbName = strcat(dbName, basename);
			toFree = 1;
		} else {
			dbName = dao_logsys_filepath(log, basename);
		}
		
		fd = open(dbName, flags, mode);
		
		if (toFree) { free(dbName); toFree = 0; }
		
		if (fd < 0){
			return (fd);
		}	

		if (fd > 2)
		{
#ifdef F_SETFD
			fcntl(fd, F_SETFD, 1);
#endif
			return (fd);
		}
		close(fd);

		if (open("/dev/null", 0) < 0)
		{
			perror("/dev/null");
			return (-1);
		}
	}
}

void dao_logsys_voidclose(int fd)
{
	int err = errno;

	(void) close(fd);
	errno = err;
}

LogEntry * dao_logentry_get(LogSystem *log)
{
	LogEntry *entry;

	entry = dao_logentry_alloc_skipclear(log, log->walkidx);

	// Jira TX-3199 DAO
	// if (log_daoreadbuf(entry) < 1)
	if (Service_findEntry(entry) < 1)
	{
		/*  0 no entries
		 * -1 an error appened */
		dao_logentry_free(entry);
		return NULL;
	}
	++log->walkidx;
	return (entry);
}
