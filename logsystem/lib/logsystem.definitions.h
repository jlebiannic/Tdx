/*========================================================================
        TradeXpress

        File:		logsystem.sqlite.h
        Author:		Frédéric Heulin (FH/Fredd)
        Date:		Fri Feb 17 10:53:15 CET 2006

        Copyright Generix-Group 1990-2011

        This file is taken from logsystem.h and has only the definitions
        that are necessary in all cases 
        (inside/outside logsystem/lib,
         sqlite,old_legacy inside logsystem/lib)
==========================================================================
  @(#)TradeXpress $Id: logsystem.definitions.h 55415 2020-03-04 18:11:43Z jlebiannic $
  Record all changes here and update the above string accordingly.
  3.00 17.02.06/FH	Copied from logsystem.h
  10.04.18 AAU (AMU) BG-198 : Extended logfilter to add env_val and env_op to implement filtering by environment in MBA mode
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#ifndef _LOGSYSTEM_LOGSYSTEM_DEFINITIONS_H
#define _LOGSYSTEM_LOGSYSTEM_DEFINITIONS_H
#include <time.h>

#include "sqlite3.h"
#include "data-access/dao/dao.h"

/* from "runtime/tr_prototypes.h" (we do not want to include all symbol, just take the needed ones */
extern char* tr_timestring (char *,time_t );
extern void	 tr_parsetime  (char *,time_t*);

#define LSVERSION 3

#ifndef _REMOVAL_C
extern int LOGSYS_EXT_IS_DIR;
#else
int LOGSYS_EXT_IS_DIR;
#endif

#define LSMAGIC_LABEL (0x2ED1DB00 | (int) 'l')

#define LSFILE_LABEL     "/.label"
#define LSFILE_FILES     "/files"
#define LSFILE_TRACE     "/trace"
#define LSFILE_TMP       "/tmp"

#define LSFILE_ADDPROG       "/bin/addprog"
#define LSFILE_REMOVEPROG    "/bin/removeprog"
#define LSFILE_CLEANUPPROG   "/bin/cleanupprog"
#define LSFILE_THRESHOLDPROG "/bin/thresholdprog"

/* 
 * sometimes we need old legacy stuff 
 */
#define LS_GENERATION_MOD 10
#define LS_VINDEX(idx,gen) ((idx) * LS_GENERATION_MOD + (gen) % LS_GENERATION_MOD)

/* Is the disk-record in use ? */
#define ACTIVE_ENTRY(rec) ((rec)->header.ctime)

/* Pointer to mmapped data. */
#define MMAPPED_RECORD(log, idx)	((void *)((char *)(log)->mmapping + (idx) * (log)->label->recordsize))
/* 
 * ... not too much fortunately
 */

enum {
	LS_FAILOK   = (1 << 0),
	LS_DONTFAIL = (1 << 1),
	LS_READONLY = (1 << 2)
};

/* ALWAYS use these, never real types ! Any data that lays on disk, should use int32. */
typedef unsigned int  LogIndex;
/*JRE 06.15 BG-81*/
typedef char* LogIndexEnv;
/*End JRE*/
typedef unsigned int  Integer;
typedef time_t      TimeStamp;
typedef double        Number;

typedef int           int32;

/* These define the fields of a record. Not all are supported. */
enum {
	FIELD_NONE = 0,
	FIELD_INDEX,
	FIELD_TIME,
	FIELD_NUMBER,
	FIELD_INTEGER,
	FIELD_TEXT,
	FIELD_USER0,
	FIELD_USER1,
	FIELD_NTYPES,		/* must be last */

	PSEUDOFIELD_OWNER	/* master mode stuff */
};

/**************************/
/* specific to old_legacy */
/**************************/

/* Often changing variables in a logsystem. */
typedef struct logmap {
	int32 magic;
	int32 version;

	int32 numused;

	int32 reserved0;

	int32 reserved1;
	int32 reserved2;

	LogIndex newest;
	LogIndex oldest;

	LogIndex lastindex;	/* last existing index */

	LogIndex firstfree;	/* freelist; first to be reused */
	LogIndex lastfree;	/*  and last */

	int32 reserved3;
	int32 reserved4;
	int32 reserved5;
	int32 reserved6;
} LogMap;

/******************************/
/* end specific to old_legacy */
/******************************/

typedef struct logfield {
	int32 type;
	int32 offset;
	int32 size;
	int32 order;
	union {
		int32 align[2];
		int32 offset;		/* when in disk */
		char *string;		/* when in memory */
	} name, format, header;
} LogField;

typedef struct {
	LogIndex idx;
	TimeStamp stamp;
} LogHints;

