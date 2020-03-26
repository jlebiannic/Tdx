/*========================================================================
hello vi: set ts=4 sw=4 :

fileio bugs on Unix.
Status monitor ? As a request/reply, or continuously updated file ?
stderr redirection already in perhost.

	E D I S E R V E R

	File:		logsystem/peruser.c
	Author:		Juha Nurmela (JN)
	Date:		Fri Jan 13 13:13:13 EET 1994

	Copyright (c) 1997 Telecom Finland/EDI Operations

	Unix and NT implementations have some tactical differences.

	NT has all client I/O done with a completion port, overlapping.
	After an overlapping read or write is started, a whole packet,
	the completion notification is waited thru the completion port.
	So control returns to sloop immediately after one client is
	processed. Just in case, prehandler-functions exist to handle
	partial completions.  This has not happened yet.

	Unix uses nonblocking I/O and select(). Each client is mapped
	by the connection descriptor with clientmap[]. Readability and
	writability interest is kept in rset and wset. prehandler stuff
	is not used here. I/O is always tried immediately before setting the
	bit in select set.  This could lead to starvation of other clients,
	if one client is very eager ? Maybe just set the flag after one or
	two successive responses and let select see next requests. This
	would also be slower for the typical bursts of requests clients make.

	Buffering scheme uses only one buffer per client, so reading and
	writing states do NOT overlap. The buffer has max size (grown as needed),
	current length and "done" offset. All (almost anyway) packets
	are TLV, so first we read the header, crack it and if required,
	read rest of the packet. Packet is processed and response (if
	required) built and sent.

	DO NOT start_packet() UNTIL THE INPUT ARGS HAVE BEEN USED !
	This restriction comes from single buffering of i/o.

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: netstuff.c 55239 2019-11-19 13:50:31Z sodifrance $ $Name:  $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 22.04.97/JN	Unix and NT code was so different, separated.
  3.01 19.08.97/JN	Garbage data slipped to nonexistent request members.
					Poking scanner can also give new config, "atomically".
					MOVE_FILE for general use.
  3.02 05.09.97/JN	General part.
					Separate general and NT/Unix parts
  3.03 16.01.98/JN	append_line()
  3.04 28.06.98/JN	StringArray caused rest of input args to shift.
  3.06
  3.07 26.11.98/KP	fd-passing changed in dowrite()
  3.08 11.12.98/HT	Authentication DEBUG(6) added into JP's authentication
					code.
  3.09 28.12.98/HT	setup_environment(user_name) moved to another place and
					username parameter added to avoid missunderstanding
					of user id 0 when ediroot is as user name.
  3.10 25.01.99/KP	Time stamp requests in debug-output.
  3.11 17.02.99/KP	Removed some extraneous svclog output.
  3.12 04.05.99/KP	Some more...
  3.13 13.05.99/JR	Accept .rlshosts IP-address besides of IpAddress
  3.14 04.06.99/HT	synthetic_handler can be also for local files.
					Clean-up also synth_handler files.
  3.15 08.06.99/HT	STX_DEBUG_# env var support
  3.16 09.06.99/HT	Closing of file is fixed in baseops. No need to
					call synthetic handler.
  3.17 10.08.99/KP	Changed check_base_list() to cope with /\ problem in NT
					(To make <Restrict> section in .rlshosts work)
  3.18 14.10.99/KP	Removed (in unix) shutdown() from endconn(), messed up
					in RLS_EXEC... will this work now? dunno...
  3.19 19.10.99/KP&HT	Set linger off before close ( endconn() ). Fixes
						mysterious 4 second wait. 
  3.20 26.10.99/KP	(NT) Calls to signal() mess up the ConsoleEventHandler,
					that is supposed to keep us alive after logoff.
					Fixed by setting our own LogoffHandler() after calls
					to signal().
  3.21 08.11.99/HT	Open files per process limit is too small by default
					in HP-UX and Solaris. Try to increase it.
  3.22 30.05.2008/CRE Export rls format functions to rlsfileformat.c

  5.01/5.02 12.06.14/SCH(CG) TX-2372 : Trick the Linux compiler to avoid optimization on __tx_version unused variable
                             SONAR violations Correction
  
  5.03 21.07.2014 TCE(CG) TX-2563 : allowing to deactivate eventlog write on window via rls.cfg and USE_ENVENTLOG

 $Log:  $
 Revision 1.15.2.1  2010-06-09 08:19:16  otto
 BugZ 9758 : enlever les copyright des sources (rte, tcl, c)

 Revision 1.15  2009-07-03 14:11:26  fredd
 Merge of BugZ 8487

 Revision 1.14.2.12  2009-06-05 12:24:13  fredd
 BugZ 8487 : clearer logs/context

 Revision 1.14.2.11  2009-05-13 12:23:05  fredd
 BugZ 8487 : harness debug calls, remove password from debug traces, except when compiled in debug mode

 Revision 1.14.2.10  2009-05-13 09:28:39  fredd
 BugZ 8487 : debug cosmetic changes

 Revision 1.14.2.9  2009-04-29 12:51:11  fredd
 BugZ 8487 : define RLS protocol version at a global level

 Revision 1.14.2.8  2009-04-22 14:17:49  fredd
 BugZ 8487 : merge of BugZ 9001

 Revision 1.14.2.7  2009-04-21 11:57:22  fredd
 BugZ 8487 : extra space prevent compiling

 Revision 1.14.2.6  2009-02-16 10:38:45  cre
 BugZ 8487 : old .rlshost compatibility

 Revision 1.14.2.5  2009-02-11 15:08:36  cre
 BugZ 8487 : old .rlshost compatibility

 Revision 1.14.2.4  2008-07-21 12:19:56  cre
 Authentification with change of system user

 Revision 1.14.2.3  2008-07-17 09:44:48  cre
 First fonctional authentification with change of system user

 Revision 1.14.2.2  2008-06-03 13:14:12  cre
 Reorganise rlshost reading

 Revision 1.14.2.1  2008-05-30 12:36:19  cre
 Export format handle functions for rls configuration file

 Revision 1.14  2008-04-04 08:30:46  jfranc
 Bugz 8336 Ajout $Name pour le what du peruser

 Revision 1.13  2008-03-27 09:57:47  eber
 Merge of BugZ 8336

 Revision 1.12.4.1  2008-02-20 17:58:19  fredd
 BugZ 8336 : more cleanup, safer/clearer debug messages, paranoid warning removal

 Revision 1.12  2008-01-03 11:55:32  fredd
 minor : cleanup some unused code, remove a few warnings

 Revision 1.11  2007/10/17 12:55:18  fredd
 minor : arpa/inet.h required for some symbols, remove warnings on Solaris x86 compilation

 Revision 1.10  2007/09/26 11:47:21  fredd
 Try to have a clearer error message when having problem with logd socket file.

 Revision 1.9  2006/05/18 10:46:32  fredd
 SQLite merge

 Revision 1.8.4.2  2006/02/22 13:40:50  fredd
 define dummy bail_out, minor windows warning removal

 Revision 1.8.4.1  2006/02/21 16:50:45  fredd
 SQLite initial version

 Revision 1.8  2004/01/05 14:36:29  erwan

 Port allocation management

 Revision 1.7  2000/06/27 11:53:39  kari
 - Removed rls->localconn related fd-passing stuff from didconnect()

 Revision 1.6  2000/04/10 15:33:19  kari
 - Restrict <Base path;R> did not work, flag was parsed incorrectly. Fixed.

 Revision 1.5  2000/03/22 15:59:24  kari
 - After releasing active entries of current Base assume that
   they are released from current Client also.
 - Debug output from lift_args caused segfaults (!) sometimes

 Revision 1.4  2000/03/22 09:56:15  kari
 - In didread(), in case of failure, do not call endconn() directly but
   just flagdead(). Otherwise there would be a read-after-free() in main().

 Revision 1.3  2000/03/21 13:48:42  kari
 - Noted a possible memory read hazard in append_string().
   Not fixed, just noted.

  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 24.09.2019 - Olivier REMAUD - Passage au 64 bits
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <sys/types.h>
#ifdef MACHINE_AIX
#include <sys/select.h> /* barf */
#define COMPAT_43
#endif
#include <time.h>
#ifndef MACHINE_WNT
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#define closesocket(x) close(x)
#ifndef SHUT_RDWR
#define	SHUT_RDWR	2
#endif
#endif
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#ifndef MACHINE_WNT
#include <endian.h>
#endif
#ifndef MACHINE_WNT
#include <dirent.h>
#include <endian.h>
#define DIRENT struct dirent
#else
#include <crtdbg.h>  /* For _CrtSetReportMode */
#endif
#include <locale.h>

#if defined(SCM_RIGHTS) && defined(MACHINE_SCO)
#define _XOPEN_SOURCE_EXTENDED 1
#endif

#if defined(MACHINE_SOLARIS) || defined(MACHINE_HPUX)
#include <sys/resource.h>
#endif

#define SILENCIO (void *)

char *EVENT_NAME = "logd";

#ifndef MACHINE_WNT
char *logging = "/tmp/errors.logd";
#else
char *xlogging = "\\\\.\\PIPE\\errors.logd";
char *logging = "C:\\tmp\\errors.logd";

int EVENT_CATEGORY = 1;
SECURITY_DESCRIPTOR sd_usr_access_only;

#endif

/* dummy variables */
double tr_errorLevel = 0;
char*  tr_programName = "";
int debugged = 0;

void   bail_out(char* fmt,...) {
	fprintf(stderr,"This is just here to compile !\n");
	return;
}

/* #define REQ_NAMES */

#include "bases.h"
#include "perr.h"
#include "rlserror.h"
#define PERUSER
#include "rlsfileformat.h"

#define SIGN_ON_PACKET_H_LENGHT		4

#ifndef GET_USER
#ifdef MACHINE_WNT
#define GET_USER	getenv("USERNAME")
#else
#define GET_USER	getenv("USER")
#endif
#endif

#define DEBUG_PREFIX " [peruser] "
#define AUTH_PREFIX  " [peruser]  [auth] "

#ifdef DEBUG
#define PASSWORD(a) NOTNUL(a)
#else
#define PASSWORD(a) a?"xxxx":"(null)"
#endif

#include "misc/config.h"

struct BASE_LIST
{
	char *path;
	char flags;
	struct BASE_LIST *next;
};

void cleanup_closing_client(Client *p);
int add_base_list(char *path ,char flags, struct BASE_LIST **list);
int CheckSignOnPacketSyntax (char *Packet, char **p_to_username,char **p_to_password, char *ipaddress);
int parse_path (char **path);
int free_base_list(struct BASE_LIST **list);
int check_base_list ( char *path ,int *type ,struct BASE_LIST **list, Client *pcli);
int CheckRestrictSection (Client *conn ,char **entry_block, char **block,char **field_block,char **value, char *Path);
int CheckAuthd (Client *conn, char **block,char **field_block,char **value, char *Path);
int error_string(Client *conn, int nRet);
int CheckAuthentification(Client *conn, char **entry_block ,char **block,char **field_block,char **value,char *ipaddress, char *Path);

int sclose(int s);
int call_identd( char **namep, struct sockaddr_in *remote, int remote_port, int local_port);

#define IDLESEC 10 /* Dirty records clean every 10...20 seconds */

extern unsigned debugbits;

/*
 * See logsystem/remote/DEBUG for more info on these.
 *   3 - show entry changes
 *  11 - dump traffic
 */

static char *user_name = "Amnesiac@nowhere";

int didconnect(Client *conn, char *dummy1, int dummy2);
int dowrite(Client *conn, char *dummy1, int dummy2);
int didwrite(Client *conn, char *dummy1, int dummy2);
int doread(Client *conn, char *dummy1, int dummy2);
int didread(Client *conn, char *dummy1, int dummy2);
int flagdead(Client *conn, char *dummy1, int dummy2);
int process(Client *conn, char *dummy1, int dummy2);

Client *newconn(HANDLE h);

static BOOL grow_buffer(struct dynbuf *buf, int size, int how);
void append_buffer(struct dynbuf *buf, void *data, int len);
void append_uint32(struct dynbuf *buf, uint32 i);
void append_time(struct dynbuf *buf, time_t t);
void append_string(struct dynbuf *buf, char *s);
void append_char(struct dynbuf *buf, int c);
void append_string_end(struct dynbuf *buf);
void free_buffer(struct dynbuf *buf);

Client *clients;
Client *monitors;

static int num_clients;

#define swapuint32(ip) (*ip = htonl(*ip))
#define swapuint64(ip) (*ip = htonl64(*ip))


unsigned long long ntohl64(unsigned long long ull) {
#ifdef MACHINE_WNT
	return ntohll(ull);
#else
	return be64toh(ull);
#endif
}

unsigned long long htonl64(unsigned long long ull) {
#ifdef MACHINE_WNT
	return htonll(ull);
#else
	return htobe64(ull);
#endif
}

void start_packet(Client *conn, int type, int serial, int status);
void flush_packet(Client *conn);

void endclient(Client *conn);

#ifdef MACHINE_WNT

/*
 *  Path of named pipe for local connections.
 *  A new one (same pathname) is always created
 *  after current gets connected.
 *  So there is always one "embryonic" client,
 *  waiting the client to open the pipe.
 */
static char nt_rendezvous_path[256];

static void new_nt_named_pipe();

/*
 *  CompletionPort for nonblocking I/O.
 *
 */
static HANDLE evq;

#else

/*
 *  Clients indexed by connection descriptor
 */
static Client **clientmap;

/*
 *  select sets
 */
static fd_set rset, wset;
static int fdhiwat;

/*
 *  inet and unix domain acceptors
 */
static int inet_rendezvous;
static int unix_rendezvous;

#endif

static struct in_addr prevaddr;

volatile int signaled_to_exit;  /* we have been asked to quit */
volatile int logd_running;      /* nonzero as long as normal operation */

/*
 *  key-codes for completion status and bookkeeping.
 *  Codes below normal are synthetic.
 */
enum {
	DEADBEEF,
	ACCEPTED,
	FIRST_NORMAL = 100,
};

/*
 *  Memory allocation ways.
 */
enum {
	DONT_FAIL,
	FAIL_OK,
};

static int client_key_next;   /* seed for client->key */

#ifdef MACHINE_WNT
#define	F_OK		0x00
#endif

static int myAccess( char *value, int mode )
{
	debug("%smyAccess %s\n", DEBUG_PREFIX, NOTNUL(value));
#ifdef MACHINE_WNT
	return _access (value , mode);
#else
	return access (value , mode);
#endif
}

extern char **nglob(char *pat, int start, int count);

/*
 *  Expand keyword to some lines and
 *  send them as one newline separated string to client.
 *  The one string is aligned to word boundary, a must.
 *
 *  Enumerate "count" bits of data starting at position "start".
 */
