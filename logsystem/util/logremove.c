/*============================================================================
	E D I S E R V E R   L O G S Y S T E M

	File:		logremove.c
	Programmer:	Mikko Valimaki
	Date:		Wed Aug 12 09:29:29 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: logremove.c 55499 2020-05-07 16:25:38Z jlebiannic $")
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.00 25.01.95/JN	Adapted from 3.1.1 logview
  3.01 21.07.97/JN	errno.h included for multithread definitions.
  3.02 19.09.97/JN	Fix for confirm question on NT, no /dev/tty.
  3.03 10.07.02/CD 	Added -? option
  3.04 13.02.03/UK	validation of field's name before removing (-f option)
  3.05 09.11.05/CD  prevent from trying to remove index less than 10
                    and do not specify extension when removing with 
                    specifing an index 
  3.06 01.03.06/CD  could not remove when using filter 
                    absIndex is unsigned and could not have a negative value 
  3.07 22/05/12/TP	When we delete entries in bplog or bplog_archive, linked
  	  	  	  	  	values are deleted in bplog_details and bplog_details_archive
  3.08 16/05/13/CD	size allocation problem generating errors on suppression via
					webaccess when installing over 64bits windows installation
  3.09 17/12/13/SCH (CG) TX-524 : Basename() is replace by a single line of code
                         Process WNT / *NIX sep difference
						 Take dao_logentry_remove() returned value into account
						 SONAR results improvement + Robustness adds.
  3.10 05/06/14/YLI (CG) TX-2565 : when an entry bplog/bplog_archive is removed by filters,
						 its bplog_details/ bplog_details_archive entries are also removed
  3.11 07/08/14/YLI (CG) TX-2565 : change the algorithm for removing blog_details/bplog__details_archive entries;see archive_bplog's algorithm
								   add the fonction writelog, loglevel=10
  3.12 12/08/14/YLI (CG) TX-2565 : if details' entries or details_archive's entries don't exist, display debug in the fonction bplogEntryRemove
					
  3.13 19/05/15/CGL (CG) WBA-410 : Deleting syslog’s entries related with bplog’s entries.
								   Deleting archive’s entries related with bplog_archive’s entries.
								   Add cleaning of orphan bplog_details.
					
  3.14 03/12/15/CGL (CG) WBA-410 : Deleting bplog_details’s entries linked with bplog's entries using the logview command and a temp file.
					
  3.15 17/02/16/CGL (CG) WBA-410 : Modification of logview query. Modification of the bplog_details's entries search process. 

  TX-3123 - 10.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
					
============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

extern int errno;

#include "logsystem/lib/logsystem.dao.h"
#include "translator/translator.h"

/*WBA-410: Declaration of functions*/
char *getenv(const char*);

/* dummy variables */
double tr_errorLevel = 0;
char*  tr_programName = "";

#undef LS_GENERATION_MOD
#define LS_GENERATION_MOD 10

#define LENGTH 100

static char *cmd;

static int force_flag = 0;

int processLogLine_database(char *line, LogSystem* plog);
int processLogLine_data_path(char *line, LogSystem* plog);
int get_confirmation(int def);

