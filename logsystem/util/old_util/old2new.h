/*========================================================================
        E D I S E R V E R

        File:		old2new.h
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Old-style logsystem definitions and structures.
========================================================================*/
/*========================================================================
  @(#)TradeXpress $Id: old2new.h 47371 2013-10-21 13:58:37Z cdemory $
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#define LS_ERROR_HEADER		"Logsys: "
#define LS_CORRUPTION_WARNING	"Possible super block corruption"
#define LS_QUESTION_REMOVE_ALL	"Remove all entries in the logsystem? (Y/N) :"
#define LS_POSITIVE_ANSWER	'Y'

/*==============================================================================
  These two values are used as defaults when the logsystem
  is created.  Their text has to match by the first letter
  with the definitions
	LS_SIZE_...	(BYTE, KILOBYTE, MEGABYTE)
	LS_TIME_...	(WEEK,DAY,HOUR,MIN)
  The system uses only the first letter when determining the
  units at runtime.  For the time the default is 'W' (WEEK)
  which is applied if the given unit is not any of the remaining
  ('D', 'H', 'M').  The corresponding unit for size is 'K'
  (KILOBYTE).
  The system converts all letters to upper case at runtime for
  evaluation purposes, thus e.g.
	bytes
	byte
	b
	baitti
	BYTE
  are all valid values for representing size unit BYTE.
  NOTE!  You can change the text here but the mentioned WEEK
         and KILOBYTE defaults have been built in to the code.
==============================================================================*/
#define LS_DEFAULT_SIZE_UNIT	"KiloByte"
#define LS_DEFAULT_TIME_UNIT	"Week"

#define LS_SIZE_BYTE		'b'
#define LS_SIZE_KILOBYTE	'K'
#define LS_SIZE_MEGABYTE	'M'
/*==============================================================================
  These four definitions are used as described above, but
  also used as qualifiers when entering a relative time.
	"now + 3 w"
	"now - 1 H"
==============================================================================*/
#define LS_TIME_WEEK		'w'	
#define LS_TIME_DAY		'd'
#define LS_TIME_HOUR		'H'
#define LS_TIME_MIN		'M'

/*==============================================================================
	Time related constants
==============================================================================*/
#define LS_TIME_NOW		"now"
#define LS_TIME_NULL		"none"
#define LS_TIME_TODAY		"today"
#define LS_TIME_TODAY2		"today"
#define LS_TIME_TOMORROW	"tomorrow"
#define LS_TIME_YESTERDAY	"yesterday"

#define LS_TIME_SUNDAY		"Sunday"
#define LS_TIME_MONDAY		"Monday"
#define LS_TIME_TUESDAY		"Tuesday"
#define LS_TIME_WEDNESDAY	"Wednesday"
#define LS_TIME_THURSDAY	"Thursday"
#define LS_TIME_FRIDAY		"Friday"
#define LS_TIME_SATURDAY	"Saturday"

#define LS_TIME_JANUARY		"January"
#define LS_TIME_FEBRUARY	"February"
#define LS_TIME_MARCH		"March"
#define LS_TIME_APRIL		"April"
#define LS_TIME_MAY		"May"
#define LS_TIME_JUNE		"June"
#define LS_TIME_JULY		"July"
#define LS_TIME_AUGUST		"August"
#define LS_TIME_SEPTEMBER	"September"
#define LS_TIME_OCTOBER		"October"
#define LS_TIME_NOVEMBER	"November"
#define LS_TIME_DECEMBER	"December"

#define LS_SHORT_WEEKDAY	3
#define LS_SHORT_MONTH		3
/*==============================================================================
	Default time format.  Make sure this is always
	in a format accepted by time entry routines, because:
	When printing name-value pairs (which are practically
	never used besides for editing purposes) all time
	fields are set to this format.
	NOTE:	Later in this file there are two fields

			MKLS_DEFAULT_FORMAT_FOR_CTIME
			MKLS_DEFAULT_FORMAT_FOR_MTIME

		which contain the same string as in here.  If you change
		that, please make the same changes there too.
		(Our compliments to MIPS RiscOS for not following
		the C-language basic rules...)
==============================================================================*/
#define LS_TIME_DEFAULT_FORMAT  "%d.%m.19%y %H:%M"