void nenumerate_keyword(Client *conn, char *keyword, int start, int count)
{
	char **v;
	extern char **environ;

	if (keyword && *keyword)
	{
		v = nglob(keyword, start, count);
	}
	else
	{
		v = environ;
	}

	start_response(conn, 0);

	while (v && *v)
	{
		char *s = *v++;

		if (DEBUG(11))
		{
			debug("%s,", NOTNUL(s));
		}

		append_line(&conn->buf, s);
	}
	append_string_end(&conn->buf);

	flush_packet(conn);
}

/* Kept for compatibility -- use nenumerate_keyword instead with start = 0 and count = 0 */
void enumerate_keyword(Client *conn, char *keyword)
{
	nenumerate_keyword(conn, keyword, 0, 0);
}

static void sig(int s)
{
	signaled_to_exit = 1;
}

/*
 *  Place to determine access regarding to remote IP-address.
 *  Only the network-address is known in this state.
 */
static BOOL
host_ok(struct sockaddr_in *sin)
{
	return (1);
}

#ifdef ES32

/*
 *  Place to determine access for remote identity.
 */
static int
verify_signon(Client *conn)
{
	conn->logon = 1;
	return (0);
}

#else

/* Place to determine access for remote identity. */
static int verify_signon(Client *conn)
{
	char *entry_block=NULL;
	char *block=NULL;
	char *field_block=NULL;
	char *value=NULL;
	int nRet;
	struct sockaddr_in a;
	int alen = sizeof(a);
	char myaddress[64];
	char Path[256];

	*(conn->buf.data + 63 ) = '\0'; /* we strdup this later on */

	if (conn->local == 1)
	{
		strcpy (myaddress, "");
	}
	else
	{
		if ( getpeername ((HANDLE) conn->net, (struct sockaddr *) &a , &alen) < 0 )
		{
			svclog('I', "%sUser = %s , Authentication : getpeername failed. Sys Error Msg: %s System Errno: %d",
				DEBUG_PREFIX,NOTNUL(getenv("USER")),NOTNUL(strerror(errno)),errno);
			return AUTH_SYSTEM_ERROR;
		}
		strcpy (myaddress, ( char *) inet_ntoa(a.sin_addr));
		debug("%sgetpeername %s\n", DEBUG_PREFIX, (char *) NOTNUL(inet_ntoa(a.sin_addr)));
	}

	nRet = CheckAuthentification(conn,&entry_block,&block,&field_block,&value,myaddress,Path);

	if (nRet == USER_ACCEPTED && conn->local != 1)
	{
		nRet = CheckAuthd (conn,&block,&field_block,&value,Path);
	}

	if (nRet == USER_ACCEPTED)
	{
		nRet = CheckRestrictSection(conn,&entry_block,&block,&field_block,&value,Path);
	}

	/*
		Free the allocated memory if needed
	*/
	if (entry_block)
	{
		free (entry_block);
	}
	if (block)
	{
		free (block);
	}
	if (field_block)
	{
		free (field_block);
	}
	if (value)
	{
		free (value);
	}

	if (nRet == USER_ACCEPTED)
	{
		conn->logon = 1;
	}

	return nRet;
}
#endif

#ifdef MACHINE_WNT

/*
 *  inetselect() thread awaits incoming connections and accepts them.
 *  The new sockets are posted as synthetic completion-events,
 *  to the mainline.
 */
DWORD WINAPI inetselect(LPVOID arg)
{
	fd_set x;
	struct sockaddr_in a;
	int alen;
	SOCKET s = (SOCKET) arg;
	SOCKET ns;
	OVERLAPPED *ov;
	time_t  curtime;
	struct tm *p_tm;

	int noflood = 0;

	FD_ZERO(&x);

	while (logd_running)
	{
		if (DEBUG(14))
		{
			debug("%sinet wait\n", DEBUG_PREFIX);
		}

		FD_SET(s, &x);
		if (select(s + 1, &x, NULL, NULL, NULL) != 1)
		{
			/*
			 *  Give some (very little) time on error.
			 *  Error-condition most likely does not
			 *  cure itself, however...
			 *  Log error only every 4G times (once).
			 */
			if (!noflood++)
				warn("%sselect: %s",DEBUG_PREFIX, NOTNUL(syserr()));

			Sleep(1);
			continue;
		}
		alen = sizeof(a);
		ns = accept(s, (struct sockaddr *) &a, &alen);
		if (ns == SOCKET_ERROR)
		{
			warn("%saccept: %s",DEBUG_PREFIX, NOTNUL(syserr()));
			Sleep(1);
			continue;
		}

		if (DEBUG(14))
		{
			debug("%sinet ",DEBUG_PREFIX);
			/* 3.10/KP Time stamp each request in debug output */
			if DEBUG(5)
			{
				time(&curtime);
				p_tm = localtime(&curtime);
				debug("(%02d:%02d:%02d) ", p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
			}
			debug("%s\n", NOTNUL(inet_ntoa(a.sin_addr)));
		}

		/* Verify remote address */
		if (!host_ok(&a))
		{
			shutdown((SOCKET) ns, 2 );
			closesocket(ns);
			continue;
		}

		ov = calloc( 1, sizeof(OVERLAPPED) );
		ov->hEvent = (HANDLE) ns;
		if (!PostQueuedCompletionStatus(evq, 0, ACCEPTED, (LPOVERLAPPED)ov)) {
			warn("%sPQCS: %s",DEBUG_PREFIX, NOTNUL(syserr()));
			shutdown((SOCKET) ns, 2 );
			closesocket(ns);
			Sleep(1);
			continue;
		}
	}
	ExitThread(0);
    return 0;
}
#endif

#ifndef MACHINE_WNT
/*
 *  The names should be changed to neutral.
 *  Read or write, and return NZ if okay.
 *  EWOULDBLOCK and EAGAIN are taken as okay (we try later).
 */
BOOL WriteFile(HANDLE h, char *buf, DWORD amt, DWORD *amtdone, void *ioctx)
{
	int n;

	if (amtdone)
	{
		*amtdone = 0;
	}

	if ((n = write(h, buf, amt)) == -1)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
		{
			return (1);     /* No room to write, still open */
		}
		return (0);
	}
	if (n == 0)
	{
		return (0); /* Wrote nothing ??? Error */
	}

	if (amtdone)
	{
		*amtdone = n;
	}

	return (1);
}

BOOL ReadFile(HANDLE h, char *buf, DWORD amt, DWORD *amtdone, void *ioctx)
{
	int n;

	if (amtdone)
	{
		*amtdone = 0;
	}

	/* Just cosmetics for debugging, clear stale errors. */
	errno = 0;

	if ((n = read(h, buf, amt)) == -1)
	{
		if (errno == EWOULDBLOCK || errno == EAGAIN)
		{
			return (1);     /* No data, still open */
		}

		debug("%sReadFile: read() returned -1.\n",DEBUG_PREFIX);
		return (0);
	}

	if (n == 0)
	{
		return (0); /* EOF, an "error" */
	}

	if (amtdone)
	{
		*amtdone = n;
	}

	return (1);
}

#endif /* ! WNT */


/*
 *  I/O handlers for nonlocal filedata.
 */
static DWORD localwrite(Client *p)
{
	DWORD ndid = 0;

	if (p->synthetic_handler)
	{
		ndid = (*p->synthetic_handler)(p, p->buf.data, p->done);
		if (ndid < 0)
		{
			ndid = 0;
		}
	}
	else
	{
		if (!WriteFile((HANDLE) p->file, p->buf.data, p->done, &ndid, NULL) || ndid != p->done)
		{
			ndid = 0;
		}
	}
	return (ndid);
}

static DWORD localread(Client *p)
{
	DWORD ndid = 0;

	if (p->synthetic_handler)
	{
		ndid = (*p->synthetic_handler)(p, p->buf.data, p->buf.len);
		if (ndid < 0)
		{
			ndid = 0;
		}
	}
	else
	{
		if (!ReadFile((HANDLE) p->file, p->buf.data, p->buf.len, &ndid, NULL))
		{
			ndid = 0;
		}
	}
	return (ndid);
}

int inetrecvfile(Client *conn, char *dummy1, int dummy2)
{
	DWORD n = 0;

	if (conn->done) {
		n = localwrite(conn);
		if (n) {
			conn->done = 0;
			conn->buf.len = 4096;
			grow_buffer(&conn->buf, conn->buf.len, DONT_FAIL);
		}
	}
	if (n)
	{
		doread(conn,"",0);
	}
	else
	{
		flagdead(conn,"",0);
	}
	return 0;
}

int inetrecvfile_enter(Client *conn, char *dummy1, int dummy2)
{
	conn->done = 0;
	conn->buf.len = 4096;
	conn->handler = inetrecvfile;

	doread(conn,"",0);
	return 0;
}

int inetsendfile(Client *conn, char *dummy1, int dummy2)
{
	DWORD n = 0;

	if (conn->done == conn->buf.len)
	{
		conn->done = 0;
		conn->buf.len = 4096;
		grow_buffer(&conn->buf, conn->buf.len, DONT_FAIL);

		n = localread(conn);
		conn->buf.len = n;
	}
	if (n)
	{
		dowrite(conn,"",0);
	}
	else
	{
		flagdead(conn,"",0);
	}
	return 0;
}

/*
 *  Client is "reading" a directory.
 *  Synthetic handler for data i/o,
 *  Mimics read(2) workings.
 */
int senddirectory(Client *p, char *buf, int buflen)
{
	char *bp;
	char *name;

#ifndef MACHINE_WNT
	DIRENT *d;

	if ((p->dirp == NULL) && ((p->dirp = opendir(p->filemask)) == NULL))
	{
		return (-1);
	}
	if ((d = readdir(p->dirp)) == NULL)
	{
clos:
		closedir(p->dirp);
		p->dirp = NULL;
		return (0);
	}
	name = d->d_name;
#else
	WIN32_FIND_DATA d;

	if (p->file == NULL) {
		p->file = FindFirstFile(p->filemask, &d);
		if ((HANDLE) p->file == INVALID_HANDLE_VALUE) {
			p->file = NULL;
			return (-1);
		}
	} else {
		if (!FindNextFile((HANDLE) p->file, &d)) {
clos:
			FindClose((HANDLE) p->file);
			p->file = NULL;
			return (-1);
		}
	}
	name = d.cFileName;
#endif
	/*
	 *  Replace each \n with something.
	 *  \r was quickly found from sleeve.
	 *  Filenames longer than buffer length (4k typical)
	 *  get truncated. Big deal.
	 *  Newline added for "line" separator.
	 */
	if (!buf || !buflen)
	{
		goto clos;
	}

	for (bp = buf; (bp - buf) < buflen - 1 && *name; ++name)
	{
		if (*name == '\n')
		{
			*bp++ = '\r';
		}
		else
		{
			*bp++ = *name;
		}
	}
	*bp++ = '\n';

	return (bp - buf); /* amount of data we give */
}

#ifdef MACHINE_WNT
/* 3.20/KP : Handler for LOGOFF -message */
BOOL WINAPI
LogoffHandler(DWORD dwType)
{
	if (dwType == CTRL_LOGOFF_EVENT)
		return 1;
    return 0;
}
#endif

#ifdef MACHINE_WNT
#if _MSC_VER >= 1500
void myInvalidParameterHandler(const wchar_t* expression,
   const wchar_t* function, 
   const wchar_t* file, 
   unsigned int line, 
   uintptr_t pReserved)

{
#ifdef _DEBUG
   FILE *stream;
   
   stream = fopen( "c:\\tmp\\peruser_handler.log", "a" );
   fwprintf(stream,L"Invalid parameter detected in function %s."
            L" File: %s Line: %d\n", function, file, line);
   fwprintf(stream,L"Expression: %s\n", expression);
   fclose(stream);
#endif
  
}
#endif
#endif