/* Comparisons in logfilter, operators in logoperator. */
enum {
	OP_NONE,
	OP_EQ,
	OP_NEQ,
	OP_LT,
	OP_LTE,
	OP_GTE,
	OP_GT,
	OP_INC,
	OP_DEC,
	OP_ADD,
	OP_SUB,
	OP_SET,
	OP_LEQ,
	OP_LNEQ
};

#ifdef OPERATOR_STRINGS
/* This are used primarily to print out operators, parsing of input is done manually in code. */
static char *operator_strings[] = {
	".",
	"=",
	"!=",
	"<",
	"<=",
	">=",
	">",
	"++",
	"--",
	"+=",
	"-=",
	":=",
	"=",
	"!="
};
#endif

/*
 * Filters, printformats and operator-lists
 * have the data in textual format, and
 * they must be compiled against a logsystem
 * before use.
 * In Master-Log use, the change of logsystem can be detected
 * when the logsys_compiled_against differs from logsys in entry.
 */

/*
 * Filtering
 *
 * The textual format can be either a single member
 * in the implicit AND operation between all filter-records;
 * or there can be multiple primaries for one record.
 *
 * STATUS="OK|Transporting*" PARTNER="kalle|ville"
 * would create following gate:
 *
 * ((STATUS=OK || STATUS=Transporting*) && (PARTNER=kalle || PARTNER=ville))
 */

typedef struct {
	char *name;			/* name of field */
	int op;				/* comparison (=, != etc) */
	char *value;			/* filter value in text format */
	LogField *field;		/* the real field (in log label) */
	int (*func)();			/* function to do the comparison */
	union FilterPrimary {
		Number    number;	/* at least one primary should */
		TimeStamp times[2];	/* exist, even ="" creates one */
		char *    text;
		Integer   integer;
	} *primaries;
	int n_primaries;		/* count valid at above array */
} FilterRecord;

typedef struct {
	FilterRecord *filters;
	int filter_count;
	int filter_max;

	struct logsystem *logsys_compiled_to;

	char *sort_key;			/* fieldname */
	int sort_order;			/* -1 descending, 1 ascending */
	int max_count;
	int offset;
    char *separator;        /* 4.00/CD separator for printout */
	char *env_val;			/* le nom de la base en mode mba */
	int  env_op;			/* operateur du filter du nom de base en mode mba */
} LogFilter;

/* Operator table used to change logentries. Operator record is identical to filter record. */

typedef struct {
	char *name;			/* name of field */
	int op;				/* comparison (=, != etc) */
	char *value;			/* filter value in text format */
	LogField *field;		/* the real field (in log label) */
	int (*func)();			/* function to do the comparison */
	union {
		Number    number;
		TimeStamp times[2];
		char *    text;
		int       integer;
	} compiled;
} OperatorRecord;

typedef struct {
	OperatorRecord *operators;
	int operator_count;
	int operator_max;

	struct logsystem *logsys_compiled_to;

} LogOperator;

/* Printout format */
typedef struct {
	char *name;
	char *format;
	LogField *field;
	int width;
} FormatRecord;

typedef struct {
	FormatRecord *formats;
	int format_count;
	int format_max;
	int estimated_linelen;		/* approx. length of filtered line */
	struct logsystem *logsys_compiled_to;
	int is_namevalue;
	char *separator;		/* 4.00/CD column separator */
} PrintFormat;

/*
 * The static part of logsystem,
 * changes only when logsystem is reconfigured.
 *
 * Number of fields varies,
 * only needed count of array-members
 * should be written to disk file.
 *
 * Strings in fields are normalized on disk,
 * they need to be corrected after reading,
 * address of label itself has to be added.
 */
typedef struct loglabel {
	int32 magic;
	int32 version;
	int32 machine;
	int32 stamp;

	int32 nfields;		/* one more there actually is, last is dummy */
	int32 recordsize;

	int32 maxcount;
	int32 cleanup_count;	/* count of records to clear when cleaning up */
	int32 cleanup_threshold;/* cleanup when freecount drops below this */
	int32 max_diskusage;	/* du .../sysname */

	int32 creation_hints;	/* number of entries to keep */
	int32 modification_hints;/*  in history hint-lists */

	int32 preferences;
	int32 labelsize;	/* true size of label */
	int32 reserved7;
	int32 reserved8;

	int32 run_addprog;	/* run addprog after addition */
	int32 run_removeprog;	/* run removeprog before removal */
	int32 run_cleanupprog;	/* run cleanupprog before default cleanup */
	int32 run_thresholdprog;/* run thresholdprog when filling up */

	struct {
		int32 start;
		int32 end;
	} types[FIELD_NTYPES];

	LogField fields[1];	/* varies, this must be last */
} LogLabel;

