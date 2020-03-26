#ifndef _TRADEXPRESS_RUNTIME_TR_CONSTANTS_H_
#define _TRADEXPRESS_RUNTIME_TR_CONSTANTS_H_

/* this is the maximum length of a RTE text variable 
 * this is accordingly the max size of a length considered by RTE pick function */
#define TR_STRING_MAX_SIZE 32 * 1024 

/*
 * Log levels
 */
#define LOGLEVEL_NONE 0 
#define LOGLEVEL_NORMAL 1 
#define LOGLEVEL_DEBUG 2 
#define LOGLEVEL_DEVEL 3 
#define LOGLEVEL_BIGDEVEL 4 
#define LOGLEVEL_ALLDEVEL 5 

#endif