/*==============================================================================
	Components in the logsystem structure
==============================================================================*/
#define LS_DIR_DATA_FILE		".base"
#define LS_DIR_EXTENSION_FILE		".ext"
#define LS_DIR_FILES_DIR		"files"
#define LS_DIR_DISPLAY_DIR		"displays"
#define LS_DIR_BIN_DIR			"bin"
#define LS_DIR_VERSION_FILE		".@(#)TradeXpress database by mkls 3.01"
#define LS_DIR_HEADER_TEXT_FILE		".header"
#define LS_DIR_FORMAT_FILE		".format"
#define LS_DIR_HELP_FILE		".help"
#define LS_DIR_LIMITS_FILE		".limits"

#define LS_RUN_ADD_PROG			"bin/addprog"
#define LS_RUN_REMOVE_PROG		"bin/removeprog"
#define LS_RUN_REFRESH_PROG		"logrefresh"
#define LS_RUN_COUNT_PROG		"bin/countprog"
#define LS_RUN_SIZE_PROG		"bin/sizeprog"
#define LS_RUN_AGING_PROG		"bin/agingprog"
#define LS_RUN_OWN_PROG			"bin/ownprog"

/*==============================================================================
	Error messages from ls_lowlib.c
==============================================================================*/
#define LS_ERR_NO_MEMORY		"Out of memory!"
#define LS_ERR_LOGSYS_NOT_SET		"Environment variable LOGSYS is not set."
#define LS_ERR_EDIHOME_NOT_SET		"Environment variable EDIHOME is not set."
#define LS_ERR_HOME_NOT_SET		"Environment variable HOME is not set."
#define LS_ERR_SYS_OPEN_FAILED		"Failed to open system \"%s\" (%d)."
#define LS_ERR_SUPERBLOCK_READ_FAILED	"Failed to read information for \"%s\" (%d)."
#define LS_ERR_SEEK_FAILED		"File seek failed."
#define LS_ERR_READ_FAILED		"Failed to read record %d."
#define LS_ERR_INVALID_LOGSYS		"No system or the system is not open."
#define LS_ERR_WRITE_FAILED		"Failed to write record %d."
#define LS_ERR_SUPERBLOCK_WRITE_FAILED	"Failed to write information for \"%s\" (%d)."
#define LS_ERR_NO_EXT_FILE		"No extensions file available."
#define LS_ERR_LOCKING_FAILED		"Locking for \"%s\" failed (%d)."
#define LS_ERR_LOCK_RELEASE_FAILED	"Unlocking \"%s\" failed (%d)."
#define LS_ERR_INVALID_OPERATOR		"Invalid operator in expression for field \"%s\"."
#define LS_ERR_INVALID_PRINTFORMAT	"Invalid format specifier for time field (\"%s\")."
#define LS_ERR_INVALID_FIELDNAME	"Invalid first character in fieldname \"%s\" (Expected a letter)."

/*==============================================================================
	Time conversion error messages
==============================================================================*/
#define LS_ERR_EMPTY_TIME		"Empty time specification."
#define LS_ERR_SYSTIME_ERROR		"Cannot get system time!"
#define LS_ERR_EXTRA_CHARS		"Extra characters in the date."
#define LS_ERR_INVALID_TIME_STRING	"Invalid time specification."
#define LS_ERR_INVALID_QUALIFIER	"Invalid time qualifier."
#define LS_ERR_INVALID_TIME		"The given string does not even look like a date!"
#define LS_ERR_ONLY_12_HOURS		"Only 12 hours before noon!"
#define LS_ERR_NUMBER_FORMAT_EXPECTED	"Expected format \"yyyymmddHHMM\", but got something else."
#define LS_ERR_INVALID_YEAR_OR_MONTH	"Invalid year or month."
#define LS_ERR_YEAR_RANGE		"Only dates between years 1903 and 2037 are possible!"
#define LS_ERR_MINUTE_RANGE		"Minutes have to be 0-59."
#define LS_ERR_HOUR_RANGE		"Hours have to be 0-23."
#define LS_ERR_DAY_RANGE		"Day of the month has to be 1-31."
#define LS_ERR_MONTH_RANGE		"Month can be 1-12."
#define LS_ERR_TOO_MANY_DAYS		"Not that many days in the month."