/* -------------------------------------------------------------------------- */
/* Code mort                                                                  */
void bail_out(char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", cmd);
	va_start(ap,fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (errno) {
		fprintf(stderr, " (%d,%s)\n", errno, dao_syserr_string(errno));
	}
	exit(1);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
static void fatal_exit(char *msg)
{
	fprintf(stderr, "%s: %s\n", cmd, msg);
	exit(1);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
static void perror_exit(char *msg)
{
	int e = errno;

	fprintf(stderr, "%s: ", cmd);
	errno = e;
	perror(msg);
	exit(1);
}
/* -------------------------------------------------------------------------- */
/*YLI(CG) 07-08-2014 TX-2565*/
/* -------------------------------------------------------------------------- */
/*Function : int getAttribute(char *node, char *attributeName, char* attribute)*/
/*extract attribute(index) for attributeName("database" or "data_path") in the node*/
/*node : <ibp:Entry database=...> ; <ibp:Data data_path=...>*/
/*return 0 : attribut found*/
/*return -1 : attribut not found*/
/* -------------------------------------------------------------------------- */
int getAttribute(char *node, char *attributeName, char* attribute)
{
    char *startOfAttribute = NULL;
    char *endOfAttribute   = NULL;
    int len;

    writeLog(10, "Searching attribute <%s> in node :\n%s\n", attributeName, node);
	/*get string ,ex : database="20"/> from node's string */
    startOfAttribute = strstr(node, attributeName);
    if (startOfAttribute) {
        startOfAttribute += strlen(attributeName)+2; 
        endOfAttribute = strchr(startOfAttribute, ' ');
        if (!endOfAttribute){
            endOfAttribute = strchr(startOfAttribute, '/');
   }
        if (endOfAttribute) {	
            len = strlen(startOfAttribute)-strlen(endOfAttribute)-1;
            strncpy(attribute, startOfAttribute, len);
            attribute[len] = 0;
            return 0;
        }
        else {
			writeLog(10,"endOfAttribute not found !\n");
		}
    }
    else {
        writeLog(10, "startOfAttribute not found !\n");
	}
    return -1;
}
/* -------------------------------------------------------------------------- */
/*Function : int processLogFile(FILE* fichier, LogSystem* plog)*/
/*read node in the logfile */
/*return 0  : index had been found and the entry has been removed*/
/*return -1 : logfile's format is wrong */
/* -------------------------------------------------------------------------- */
int processLogFile(FILE* fichier, LogSystem* plog) {

	char s[2048];
    char c;
	int i           = 0;
	int isRightFile = 0;
	char 		*baseSyslog = NULL;
	char 		*baseArchive = NULL;
	char		*home		= NULL;
	LogSystem   *syslog  	= NULL;
	LogSystem   *archive  	= NULL;
	

	/* Concatenate the directory where there are log files to see linked  */
	/* index in the other database                                        */

	c = getc(fichier);
	
	while(c != EOF) {
	
		if ( c == 255 ){
			fclose(fichier);
	    	return 0;
		}
	
		/* Search the begin symbol of xml tag */
		if ( c == '<'){
			i = 0;
			s[i] = c;
		
			/* Read the file to end symbol of xml tag or EOF */
			do{
				c = getc(fichier);
				i++;
                s[i] = c;
            } while ((c != EOF)&&(c != '>'));
			
			if (c == EOF){
				writeLog(10,"Malformed logfile \n");
                return 0; 
            }
			
			s[i+1] = '\0';

			
			/* CGL(CG) 19/05/15 WBA-410: Deleting syslog’s entries related with bplog’s entries. */
			if(strstr(s,"$SYSLOG") != NULL){
				home = getenv("HOME");
				baseSyslog = malloc(256);
				strcpy(baseSyslog, home);
#ifdef MACHINE_WNT
				strcat(baseSyslog, "\\syslog");
#else
				strcat(baseSyslog, "/syslog");
#endif	
				if ((syslog = dao_logsys_open(baseSyslog, LS_FAILOK)) == NULL) {
					perror_exit(baseSyslog);
				}else{
					/* call functions to processing the tags */
					if (processLogLine_database(s, syslog) == 0){
						isRightFile ++ ;
					}
					if (processLogLine_data_path(s, syslog) == 0){
						isRightFile ++ ;	
					}
				}
				free(baseSyslog);
			/* Deleting archive’s entries related with bplog_archive’s entries. */
			}else if (strstr(s,"$ARCHIVE") != NULL){
				home = getenv("HOME");
				baseArchive = malloc(256);
				strcpy(baseArchive, home);
#ifdef MACHINE_WNT
				strcat(baseArchive, "\\archive");
#else
				strcat(baseArchive, "/archive");
#endif	
				if ((archive = dao_logsys_open(baseArchive, LS_FAILOK)) == NULL) {
					perror_exit(baseArchive);
				}else{	
					/* call functions to processing the tags */
					if (processLogLine_database(s, archive) == 0){
						isRightFile ++ ;
					}
					if (processLogLine_data_path(s, archive) == 0){
						isRightFile ++ ;	
					}
				}
				free(baseArchive);
			}else{
				/* call functions to processing the tags */
			if (processLogLine_database(s, plog) == 0){
				isRightFile ++ ;
			}
			if (processLogLine_data_path(s, plog) == 0){
				isRightFile ++ ;	
			}
			/* FIN WBA-410 */
		}
		}
		c = getc(fichier);
	}
	
    fclose(fichier);
	fichier =  NULL;
	if (isRightFile == 0){
		return -1;
	}
	else {
		return 0;
	}
} 
/* -------------------------------------------------------------------------- */
/* Function  : processLogLine_database(char *line, LogSystem* plog) */
/* return 0  : "database" tag had been found and the entry has been removed*/
/* return -1 : "database" tag had not been found*/
/* -------------------------------------------------------------------------- */
int processLogLine_database(char *line, LogSystem* plog){

    int found = 0;
	long original_index 	= 0;
	long syslog_index 		= 0;
    char basename[32];
    char indexstr[16];
	char syslog_indexstr[256];
	char lineSyslog[512];
	char baseSyslog[1024];
	char *index 			= NULL;
	FILE *logRouter  		= NULL;
	
	/* if not found -> exit */
	if (strstr(line , "<ibp:Entry database=") == NULL) {
		/* nothing to do then exit */
		return -1;
	}
	
	/* Get the content of xml part */
	if (!getAttribute(line, "database", basename)){
		if (!getAttribute(line, "index", indexstr)){
			original_index = atol(indexstr);
			if (original_index) {
				found++;
			}
		}
	}
	
	/* if content don't exist -> exit */
    if (!found){
        writeLog(10, "Malformed Entry node :\n%s\n, this link will be removed\n", line);
        return -1;
    }
	
	/*CGL(CG) 03/12/2015 WBA-410 Search index's syslog to get the second index's syslog */
	if (strcmp(plog->sysname, getenv("SYSLOG")) == 0){
		strcpy(baseSyslog, getenv("SYSLOG"));
#ifdef MACHINE_WNT
		strcat(baseSyslog, "\\files\\l\\");
#else
		strcat(baseSyslog, "/files/l/");
#endif
		strcat(baseSyslog, indexstr);
		logRouter = fopen(baseSyslog, "r+");
		if (logRouter != NULL){
			while(fgets(lineSyslog, 512, logRouter) != NULL) {
				if(strstr(lineSyslog, "SYSLOG_ROUTER_ENTRY") != NULL){
				index = strchr(lineSyslog, '=');
				strcpy(syslog_indexstr,index+1);
				syslog_index = atol(syslog_indexstr);
				}
			}
			fclose(logRouter);
		}
	}
	
	if (syslog_index == 0){
      writeLog(10, "Malformed Entry node :\n%s\n, this link will be removed\n", lineSyslog);
	}else{
		dao_logentry_remove(plog, syslog_index, NULL);
   }
	writeLog(10, "database name=%s, removed index=%li\n", basename+1, original_index);
	dao_logentry_remove(plog, original_index, NULL);
	
	return 0;
}

/* -------------------------------------------------------------------------- */
/* Function  : processLogLine_data_path(char *line, LogSystem* plog) */
/* return 0  : "data_path" tag had been found and the entry has been removed*/
/* return -1 : "data_path" tag had not been found*/
/* -------------------------------------------------------------------------- */
int processLogLine_data_path(char *line, LogSystem* plog){
    int found = 0;
	long original_index   = 0;
	long syslog_index 	  = 0;
    char indexstr[16];
	char syslog_indexstr[256];
	char lineSyslog[512];
	char baseSyslog[1024];
	char *index 			= NULL;
	FILE *logRouter  	  = NULL;

	/* if not found -> exit */
	if (strstr(line , "<ibp:Data data_path=") == NULL) {
		/* nothing to do then exit */
		return -1;
	}
	
	/* Get the content of xml part */
	if (!getAttribute(line, "data_path", indexstr)){
		original_index = atol(indexstr);
		if (original_index) {
			found++;
		}
	}
	
	/* if content does not exist -> exit */
    if (!found){
        writeLog(10, "Malformed Entry node :\n%s\n, this link will be removed\n", line);
        return -1;
    }
	
	/*CGL(CG) 03/12/2015 WBA-410 Search index's syslog to get the second index's syslog */
	if (strcmp(plog->sysname, getenv("SYSLOG")) == 0){
		strcpy(baseSyslog, getenv("SYSLOG"));
#ifdef MACHINE_WNT
		strcat(baseSyslog, "\\files\\l\\");
#else
		strcat(baseSyslog, "/files/l/");
#endif
		strcat(baseSyslog, indexstr);
		logRouter = fopen(baseSyslog, "r+");
		if (logRouter != NULL){
			while(fgets(lineSyslog, 512, logRouter) != NULL) {
				if(strstr(lineSyslog, "SYSLOG_ROUTER_ENTRY") != NULL){
				index = strchr(lineSyslog, '=');
				strcpy(syslog_indexstr,index+1);
				syslog_index = atol(syslog_indexstr);
				}
			}
		fclose(logRouter);
    }
	}
	
	if (syslog_index == 0){
      writeLog(10, "Malformed Entry node :\n%s\n, this link will be removed\n", lineSyslog);
	}else{
		dao_logentry_remove(plog, syslog_index, NULL);
   }
    writeLog(10, "removed index=%li\n", original_index);		
	dao_logentry_remove(plog, original_index, NULL);
	
    return 0;
}

/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Function:void bplogEntryRemove(char* baseBplog,char *nIndex,int isArchive) */
/*            "baseBplog" : the bplog/bplog_archive path                      */
/*            "nIndex"    : the bplog/bplog_archive index                     */
/*            "isArchive" : isArchive = 0 , the base is bplog                 */
/*            				isArchive = 1 , the base is bplog_archive         */                          
/*          Search for the file log in database bplog/bplog_archive 		  */
/*         	in order to remove its bplog_details/bplog_details_archive entries*/
/* -------------------------------------------------------------------------- */

void bplogEntryRemove(char *baseBplog, char *nIndex, int isArchive){

	/* 3.07 */
	char 			* logDir      = NULL;
	char 			*filePath     = NULL;
	char            *newBaseBp    = NULL;
	FILE			*logEntryFile = NULL;
	LogSystem   	*logdetails   = NULL;
	char 			*tTmpFile	  			= NULL;
	char 			*tOutFile	  			= NULL;
	FILE			*logviewBplogDetails 	= NULL;
	int 			nRet          = 0;
	long 			index					= 0;
	char 			command[512];
	char 			line[512];
	char			indexBlogDetails[16];
	
	
	tTmpFile = malloc(1024);
	tOutFile = malloc(1024);
	logDir = malloc(strlen(baseBplog)+20);
	strcpy(logDir, baseBplog);

	/* Concatenate the directory where there are log files to see linked  */
	/* index in the other database                                        */
	/* 3.09 WNT / *NIX discrimination */
#ifdef MACHINE_WNT
	strcat(logDir, "\\files\\log\\");
#else
	strcat(logDir, "/files/log/");
#endif	
	/* 3.09 */
	/* 3.08  Need to add the index size */
	filePath = malloc(strlen(logDir)+strlen(nIndex)+1);  
	strcpy(filePath, logDir);
	/* Concatenate the index number of file to open                       */
	strcat(filePath, nIndex);

	/* Open this file                                                     */
	logEntryFile = fopen(filePath, "r");

	/* if we can open this file                                           */
	if (logEntryFile != NULL){
		newBaseBp = (char *)malloc(strlen(baseBplog)+30);
		if ( !isArchive ){
			/* database is bplog*/		
			strcpy(newBaseBp, baseBplog);
			strcat(newBaseBp, "_details");
			/* Open bplog_details to delete linked entries                */
			if ((logdetails = dao_logsys_open(newBaseBp, LS_FAILOK)) == NULL) {
				perror_exit(newBaseBp);
			} else {
					nRet = processLogFile(logEntryFile, logdetails);
					/*file log format incorrect, removes it*/
					if (nRet == -1){
					   /*3.12  if bplog_details' entries  not exist*/
						writeLog(10, "bplog_details' entries not found\n");
					}
			}
			dao_logsys_close(logdetails);

			/* CGL(CG) 29/10/15 WBA-410: Creation of temp file*/
#ifdef MACHINE_WNT
			tmpnam_s (tTmpFile,1024);
			strcpy(tOutFile, getenv("HOME"));
			strcat(tOutFile, "\\tmp");
			strcat(tOutFile, tTmpFile);
#else
			memset(tOutFile, 0, sizeof(tOutFile));
			strncpy(tOutFile, "/tmp/myFile-XXXXXX",18);
			mkstemp (tOutFile);
#endif
			/*WBA-410 : Modification of logview query. Modification of the bplog_details's entries search process*/
			strcpy(command, "logview -l -s ");
			strcat(command, newBaseBp);
			strcat(command, " -f BP_INDEX=");
			strcat(command, nIndex);
			strcat(command, " INDEX");
			strcat(command, " >");
			strcat(command, tOutFile);
			system(command);
			
			/*Read temp file*/
			logviewBplogDetails = fopen(tOutFile, "r");
			if (logviewBplogDetails != NULL){
				while(fgets(line, 512, logviewBplogDetails) != NULL) {
					int i = 0;
					int t = 0;
					int nb = strlen(line);
					while(i <= nb){
						if(line[i]!=' '){
							indexBlogDetails[t] = line[i];
							t=t++;
					}
						 i=i++;
					}
					index = atol(indexBlogDetails);
					
					/* Open bplog_details to delete linked entries                */
					if ((logdetails = dao_logsys_open(newBaseBp, LS_FAILOK)) == NULL) {
						perror_exit(newBaseBp);
					}else{
						dao_logentry_remove(logdetails, index, NULL);
					}
					dao_logsys_close(logdetails);
				}
				fclose(logviewBplogDetails);
			}
				free(tTmpFile);
				free(tOutFile);
			remove(logviewBplogDetails);
				remove(tOutFile);
			/* Fin WBA-410 */

		} else{
			/* database is bplog_archive */
			strncpy(newBaseBp, baseBplog, strlen(baseBplog)-7);
			newBaseBp[strlen(baseBplog)-7] = '\0';
			strcat(newBaseBp, "details_archive");
			/* Open bplog_details_archive to delete linked entries        */
			if ((logdetails = dao_logsys_open(newBaseBp, LS_FAILOK)) == NULL) {
				perror_exit(newBaseBp);
			} else {
					nRet = processLogFile(logEntryFile, logdetails);
					/*file log format incorrect, removes it*/
					if (nRet == -1){
						/*3.12   if bplog_details_archive's entries  not exist */
						writeLog(10, "bplog_details_archive's entries not found\n");
					}
			}
			dao_logsys_close(logdetails);
		}
		free(newBaseBp);
	}
	free(filePath);
	free(logDir);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* Function : void removeAllEntry_details(LogSystem * log, char * newSysname) */
/*            Opens the linked database and delete all entries                */
void removeAllEntry_details(char * newSysname)
{
	LogSystem * log_details = NULL;

	/* Open the database "newSysname"                                         */
	if ((log_details = dao_logsys_open(newSysname, LS_FAILOK)) == NULL) {
		perror_exit(newSysname);
	} else {
		dao_logsys_removebyfilter(log_details, NULL, NULL);
	}

	dao_logsys_close(log_details);
}
/*End YLI(CG) 07-08-2014 TX-2565*/
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
main(int argc, char **argv)
{
	extern char 	*optarg;
	extern int  	optind, opterr, optopt;

	char        	*sysname = NULL;
/*3.06 absIndex is not signed, we cannot init it to -1 */ 
	LogIndex    	absIndex = 0;  
/*3.06 absIndexSet is set when -i option is used */ 
	int    	        absIndexSet = 0;  
	char        	*extension = NULL;
	LogFilter   	*logFilter = NULL;
	LogSystem   	*log;
	LogEntry    	*logEntry;
	FILE        	*fp;
	int  	    	c;
	/* 3.07 */
	char			numIndex [10];
	char			*newSysname = NULL;
	/* 3.07 */
	/* 3.09 */
	char * basename = NULL;
	char * p        = NULL;
	/* 3.09 */
/* 3.04 */
	int 		nunk = 0;
	FilterRecord	*g;
/* end of 3.04 */

	int logsys_useTransaction = 0;
/*3.10*/	
	char 	 idxBplog [10] ;
	LogIndex * indexes	= NULL;
	int 	 		 n  = 0;
	int 	 		 i  = 0;
/*3.10*/	

    tr_UseLocaleUTF8();

    /* ---------------------------------------------------------------------- */
	fflush(stderr);
    dao_logsys_compability_setup();


    /* ---------------------------------------------------------------------- */

	cmd = argv[0];
	opterr = 0;
	/* ---------------------------------------------------------------------- */
	/* Identify given options                                                 */
	/* ---------------------------------------------------------------------- */
	while ((c = getopt(argc, argv, "?IOtqs:i:e:f:F:")) != -1){
		switch (c){
		default:
			fprintf(stderr, "%s: Invalid option '%c'.\n", cmd, optopt);
		case '?':
			fprintf(stderr, "\
Valid options are:\n\
	-s sysname		logsystem name\n\
	-i index		absolute index to remove\n\
	-q			no questions asked...\n\
	-e extension		extension of file to remove\n\
	-f 'name op value'	insert an entry to filter\n\
	-F filterfile		read filter from file\n\
	-t			use transaction mode on base\n\
");
			exit (2);
			break;
		case 's':
			sysname = optarg;
			break;
        case 't' :
			logsys_useTransaction = 1;
			break;
		case 'i':
            strcpy (numIndex, optarg);
			absIndex = atoi(optarg);
            absIndexSet = 1;
			break;
		case 'q':
			force_flag = 1;
			break;
		case 'e':
			extension = optarg;
			break;

		case 'f':
			/* Add one field into filter. */
			dao_logfilter_add(&logFilter, optarg);
			break;
		case 'F':
			/* Read filter from file. */
			if (!strcmp(optarg, "-")){
				fp = stdin;
            } else {
                if ((fp = fopen(optarg, "r")) == NULL) {
			    	perror_exit(optarg);
            }
            }

			dao_logfilter_read(&logFilter, fp);
			if (fp != stdin) {
				fclose(fp);
			}
			break;
		}
	}
	/* End while                                                              */
	/* ---------------------------------------------------------------------- */

	if (optind < argc) {
		fatal_exit("Excess arguments");
	}

	if (!sysname) {
		fatal_exit("No system name given");
	}

	if ((log = dao_logsys_open(sysname, LS_FAILOK)) == NULL) {
		perror_exit(sysname);
	}

	if ((logsys_useTransaction == 1) && (dao_logsys_begin_transaction(log) == -1)) {
		exit(1);
	}

	/* ---------------------------------------------------------------------- */
	/* 3.07 */
	/* Get the name of the database without the complete directory            */
	/* 3.08 add one for the final 0 char */
	/* 3.09 remove basename() function call                                   */
#ifdef MACHINE_WNT
	if (strstr(sysname, "\\")) { 
		basename = (p = strrchr (sysname, '\\')) ? p + 1 : sysname;
	} else {
		basename=sysname;
	}
#else
	if (strstr(sysname, "/")) { 
		basename = (p = strrchr (sysname, '/')) ? p + 1 : sysname;
	} else {
		basename=sysname;
	}
#endif
	/* 3.09 */
	/* 3.07 */
	/* if index of a entry is a parameter                                     */
	if (absIndexSet){
        int nRet;
        char errmsg[256];
        
		/* 3.10 */
		if ( strcmp(basename, "bplog") == 0){
			/* 0 : this base is bplog, remove associated bplog_details entries*/
			bplogEntryRemove(sysname, numIndex, 0);
				}
		if ( strcmp(basename, "bplog_archive") == 0){
			/* 1 : this base is bplog_archive, remove associated bplog_details_archive entries*/
			bplogEntryRemove(sysname, numIndex, 1);
        	}
		/* END 3.10 */
    	/* 3.07 */
        /* 3.05/CD do not specify any index (use NULL as 3rd arg) */
        if (nRet=dao_logentry_remove(log, absIndex, NULL)){
			sprintf(errmsg, "Cannot remove index %d: %s", absIndex, dao_syserr_string(errno));
			if (logsys_useTransaction == 1) {
				dao_logsys_end_transaction(log);
			}
			fatal_exit(errmsg);
        }
	} else{/* else index of an entry is not a parameter                              */
/* 3.04 */
		nunk = dao_logsys_compilefilter(log, logFilter);
		/* nunk = 0: field exists */
		if (nunk != 0){
			for (g = logFilter->filters; g < &logFilter->filters[logFilter->filter_count]; ++g){
				g->field = NULL;
				g->field = dao_logsys_getfield(log, g->name);
				if (!g->field) {
					fprintf (stderr, "Error: Field %s does not exist\n",g->name);
			}
			}

			if ((logsys_useTransaction == 1) && (dao_logsys_end_transaction(log) == -1)) {
				exit(1);
			}
			exit (0);
		}
/* end of 3.04 */
		
		if (!force_flag && (!logFilter || !logFilter->filter_count)){
			/* Print a warning and wait for confirmation to continue.
			 * force_flag can override this question. */

			fprintf(stderr, "\
Warning: %s will remove all entries in %s.\n\
Are you sure you want to continue (y/n) ? ",
				cmd, sysname);

			if (get_confirmation('n') != 'y'){
				fprintf(stderr, "Cancelled.\n");
				if (logsys_useTransaction == 1) {
					dao_logsys_end_transaction(log);
				}
				exit(2);
			}
			fprintf(stderr, "Continuing...\n");
		}
		
		/* 3.10 remove an entry bplog and its bplog_detail entries according to filters */
    	if ( strcmp(basename, "bplog") == 0 && logFilter != NULL ){
			indexes = dao_logsys_list_indexed(log, logFilter);
			while ((i = indexes?indexes[n]:0) != 0){
				n++;
				sprintf(idxBplog, "%d", i); 
				bplogEntryRemove(sysname, idxBplog, 0);
			}
		}
		/*without filter-> all of bplog and its bplog_details will be removed */
    	if ( strcmp(basename, "bplog") == 0 && logFilter == NULL ){
			newSysname = (char *)malloc(strlen(sysname)+30);			
			strcpy(newSysname, sysname);
			strcat(newSysname, "_details");
			/*YLI(CG) 07-08-2014 TX-2565*/
			removeAllEntry_details(newSysname);
			free(newSysname);
    	}

		/* remove an entry bplog_archive and its bplog_details_archive entries according to filters*/
		if ( strcmp(basename, "bplog_archive") == 0 && logFilter != NULL ){
			indexes = dao_logsys_list_indexed(log, logFilter);
			while ((i = indexes?indexes[n]:0) != 0){
				n++;
				sprintf(idxBplog, "%d", i); 
				bplogEntryRemove(sysname, idxBplog, 1);
			}
		}
		/*without filter-> all of bplog_archive and its bplog_details_archive will be removed*/			
    	if ( strcmp(basename, "bplog_archive") == 0 && logFilter == NULL ){
			newSysname = (char *)malloc(strlen(sysname)+30);		
			strncpy(newSysname, sysname, strlen(sysname)-7);
			newSysname[strlen(sysname)-7] = '\0';
			strcat(newSysname, "details_archive");
			/*YLI(CG) 07-08-2014 TX-2565*/
			removeAllEntry_details(newSysname);
			free(newSysname);
		}
		/*END 3.10 */
		dao_logsys_removebyfilter(log, logFilter, extension);
	}
	/* 3.07 */
	
	if ((logsys_useTransaction == 1) && (dao_logsys_end_transaction(log) == -1)) {
		exit(1);
	}

	exit (0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : int get_confirmation(int def)                                   */
/*            Asks the user to confirm total deletion of entries              */
/* -------------------------------------------------------------------------- */
int get_confirmation(int def)
{
	FILE *fp;
	char line[64];

	fprintf(stderr, "%c", def);
	fflush(stderr);

#ifdef MACHINE_WNT
	if ((fp = fopen("CON", "r")) == NULL) {
		perror("\nCannot open CON");
	}
#else
	if ((fp = fopen("/dev/tty", "r")) == NULL) {
		perror("\nCannot open /dev/tty");
	}
#endif

	if (!fp) {
		return ('n');
	}

	if (fgets(line, sizeof(line), fp) == NULL) {
		line[0] = 'n';
	}

	fclose(fp);

	if (line[0] == '\n' || line[0] == 0) {
		line[0] = def;
	}

	return (line[0]);
}
/* -------------------------------------------------------------------------- */

