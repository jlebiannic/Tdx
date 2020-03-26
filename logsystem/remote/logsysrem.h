#ifndef __LOGSYSREM_H
#define __LOGSYSREM_H
/* TradeXpress $Id: logsysrem.h 53221 2017-06-22 13:20:01Z tcellier $ */
#ifdef MACHINE_WNT
#define PSEP "\\"
/* Define some file type macros, as they seem not to bedefined in NT... */
/* 25.06.98/KP */
#define S_ISFIFO(m)     (((m)&(0170000)) == (0010000))
#define S_ISDIR(m)      (((m)&(0170000)) == (0040000))
#define S_ISCHR(m)      (((m)&(0170000)) == (0020000))
#define S_ISBLK(m)      (((m)&(0170000)) == (0060000))
#define S_ISREG(m)      (((m)&(0170000)) == (0100000))
#else
#define PSEP "/"
#define HANDLE int
#define DWORD  int
#define BOOL   int
#endif

#define NOTNUL(a) a?a:""

#define USE_2_0_SOCKS 0

#ifdef MACHINE_WNT
#if USE_2_0_SOCKS
#include <winsock2.h>
#else
#include <winsock.h>
#endif
#else
typedef int          OVERLAPPED;
typedef int          SOCKET;
#endif

#include <stdio.h>
#include <time.h>
#include <sys/types.h>

#include "rlslib/rls.h"
#include "logsystem/lib/logsystem.h"

#ifndef EFTYPE
#define EFTYPE (-1) /* EFTYPE was a bad choice, few unices define it */
#endif

/* filter types */
#define FILE_FILTER_TYPE_RTE      0 /* tr_wildmat */

/* filter action types */
#define FILE_FILTER_ACTION_READ   0
#define FILE_FILTER_ACTION_FIND   1
#define FILE_FILTER_ACTION_FILTER 2

typedef struct filefilter {
	int direction;
	int type; /* XXX in case we want to give different type of filters : proprietary, file_globbing, posix, pcre, etc… */
	int action; /* filter / find / read ? */
	size_t chuncksize;
	char *filter;
	off_t prevFound;
} FileFilter;

typedef struct filestruct {
	int fnum;
	char *name;
	int bReadonly:1,
	    bAppend:1,
	    :0;
	FileFilter *filter;
} FileStruct;

typedef struct Client Client;
typedef struct Base   Base;

typedef struct stat   stat_t;
typedef union rlsvalue rlsvalue_t;

/*
 *  See nilops for stubs to fill in for another basestyle.
 *  Global baseops[] contains all different ways,
 *  last one is &nilops which always fails everything.
 */
