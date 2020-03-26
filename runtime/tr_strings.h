/* TradeXpress tr_strings.h */
/* #define TR_DEFAULT_DATE_FORMAT		"%d.%m.19%y %H:%M" */
#define TR_DEFAULT_DATE_FORMAT		"%d.%m.%Y %H:%M" /* 100998/JR */
/*======================================================================
======================================================================*/
#define TR_ERR_CANNOT_CREATE_PROCESS	"Failed to fork a process (%d)"
#define TR_ERR_WAIT_FAILED		"Waiting a child process failed"
#define TR_WARN_TOO_MANY_ARGS_IN_EXEC	"Too many arguments in calling a process"
#define TR_ERR_EXEC_FAILED		"Failed to execute \"%s\" (%d)"

#define TR_MSG_TERMINATED_BY_SIGNAL	"\"%s\": terminated by signal number %d"
#define TR_MSG_CORE_DUMPED		"Produced a core file for debugging"

#define TR_ERR_FLOATING_POINT_EXEPTION	"Floating point exception"
#define TR_ERR_BUSS_ERROR		"Bus error"
#define TR_ERR_SEGMENTATION_FAULT	"Segmentation violation"
#define TR_ERR_RECEIVED_SIGNAL		"Received signal %d."

#define TR_ERR_SIGNAL_ERROR		"Failed to set the signal handling function"
#define TR_ERR_SIGNAL_ALREADY_CAUGHT	"Signal has already been caught!"


/*======================================================================
	tr_log.c
======================================================================*/
#define TR_WARN_HEADER			"%s: Line %d WARNING: "
#define TR_WARN_HEADER_WITH_DATALINE	"%s: Line %d (data line %.0lf) WARNING: "
#define TR_FATAL_HEADER			"%s: Line %d FATAL: "
#define TR_FATAL_HEADER_WITH_DATALINE	"%s: Line %d (data line %.0lf) FATAL: "
#define TR_FATAL_WARNING_EXIT		"Exiting due to error level setting."
/*======================================================================
	tr_pick.c
======================================================================*/
#define TR_FATAL_TOO_MANY_WINDOW_LINES	"Too many lines in a single window."
#define	TR_WARN_ILLEGAL_LINENUMBER	"Attempting to pick from illegal line (%d)"
#define	TR_WARN_PICK_OUTSIDE_WINDOW	"Attempting to pick beyond defined window size (%d - %d)"
#define TR_WARN_UNEXPECTED_EOF		"Attempting to pick beyond end of file"
#define TR_WARN_ILLEGAL_POSITION	"Attempting to access text at illegal position (%d)"
#define TR_WARN_POSITION_AFTER_TEXT	"Attempting to access beyond text"
#define TR_WARN_NEGATIVE_LENGTH		"Attempting to obtain a negative number of characters! (%d)"

/*======================================================================
	tr_strcsep.c
======================================================================*/
#define TR_FATAL_OUT_OF_MEMORY "Attempting to allocate insufficient memory."
#define TR_WARN_ILLEGAL_TEXT_POSITION "Attempting to split illegal text at position (%d)"

/*======================================================================
======================================================================*/
#define TR_WARN_NEGATIVE_INDEX_WIDTH	"The defined numeric index width was negative, set to 4 (%d)."
#define TR_WARN_HUGE_INDEX_WIDTH	"The defined numeric index width was too large, set to 4 (%d)."
#define TR_ERR_ARRAY_INDEX_TOO_LARGE	"Array index %.0lf was too large."

#define TR_ERR_DIVISION_BY_ZERO		"Division by zero."

/*======================================================================
	tr_writebuf.c
======================================================================*/
#define TR_WARN_PLACE_ILLEGAL_LINE	"Attempting to place text at illegal line (%d)."
#define TR_WARN_PLACE_ILLEGAL_POS	"Attempting to place text at illegal position (%d)."
#define TR_WARN_FLUSH_ILLEGAL_MIN	"Illegal value (%d) specified for the minimun output width."
#define TR_WARN_FLUSH_ILLEGAL_MAX	"Illegal value (%d) specified for the maximum output width."
#define TR_WARN_FLUSH_MIN_GT_MAX	"Minimum (%d) output width exceeds the maximum (%d)!"
#define TR_WARN_NOT_FLUSHABLE		"Table \"%s\" is not a flushable buffer."
#define TR_WARN_LONG_LINE_TRUNCATED	"Line %d was too long. Truncated (the entire line is below).\n\"%s\""

/*======================================================================
	tr_logsys.c
======================================================================*/
#define TR_WARN_CANNOT_CREATE_NEW	"Cannot create a new entry to logsystem \"%s\""
#define TR_WARN_LOGFIELD_UNKNOWN	"Logsystem \"%s\" has no field \"%s\""
#define TR_WARN_LOGFIELD_MISMATCH	"Logsystem \"%s\" has field \"%s\" with different type, %d"
#define TR_WARN_LOGENTRY_NOT_SET	"Log entry does not contain anything."
#define TR_FATAL_LOGSYS_OPEN_FAILED	"Failed to open data base \"%s\"."
#define TR_FATAL_ILLEGAL_LOGSYS		"Data base \"%s\" is not the same kind as the defined model."
#define TR_FATAL_NO_LOGSYS		"No data base specified!"
#define TR_WARN_MODIFYING_LOGSYS_TIMES	"Data base system time stamps cannot be changed directly."
#define TR_WARN_MODIFYING_LOGSYS_INDEX	"Data base index cannot be changed directly."
#define TR_WARN_TRAVERSE_AFTER_CREATION "Cannot get the next entry since previous was created."
#define TR_WARN_BAD_TIME_VALUE		"\"%s\" is not a valid time value."