int main(int argc, char **argv)
{
	extern int opterr, optind;
	extern char *optarg;
	int c, i;
	char rlscfg[1024];
#if defined(MACHINE_SOLARIS) || defined(MACHINE_HPUX)
	struct rlimit rlp;
	char Temp[256];

	if (getrlimit (RLIMIT_NOFILE , &rlp) == -1 )	
		svclog('I', "%sWarning getrlimit failed, Errno %d , Err Msg %s",DEBUG_PREFIX, errno , NOTNUL(strerror(errno)));
		
	svclog('I', "%srlim_cur = %d , rlim_max = %d",DEBUG_PREFIX, rlp.rlim_cur, rlp.rlim_max); 

	if (rlp.rlim_cur < rlp.rlim_max)
	{
		rlp.rlim_cur = rlp.rlim_max;
		if (setrlimit (RLIMIT_NOFILE , &rlp) == -1 )	
			svclog('I', "%sWarnings setrlimit failed, Errno %d , Err Msg %s",DEBUG_PREFIX, errno , NOTNUL(strerror(errno)));
	}
		
#endif

#ifdef MACHINE_WNT
#if _MSC_VER >= 1500
/* add CRT HANDLER */

   _invalid_parameter_handler oldHandler, newHandler;
   newHandler = myInvalidParameterHandler;
   oldHandler = _set_invalid_parameter_handler(newHandler);

   /* Disable the message box for assertions. */
   _CrtSetReportMode(_CRT_ASSERT, 0);
#endif
#endif  


	log_UseLocaleUTF8();

	debug("%sEntering main(%d,[", DEBUG_PREFIX,  argc);
	for (i=0; i < argc; i++) {
		if (i!=0) {debug(",");}
		debug("%s", NOTNUL(argv[i]));
	}
	debug("])\n");

	debugbits = 0;
	init_bases();
	init_base(init_nilops); /* make sure nil-db is last */

	while ((c = getopt(argc, argv, "dD:")) != -1)
	{
		switch (c)
		{
			case 'd':
				debugbits = -1;
				debugged = 1;
				break;
			case 'D':
				if (!strncmp(optarg,"0x",2))
				{
					/* note the base 16 conversion, input 0x5 for DEBUG(3) and DEBUG(1), 0x800 for DEBUG(11) */
					debugbits = strtol(optarg+2,NULL,16);
				}
				else
				{
					/* note the base 2 conversion, input 101 for DEBUG(3) and DEBUG(1), 10000000000 for DEBUG(11) */
					debugbits = strtol(optarg,NULL,2);
				}
				if (debugbits == 0)
				{
					debugbits = -1;
				}
				debug("%sDEBUG: 0x%x\n",DEBUG_PREFIX, debugbits );
				break;
			default: break;
		}
	}

	/* 3.15/HT */
	for ( c = 0; c < 32; c++ )
	{
		char buf[32];
		sprintf( buf, "STX_DEBUG_%d", c );
		if (getenv(buf))
		{ 
			debugbits |= ( 1 << c );
		}
	}

	if (optind < argc) {
		
		user_name = strstr(argv[optind++], "@");
		
		if (user_name != NULL) {/* removing the 'xxx@' */
			user_name = user_name + 1;
		} else { /* no '@' => keep all */
			user_name = argv[optind - 1];
		}
		
		debug("%s[%s] => user_name = [%s] \n", DEBUG_PREFIX, NOTNUL(argv[optind-1]), NOTNUL(user_name));
	}

	setup_environment(user_name);

	/* HUP terminates us, like INT and TERM. */
#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif
#ifdef SIGBREAK
	signal(SIGBREAK, sig);
#endif
#ifdef SIGHUP
	signal(SIGHUP, sig);
#endif
	signal(SIGTERM, sig);
	signal(SIGINT, sig);

	/* Catch continuous tries from same address, no need to fill logs with same message... */
	memset(&prevaddr, 0xFF, sizeof(prevaddr));

	/* Slave threads can keep an eye on this. */
	logd_running = 1;

#ifdef MACHINE_WNT
	/* 3.20/KP : Set LogoffHandler to ignore logoff messages. */
	SetConsoleCtrlHandler(LogoffHandler, 1);

	/*
	 *  Lengthy initialization.
	 *
	 *  First the security tokens we need,
	 *  then rendezvouses for named pipes and tcp.
	 */
	{
		char *SYSTEM = "SYSTEM";
		SECURITY_DESCRIPTOR *sd = &sd_usr_access_only;
		TOKEN_USER *token;
		ACL *acl;
		int aclsz;
		SID *sidusr, *sidsys;
		DWORD n;
		char *dom;
		char *sysdom;
		DWORD m;
		SID_NAME_USE sidtyp;

		sidtyp = SidTypeUser;
		n = 0;
		m = 0;
		LookupAccountName(NULL, user_name, NULL, &n, NULL, &m, &sidtyp);

		sidusr = calloc(1,n);
		dom    = calloc(1,m);
		if (!sidusr || !dom)
			fatal("%sallocate sids",DEBUG_PREFIX);

		if (!LookupAccountName(NULL, user_name, sidusr, &n, dom, &m, &sidtyp))
			fatal("%sLookupAccountName: %s: %s",DEBUG_PREFIX, NOTNUL(user_name), NOTNUL(syserr()));

		sidtyp = SidTypeUser;
		n = 0;
		m = 0;
		LookupAccountName(NULL, SYSTEM,  NULL, &n, NULL, &m,  &sidtyp);

		sidsys = calloc(1,n);
		sysdom = calloc(1,m);
		if (!sidsys || !sysdom)
			fatal("%sallocate sids",DEBUG_PREFIX);

		if (!LookupAccountName(NULL, SYSTEM, sidsys, &n, sysdom, &m, &sidtyp))
			fatal("%sLookupAccountName: %s: %s",DEBUG_PREFIX, SYSTEM, NOTNUL(syserr()));

		aclsz = GetLengthSid(sidusr)
		      + sizeof(ACL)
		      + sizeof(ACCESS_ALLOWED_ACE)
		      + GetLengthSid(sidsys)
		      + sizeof(ACL)
		      + sizeof(ACCESS_ALLOWED_ACE);
		
		acl = calloc(1, aclsz);
		if (!acl)
			fatal("%sallocate acl",DEBUG_PREFIX);

		if (!InitializeAcl(acl, aclsz, ACL_REVISION))
			fatal("%sInitializeAcl: %s",DEBUG_PREFIX, NOTNUL(syserr()));

		/* The user */
		if (!AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, sidusr) || !AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_ALL, sidsys))
			fatal("%sAddAccessAllowedAce: %s",DEBUG_PREFIX, NOTNUL(syserr()));

		if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
			fatal("%sInitializeSecurityDescriptor: %s",DEBUG_PREFIX, NOTNUL(syserr()));

		/* Set access right for everyone */
		if (!SetSecurityDescriptorDacl(sd, 1, NULL, 0))
			fatal("%sSetSecurityDescriptorDacl: %s",DEBUG_PREFIX, NOTNUL(syserr()));

		sprintf(nt_rendezvous_path, "\\\\.\\PIPE\\logd.%s", NOTNUL(user_name));

		new_nt_named_pipe();
	}

	/*
	 *  We have to create the passive socket ourselves,
	 *  and then tell perhost what we got.
	 */
	{
		static WSADATA w;
		DWORD tid;
		int s, alen, on = 1, off = 0;
		struct sockaddr_in sin;
		int port, maxport;

#if USE_2_0_SOCKS
		if (WSAStartup(MAKEWORD(2,0), &w)) {
#else
		if (WSAStartup(MAKEWORD(1,1), &w)) {
#endif
			fatal("%sWinsock start: %s",DEBUG_PREFIX, NOTNUL(syserr()));
		}
		memset(&sin, 0, sizeof(sin));
		sin.sin_family      = AF_INET;
		sin.sin_addr.s_addr = INADDR_ANY;
		if ((s = socket(sin.sin_family, SOCK_STREAM, 0)) == -1)
			fatal("%sSOCK_STREAM: %s",DEBUG_PREFIX, NOTNUL(syserr()));

		if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (void *) &on, sizeof(on)) == -1)
			warn("%sTCP_NODELAY: %s",DEBUG_PREFIX,NOTNUL(syserr()));

		if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, (void *)&off,sizeof(off)) == -1)
			warn("%sSO_OOBINLINE: %s",DEBUG_PREFIX, NOTNUL(syserr()));

		if (getenv("RLS_PORT_MIN"))
			sscanf(getenv("RLS_PORT_MIN"), "%d", &port);
		else
			port = LD_DEFPORT + 1;

		if (getenv("RLS_PORT_MAX")) 
			sscanf(getenv("RLS_PORT_MAX"), "%d", &maxport);
		else 
			maxport = -1;

		do {
			if (maxport > 0 && port > maxport)
			{
				fatal("%sbind: exceed maximum port number : %s",DEBUG_PREFIX, NOTNUL(syserr()));
				close(s);
				return -1;
			} 
			sin.sin_port = htons(port++);
		}
		while (bind(s, (struct sockaddr *) &sin, sizeof(sin)));
	
		if (listen(s, 10))
			fatal("%Slisten: %s",DEBUG_PREFIX, NOTNUL(syserr()));

		alen = sizeof(sin);
		if (getsockname(s, (struct sockaddr *) &sin, &alen))
			fatal("%sgetsockname: %s",DEBUG_PREFIX, NOTNUL(syserr()));

		CreateThread(NULL, 0, inetselect, (LPVOID) s, 0, &tid);

		printf("%5lu\n", (unsigned long) ntohs(sin.sin_port));
		fflush(stdout);
	}
#else
	{
		/* Unix domain acceptor */
		struct sockaddr_un un;

		unix_rendezvous = socket(AF_UNIX, SOCK_STREAM, 0);
		if (unix_rendezvous == -1)
		{
			fatal("%sunix socket: %s",DEBUG_PREFIX, NOTNUL(syserr()));
		}

		memset(&un, 0, sizeof(un));
		un.sun_family = AF_UNIX;

		sprintf(un.sun_path, "%s/logdsock", getenv("HOME"));

		unlink(un.sun_path);

		debug("%stry and listen on : %s\n",DEBUG_PREFIX, NOTNUL(un.sun_path));

		if (bind(unix_rendezvous, SILENCIO &un, sizeof(un)) == -1)
		{
			fatal("%sunix bind: %s.\nThis may be related to the creation of the socket file : logdsock (%s).",
				DEBUG_PREFIX, NOTNUL(syserr()), NOTNUL(un.sun_path));
		}

		if (listen(unix_rendezvous, 10) == -1)
		{
			fatal("%sunix listen: %s.\nThis may be related to the creation of the socket file : logdsock (%s).",
				DEBUG_PREFIX, NOTNUL(syserr()), NOTNUL(un.sun_path));
		}

		/*
		 *  Inherited the user-specific inet socket from mapper-daemon
		 *  as fd 0 (and 1, but NOT 2), all tcp-options have been
		 *  configured already there.
		 *
		 *  XXX should move that code here, to keep them on one file.
		 */
		
		inet_rendezvous = 0;

		fdhiwat = 3;

		if (fdhiwat < inet_rendezvous + 1)
		{
			fdhiwat = inet_rendezvous + 1;
		}

		if (fdhiwat < unix_rendezvous + 1)
		{
			fdhiwat = unix_rendezvous + 1;
		}

		clientmap = calloc(fdhiwat, sizeof(*clientmap));
		if (!clientmap)
		{
			fatal("%sallocate clientmap",DEBUG_PREFIX);
		}

		FD_ZERO(&rset);
		FD_ZERO(&wset);
	}
#endif
	/* Jira TX-2563 : add reading of rls.cfg to allow disabling wrting on windows eventlog */
	#ifndef MACHINE_WNT
		sprintf(rlscfg,"%s/config/rls.cfg",getenv("EDIHOME"));
	#else
		sprintf(rlscfg,"%s\\config\\rls.cfg",getenv("EDIHOME"));
	#endif

	readConfFileToEnv(rlscfg);
	
	if (getenv("USE_EVENTLOG")){
		sscanf(getenv("USE_EVENTLOG"), "%d", &nolog);
	}
	/* END TX-2563 */
	
	svclog('I', "%s%s active\n",DEBUG_PREFIX, NOTNUL(user_name));

	/* Main loop, until we are asked to quit. */
	{
		int idlecnt = 0;

		while (!signaled_to_exit)
		{
			debug("%sMain loop, until we are asked to quit\n",DEBUG_PREFIX);
#ifdef MACHINE_WNT
		{
			union {
				OVERLAPPED *ov;
				Client *conn;
			} x;

			SOCKET s;
			DWORD cc = 0;
			DWORD key = 0;
			BOOL ok;

			cc = 0;
			key = 0;
			x.conn = NULL;

			ok = GetQueuedCompletionStatus(evq, &cc, &key, &x.ov, IDLESEC * 1000);

			/*
			** Get Socket handel from Overlapped structure
			** This is used only when socket is transferred
			** with QueuedCompletionStatus functions.
			** See inetselect() for details.
			*/
			if (x.ov)
				s = (SOCKET)x.ov->hEvent;
			else
				s = INVALID_SOCKET;

			if (DEBUG(16))
			{
				debug("%smain loop %s, read/written %d, status key %s (%d), conn statut key %s : %d)\n",
					DEBUG_PREFIX,
					ok ? "OK" : "KO",
					cc, 
					key == ACCEPTED ? "ACCEPTED" : key >= FIRST_NORMAL ? "NORMAL" : "SYNTHETIC",
					key,
					key == ACCEPTED ? "ACCEPTED (handle" : x.conn != NULL ? x.conn->key >= FIRST_NORMAL ?
						"NORMAL (key" : "SYNTHETIC (key" : "REJECTED (key",
					key == ACCEPTED ? s : x.conn != NULL ? x.conn->key : -1
				);
			}

			if (!x.ov)
			{
				/* Nothing coming. Cleanup dirty records. */
				if (idlecnt < 2)
					if ((++idlecnt == 2) && (DEBUG(8)))
						debug("%sidle, %d clients\n", DEBUG_PREFIX, num_clients);

				global_periodic_sync();
				continue;
			}
			idlecnt = 0;

			switch (key) {

			case ACCEPTED:
				/*
				 *  Synthetic from acceptor thread. A socket.
				 */
				if (DEBUG(15))
				{
					debug("%spick s %d ok %d\n", DEBUG_PREFIX, s, ok);
				}

				if (!ok)
				{
					if ( s != INVALID_SOCKET )
					{
						shutdown((SOCKET) s, 2 );
						closesocket(s);
					}
				}
				else
					didconnect(newconn(s),"",0);

				free( x.ov ); /* See inetselect() */
				break;

			default:
				/*
				 * For exec
				*/
				if (x.conn->skip_client)
				        break;
				/*
				 *  Real completion event. OVERLAPPED/conn.
				 *  If some I/O was done, but not yet all of it,
				 *  continue current transfer.
				 *  Otherwise call completion handler.
				 *  completion events complicate this, cannot release
				 *  a client until the event completes.
				 */
				if (key != x.conn->key)
					fatal("%skey %d != conn->key %d",DEBUG_PREFIX, key, x.conn->key);

				x.conn->posted = 0;

				if (ok && !x.conn->dead) {
					if (cc && (x.conn->done += cc) < x.conn->buf.len)
						(*x.conn->prehandler)(x.conn);
					else
						(*x.conn->handler)(x.conn,"",0);
				}
				if (!ok || x.conn->dead) {
					if (x.conn->posted)
						flagdead(x.conn,"",0);
					else
						endconn(x.conn,"",0);
				}
				break;
			}
		}
#else
		{
			static int logselecterr; /* Every 4Gth error logged from select */
			struct timeval tv;
			int nready;
			Client *conn;
			fd_set r, w;

			FD_SET(inet_rendezvous, &rset);
			FD_SET(unix_rendezvous, &rset);

			tv.tv_sec = IDLESEC;
			tv.tv_usec = 0;
			r = rset;
			w = wset;

			nready = select(fdhiwat, &r, &w, NULL, &tv);

			if (DEBUG(16))
			{
				debug("%smain loop %d/%d\n", DEBUG_PREFIX, nready, fdhiwat);
			}

			if (nready == -1)
			{
				/*
				 *  Signals just set the flag. When this happens
				 *  inside select, break out before logging one
				 *  spurious EINTR message.
				 *  Other errors should not happen, when they do,
				 *  dont cause continuous stream of error messages,
				 *  it rarely fixes automatically.
				 */
				if (signaled_to_exit)
				{
					break;
				}

				if (!logselecterr++)
				{
					warn("%sselect: %s",DEBUG_PREFIX, NOTNUL(syserr()));
				}

				sleep(1);
				continue;
			}

			{
				int pid, status = 0;

				while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
				{
					/* Exit status from a finished rls exec. */
					status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

					for (i = 0; i < fdhiwat; ++i)
					{
						if ((conn = clientmap[i]) && conn->child_pid == pid)
						{
							conn->child_pid = 0;
							conn->child_exited = status;
							break;
						}
					}
				}
			}

			if (nready == 0)
			{
				/* Nothing coming. Cleanup dirty records. */
				if (idlecnt < 2)
				{
					if ((++idlecnt == 2) && (DEBUG(8)))
					{
						debug("%sidle, %d clients\n", DEBUG_PREFIX, num_clients);
					}
				}

				global_periodic_sync();
				continue;
			}

			idlecnt = 0;

			/* New connection from inet domain ? */
			if (FD_ISSET(inet_rendezvous, &r))
			{
				struct sockaddr_in a;
				int ns, alen = sizeof(a);				

				nready--;

				ns = accept(inet_rendezvous, (struct sockaddr *) &a, &alen);
				if (ns == -1)
				{
					warn("%saccept inet: %s",DEBUG_PREFIX, NOTNUL(syserr()));	
					sleep(1); 
					continue;
				} 

				if (DEBUG(14))
				{
					debug("%sinet %s\n", DEBUG_PREFIX, NOTNUL(inet_ntoa(a.sin_addr)));
				}

				/* Verify remote address */
				if (host_ok(&a))
				{
					didconnect(newconn(ns),"",0);
				}
				else
				{
					shutdown( ns, SHUT_RDWR );
					closesocket(ns);
				}				
			}

			/* New connection from unix domain ? */
			if (FD_ISSET(unix_rendezvous, &r))
			{
				struct sockaddr_un a;
				int ns, alen = sizeof(a);

				nready--;

				ns = accept(unix_rendezvous, (struct sockaddr *) &a, &alen);
				if (ns == -1) {
					warn("%saccept unix: %s",DEBUG_PREFIX, NOTNUL(syserr()));
					sleep(1);
					continue;
				}

				if (DEBUG(14))
				{
					debug("%sunix %s\n", DEBUG_PREFIX, NOTNUL(a.sun_path));
				}

				/*
				 *  Could check owner of another end of unix-socket-pair.
				 *  (That would mandate the client to bind his/her end).
				 */
				conn = newconn(ns);
				conn->local = 1;
				didconnect(conn,"",0);
			}

			/*
			 *  Go thru all other ready descriptors, which have a client.
			 *  Could use nready as another loop-end criteria,
			 *  but not, as nready is (wrt OS) either
			 *  total number in both sets, or
			 *  number of _descriptors_ in either set...
			 *  Lose some cycles.
			 *  Notice rset and wset can change,
			 *  but we run thru current r and w.
			 */
			for (i = 0; i < fdhiwat; ++i)
			{
				if (FD_ISSET(i, &r))
				{
					FD_CLR(i, &rset);
					nready--;
					if ((conn = clientmap[i]) != NULL)
					{
						doread(conn,"",0);
						if (conn->dead)
						{
							endconn(conn,"",0);
						}
					}
				}
				if (FD_ISSET(i, &w))
				{
					FD_CLR(i, &wset);
					nready--;
					if ((conn = clientmap[i]) != NULL)
					{
						dowrite(conn,"",0);
						if (conn->dead)
						{
							endconn(conn,"",0);
						}
					}
				}
				if ((conn = clientmap[i]) && conn->dead)
				{
					endconn(conn,"",0);
				}
			}
		}
#endif
		}
		if (DEBUG(8))
		{
			debug("%squit\n", DEBUG_PREFIX);
		}

		logd_running = 0;
		unplug_all_bases();

		svclog('I', "%s%s terminated",DEBUG_PREFIX, NOTNUL(user_name));
		exit(0);
	}
	return 0;
}