typedef struct baseops {

	/*
	 *  Open and close for the underlying base.
	 *
	 *  What is returned from open_base(char *) is kept in base->internal,
	 *  and given to close_base(void *). These are called only
	 *  when the base is first opened and last closed.
	 *  init_client and close_client work for each client.
	 */
	void *(*open_base)(char *basename);
	void (*close_base)(void *internal_data);
	/*
	 *  Place to release any data hooked to Client struct
	 *  before a client is ended, or to clear
	 *  references to this client.
	 *
	 *  init_client(Client *p) and
	 *  close_client(Client *p) work together,
	 *  analogous to open_base() and close_base().
	 *
	 *  client->base->internal are valid during the calls.
	 */
	void (*close_client)(Client *p);

	/*
	 *  Called now and then when nothing to do.
	 */
	void (*periodic_sync)(void *base_internal_data);

	/*
	 *  Client asks about the base in general
	 *  Reply generated.
	 */
	void (*get_base_info)(Client *);
	void (*tell_named_field)(Client *, char *fldname);
	void (*tell_next_field)(Client *, int fldnum, int how);

	/*
	 *  Client prepares the filter for a "find()"
	 *  No reply.
	 */
	void (*clear_filter)(Client *);
	void (*set_filter_order)(Client *, int order);
	void (*set_filter_key)(Client *, char *keyname);
	void (*add_filter)(Client *, char *filterstring);

	/*
	 *  Client finishes the filter and gets back the
	 *  type of the keyfield.
	 *  Client picks the next matching record (and keyvalue).
	 */
	void (*compile_filter)(Client *);
	void (*get_next_match)(Client *);

	/*
	 *  read is just a positioning command. No data
	 *  is transferred (just an indication of existence).
	 *  write is asking the server to update disk etc.
	 *  (unnecessary).
	 *  copy sets values of currently selected record
	 *  from values in given record.
	 */
	void (*read_entry)(Client *, int idx);
	void (*write_entry)(Client *);
	void (*copy_entry)(Client *, int idx);

	/*
	 *  A new empty record.
	 *  Purge everything but the record itself.
	 *  Destroy the record totally.
	 */
	void (*create_new)(Client *);
	void (*remove_subfiles)(Client *, int idx, char *ext);
	void (*remove_entry)(Client *, int idx);

	/*
	 *  get/set values from current record or general files.
	 *
	 *  (general files only in the case where the 'name'
	 *  also encodes the extension of the supplemental 'parameter' file)
	 *  In other words, name "d.COUNTER" is a parameter "COUNTER"
	 *  in supplemental file <INDEX>.d but this depends totally on
	 *  the base style. Other basetypes could just as well
	 *  use, simply, a field named "d.COUNTER" within current record.
	 */
	void (*getavalue)(Client *, int how, char *name);
	void (*setavalue)(Client *, int how, char *name, rlsvalue_t *value);

	char *catch_missing_fcn_0; /* just to catch typos ... */

	/*
	 *  Open a general file or raw data under given record.
	 */
	int  (*open_file)(Client *, int idx, char *ext, char *mode, int perms);

	/*
	 *  Which extensions exist in base,
	 *  all possible extensions for zero index, and
	 *  just the specific existing extensions for nonzero index.
	 */
	void (*enumerate_extensions)(Client *, int idx);

	/*
	 *  After a base is (re)opened, underlying base
	 *  has a chance to do whatever.
	 *  client->base->internal are all valid.
	 */
	void (*init_client)(Client *);

	/*
	 *  For misc usage,
	 *  what is the current index with this client.
	 *  return 0 for no index.
	 */
	int (*get_curidx)(Client *);

	/*
	 *  
	 *
	 */
	void (*clear_parms)(Client *, int idx, char *ext);

	/*
	 *  Return names (and values if fetch) for parameters
	 *  "under" given extension.
	 */
	void (*enum_parms)(Client *, int idx, char *ext);
	void (*fetch_parms)(Client *, int idx, char *ext);

	char *catch_missing_fcn_1; /* just to catch typos again ... */

	int (*stat_file)(Client *, int idx, char *ext, stat_t *st);

	void (*save_parms)(Client *, int idx, char *ext, char *s);
	
	/* add a generic method to filter, printformat, and set general params like sortorder, maxcount ... */
	void (*add_filterparam)(Client *, char *filterstring);

	/* how much entries do we have for a filter (needed at first for max page calculation) */
	void (*entry_count)(Client *);

	/*
	 * int (*reserved_1)();
	 * int (*reserved_2)();
	 * int (*reserved_3)();
	 */
	int (*reserved_4)();
	int (*reserved_5)();
	int (*reserved_6)();
	int (*reserved_7)();
	int (*reserved_8)();
	int (*reserved_9)();
	int (*reserved_A)();
	int (*reserved_B)();
	int (*reserved_C)();
	int (*reserved_D)();
	int (*reserved_E)();
	int (*reserved_F)();

	char *catch_missing_fcn_2; /* just to catch typos again ... */
} baseops_t;

struct Base {
	baseops_t *ops; /* to appropriate method struct */
	void *internal;      /* base-specific, per base base->internal data */
	char *name;          /* canonical name of base */
	Base *next;          /* bases are in linked list */
	int refcnt;          /* of clients in here */
};

struct dynbuf {
	char *data;
	int   len;
	int   size;
};

/*
 *  Each client connection holds a pointer to an open logsystem
 *  and to an active record.
 *  Each logsystem holds a list of active records.
 *
 *  LogSystems keep reference-count so they can be closed
 *  when no more clients use them.
 *
 *  LogEntries are wrapped inside XLogEntries,
 *  these contain a link (for base -> active entries)
 *  and another reference-count for multiple clients
 *  using a single record concurrently.
 *  XLogEntry also contains misc flags; DIRTY is set whenever
 *  the data has been modified but not yet written,
 *  REMOVED is set when the record has been deleted
 *  but another client still uses the record.
 *  IDLE is set every n seconds, and when it was already set,
 *  dirty record written out.
 */
struct Client {

#ifdef MACHINE_WNT
	/*
	 *  This OVERLAPPED has to be first and be included in struct !
	 *  GetQueuedCompletionStatus() gives only the ov pointer for failures,
	 *  and, anyway, key might not be as wide as a pointer...
	 */
	OVERLAPPED ov;
	int (*handler)(Client *, char *, int);
	void (*prehandler)();
#else
	/*
	 *  Client is in special state,
	 *  where the network i/o is handled in handler()
	 *  instead of the normal doread/process/dowrite req./response.
	 */
	int (*handler)(Client *, char *, int);

	/*
	 *  If > 0, sent with fildesc passing method during packet flush.
	 */
	int fdpass;
#endif
	/*
	 *  Peek what was returned from baseops/get-num-value.
	 */
	int sideways_num_result;
	/*
	 *  Handles the rd/wr data for "synthesized files".
	 */
	int (*synthetic_handler)(Client *, char *buf, int cc);

	/*
	 *  Links for clients and monitors.
	 */
	Client *next, *prev;
	Client *nextmon, *prevmon;

	HANDLE net;                  /* of the named pipe or socket */

	/*
	 *  Local file- and directory handles.
	 *  Mask for directory reading.
	 *  Used when the client is doing i/o to files etc.
	 */
	void *file;                  /* for I/O to regular files. */
	void *dirp;                  /* U*X directory ops */
	char *filemask;