/*==============================================================================
==============================================================================*/
#define LS_ERR_NULL_FILENAME		"No filename given"
#define LS_ERR_FILE_OPEN_FAILED		"Opening file \"%s\" failed (%d)"
#define LS_ERR_EMPTY_PARSE_BUFFER	"No buffer or empty buffer for expression"
#define LS_ERR_NO_SUCH_FIELD		"Field \"%s\" does not belong to this system"

/*==============================================================================
==============================================================================*/
#define LS_ERR_INVALID_NUMERIC_COMPARISON "Invalid operator for comparing numeric (%d)."
#define LS_ERR_INVALID_TIME_COMPARISON "Invalid operator for comparing time (%d)."
#define LS_ERR_INVALID_TEXT_COMPARISON "Invalid operator for comparing text (%d)."
#define LS_ERR_CORRUPTED_SUPERBLOCK	"Corrupted system information."

#define LS_ERR_CANNOT_REMOVE_TWICE	"Entry with index %d has already been removed."
#define LS_ERR_ALREADY_REMOVED		"Entry with index %d has already been replaced."
/*==============================================================================
	logchange
==============================================================================*/
#define LS_ERR_INVALID_SORT_FIELD	"The specified sort key \"%s\" is not part of the system."
#define LS_ERR_NON_EXISTING_EXT_FIELD	"The specified field \"%s\" is not part of the system."
#define LS_ERR_INVALID_EXT_FIELD	"Only text field can be used as extension or application specifier."
#define LS_ERR_EMPTY_EXT_FIELD		"The specifier field is empty."
/*==============================================================================
==============================================================================*/
#define LS_MOTIF_FILE_EDITOR		"fer"
#define LS_ERR_EMPTY_COMMAND_STRING	"Empty command string given."
#define LS_WARN_DIR_OPEN_FAILED		"Failed to scan directory \"%s\" (%d)."
/*==============================================================================
	mkls
==============================================================================*/
#define MKLS_ERR_NO_FIELD_NAME		"No field name specified."
#define MKLS_ERR_TOO_MANY_FIELDS	"Too many fields spcified (%d is the maximum)."
#define MKLS_ERR_TOO_LONG_FIELD_NAME	"Field name \"%s\" is too long (%d characters, %d allowed)."
#define MKLS_ERR_NO_TYPE_FOR_FIELD	"No type specified for field \"%s\"."
#define MKLS_ERR_INVALID_LEN_FOR_FIELD	"Invalid length \"%s\" specified for field \"%s\"."
#define MKLS_ERR_UNKNOWN_IDENTIFIER	"Unknown action identifier \"%s\"."
#define MKLS_DEFAULT_FORMAT_FOR_INDEX	"INDEX=%.0lf"
#define MKLS_DEFAULT_FORMAT_FOR_CTIME	"CTIME=%d.%m.19%y %H:%M"
#define MKLS_DEFAULT_FORMAT_FOR_MTIME	"MTIME=%d.%m.19%y %H:%M"
#define MKLS_DEFAULT_HEADER_FOR_INDEX	"INDEX=INDEX"
#define MKLS_DEFAULT_HEADER_FOR_CTIME	"CTIME=CTIME"
#define MKLS_DEFAULT_HEADER_FOR_MTIME	"MTIME=MTIME"
#define MKLS_USAGE			"Usage: %s sysname file"