void endconn(Client *conn, char *dummy1, int dummy2)
{
#ifdef MACHINE_WNT
	DWORD n;
#endif
	struct linger l;

	if (DEBUG(13))
	{
		if (DEBUG(5))
		{
			time_t  curtime;
			struct tm *p_tm;

			time(&curtime);
			p_tm = localtime(&curtime);
			debug("%s(%02d:%02d:%02d) ", DEBUG_PREFIX, p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
		}
		else
		{
			debug("%s", DEBUG_PREFIX);
		}

		debug("%d Closing...\n", conn->key);
	}

#ifdef MACHINE_WNT
	/*
	 * XXX Move this to proper place...
	 */
	/*
	**  3.14/HT 
	**
	**  3.16/HT yhis fix erase previous 3.14 fix uhhh...
	*/
	if (conn->handler == inetrecvfile
	 && conn->synthetic_handler == NULL
	 && conn->done)
		WriteFile((HANDLE) conn->file, conn->buf.data, conn->done, &n, NULL);

#endif

	/*
	 *  Free filter and
	 *  release base (synchs changed records).
	 *
	 *  Remove client from lists,
	 *  close network and free client structures.
	 */
	cleanup_closing_client(conn);

	if (conn->next == conn)
	{
		/*
		 *  Last.
		 */
		clients = NULL;
	}
	else
	{
		/*
		 *  Take off. Bump root pointer if just this one taken off.
		 */
		if (clients == conn)
		{
			clients = conn->next;
		}
		conn->next->prev = conn->prev;
		conn->prev->next = conn->next;
	}
	if (conn->monitoring)
	{
		if (conn->nextmon == conn)
		{
			monitors = NULL;
		}
		else
		{
			if (monitors == conn)
			{
				monitors = conn->nextmon;
			}
			conn->nextmon->prevmon = conn->prevmon;
			conn->prevmon->nextmon = conn->nextmon;
		}
	}

	num_clients--;

#ifdef MACHINE_WNT
	if (conn->local) {
		DisconnectNamedPipe(conn->net);
		CloseHandle(conn->net);
	} else {
		/*
		**  30.10.1998/HT&JP  shutdown added
		*/
		shutdown((SOCKET) conn->net, 2 );
		closesocket((SOCKET) conn->net);
	}
#else
	FD_CLR(conn->net, &rset);
	FD_CLR(conn->net, &wset);

	clientmap[conn->net] = NULL;
	/*
	 * 3.18/KP : Removed, see comments in version history.
     */
	
	/*
	 * 3.19/KP&HT : Added, see version comments
	 */
	l.l_onoff  = 0;
	l.l_linger = 0;
	setsockopt(conn->net, SOL_SOCKET, SO_LINGER, (void *) &l, sizeof(l));

	close(conn->net);
#endif

	/*
	 *  Just in case
	 */
	conn->key = DEADBEEF;
	conn->dead = 1;

#ifdef MACHINE_WNT
	if (conn->file)
	{
		CloseHandle((HANDLE) conn->file);
	}
#else
	if (conn->file)
	{
		close((int) conn->file);
	}
	if (conn->dirp)
	{
		closedir(conn->dirp);
	}
#endif
	if (conn->filemask)
	{
		free(conn->filemask);
		conn->filemask = NULL;
	}

	free_buffer(&conn->buf);
	free(conn);

	if (DEBUG(13))
	{
		if (DEBUG(5))
		{
			time_t  curtime;
			struct tm *p_tm;

			time(&curtime);
			p_tm = localtime(&curtime);
			debug("%s(%02d:%02d:%02d) ", DEBUG_PREFIX, p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
		}
		else
		{
			debug("%s", DEBUG_PREFIX);
		}

		debug("Closed\n");
	}
}

int vreply(Client *conn, int type, int status, ...)
{
	char num[512];
	va_list ap;
	unsigned argtype;
	int i;
	char *s;
	double d;
	time_t t;
	struct dynbuf *buf = &conn->buf;

	start_packet(conn, type, conn->request->serial, status);

	va_start(ap, status);

	while ((argtype = va_arg(ap, unsigned)) != _End)
	{
		switch (argtype)
		{
			default:
				if (DEBUG(11))
				{
					debug("\n%soops !!\n", DEBUG_PREFIX);
				}

				*(int*)0=0;
#ifndef MACHINE_WNT
			/* Just an int. Set a flag to use sendmsg() once. */
			case _Descriptor:
				append_uint32(buf, i = va_arg(ap, int));
				if (DEBUG(11))
				{
					debug("fd=%d,", i);
				}

				conn->fdpass = i;
				break;
#endif
			case _Time:
				append_time(buf, t = va_arg(ap, time_t));
				if (DEBUG(11))
				{
					debug("%ull,", (unsigned long long) t);
				}
				break;
			case _Index:
			case _Int:
				append_uint32(buf, i = va_arg(ap, int));
				if (DEBUG(11))
				{
					debug("%d,", i);
				}

				break;
			case _String:
				append_string(buf, s = va_arg(ap, char *));
				break;
			case _Double:
				sprintf(num, "%lf", d = va_arg(ap, double));
				append_string(buf, num);
				conn->sideways_num_result = d + .5; /* paranoia */
				break;
		}
	}
	va_end(ap);

	flush_packet(conn);

	return (0);
}

void reply(Client *p, int code, int status)
{
	vreply(p, code, status, _End);
}

void free_stringarray(char **v)
{
	char **vv;

	if (v)
	{
		for (vv = v; *vv; ++vv)
		{
			free(*vv);
		}
		free(v);
	}
}

void lift_args(Client *conn, ...)
{
	va_list ap;
	unsigned argtype;
	char *cp;
	char *end_reqbuf;
	double *dp;
	char **sp;
	char ***vp;
	uint32 *ip;
	int i;
	time_t  curtime;
	time_t *t;
	struct tm *p_tm;

	if (DEBUG(11)) {
		debug("%s%d -> #%d ", DEBUG_PREFIX, conn->key, conn->request->serial);

		/*
		 * 3.10/KP Time stamp each request in debug output
		 */
		time(&curtime);
		p_tm = localtime(&curtime);
		debug("(%02d:%02d:%02d) ", p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);

		if (conn->request->type < MAXREQTYPE) {
			debug("%s [", NOTNUL(req_names[conn->request->type]));
		} else {
			debug("?%d [", conn->request->type);
		}
	}

	cp = (char *) (conn->request + 1);
	end_reqbuf = cp + conn->request->size - sizeof(*conn->request);

	va_start(ap, conn);

	while ((argtype = va_arg(ap, unsigned)) != _End)
	{
		switch (argtype)
		{
			default: *(int*)0=0;
			case _Time:
				t = va_arg(ap, time_t *);
				if (cp < end_reqbuf)
				{
					unsigned long long ull = htonl64(*(unsigned long long *) cp);
					*t = (time_t) ull;
					cp += sizeof(unsigned long long);
				}
				else
				{
					*t = 0;
				}

				if (DEBUG(11))
				{
					debug("%ull,", (unsigned long long) *t);
				}

				break;
			case _Index:
			case _Int:
				ip = va_arg(ap, uint32 *);
				if (cp < end_reqbuf)
				{
					*ip = htonl(*(uint32 *) cp);
					cp += sizeof(uint32);
				}
				else
				{
					*ip = 0;
				}

				if (DEBUG(11))
				{
					debug("%d,", (int) *ip);
				}

				break;
			case _String:
				sp = va_arg(ap, char **);
				if (cp < end_reqbuf)
				{
					*sp = cp;
					cp += strlen(cp) + 1;
				}
				else
				{
					*sp = NULL;
				}

				if (DEBUG(11))
				{
					debug("%s,", NOTNUL(*sp));
				}

				break;
			case _StringArray:
				/*
				 *  Empty string (smells like TCL) terminates
				 *  the string array.
				 *  Vector itself and strings from freestore.
				 *  Location of realloc guarantees space
				 *  for the terminating NULL.
				 */
				if (DEBUG(11))
				{
					debug("[");
				}

				vp = va_arg(ap, char ***);
				i = 0;
				for (;;)
				{
					if (i++)
					{
						*vp = realloc(*vp, i * sizeof(**vp));
					}
					else
					{
						*vp = calloc(1,sizeof(**vp));
					}
					if (!*vp)
					{
						fatal("%slift_args buf",DEBUG_PREFIX);
					}

					if (cp >= end_reqbuf || *cp == 0)
					{
						cp++; /* force skipover of terminating \0\0\0\0 */
						break;
					}

					(*vp)[i - 1] = strdup(cp);
					cp += strlen(cp) + 1;

					while (cp < end_reqbuf && ((unsigned) cp & (REQROUND - 1)))
					{
						cp++;
					}

					if (DEBUG(11))
					{
						debug("%s,", NOTNUL((*vp)[i - 1]));
					}

				}

				(*vp)[i - 1] = NULL;

				if (DEBUG(11))
				{
					debug("],");
				}

				break;
			case _Double:
				dp = va_arg(ap, double *);
				if (cp < end_reqbuf)
				{
					*dp = atof(cp);
					cp += strlen(cp) + 1;
				}
				else
				{
					*dp = 0.0;
				}

				if (DEBUG(11))
				{
					debug("%lf,", *dp);
				}

				break;
		}

		while (cp < end_reqbuf && ((unsigned) cp & (REQROUND - 1)))
		{
			cp++;
		}
	}
	va_end(ap);

	if (DEBUG(11))
	{
		debug("] %d\n", conn->request->size);
	}
}

void dump_request(Client *conn)
{
	time_t  curtime;
	struct tm *p_tm;

	int datalen = conn->request->size - sizeof(*conn->request);
	unsigned char *p = (void *) (conn->request + 1);

	debug("%s%d -> #%d ",DEBUG_PREFIX, conn->key, conn->request->serial);

	/* 3.10/KP Time stamp each request in debug output */
	if (DEBUG(5))
	{
		time(&curtime);
		p_tm = localtime(&curtime);
		debug("(%02d:%02d:%02d) ", p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
	}

	if (conn->request->type >= 0 && conn->request->type < MAXREQTYPE)
	{
		debug("%s [", NOTNUL(req_names[conn->request->type]));
	}
	else
	{
		debug("?%d [", conn->request->type);
	}

	if (datalen > 256)
	{
		datalen = 256;
	}

	while (--datalen >= 0)
	{
		debug((*p >= ' ' && *p < 0177) ? "%c" : "\\%o", *p);
		p++;
	}
	debug("] %d\n", conn->request->size);
}

/*
 *  Let log2 and others know about changed entry.
 *  what is either a request-code (e.g. REMOVE)
 *  or 0 for field-changes.
 *  Events from field-changes are deferred until the record is really written.
 *
 *  One client can monitor only one log, they can open multiple connections
 *  if really necessary (hmm.... masterlog ?!?!?!).
 */
void gen_event(void *internal_base, int what, LogIndex idx)
{
	Client *p, *nextmon;

	if ((p = monitors) == NULL)
	{
		return;
	}

	do
	{
		/*
		 *  p could be erased inside vreply...
		 */
		nextmon = p->nextmon;

		if (p->base->internal == internal_base)
		{
			vreply(p, EVENT, 0, _Int, what, _Index, idx, _End);
		}

	}
	while (monitors && (p = nextmon) && p != monitors);
}

void adjust_linger(HANDLE h)
{
	SOCKET s = (SOCKET) h;
	struct linger l;
	int n = sizeof(l);
	int linger_time = 1 * 60;

	if (getsockopt(s, SOL_SOCKET, SO_LINGER, (void *) &l, &n) == 0
	 && (l.l_onoff == 0 || l.l_linger < linger_time)) {
		/*
		 *  Not set, or rather short time; set to 5 minutes.
		 */
		l.l_onoff  = 1;
		l.l_linger = linger_time;
		setsockopt(s, SOL_SOCKET, SO_LINGER, (void *) &l, sizeof(l));
	}
}

int pokepeek_scanner(int how, char *if_poke_maybe_new_cfgfile)
{
#ifdef MACHINE_WNT
	char buf[1024];
	HANDLE h;
	DWORD n;
#endif
	int error = 0;
	char *newcfg = if_poke_maybe_new_cfgfile;

	/*
	 *  "" changed to NULL.
	 *
	 *  Configuration file is always replaced, opening
	 *  the fifo/pipe fails when scanner is down.
	 */
	if (newcfg && *newcfg == 0)
	{
		newcfg = NULL;
	}

#ifdef MACHINE_WNT

	if (newcfg && how == POKE_SCANNER) {
		char *oldcfg = map_special(RLSFILE_SCANNER);

		if (!MoveFileEx(newcfg, oldcfg,
				MOVEFILE_REPLACE_EXISTING |
				MOVEFILE_COPY_ALLOWED))
			error = GetLastError();
	}

	sprintf(buf, "\\\\.\\PIPE\\scanner.%s", NOTNUL(user_name));
	h = CreateFile(buf,
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (h == INVALID_HANDLE_VALUE) {
		error = GetLastError();
		if (error == 0)
			error = -1;
	} else {
		if (how == POKE_SCANNER) {
			if (!WriteFile(h, "r", 1, &n, NULL) || n != 1) {
				if (error == 0)
					error = GetLastError();
				if (error == 0)
					error = -1;
			}
		}
		CloseHandle(h);
	}
#else /* U*X */

	if (newcfg && how == POKE_SCANNER)
	{
		char *oldcfg = map_special(RLSFILE_SCANNER);

		if (rename(newcfg, oldcfg))
		{
			error = errno ? errno : -1;
		}
	}

	error = system(how == POKE_SCANNER ? "scankick" : "scankick -P");

	if (error) {
		/*
		 *  Only success/failure return from scankick.
		 */
		error = -1;
	}
#endif /* U*X */
	return (error);
}

void start_response(Client *conn, int status)
{
	start_packet(conn, RESPONSE, conn->request->serial, status);
}

void start_packet(Client *conn, int type, int serial, int status)
{
	struct dynbuf *buf = &conn->buf;
	time_t  curtime;
	struct tm *p_tm;

	if (DEBUG(11))
	{
		debug("%s%d <- #%d ",DEBUG_PREFIX, conn->key, serial);
	}

	/* 3.10/KP Time stamp each request in debug output */
	if (DEBUG(5) && DEBUG(11))
	{
		time(&curtime);
		p_tm = localtime(&curtime);
		debug("(%02d:%02d:%02d) ", p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec);
	}

	if (DEBUG(11))
	{
		debug("%s %d [", NOTNUL(req_names[type]), status);
	}

	buf->len = 0;

	append_uint32(buf, type);
	append_uint32(buf, 0);         /* length, patched later */
	append_uint32(buf, serial);
	append_uint32(buf, status);
}

void flush_packet(Client *conn)
{
	((struct reqheader *) (conn->buf.data))->size = htonl(conn->buf.len);

	if (DEBUG(11))
	{
		debug("] %d\n", conn->buf.len);
	}

	if (DEBUG(7))
	{
		debug("%s%d Give cc %d\n",DEBUG_PREFIX, conn->key, conn->buf.len);
	}

	conn->done = 0;
#ifdef MACHINE_WNT
	if (conn->handler == didread
	 || conn->handler == didconnect
	 || conn->handler == flagdead)
		conn->handler = didwrite;

	/* else special handler */
#endif
	dowrite(conn,"",0);
}


int crack_header(struct reqheader *req)
{
	swapuint32(&req->type);
	swapuint32(&req->size);
	swapuint32(&req->serial);

	if (req->size < sizeof(*req))
	{
		return (0);
	}
	if (req->type >= MAXREQTYPE)
	{
		return (0);
	}

	return (1);
}

Client *newconn(HANDLE h)
{
	Client *conn;

	conn = calloc(1, sizeof(*conn));
	if (!conn)
	{
		fatal("%scalloc",DEBUG_PREFIX);
	}

	if (client_key_next < FIRST_NORMAL)
	{
		client_key_next = FIRST_NORMAL;
	}

	conn->key        = client_key_next++;
	conn->started    = time(NULL);
	conn->net        = h;

#ifdef MACHINE_WNT
	conn->handler    = flagdead;
	conn->prehandler = flagdead;
#endif

	grow_buffer(&conn->buf, 4096, DONT_FAIL);

	/*
	 *  Link into list both ways.
	 */
	if ((conn->next = clients) != NULL)
	{
		conn->prev = clients->prev;
	}
	else
	{
		clients = conn;
	}
	clients->prev = conn;
	conn->prev->next = conn;
	clients = conn;

	num_clients++;

#ifdef MACHINE_WNT
	/*
	 *  Mapping ready "descriptors" to clients
	 *  uses IO Completion Events on NT
	 */
	evq = CreateIoCompletionPort(h, evq, conn->key, 0);
	if (!evq)
		fatal("%sCICP: %s",DEBUG_PREFIX, NOTNUL(syserr()));
#else
	/*
	 *  On U*X we use an array of Client pointers,
	 *  indexed by descriptors.
	 */
	if (fdhiwat < conn->net + 1) {
		int old = fdhiwat;
		fdhiwat = conn->net + 1;
		clientmap = realloc(clientmap, fdhiwat * sizeof(*clientmap));
		memset(&clientmap[old], 0, (fdhiwat - old) * sizeof(*clientmap));
	}
	clientmap[conn->net] = conn;

	/*
	 *  Error not checked. GETFL can fail only when h is invalid.
	 */
	fcntl(h, F_SETFL, fcntl(h, F_GETFL, 0) | O_NONBLOCK);

#endif

	if (DEBUG(8))
	{
		debug("%sNumber of clients: %d\n",DEBUG_PREFIX, num_clients);
	}

	return (conn);
}

static BOOL grow_buffer(struct dynbuf *buf, int size, int how)
{
	if (buf->size < size)
	{
		/* XXX should we really limit allocation to an arbitrary limit of 1Mb ? */
		if (size > 1 * 1024 * 1024)
		{
			if (how == DONT_FAIL)
			{
				fatal("%sgrow: want %d bytes !!",DEBUG_PREFIX, size);
			}
			else
			{
				if (DEBUG(7))
				{
					debug("%sgrow: want %d bytes !!",DEBUG_PREFIX, size);
				}
			}

			return (0);
		}
		buf->size = size;
		if (buf->data)
		{
			buf->data = realloc(buf->data, buf->size);
		}
		else
		{
			buf->data = calloc( 1, buf->size);
		}
		if (!buf->data)
		{
			if (how == DONT_FAIL)
			{
				fatal("%sgrow: %s",DEBUG_PREFIX, NOTNUL(syserr()));
			}
			else
			{
				if (DEBUG(7))
				{
					debug("%sgrow: %s",DEBUG_PREFIX, NOTNUL(syserr()));
				}
			}
			
			return (0);
		}
	}
	return (1);
}

void append_buffer(struct dynbuf *buf, void *data, int len)
{
	if (grow_buffer(buf, buf->len+len, DONT_FAIL))
	{
		memset(buf->data+buf->len,0,len);
	}
	buf->len+=len;

	memcpy(buf->data + buf->len - len, data, len);
}

void
append_char(struct dynbuf *buf, int c)
{
	buf->len++;

	grow_buffer(buf, buf->len, DONT_FAIL);

	buf->data[buf->len - 1] = c;
}

void
append_time(struct dynbuf *buf, time_t t)
{
	unsigned long long ull = (unsigned long long) t;
	swapuint64(&ull);
	append_buffer(buf, &ull, sizeof(ull));
}

void
append_uint32(struct dynbuf *buf, uint32 i)
{
	swapuint32(&i);
	append_buffer(buf, &i, sizeof(i));
}

void append_string_end(struct dynbuf *buf)
{
	do
	{
		append_buffer(buf, "", 1);
	}
	while (buf->len & 3);
}

void
append_string(struct dynbuf *buf, char *s)
{
	/*
	 * ""       0 + 1 + 3 = 4, &= ~3 = 4
	 * "X"      1 + 1 + 3 = 5, &= ~3 = 4
	 * "XX"     2 + 1 + 3 = 6, &= ~3 = 4
	 * "XXX"    3 + 1 + 3 = 7, &= ~3 = 4
	 * "XXXX"   4 + 1 + 3 = 8, &= ~3 = 8
	 */
	/*
	 * XXX Note! A possible hazard here.
	 * 0-3 bytes _after_ end of string 's' are copied in
	 * thus resulting "array bounds read" in append_buffer().
	 *
	 * Better way would be to use append_string_end() ?
	 */
	append_buffer(buf, s, (strlen(s) +1  + 3) & ~3);
	if (DEBUG(11))
	{
		debug("%s,", NOTNUL(s));
	}
}

void
append_line(struct dynbuf *buf, char *s)
{
	append_buffer(buf, s, strlen(s) );
	append_buffer(buf, "\n", 1);
}

/*
 *  Align packet after multiple appended strings.
 *  Then ship it.
 */
void flush_lines(Client *conn)
{
	append_string_end(&conn->buf);

	flush_packet(conn);
}

void
free_buffer(struct dynbuf *buf)
{
	if (buf->data) {
		free(buf->data);
		buf->data = NULL;
	}
	buf->size = 0;
}

#ifdef MACHINE_WNT

void
new_nt_named_pipe()
{
	Client *conn;
	HANDLE h;
	BOOL ok;
	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = &sd_usr_access_only;
	sa.bInheritHandle = 0;

	h = CreateNamedPipe(nt_rendezvous_path,
		PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
		PIPE_WAIT | PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
		PIPE_UNLIMITED_INSTANCES,
		0,
		0,
		0,
		&sa);

	if (h == INVALID_HANDLE_VALUE)
		fatal("%s%s: %s",DEBUG_PREFIX, NOTNUL(nt_rendezvous_path), NOTNUL(syserr()));

	conn = newconn(h);
#ifdef MACHINE_WNT
	conn->handler = didconnect;
#endif
	conn->local = 1;

	ok = ConnectNamedPipe(conn->net, &conn->ov);

	/* XXX set 'posted' flag wrt ok || PENDING */
	if (!ok && GetLastError() != ERROR_IO_PENDING)
		fatal("%sConnectNamedPipe: %s",DEBUG_PREFIX, NOTNUL(syserr()));
	
	if (DEBUG(9))
	{
		debug("%s%d ok %d, error %d\n",DEBUG_PREFIX, conn->key, ok, GetLastError());
	}
}
#endif

int didconnect(Client *conn, char *dummy1, int dummy2)
{
#ifdef MACHINE_WNT
	/* Need another rendezvous, current got connected. */
	if (conn->local)
		new_nt_named_pipe();
#endif

	if (DEBUG(3))
	{
		debug("%s%d Connect %s h %d\n",DEBUG_PREFIX, conn->key, conn->local ? "local" : "remote", conn->net);
	}

	/* First input packet. */
	conn->buf.len = sizeof(struct signon);
	conn->done = 0;
#ifdef MACHINE_WNT
	conn->handler = didread;
#endif
	doread(conn,"",0);
	return 0;
}

/* Prehandlers for read/write */
int dowrite(Client *conn, char *dummy1, int dummy2)
{
	DWORD n;
	BOOL ok = 0;

#ifdef MACHINE_WNT
	conn->prehandler = dowrite;
#endif

	if (DEBUG(16))
	{
		debug("%s%d dowrite len %d, done %d\n",DEBUG_PREFIX, conn->key, conn->buf.len, conn->done);
	}

#ifndef MACHINE_WNT
	if (conn->fdpass > 0)
	{
		/*
		 * 1.7/KP
		 * Removed for STX 4.3.1, as this is not used anywhere. Right? RIGHT? (KP)
		 *
		 * Segfault, just in case...
		 */
		*(int*)0=0;
	}
	else
	{
#endif
		ok = WriteFile(conn->net,
			conn->buf.data + conn->done,
			conn->buf.len - conn->done, &n,
#ifdef MACHINE_WNT
			&conn->ov
#else
			0
#endif
		);
#ifndef MACHINE_WNT
	}
#endif

	if (DEBUG(16))
	{
		debug("%s%d dowrite %d, did %d, %s\n",DEBUG_PREFIX, conn->key, conn->buf.len - conn->done, n, ok ? "OK" : NOTNUL(syserr()));
	}

#ifdef MACHINE_WNT
	if (ok)
	{
		if (!n)
			ok = 0;
		conn->posted = 1;
	}
	else
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			conn->posted = 1;
			ok = 1;
		}
	}
#else
	if (ok)
	{
		if ((conn->done += n) == conn->buf.len)
		{
			didwrite(conn,"",0);
		}
		else
		{
			FD_SET(conn->net, &wset);
		}
	}
#endif
	if (!ok)
	{
		flagdead(conn,"",0);
	}
	return 0;
}

int doread(Client *p, char *dummy1, int dummy2)
{
	DWORD n;
	BOOL ok;

#ifdef MACHINE_WNT
	p->prehandler = doread;
#endif

	if (!grow_buffer(&p->buf, p->buf.len, FAIL_OK)) {
		warn("%sCannot grow buffer to length %d, %s",DEBUG_PREFIX, p->buf.len, NOTNUL(syserr()));
		flagdead(p,"",0);
		return -1;
	}
	if (DEBUG(16))
	{
		debug("%s%d doread len %d, done %d\n",DEBUG_PREFIX, p->key, p->buf.len, p->done);
	}


	ok = ReadFile(p->net,
		p->buf.data + p->done,
		p->buf.len - p->done, &n,
#ifdef MACHINE_WNT
		&p->ov
#else
		0
#endif
	);

	if (DEBUG(16))
	{
		debug("%s%d doread %d, did %d, %s\n",DEBUG_PREFIX, p->key, p->buf.len - p->done, n, ok ? "OK" : NOTNUL(syserr()));
	}

#ifdef MACHINE_WNT
	if (ok) {
		if (!n)
			ok = 0;
		p->posted = 1;
	} else {
		if (GetLastError() == ERROR_IO_PENDING) {
			ok = 1;
			p->posted = 1;
		}
	}
#else
	if (ok)
	{
		if ((p->done += n) == p->buf.len)
		{
			didread(p,"",0);
		}
		else
		{
			FD_SET(p->net, &rset);
		}
	}
	else
	{
		if (p->handler == inetrecvfile && p->synthetic_handler == NULL && p->done)
		{
			WriteFile((HANDLE) p->file, p->buf.data, p->done, &n, NULL);
		}
	}

#endif

	if (!ok)
	{
		flagdead(p,"",0);
	}
	return 0;
}

/*
 *  After one packet is completely transferred,
 *  another of these is called.
 *
 *  When one "xaction" does not generate a response output,
 *  readagain prepares for next input packet.
 */

int didwrite(Client *conn, char *dummy1, int dummy2)
{
	if (conn->done != conn->buf.len) {
		flagdead(conn,"",0);
		return -1;
	}
	/*
	 *  Turn around and get next input packet.
	 */
#ifndef MACHINE_WNT
	if (conn->handler)
	{
		(*conn->handler)(conn,"",0);
	}
	else
	{
		readagain(conn,"",0);
	}
#else
	readagain(conn,"",0);
#endif
	return 0;
}

int readagain(Client *conn, char *dummy1, int dummy2)
{
	conn->buf.len = sizeof(struct reqheader);
	conn->done = 0;
#ifdef MACHINE_WNT
	conn->handler = didread;
#endif
	doread(conn,"",0);
	return 0;
}

int didread(Client *conn, char *dummy1, int dummy2)
{
	int n, nRet;

	conn->request = (void *) conn->buf.data;

	if (conn->done != conn->buf.len)
	{
		flagdead(conn,"",0);
		return -1;
	}
#ifndef MACHINE_WNT
	if (conn->handler)
	{
		(*conn->handler)(conn,"",0);
		return -1;
	}
#endif
	if (!conn->logon)
	{
		/* Very first, special packet. */

		if ((nRet = verify_signon(conn)) != 0 )
		{
			if (!(error_string ( conn, nRet )))
			{
				vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _End);
			}
			
			/*
			 * 1.4/KP
			 *
			 * flagdead() insted of endconn() (which would free the whole conn
			 * structure. endconn() is called later, by our parent. Hopefully.
			 */
			flagdead(conn,"",0); 
			return -1;
		}
		vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _End);
		return -1;
	}
	if (conn->done == sizeof(struct reqheader))
	{
		/*
		 *  First checkpoint.
		 */
		if (!crack_header(conn->request))
		{
			flagdead(conn,"",0);
			return -1;
		}
		n = conn->request->size - sizeof(struct reqheader);
		if (n)
		{
			conn->buf.len += n;
			doread(conn,"",0);
			return -1;
		}
	}
	/*
	 *  Received the whole packet.
	 *  See what needs to be done, if anything.
	 */
	if (!process(conn,"",0))
	{
		flagdead(conn,"",0);
	}
	return 0;
}