#define LABELSIZE(lab) ((lab)->labelsize)

/* structures to handle sql indexes 
 * used notably in .cfg files 
 * and RTE while/find commands */
typedef struct logsqlindex {
	char *name;
	int n_fields;
	LogField **fields;	/* varies, this must be last */
} LogSQLIndex;

typedef struct logsqlindexes {
	int n_sqlindex;
	LogSQLIndex **sqlindex;	/* varies, this must be last */
} LogSQLIndexes;

/*JRE 06.15 BG-81*/
typedef struct logenv logenv;
struct logenv {
	
	char * env;
	
	char * base;

	struct logenv * pSuivant;

};

typedef logenv* LogEnv;
/*End JRE*/

/* Only in memory, no corresponding thing in filesystem. */
typedef struct logsystem {
	char *sysname;
	char *owner;		/* unix username */

	int open_mode;

	int labelfd;
	int datafd;
    sqlite3 *datadb_handle; /* sqlite3 database handler */
	Dao *dao; /* Jira TX-3199 DAO: abstraction base de donnée */
	char *table; /* Jira TX-3199 DAO: nom table courante */

	int cleanupfd;		/* lock during cleanup */

	time_t last_reread;

	int refcnt; /* foallocation.cr logd */

	LogLabel *label;

	int sorter_offset;	/* used when sorting a list of entries */

	LogIndex walkidx;	/* used when walking all the records */
	LogIndex *indexes; /* list of indexes of the current research */
	int indexes_count;

	void *next;		/* hooks for logd links, "other LSses" */

    /******************************/
    /* specific to old_legacy     */
    /******************************/
	int mapfd;
	LogMap *map;
	void *mmapping;		/* if the datafile is mmapped */
	void *mmapping_end;	/* one over last mmapped byte */
    /******************************/
    /* end specific to old_legacy */
    /******************************/
    
    /*JRE 06.15 BG-81*/
    /******************************/
    /* specific to sqlite	      */
    /******************************/
    LogEnv listSysname;		/*list of logsystem names*/
    LogIndexEnv *indexesEnv;  /* list of indexes of the current research for muti base */
    /******************************/
    /* end specific to sqlite     */
    /******************************/
	/*End JRE*/
	
	LogSQLIndexes *sqlindexes;
} LogSystem;

/* 
 * specific to the old_legacy case :
 * Each record has a generation counter which is included in the
 * virtual index shown to user.
 * The physical index is multiplied by 10 and generation modulo 10
 * is added to that.
 * So phys. index 123 might show as 1230 to 1239 to user.
 * This is done to prevent modifications to records after the records
 * have been destroyed and re-used.
 */

/*
 * The DiskLogRecord is the real entry (in disk on old_legacy)
 * Size of DiskLogRecord is variable,
 * label->recordsize is the true size.
 *
 * Make sure you know what you are doing,
 * if changing this. Doublecheck with readconfig.
 * The builtin fields have to be sorted by type.
 */
typedef union disklogrecord {
	double align;
	char buffer[1];			/* varies */
	struct {
		LogIndex idx;		/* Virtual index */
        /* legacy : includes generation 
         * sqlite : same as index */
		LogIndex nextfree;	/* Other uses too... */
		TimeStamp ctime;	/* Creation time */
		TimeStamp mtime;	/* Last writeout time */
		Integer generation;	
        /* legacy : Incremented in destroy 
         * sqlite : not used anymore */
		Integer reserved1;
		char rest[1];		/* varies */
	} header;
} DiskLogRecord;

/* The LogEntry is constructed in memory.
 * The record is the real entry in disk in old legacy case. */
typedef struct logentry {
	LogIndex idx;
    /* legacy : The PHYSICAL index in datafile
     * sqlite : same as virtual and real index in the sqlite database (primary key) */
	LogSystem *logsys;

	union disklogrecord *record;
} LogEntry;

/*JRE 06.15 BG-81*/
typedef struct logentryenv {
	LogIndexEnv idx;
    /* legacy : The PHYSICAL index in datafile
     * sqlite : same as virtual and real index in the sqlite database (primary key) */
	LogSystem *logsys;

	union disklogrecord *record;
} LogEntryEnv;
/*End JRE*/

#define record_buffer	   record->buffer

#ifdef SIZEOF
#undef SIZEOF
#endif
#define SIZEOF(array) (sizeof(array)/sizeof((array)[0]))

#endif