#define TR_FATAL_INVALID_GETFP_ARGS	"Invalid argumets passed to a file opening routine (internal)."
#define TR_FATAL_INVALID_GETFP_MODE	"Invalid mode \"%s\" specified for file opening (internal)."
#define TR_WARN_INVALID_MODE_IN_FILETAB	"Invalid mode exists in the file table (internal)."
#define TR_WARN_GETFP_FOR_READ_FAILED	"Failed to open \"%s\" for reading (error code %d)."
#define TR_WARN_GETFP_FOR_WRITE_FAILED	"Failed to open \"%s\" for writing (error code %d).\nUsing this logging stream instead."
#define TR_WARN_TOO_MANY_FILES_FOR_READ "Too many open files.  Could not open \"%s\" for reading."
#define TR_WARN_TOO_MANY_FILES_FOR_WRITE "Too many open files.  Could not open \"%s\" for writing.\nUsing this logging stream insted."

#define TR_WARN_UNLINK_FAILED		"Failed to remove file \"%s\" (error code %d)."
#define TR_WARN_FILE_R_OPEN_FAILED	"Failed to open file \"%s\" for reading (error code %d)."
#define TR_WARN_FILE_W_OPEN_FAILED	"Failed to open file \"%s\" for writing (error code %d)."
#define TR_WARN_FILE_LINK_FAILED	"Failed to rename \"%s\" to \"%s\" (error code %d)."

#define TR_FATAL_ILLEGAL_SYS_FILE	"Illegal system file handle %d (internal)."
#define TR_WARN_REOPEN_FAILED		"Redirecting file descriptor %d to \"%s\" failed (%d)."

#define TR_WARN_EMPTY_TEXT_TO_NUM	"Converting empty string to number yields 0"
#define TR_WARN_NON_NUMERIC_CONV	"String \"%s\" contains non numeric characters. Conversion yields 0."
#define TR_WARN_EXTRA_CHARS_IN_CONV	"String \"%s\" is not a valid number. Conversion yields 0."

#define TR_FATAL_BUILD_OVERFLOW		"Internal memory overflow.  Exiting due to a possible corruption."

#define TR_FATAL_NO_MEMORY		"No memory available."
#define TR_FATAL_TOO_MANY_CHAR_DIRS	"Too many search directories for character set specified (%d allowed)."
#define TR_FATAL_INVALID_COMMANDLINE	"Invalid command line option or missing option argument."
#define TR_FATAL_GETCWD_FAILED		"Could not determine the current working directory."

#define TR_WARN_HOME_NOT_SET		"Environment variable \"HOME\" is not set."
#define TR_WARN_DIR_OPEN_FAILED		"Failed to open directory \"%s\" for scanning (error code %d)."
#define TR_DIR_COUNTERS			".counters"
#define TR_WARN_SEEK_FAILED		"File seek failed."
#define TR_WARN_LOCKING_FAILED		"Locking for \"%s\" failed (error code %d)."
#define TR_WARN_LOCK_RELEASE_FAILED	"Unlocking \"%s\" failed (error code %d)."

#define TR_ERR_DEPENDENCY_ERROR		"Segment \"%s\" has a violated dependency (\"%s\")."
/* 20.09.99/JH */
#define TR_ERR_MSG_DEPENDENCY_ERROR	"Message \"%s\" has a violated dependency (\"%s\")."

#define TR_ERR_NO_SUCH_SEGMENT		"No segment \"%s\" in this message (%s)."
#define TR_ERR_TOO_DEEP_NESTING		"Too many nested groups for segment \"%s\"(limit is %d)."
#define TR_ERR_NON_NUMERIC_REPETITION   "Non numeric repetition specifier for segment \"%s\" in group \"%s\" (%s)."
#define TR_ERR_INVALID_GROUP_LIST       "Invalid group list for segment \"%s\" given (\"%s\")."
#define TR_ERR_GRPLIST_CONFLICT		"Conflict in group specification for segment \"%s\" (was \"%s\", expected \"%s\")."

#define TR_WARN_NO_MDS_SPECIFIED	"No decimal separator (MDS) specified for the message."
#define TR_WARN_NO_ADS_SPECIFIED	"No inhouse decimal separator (IDS) specified."

#define TR_WARN_LOAD_SEP_MISMATCH	"Separators differ from the previous load. Using the old values."

#define TR_WARN_NO_VALUE_IN_COLUMN	"There is no value for key \"%s\"(%d) in column %d."
#define TR_WARN_NO_SUCH_CODETABLE	"Code conversion file \"%s\" not found."

#define TR_WARN_CANNOT_GET_DEFAULTS	"Cannot open defaults file \"%s\" (error code %d)"
#define TR_WARN_UNKNOWN_DEFAULT		"Unknown default specification \"%s\""

#define TR_WARN_WEIRD_SEGMENT		"Empty segment \"%s\" in the message"
#define TR_WARN_MESSAGE_NOT_SAVED	"Incoming message was not saved. Cannot print."
#define TR_WARN_STAT_FAILED		"Failed to retrieve file information for \"%s\" (error code %d)"

#define TR_FILE_DIRECTORY		"DIRECTORY"
#define TR_FILE_REGULAR			"REGULAR"
#define TR_FILE_SPECIAL			"SPECIAL"