int flagdead(Client *conn, char *dummy1, int dummy2)
{
	conn->dead = 1;
	return 0;
}

void client_write_blocked(Client *conn)
{
#ifndef MACHINE_WNT
	FD_CLR(conn->net, &rset);
	FD_SET(conn->net, &wset);
#endif
}

int CheckAuthOnFile(Client *conn, char **entry_block ,char **block,char **value,char *ipaddress, char *Path)
{
	int nRet, nRet2, nRet3, nRet4;
	int Count = 0;
	int AuthOK=0;
	int Yes=0;

	char *p;
	char *file_block= NULL;
	char *user_found=NULL;
	char *pwd_found=NULL;
	char *user=NULL;
	char *pword=NULL;
	char *ip_block=NULL;
	char *ip_adress=NULL;
	int n;
	struct sockaddr_in server_addr;
	char *Packet;
	char *login_block=NULL;

	/* Get the file block */
	nRet = File_read(&file_block , Path);
	if (nRet != 0 )
	{
		if (debugged)
		{
			debug("%sError on getting file \"%s\"\n",AUTH_PREFIX, NOTNUL(Path));
		}
		return nRet;
	}
	
	/* Seeking entry block */
	while(1)
	{
		nRet = GetFieldNew( START_OF_BLOCK , END_OF_BLOCK , file_block, ENTRY_BLOCK , entry_block );
		switch (nRet)
		{
			case BLOCK_FOUND:
				/* Seeking auth block */
				while(1)
				{
					nRet2 = GetFieldNew( START_OF_BLOCK , END_OF_BLOCK , *entry_block, AUTH_BLOCK , block);
					switch (nRet2)
					{
						case BLOCK_FOUND:
							/* Auth block found */
							if (debugged)
							{
								debug("%s'<Auth' block found :\n\t%s\n",AUTH_PREFIX, NOTNUL(*block));
							}
							
							/* Seeking Username block */
							while(1)
							{
								nRet3 = GetFieldNew( START_OF_BLOCK , END_OF_BLOCK , *block , USERNAME_FIELD , &login_block );
								switch (nRet3)
								{
									/* Success , <Username block found */
									case BLOCK_FOUND: 
										AuthOK = 0;
										if (debugged)
										{
											debug("%s'<Username' block found :[%s]\n",AUTH_PREFIX, NOTNUL(login_block));
										} 
										nRet4 = GetUserAndPwd(login_block, &user_found, &pwd_found);
										if (nRet4 != BLOCK_FOUND)
										{
											if (debugged)
											{
												debug("%sUser not found : GetUserAndPwd() return %d \n",AUTH_PREFIX, nRet3);
											}
											return nRet4;
										}
										if (debugged)
										{
											debug("%sGetUserAndPwd(%s) found user=%s & passwd=%s \n",AUTH_PREFIX, NOTNUL(login_block),
												NOTNUL(user_found),PASSWORD(pwd_found));
										}
										
										/* local user case */
										if ((user_found != NULL) && (strcmp (user_found , "localuser") == 0 ))
										{
											if (debugged)
											{
												debug("%sLocaluser found (so we must replace it by with %s)\n",AUTH_PREFIX,
													NOTNUL(GET_USER) );
											}
											user_found = malloc(strlen(GET_USER) + 1);
											if (user_found == NULL)
											{
												svclog('I',  "%sInternal memory error. Sys Error Msg: %s System Errno: %d",AUTH_PREFIX,
													NOTNUL(strerror(errno)),errno);
												return INTERNAL_ERROR;
											}
											strcpy (user_found , GET_USER);
										}
										
										/* test of user & password */
										/* Only the string data is processed */
										Packet = strdup (conn->buf.data + SIGN_ON_PACKET_H_LENGHT );
										nRet4 = CheckSignOnPacketSyntax (Packet, &user, &pword, ipaddress);
										if (nRet4 != USER_ACCEPTED)
										{
											free (Packet);
											break;
										}
										if (debugged)
										{
											debug("%sCheckSignOnPacketSyntax() = %d, found user=%s & passwd=%s\n",AUTH_PREFIX, nRet,
												NOTNUL(user),PASSWORD(pword));
										}
										debug("%sMatching ? \n",AUTH_PREFIX);
										
										/* Packet is syntaxically correct (user & passwd get) */
										if ((user_found != NULL) && (strcmp (user_found , "") != 0))
										{
											if (debugged)
											{
												debug("%sTesting user [%s] \n",AUTH_PREFIX, NOTNUL(user_found));
											}
											if ( strcmp (user_found , "localuser") == 0 )
											{
												strcpy (conn->userid , GET_USER);
												if ( strcmp (user , conn->userid) != 0 )
												{
													if (debugged)
													{
														debug("%s\t[%s] != [localuser]\n",AUTH_PREFIX, NOTNUL(user));
													}
													free (Packet);
													
													/*  le user n'est pas le bon => Suivant ! */
													break;
												}
											}
											else
											{
												strcpy (conn->userid , user);
												if ( strcmp (user_found , user) != 0 )
												{
													if (debugged)
													{
														debug("%s\t[%s] != [%s]\n",AUTH_PREFIX, NOTNUL(user_found), NOTNUL(user));
													}
													free (Packet);
													/*  le user n'est pas le bon => Suivant ! */
													break;
												}
												if (debugged)
												{
													debug("%s\t[%s] == [%s]\n",AUTH_PREFIX, NOTNUL(user_found), NOTNUL(user));
												}
											}
										}
										if ((pwd_found != NULL) && (strcmp (pwd_found , "") != 0))
										{
											if (debugged)
											{
												debug("%sTesting password [%s] \n",AUTH_PREFIX, PASSWORD(pwd_found));
											}
											if ( strcmp (pwd_found , pword) != 0 )
											{
												if (debugged)
												{
													debug("%s%s != %s\n",AUTH_PREFIX, PASSWORD(pwd_found), PASSWORD(pword));
												}
												free (Packet);
												
												/*  le password n'est pas le bon => Suivant ! */
												break; 
											}
										}
										AuthOK = 1;
										free (Packet); /* user, pword are pointer in Packet */
										break;
										/* User Syntax error --> quit */
									case NOT_PAIR_FOUND:
										LogError (Path , START_OF_BLOCK , USERNAME_FIELD , NOT_PAIR_FOUND);
										return NOT_PAIR_FOUND;
										break;
										/* Username Field is not given at all*/
									case NOT_FIELD_FOUND:
										if (debugged) {  debug("%s'<Username' block not found \n",AUTH_PREFIX); }
										break;
									default:
										if (debugged) { debug("%sSeek Username block return %d \n",AUTH_PREFIX, nRet2); }
										return INTERNAL_ERROR;
										break;
								} /* end switch */
								if ((nRet3 == NOT_FIELD_FOUND) || (AuthOK == 1))
								{
									break;
								}
							} /* end while */
							if (AuthOK == 1)
							{
								if (debugged) { debug("%sAuthentification user/password OK\n",AUTH_PREFIX); }
								/* Test de l'adresse IP */
								/* TODO : Externaliser pour impl�menter une fois les deux tests */
								if ( conn->local == 1)
								{
									/*  le user est local =< pas de test de l'adresse IP */
									if (debugged) { debug("%slocal user => no need of testing IP adress\n",AUTH_PREFIX); }
									return USER_ACCEPTED;
								}

								Count=0;
								Yes=1;
								while(Yes)
								{
									nRet = GetIPAdress( *block, &ip_block, &ip_adress);
									if (debugged) { debug("%sGetIPAdress return %d\n",AUTH_PREFIX, nRet); }
									switch (nRet)
									{
										case BLOCK_FOUND:
											if (( p = strstr ( ip_adress , "*" )) != NULL ) { *p = '\0'; }
											if (debugged) { debug("%sipaddress = %s \n",AUTH_PREFIX, NOTNUL(ip_adress)); }
											if ( strcmp (ip_adress , "localhost") == 0 )
											{
												if (debugged)
												{ 
													debug("%slocalhost found\n",AUTH_PREFIX);
													debug("%smyipaddress %s\n",AUTH_PREFIX, NOTNUL(ipaddress));
												}
												n = sizeof(server_addr);
												if (getsockname((SOCKET)conn->net, (struct sockaddr *) &server_addr, &n))
												{
													svclog('I',
														"%sUser = %s , Authentication : getsockname failed. Sys Error Msg: %s System Errno: %d",
														AUTH_PREFIX, NOTNUL(getenv("USER")),NOTNUL(strerror(errno)),errno);
													return AUTH_SYSTEM_ERROR;
												}

												if (strncmp((char *)inet_ntoa(server_addr.sin_addr), ipaddress,
													strlen((char *)inet_ntoa(server_addr.sin_addr))) == 0)
												{
													if (debugged)
													{
														debug("%s\t[%s] == [%s]\n", AUTH_PREFIX,
															(char *) NOTNUL(inet_ntoa(server_addr.sin_addr)), NOTNUL(ipaddress));
													}
													return USER_ACCEPTED;
												}
												if (debugged)
												{
													debug("%s\t[%s] != [%s]\n",AUTH_PREFIX,
														(char *) NOTNUL(inet_ntoa(server_addr.sin_addr)) , NOTNUL(ipaddress));
												}
											}
											else
											{
												if ( strncmp ( ip_adress , ipaddress, strlen (ip_adress)) == 0 )
												{
													return USER_ACCEPTED;
												}
											}
											break;

											/* ipaddress Syntax error --> quit */
										case NOT_PAIR_FOUND:
											LogError (Path , START_OF_BLOCK , IP_ADDRESS_FIELD2 , NOT_PAIR_FOUND);
											LogError (Path , START_OF_BLOCK , IP_ADDRESS_FIELD , NOT_PAIR_FOUND);
											return NOT_PAIR_FOUND;
											break;
											
											/* ipaddress Field is not given at all */
										case NOT_FIELD_FOUND:
											if (Count == 0)
											{
												return USER_ACCEPTED;
											}
											Yes=0;
											break;

										default:
											return INTERNAL_ERROR;
											break;
									}
									Count++;
								}					
							}
							break;
							/* User Syntax error --> quit */
						case NOT_PAIR_FOUND:
							LogError (Path , START_OF_BLOCK , USERNAME_FIELD , NOT_PAIR_FOUND);
							return NOT_PAIR_FOUND;
							break;
							/* User Field is not given at all*/
						case NOT_FIELD_FOUND:
							if (debugged) { debug("%s'<Auth' block not found \n",AUTH_PREFIX); }
							break;
						default:
							if (debugged) { debug("%sSeek Auth block return %d \n",AUTH_PREFIX, nRet); }
							return INTERNAL_ERROR;
							break;
					} /* end switch */
					if (nRet2 != BLOCK_FOUND) {
						break;
					}
				} /* end while */
				break;
				/* User Syntax error --> quit */
			case NOT_PAIR_FOUND:
				LogError (Path , START_OF_BLOCK , ENTRY_BLOCK , NOT_PAIR_FOUND);
				return NOT_PAIR_FOUND;
				break;
				/* User Field is not given at all*/
			case NOT_FIELD_FOUND:
				if (debugged) { debug("%s'<Entry' block not found \n",AUTH_PREFIX); }
				break;
			default:
				if (debugged) { debug("%sSeek Entry block return %d \n",AUTH_PREFIX, nRet); }
				return INTERNAL_ERROR;
				break;
		} /* end switch */
		if (nRet != BLOCK_FOUND) {
			if (debugged) {  debug("%sNo more Entry Block (%d) \n",AUTH_PREFIX, nRet); }
			break;
		}
	} /* end while */
	return nRet;
}