#define MKLS_ERR_UNABLE_TO_READ_CONFIG	"Cannot read configuration file \"%s\" (%d)."
#define MKLS_ERR_UNABLE_TO_CREATE_DIR	"Failed to create directory \"%s\" (%d)."
#define MKLS_ERR_UNABLE_TO_CHANGE_DIR	"Failed to change directory \"%s\" (%d)."
#define MKLS_ERR_UNABLE_TO_CREATE_FILE	"Failed to create file \"%s\" (%d)."
#define MKLS_ERR_UNABLE_TO_WRITE_FILE	"Failed to write to file \"%s\" (%d)."

#define MKLS_TEXT_MAX_COUNT		"MAX_COUNT"
#define MKLS_TEXT_MAX_AGING		"MAX_AGING"
#define MKLS_TEXT_MAX_SIZE		"MAX_SIZE"
#define MKLS_TEXT_AGING_CHECK		"AGING_CHECK"
#define MKLS_TEXT_AGING_UNIT		"AGING_UNIT"
#define MKLS_TEXT_SIZE_CHECK		"SIZE_CHECK"
#define MKLS_TEXT_SIZE_UNIT		"SIZE_UNIT"
#define MKLS_TEXT_OWN_PROG		"OWN_PROG"
#define LS_SUPERBLOCK_SIZE      2048
#define LS_NUM_FIELDS           35
#define LS_FIELD_ID_SIZE        19
#define LS_UNIT_SIZE            15
#define LS_SYSNAME_SIZE         31
#define END_OF_CHAIN            -1

#define LS_TYPE_NONE -3
#define LS_TYPE_TIME -2
#define LS_TYPE_NUMERIC -1
#define LS_TYPE_TEXT 0
/* 0 and positive integers are the length of text fields */

typedef struct {
	double  index;
	time_t  creation;
	time_t  modification;
	int     next;
} OLD_LogEntry;

typedef struct {
	char name[LS_FIELD_ID_SIZE + 1];
	int  length;
	int  pos;
} OLD_FieldInfo;

typedef struct {
	int length;             /* length of value field */
	int op;                 /* comparison operator */
	int pos;                /* position of the field */
	union {
		char   *text;   /* text field */
		double num;     /* numeric field */
		time_t tim[2];  /* time field */
	} value;
} OLD_LogFilter;

typedef OLD_LogFilter ValueTable;
typedef char *    OLD_PrintFormat[LS_NUM_FIELDS];

/*======================================================================
Logsystem definition.
======================================================================*/
typedef struct {
	char        sysName[LS_SYSNAME_SIZE + 1];       /* Free form name */
	/*==============================================================
	System core data.
	This data is altered by the programs only and its
	corruption is not a funny thing to happen.
	==============================================================*/
	int numEntries;       /* Total number of entries */
	int insertPoint;      /* Where to add the next one */
	int freeSpot;         /* Spot freed up by removing something */
	int cleaningUp;       /* A user clean up program is running if set */
	int ownProgCounter;   /* Number of additions before next own p rogram */
	int sizeCheckCounter; /* Number of additions before next size check */
	int agingCheckCounter;/* Number of additions before next aging check */

	/*==============================================================
	System field definitions.
	==============================================================*/
	int numFields;
	int entrySize;
	OLD_FieldInfo   field[LS_NUM_FIELDS];
	/*==============================================================
	Runtime fields.
	==============================================================*/
	FILE *fp;             /* The file pointer to this file */
	int   dirty;          /* Have we written to the super block ? */
	char *path;           /* The complete path to this system */
	int   pathLength;     /* length of the path */
	OLD_PrintFormat *printFormat;       /* Default print format */
} OLD_LOGSYS;

typedef struct {
	int maxNumEntries;
	int maxAging;
	int maxSize;
	int agingCheckInterval;
	char agingUnit[LS_UNIT_SIZE + 1];
	int sizeCheckInterval;
	char sizeUnit[LS_UNIT_SIZE + 1];
	int ownProgInterval;
} OLD_Limits;

typedef struct {
	time_t	creation;
	time_t  modification;
	int     next;
} OLD_Header;