	/*
	 *  Current request/response buffer, under processing,
	 *  and offset into it.
	 */
	struct reqheader *request;   /* into dynbuf temporarily */
	struct dynbuf buf;
	int done;

	time_t started;              /* connect time */

	u_int monitoring:1,   /* is linked in monitors-list */
	      local:1,        /* client is locally connected */
	      logon:1,        /* has passed login phase */
	      dead:1,         /* net error occurred. To be kicked away soon */
	      posted:1,       /* asynchronous i/o is active */
		skip_client:1,  /* leave this client to child thread */
	      :0;

	int key;              /* for debugging, currently */
	int pid;              /* of local client */
	int umask;            /* for creating files. */
	int child_pid;        /* from last exec, cleared when waited */

	Base *base;           /* current base, also linked in 'bases' */
	void *internal;       /* base specific, per client p->internal data */
	/*
	 *  Current filter.
	 *
	 *  These should be in p->base->internal really,
	 *  but as "legacyops" needs so little additional room,
	 *  the data is here instead...
	 */
	LogFilter *curfilter; /* current filter. */
	LogField  *keyfield;  /* quick path to filter key */

	int walkidx;          /* physical index for GET_NEXT_MATCH */

	int child_exited;     /* exit code from last exec, updated when child_pid waited */
	char RunFlag;				/* Auth <Run - tag value */
	char FileFlag;          	/* Auth <File - tag value */
	struct BASE_LIST *base_list;/* Auth <Base - tag value */
	char userid[60];             /* Auth <Username value */

};

extern baseops_t *baseops[];

extern Base   *bases;
extern Client *clients;
extern Client *monitors;

extern unsigned debugbits;

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG(x) (debugbits & (1 << (x)))

/*
 *  Common routines to get/set textual parameters from a real file.
 */
extern void getaparm(Client *, int how, char *path, char *name);
extern void setaparm(Client *, int how, char *path, char *name, void *value);

/*
 *  Common routines to open a real file for client,
 *  and to make a real directory.
 */
extern BOOL do_really_open_file(Client *, char *path, char *mode, int perms);
extern BOOL do_really_make_directory(Client *, char *path, int perms);

/*
 *  Common routine to synthesize a "file" for client without 
 *  a real filesystem file.
 *  The usual 'int cc = (*fcn)(Client *, char *buf, int nbytes)' usage
 *  with (*fcn)(Client *, NULL, 0) to flush when "writing to file".
 *  Return from fcn is either -1 on error and nbytes on success
 *  (or the last partial buffer when "reading from file")
 *  When reading, fcn must return 0 after all data is transferred.
 *
 *  mode is "r", "w" or "a".
 *
 *  I/O is sequential, no seeks are requested.
 */

int do_synthetic_file(Client *p, int (*handler_fcn)(Client *, char *, int), char *open_mode);

int transform_into_a_dataflow(Client *p, void *handler_arg, char *mode);

int do_synthetic_handle( Client *p, char *buf, int cc );

/*
 *  Sloppy prototypes...
 */
int inetsendfile(Client *conn, char *dummy1, int dummy2);
int inetrecvfile(Client *conn, char *dummy1, int dummy2);
int inetrecvfile_enter(Client *conn, char *dummy1, int dummy2);

int readagain(Client *conn, char *dummy1, int dummy2);

int  senddirectory(Client *p, char *buf, int buflen);

char *map_special(int);

Client *newconn(HANDLE h);

int vreply(Client *p, int code, int status, ...);
void reply(Client *p, int code, int status);
void adjust_linger(HANDLE h);
void client_write_blocked(Client *conn);
int reply_with_a_descriptor(Client *p, HANDLE h);
void enumerate_keyword(Client *conn, char *keyword);
void nenumerate_keyword(Client *conn, char *keyword, int start, int count);
int pokepeek_scanner(int how, char *if_poke_maybe_new_cfgfile);
int do_rls_exec(Client *p, char *mode, char **cmd);
void flush_lines(Client *conn);
void start_response(Client *, int status);
void endconn(Client *conn, char *dummy1, int dummy2);
void rls_execute(char **command);
void free_stringarray(char **v);
BOOL do_open_file(Client *p, int spec, int idx, char *ext, char *mode, int perms);
/* block/paginated file functions (for open, we add options param) */
void do_open_file_paginated(Client *p, char *ext, int perms);
void do_seek_file(Client *p, double offset, int position);
void do_read_file(Client *p, double size, int reserved1);
void do_close_file(Client *p);
void do_clear_file_filter(Client *p);
void do_set_file_filter_order(Client *p, int order);
void do_add_file_filterparam(Client *p, char *filterop);
void do_compile_file_filter(Client *p);
void do_get_next_file_match(Client *p);
/* --- */
void append_line(struct dynbuf *buf, char *s);
void lift_args(Client *conn, ...);
void dump_request(Client *conn);
void unplug_all_bases();
void global_periodic_sync();
void setup_environment(char *user_name);
void listparms(Client *p, char *path, int how);
#endif