/*
	Notice: The signon packet is checked ,password and username retrieved
			only if there is a entry which demands this operation.
*/
int CheckAuthentification(Client *conn, char **entry_block ,char **block,char **field_block,char **value,char *ipaddress, char *Path)
{
	int nRet;
	char *p, *p2;
	char EdihomeRLSFilePath[256], HomeRLSFilePath[256];

	if ((p = getenv (USR_ENV_HOME)) == NULL)
	{
		svclog('I',  "%sUser = %s , Authentication : Environment variable %s not set. Sys Error Msg: %s System Errno: %d",
			DEBUG_PREFIX, NOTNUL(GET_USER), USR_ENV_HOME,NOTNUL(strerror(errno)),errno);
		return ENV_VAR_ERROR;
	}
	if ((p2 = getenv (EDI_ENV_HOME)) == NULL)
	{
		svclog('I',  "%sUser = %s , Authentication : Environment variable %s not set. Sys Error Msg: %s System Errno: %d",
			DEBUG_PREFIX, GET_USER ? GET_USER : "" , EDI_ENV_HOME ,NOTNUL(strerror(errno)),errno);
		return ENV_VAR_ERROR;
	}

#ifdef MACHINE_WNT
	sprintf ( HomeRLSFilePath , "%s\\%s" , p , RLSHOSTS);
	sprintf ( EdihomeRLSFilePath , "%s\\lib\\%s" , p2 , RLSHOSTS);
#else
	sprintf ( HomeRLSFilePath , "%s/%s" , p , RLSHOSTS);
	sprintf ( EdihomeRLSFilePath , "%s/lib/%s" , p2 , RLSHOSTS);
#endif

	/* Seeking on first file */
	if (debugged) { debug("%sSeeking Authentification on \"%s\"\n",AUTH_PREFIX, NOTNUL(HomeRLSFilePath)); }
	nRet = CheckAuthOnFile(conn, entry_block ,block,value,ipaddress, HomeRLSFilePath);
	Path = HomeRLSFilePath;
	if (nRet != BLOCK_FOUND) {
		/* Seeking on second file */
		if (debugged) { debug("%sSeeking Authentification on \"%s\"\n",AUTH_PREFIX, NOTNUL(EdihomeRLSFilePath)); }
		nRet = CheckAuthOnFile(conn, entry_block ,block,value,ipaddress, EdihomeRLSFilePath);
		Path = EdihomeRLSFilePath;
	}

	return nRet;
}

/********************************************************************
 *
 * 	frees the memory allocated for the base_list
 *
 *******************************************************************
 */
int free_base_list(struct BASE_LIST **list)
{
	struct BASE_LIST *t,*p;

	p = *list;

	while (p != NULL)
	{
		t = p->next;

		if ( p->path )
		{
			free ( p->path );
		}

		free ( p );
		p = t;
	}

	*list = NULL;
	return(0);
}

/*
	Simple compare for the list, returns 1 if match found
	flag not processed yet
*/
int check_base_list ( char *path ,int *type ,struct BASE_LIST **list, Client *pcli)
{
	struct BASE_LIST *p;
	char *ptr1,*ptr2,*ptr3;
#ifdef MACHINE_WNT
	char *tcp;
#endif

	if (DEBUG(6))
	{
		debug("%schecking path 0x%X = %s\n",DEBUG_PREFIX, path, NOTNUL(path));
	}

	if ((ptr1 = strstr ( path , ":")) == NULL )
	{
		ptr1 = path;
	}
	else
	{
		ptr1++;
	}

#ifdef MACHINE_WNT
	/*
	 * 3.17/KP : Convert slashes to backslashes
	 * For that purpose, alloc a local duplicate for ptr1, as
	 * altering the original might not be nice...
	 * Free the dupped one below before returning.
	 */
	ptr1 = strdup(ptr1);
	for (tcp = ptr1; *tcp; tcp++)
		if (*tcp == '/')
			*tcp = '\\';
#endif

	p = *list;

	while (p != NULL)
	{
		if (DEBUG(6))
		{
			svclog ('I' , "%sp->path = %s",DEBUG_PREFIX, NOTNUL(p->path));
			debug("%swhile = p->path %s\n",DEBUG_PREFIX, NOTNUL(p->path));
		}

		if (p->path == NULL)
		{
			p = p->next;
			continue;
		}

		if (strcmp("*",p->path) == 0)
		{
			*type = p->flags;
			if (DEBUG(6))
			{
				debug("%sMATCH_FOUND = %s\n",DEBUG_PREFIX, NOTNUL(p->path));
			}

#ifdef MACHINE_WNT
			free (ptr1);
			ptr1 = NULL;
#endif
			return MATCH_FOUND;
		}

		if ((ptr2 = strstr ( p->path , ":")) == NULL )
		{
			ptr2 = p->path;
		}
		else
		{
			ptr2++;
		}


		if ( strcmp ( ptr1 , ptr2 ) == 0 )
		{
			*type = p->flags;
			if (DEBUG(6))
			{
				debug("%sMATCH_FOUND = %s\n",DEBUG_PREFIX, NOTNUL(ptr2));
			}

#ifdef MACHINE_WNT
			free (ptr1);
			ptr1 = NULL;
#endif
			return MATCH_FOUND;
		}

		/*
		 * 3.17/KP : Try if this path is a home-relative one...
		 */
		if (getenv("HOME")) {
			ptr3 = malloc(strlen(getenv("HOME")) + strlen(ptr1) + 2);
#ifdef MACHINE_WNT
			sprintf(ptr3, "%s\\%s", getenv("HOME"), ptr1);

			/*
			 * Cut possible C: (drive letter + colon) from the beginning...
			 */
			if (strstr(ptr3, ":"))
				strcpy(ptr3, strstr(ptr3, ":")+1);

			/*
			 * Convert / -> \
			 */
			for (tcp = ptr3; *tcp; tcp++)
				if (*tcp == '/')
					*tcp = '\\';
#else
			sprintf(ptr3, "%s/%s", getenv("HOME"), ptr1);
#endif
			
			if ( strcmp ( ptr3 , ptr2 ) == 0 )
			{
				*type = p->flags;
				if (DEBUG(6))
				{
					debug("%sMATCH_FOUND = %s\n",DEBUG_PREFIX, NOTNUL(ptr2));
				}

				free (ptr3);
				ptr3 = NULL;
#ifdef MACHINE_WNT
				free (ptr1);
				ptr1 = NULL;
#endif
				return MATCH_FOUND;
			}
			else
			{
				if (DEBUG(6))
				{
					debug("%sNO_MATCH_FOUND ('%s' <-> '%s'\n",DEBUG_PREFIX, NOTNUL(ptr3), NOTNUL(ptr2));
				}
			}

			free (ptr3);
			ptr3 = NULL;
		}

		p = p->next;
	}

	if (DEBUG(6))
	{
		debug("%sNO_MATCH_FOUND\n",DEBUG_PREFIX);
	}

#ifdef MACHINE_WNT
	free (ptr1);
	ptr1 = NULL;
#endif
	return(NO_MATCH_FOUND);
}

/*
	Checks that signon packet has a correct syntax:
	4 bytes for header, not used yet
	username
	point-virgule
	password
	point-virgule
	the rest of the packet will fillled with spaces until 60 bytes
*/
int CheckSignOnPacketSyntax (char *Packet, char **p_to_username,char **p_to_password, char *ipaddress)
{
	char *u,*p;

	/*
		Extract the username and password from the packet
		by setting pointers to them
	*/
	*p_to_username = Packet;
	if (( u = strstr ( Packet , DELIMITER)) == NULL )
	{
		svclog('I',  "%sUser = %s , Authentication error : Illegal Signon packet Header from Client %s, first %s not found (packet='%s')",
			DEBUG_PREFIX, NOTNUL(getenv("USER")), NOTNUL(ipaddress),DELIMITER,NOTNUL(*p_to_username));
		return SIGNON_PACKET_SYNTAX_ERROR;
	}

	*u = '\0';

	*p_to_password = u + 1;
	if (( p = strstr ( u + 1 , DELIMITER)) == NULL )
	{
		svclog('I',  "%sUser = %s , Authentication error : Illegal Signon packet Header from Client %s, second %s not found (packet='%s')",
			DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL(ipaddress),DELIMITER,NOTNUL(*p_to_username));
		return SIGNON_PACKET_SYNTAX_ERROR;
	}
	*p = '\0';

	return USER_ACCEPTED;
}

int CheckRestrictSection (Client *conn ,char **entry_block, char **block,char **field_block,char **value, char *Path)
{
	/*
		At this stage auth block is OK
		next collect the restriction block to the linked lists
		this information will be set to the client struct
	*/
	int nRet;
	int Count=0, Yes=0;
	char *p;

	if ( (nRet = GetFieldNew ( START_OF_BLOCK , END_OF_BLOCK , *entry_block, RESTRICT_BLOCK , block )) != BLOCK_FOUND )
	{
		switch (nRet)
		{
			case NOT_PAIR_FOUND:
				LogError (Path , START_OF_BLOCK , RESTRICT_BLOCK , NOT_PAIR_FOUND);
				return nRet;
				break;

			case NOT_FIELD_FOUND:
				return USER_ACCEPTED;
				break;

			default:
				return INTERNAL_ERROR;
				break;
		}
	}

	/*
		Check <File first, permission to handle files in the server
	*/

	switch (nRet = GetFieldNew ( START_OF_BLOCK , END_OF_BLOCK , *block, FILE_FIELD, field_block ))
	{
		/* Success , <File-block found */
		case BLOCK_FOUND:
			GetValue ( START_OF_BLOCK , END_OF_BLOCK , *field_block , FILE_FIELD , value );
			if (strlen(*value) <= 1)
			{
				switch (**value)
				{
					case 'R':
						/* User has only read access to the file system */
						conn->FileFlag=**value;
						break;

					case 'N':
						/* User has no access to the file system */
						conn->FileFlag=**value;
						break;

					case '\0':
						/* As a default no restrictions set, i.e. no value given */
						break;

					default:
						/* All other values as listed above are error situations */
						LogError (Path , START_OF_BLOCK , FILE_FIELD , INV_FLAG_IN_FILE_FIELD);
						return INV_FLAG_IN_FILE_FIELD;
						break;
				}
				break;
			}
			else
			{
				LogError (Path , START_OF_BLOCK , FILE_FIELD , INV_FLAG_IN_FILE_FIELD);
				return INV_FLAG_IN_FILE_FIELD;
			}

			/* <File Syntax error --> quit */
		case NOT_PAIR_FOUND:
			LogError (Path , START_OF_BLOCK , FILE_FIELD , NOT_PAIR_FOUND);
			return NOT_PAIR_FOUND;
			break;

		/* <File Field is not given at all --> no restrictions set*/
		case NOT_FIELD_FOUND:
			break;

		default:
			/* To a client this is shown as a "Unknown error" */
			return INTERNAL_ERROR;
			break;
	}

	/*
		then Check <Run, can user run programs in server ???
	*/

	switch (nRet = GetFieldNew ( START_OF_BLOCK , END_OF_BLOCK , *block, RUN_FIELD, field_block ))
	{
		/* Success , <Run block found */
		case BLOCK_FOUND:
			GetValue ( START_OF_BLOCK , END_OF_BLOCK , *field_block , RUN_FIELD , value );

			if (strlen(*value) <= 1)
			{

				switch (**value)
				{
					/* User is Not allowed to run programs */
					case 'N':
						conn->RunFlag=**value;
						break;

					case '\0':
					/* As a default no restrictions set, i.e. no value given */
						break;

					default:
					/* All other values as listed above are error situations */
						LogError (Path , START_OF_BLOCK , RUN_FIELD , INV_FLAG_IN_RUN_FIELD);
						return INV_FLAG_IN_RUN_FIELD;
						break;
				}
				break;
			}
			else
			{
				LogError (Path , START_OF_BLOCK , RUN_FIELD , INV_FLAG_IN_RUN_FIELD);
				return INV_FLAG_IN_RUN_FIELD;
			}

		/* <Run Syntax error --> quit */
		case NOT_PAIR_FOUND:
			LogError (Path , START_OF_BLOCK , RUN_FIELD , NOT_PAIR_FOUND);
			return NOT_PAIR_FOUND;
			break;

		/* <Run Field is not given at all --> User is allowed to run all programs*/
		case NOT_FIELD_FOUND:
			break;

		default:
			/* To a client this is shown as a "Unknown error" */
			return INTERNAL_ERROR;
			break;
	}

	/*
		then Check <Base , are operations for certains bases restricted ???
		<Base PATH DELIMITER FLAG>
	*/
	Count=0;
	Yes=1;
	while(Yes)
	{
		switch (nRet = GetFieldNew ( START_OF_BLOCK , END_OF_BLOCK , *block, BASE_FIELD, field_block ))
		{
			/* Success , <Base-block found */
			case BLOCK_FOUND:

				GetValue ( START_OF_BLOCK , END_OF_BLOCK , *field_block , BASE_FIELD , value );

				/* Find FLAG first */
				if (( p = strstr ( *value , DELIMITER)) == NULL )
				{
					/* No DELIMITER found , we have two choices <Base > or <Base /morko/base> */
					if (strlen(*value) != 0)
					{
						/* we have a path <Base /morko/base */
						if ((nRet = parse_path (value)) != USER_ACCEPTED)
						{
							return nRet;
						}

						if (myAccess (*value , F_OK) != 0)
						{
							LogError (Path , START_OF_BLOCK , BASE_FIELD , INV_PATH_IN_BASE_FIELD);
							return INV_PATH_IN_BASE_FIELD;
						}


						if ( add_base_list (*value, DEFAULT_BASE_FLAG_N , &(conn->base_list)) == -1 )
						{
							fatal("%smalloc add_base_list",DEBUG_PREFIX);
						}

						break;
					}
				}
				else
				{
					if (strlen(*value) == 1)
					{
						/*	<Base :> Only delimiter given */
						break;

					}
					else
					{
						if (strncmp ( *value , DELIMITER , strlen(DELIMITER)) == 0 )
						{
							/*	<Base :N> Only flag given missing the parsing */

							if (strlen((p + 1)) <= 1)
							{
								switch (*(p + 1))
								{
									case 'R':
									/* Only read access to the base */
										*p = 'R';
										break;

									case 'N':
									/* No access to the base at all*/
										*p = 'N';
										break;

									case '\0':
										/* If value is not given so as a default NO rights at all */
										*p = DEFAULT_BASE_FLAG_N;
										break;

									default:
										LogError (Path , START_OF_BLOCK , BASE_FIELD , INV_FLAG_IN_BASE_FIELD);
										return INV_FLAG_IN_BASE_FIELD;
										break;
								}
							}
							else
							{
								LogError (Path , START_OF_BLOCK , BASE_FIELD , INV_FLAG_IN_BASE_FIELD);
								return INV_FLAG_IN_BASE_FIELD;
							}


							if ( add_base_list ("*", *p , &(conn->base_list)) == -1 )
							{
								fatal ("%smalloc add_base_list",DEBUG_PREFIX);
							}

							break;

						}
						else
						{
							if ((*(p + 1)) == '\0')
							{
								/*	<Base /morko/base;> Only path given with delimiter */
								/* If value is not given so as a default NO rights at all */

								*p = '\0';
								if ((nRet = parse_path (value)) != USER_ACCEPTED)
								{
									return nRet;
								}

								if (myAccess (*value , F_OK) != 0)
								{
									LogError (Path , START_OF_BLOCK , BASE_FIELD , INV_PATH_IN_BASE_FIELD);
									return INV_PATH_IN_BASE_FIELD;
								}

								if ( add_base_list (*value, DEFAULT_BASE_FLAG_N , &(conn->base_list)) == -1 )
								{
									fatal ("%smalloc add_base_list",DEBUG_PREFIX);
								}

								break;

							}
							else
							{
								char tmpflag;

								/*	<Username /morko/base:R > Both given */
								if (strlen((p + 1)) <= 1)
								{
									switch (*(p + 1))
									{
										case 'R':
										case 'N':
											break;

										case '\0':
											*(p + 1) = DEFAULT_BASE_FLAG_N;
											break;

										default:
											LogError (Path , START_OF_BLOCK , BASE_FIELD , INV_FLAG_IN_BASE_FIELD);
											return INV_FLAG_IN_BASE_FIELD;
									}
									tmpflag = *(p+1);

									*p = '\0';
									if ((nRet = parse_path (value)) != USER_ACCEPTED)
									{
										return nRet;
									}

									if (myAccess (*value , F_OK) != 0)
									{
										LogError (Path , START_OF_BLOCK , BASE_FIELD , INV_PATH_IN_BASE_FIELD);
										return INV_PATH_IN_BASE_FIELD;
									}

									/*
									 * 1.6/KP : Argh. parse_path alters the contents of 'value', so *(p+1)
									 * points to somewhere... nowhere.
									 */
									if ( add_base_list (*value, tmpflag , &(conn->base_list)) == -1 )
									{
										fatal ("%smalloc add_base_list",DEBUG_PREFIX);
									}

									break;
								}
								else
								{
									LogError (Path , START_OF_BLOCK , BASE_FIELD , INV_FLAG_IN_BASE_FIELD);
									return INV_FLAG_IN_BASE_FIELD;
									break;
								}
							}
						}
					}
				}
				break;

			/* <Base Syntax error --> quit */
			case NOT_PAIR_FOUND:
				LogError (Path , START_OF_BLOCK , BASE_FIELD , NOT_PAIR_FOUND);
				return NOT_PAIR_FOUND;
				break;

			/* <Base Field is not given at all */
			case NOT_FIELD_FOUND:
				return USER_ACCEPTED;
				break;

			default:
				return INTERNAL_ERROR;
				break;
		}
		Count++;
	}
	return USER_ACCEPTED;

}

/********************************************************************
 *
 * 	add_base_list adds given path to base with flags set to base_list
 *
 * 	returns -1 on error (malloc errror)
 *			 0 on success
 *
 *******************************************************************
 */
int add_base_list(char *path ,char flags, struct BASE_LIST **list)
{
struct BASE_LIST *t,*p;
#ifdef MACHINE_WNT
char *x;

	x = path;
	while (*x) {
		if ( *x == '/' ) *x = '\\';
		x++;
	}
#endif

	if ((t = ( struct BASE_LIST * ) calloc(1,sizeof(struct BASE_LIST))) == NULL)
	{
		return(-1);
	}

	/* memset not needed with calloc */

	if ( strlen ( path ) > 0 )
	{
		if (( t->path =  ( char *) calloc(1,strlen(path) + 1)) == NULL )
		{
			return -1;
		}

		strcpy(t->path, path);
	}

	t->flags =  flags;

	if (*list == NULL)
	{
		*list = t;
	}
	else
	{
		p = *list;

		while (p->next != NULL)
		{
			p = p->next;
		}

		p->next = t;
	}
	return(0);
}

/*
	Returns characters behind the last
	/ character, if not found returns
	the same string
*/
char *GetName (char *string)
{
	char *p,*t;
	t = p = string;
	while(*p != '\0')
	{
		if (*p == '/')
		{
			t = p + 1;
		}
		p++;
	}
	return t;
}

#define PATHDELIMS	"/"

int parse_path (char **path)
{
	char *dup_path;
	char *ptr_to_home;
	char *ptr_to_env;
	char *p;

#ifdef MACHINE_WNT
	if (strncmp(*path , ".\\", 2) == 0 )
	{
		strcpy ( *path , *path + 2 );
	}
#endif

	if (strncmp(*path , "./", 2) == 0 )
	{
		strcpy ( *path , *path + 2 );
	}

	dup_path = strdup ( *path );
	CutOfSpaces(*path);

	switch (**path)
	{

#ifdef MACHINE_WNT
		case '\\':
#endif
		case '/':
			/* This is a absoluth path nothing to do*/
			break;

		case '$':
			/* Use given environment variable */
			if (( p = strstr (*path , PATHDELIMS)) != NULL)
			{
				*p = '\0';
				if ((ptr_to_env = getenv (*path + 1)) == NULL)
				{
					free  ( dup_path );
					svclog('I',  "%sUser = %s , Authentication : Environment variable %s not set. Sys Error Msg: %s System Errno: %d",
						DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL((*path + 1)),NOTNUL(strerror(errno)),errno);
					return BASE_ENV_VAR_ERROR;
				}

				strcpy (dup_path , dup_path + strlen(*path) + 1);
			}
			else
			{
				if ((ptr_to_env = getenv (*path + 1)) == NULL)
				{
					free  ( dup_path );
					svclog('I',  "%sUser = %s , Authentication : Environment variable %s not set. Sys Error Msg: %s System Errno: %d",
						DEBUG_PREFIX,NOTNUL(getenv("USER")), NOTNUL((*path + 1)),NOTNUL(strerror(errno)),errno);
					return BASE_ENV_VAR_ERROR;
				}
				strcpy (dup_path , "");
			}

			if (*path)
			{
				free (*path);
			}

			if (( *path = calloc ( 1, strlen (dup_path) + strlen(ptr_to_env) + 5)) == NULL )
			{
				fatal("%sallocate (Auth) parse_path",DEBUG_PREFIX);
			}

			strcpy ( *path ,ptr_to_env);

			if (*dup_path)
			{
#ifdef MACHINE_WNT
				strcat ( *path , "\\");
#else
				strcat ( *path , "/");
#endif
				strcat ( *path , dup_path);
			}
			break;

		default:

#ifdef MACHINE_WNT
			/*
				In Windows this is a special case because the drive is
				included to the path. ie . c:\kjkkj ans so on
			*/
			if (p = strstr (dup_path , ":" )) {
				break;
			}
#endif

			/* Use HOME variable this is a relative path*/
			if ((ptr_to_home = getenv (USR_ENV_HOME)) == NULL)
			{
				svclog('I', "%sUser = %s , Authentication : Environment variable %s not set. Sys Error Msg: %s System Errno: %d",
					DEBUG_PREFIX,NOTNUL(getenv("USER")), USR_ENV_HOME,NOTNUL(strerror(errno)),errno);
				free  ( dup_path );
				return ENV_VAR_ERROR;
			}
			if (*path)
			{
				free (*path);
			}

			if ((*path = calloc (1, strlen (dup_path) + strlen(ptr_to_home) + 2)) == NULL)
			{
				fatal("%sallocate (Auth) parse_path",DEBUG_PREFIX);
			}
			strcpy ( *path ,ptr_to_home);
#ifdef MACHINE_WNT
			strcat ( *path , "\\");
#else
			strcat ( *path , "/");
#endif
			strcat ( *path , dup_path);
			break;
	}

	free  ( dup_path );

	if (DEBUG(6))
	{
		svclog ('I' , "%sparse_name path = %s",DEBUG_PREFIX,NOTNUL(*path));
		debug("%sparse_name %s\n",DEBUG_PREFIX, NOTNUL(*path));
	}

	CutOfSpaces(*path);
	return USER_ACCEPTED;
}

int CheckAuthd (Client *conn, char **block,char **field_block,char **value, char *Path)
{
	/*
		At this stage auth block is OK
		next check if authd utility is needed to use
	*/

	int nRet;
	int n;
	struct sockaddr_in server_addr, client_addr;
	char *REMOTE_LOGNAME=NULL;


	/*
		Find <UseAuthd-block first
	*/

	switch (nRet = GetFieldNew ( START_OF_BLOCK , END_OF_BLOCK , *block, USEAUTHD_BLOCK, field_block ))
	{
		/* Success , <UseAuthd-block found */
		case BLOCK_FOUND:
			GetValue ( START_OF_BLOCK , END_OF_BLOCK , *field_block , USEAUTHD_BLOCK , value );

			if (strlen(*value) != 0)
			{
				if (strcmp(*value ,USEAUTHD_BLOCK_ON) == 0)
				{
					n = sizeof(server_addr);
					/* Get information from the Authd server */
					if (getsockname((SOCKET)conn->net, (struct sockaddr *) &server_addr, &n))
					{
						svclog('I',  "%sUser = %s , Authentication : Authd : getsockname failed. Sys Error Msg: %s System Errno: %d",
							DEBUG_PREFIX,NOTNUL(getenv("USER")),NOTNUL(strerror(errno)),errno);
						return AUTH_SYSTEM_ERROR;
					}

					n = sizeof(client_addr);
					if (getpeername((SOCKET)conn->net, (struct sockaddr *) &client_addr, &n))
					{
						svclog('I',  "%sUser = %s , Authentication : Authd : getpeername failed. Sys Error Msg: %s System Errno: %d",
							DEBUG_PREFIX,NOTNUL(getenv("USER")),NOTNUL(strerror(errno)),errno);
						return AUTH_SYSTEM_ERROR;
					}

					if (!(nRet = call_identd(&REMOTE_LOGNAME, &client_addr, ntohs(client_addr.sin_port), ntohs(server_addr.sin_port))))
					{
						if (strcmp(REMOTE_LOGNAME , conn->userid ) == 0)
						{
							free(REMOTE_LOGNAME);
							return USER_ACCEPTED;
						}
						else
						{
							svclog('I',  "%sUser = %s , Authentication : Authd failed : mismatch with usernames.",
								DEBUG_PREFIX,NOTNUL(getenv("USER")));
							return AUTHD_MISMATCH_USER;
						}
					}
					else
					{
						return AUTH_SYSTEM_ERROR;
					}
				}
				else
				{
					LogError (Path , START_OF_BLOCK , USEAUTHD_BLOCK, INV_VAL_IN_AUTHD_FIELD);
					return INV_VAL_IN_AUTHD_FIELD;
				}
			}
			else
			{
				return USER_ACCEPTED;
			}

			/* <UseAuthd Syntax error --> reject user */
			case NOT_PAIR_FOUND:
				LogError (Path , START_OF_BLOCK , USEAUTHD_BLOCK , NOT_PAIR_FOUND);
				return NOT_PAIR_FOUND;
				break;

			/* <UseAuthd is not given at all --> no restrictions set*/
			case NOT_FIELD_FOUND: 
				return USER_ACCEPTED;
				break;
				
			default: 
				return INTERNAL_ERROR;
				break;
	}
}

int error_string(Client *conn, int nRet)
{
	switch (nRet)
	{
		case AUTH_SYSTEM_ERROR:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, AUTH_SYSTEM_ERROR_S ,_End);
			break;
		case ENV_VAR_ERROR:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, ENV_VAR_ERROR_S ,_End);
			break;
		case BASE_ENV_VAR_ERROR:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, BASE_ENV_VAR_ERROR_S ,_End);
			break;
		case RLSHOSTS_FILE_READ_ERROR:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, RLSHOSTS_FILE_READ_ERROR_S ,_End);
			break;
		case NOT_PAIR_FOUND:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, NOT_PAIR_FOUND_S ,_End);
			break;
		case NOT_FIELD_FOUND:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, NOT_FIELD_FOUND_S ,_End);
			break;
		case INV_FLAG_IN_FILE_FIELD:
		case INV_FLAG_IN_RUN_FIELD:
		case INV_FLAG_IN_BASE_FIELD:
		case INV_VAL_IN_AUTHD_FIELD:
		case INV_PATH_IN_BASE_FIELD:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, INV_FLAG_IN_FIELD_S ,_End);
			break;
		case SIGNON_PACKET_SYNTAX_ERROR:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, SIGNON_PACKET_SYNTAX_ERROR_S ,_End);
			break;
		case GEN_AUTH_ERROR:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, GEN_AUTH_ERROR_S ,_End);
			break;
		case INTERNAL_ERROR:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, INTERNAL_ERROR_S ,_End);
			break;
		case AUTHD_MISMATCH_USER:
			vreply(conn, HELLO, nRet, _Int, RLS_PROTOCOL_VERSION, _String, AUTHD_MISMATCH_USER_S ,_End);
			break;
		default:
			return 0;
			break;
	}

	return 1;
}

int nswrite(int fd, char *buf, int nleft)
{
	int n;
	int nwritten = 0;

	while (nleft)
	{
		n = send(fd, buf, nleft, 0);
#ifdef MACHINE_WNT
		if ( n == SOCKET_ERROR )
			svclog('I', "%sNSWRITE : send failed err=%d\n",DEBUG_PREFIX, WSAGetLastError());
#endif
		if (n < 0 && errno != EINTR)
		{
			return (-1);
		}
		if (n == 0)
		{
			break;
		}
		if (n > 0)
		{
			buf      += n;
			nwritten += n;
			nleft    -= n;
		}
	}
	return (nwritten);
}

int nsread(int fd, char *buf, int nleft)
{
	int n;
	int nred = 0;

	while (nleft)
	{
		n = recv(fd, buf, nleft, 0);
#ifdef MACHINE_WNT
		if ( n == SOCKET_ERROR )
			svclog('I', "%sNSREAD: recv failed err=%d\n",DEBUG_PREFIX, WSAGetLastError()); 
#endif
		if (n < 0 && errno != EINTR)
		{
			return (-1);
		}
		if (n == 0)
		{
			break;
		}
		if (n > 0)
		{
			buf   += n;
			nred  += n;
			nleft -= n;
		}
	}
	return (nred);
}

int call_identd( char **namep, struct sockaddr_in *remote, int remote_port, int local_port) /* ports in host byteorder */
{
	int s, n;
	struct servent *sp;
	char *cp;
	struct sockaddr_in identd;
	char buf[1024];

	if ((sp = getservbyname("auth", "tcp")) == NULL)
	{
		svclog('I', "%sWarning : getservbyname for authd failed, standard port 113 will be used",DEBUG_PREFIX);
	}

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		svclog('I', "%sUser = %s , Authentication : Authd : socket failed. Sys Error Msg: %s System Errno: %d",
			DEBUG_PREFIX,NOTNUL(getenv("USER")),NOTNUL(strerror(errno)),errno);
		return (1);
	}

	memset(&identd, 0, sizeof(identd));
	identd.sin_family = AF_INET;
	identd.sin_addr   = remote->sin_addr;
	identd.sin_port   = (sp ? sp->s_port : 113);

	if (connect(s, (struct sockaddr *) &identd, sizeof(identd)))
	{
		sclose(s);
		svclog('I', "%sUser = %s , Authentication : Authd : connect to : %s port : %d failed. Sys Error Msg: %s System Errno: %d",
		       DEBUG_PREFIX,NOTNUL(getenv("USER")), 
			   NOTNUL(( char *) inet_ntoa(remote->sin_addr)), 
			   (sp ? sp->s_port : 113) ,
			   NOTNUL(strerror(errno)),
			   errno);
		return (1);
	}
	sprintf(buf, "%d, %d\r\n", remote_port, local_port);

	nswrite(s, buf, strlen(buf));
	shutdown(s, 1);
	n = nsread(s, buf, sizeof(buf) - 1);
	sclose(s);

	if (n <= 0)
	{
		svclog('I', "%sUser = %s , Authentication : Authd : socket read failed. Sys Error Msg: %s System Errno: %d",
			DEBUG_PREFIX,NOTNUL(getenv("USER")),NOTNUL(strerror(errno)),errno);
		return (1);
	}
	cp = buf + n;
	while (--cp >= buf && isspace(*cp))
	{
		*cp = 0;
	}

	cp = buf;
	while (*cp && *cp != ':')
	{
		cp++;
	}
	while (*cp && *cp != ':')
	{
		cp++;
	}
	while (*cp && *cp != ':')
	{
		cp++;
	}
	while (*cp && isspace(*cp))
	{
		++cp;
	}

	if (*cp == 0)
	{
		return (2);
	}

	*namep = strdup(cp);
	return 0;
}

int freet(char **sp)
{
	if (*sp) {
		free(*sp);
		*sp = NULL;
	}
    return 0;
}

int sclose(int s)
{
#ifdef MACHINE_WNT
	shutdown((SOCKET) s, 2 );
	return (closesocket(s));
#else
	shutdown( s, SHUT_RDWR );
	return (close(s));
#endif
}

#include "conf/neededByWhat.c"
