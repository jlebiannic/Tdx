#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: perhost.c 54591 2018-09-14 14:10:20Z cdemory $ $Name:  $")
/*===========================================================================
    History

 4.00	23.12.1998/HT	Started history. Passwd -handling changed.
 4.01	26.01.99/KP	Log incoming requests in debug mode (with timestamp)
			Use setlogin() in DEC only (wassit anyway?)
 4.02	18.05.99/HT	SIGHUP handling.
 4.03	07.07.99/HT	Avoiding hang-ups
 4.04	05.08.99/JR	Linux signal handling and program termination
 			coded much better and cleaner way.
 4.05   23.08.04/ST     Write PID in /var/run/perhost.pid so that
                        ediadm may stop the perhost.
 4.06   31.08.04/CD	Added the parameter -p to setup the listening port 
                        /var/run/perhost.port.pid will be created
			go into error if an unknown option is given in the 
			command line
 4.06   31.08.04/CD	creates a /tmp/errors.port.logd file so we can have
                        more than one perhost, with a different logfile for
			each
 4.07   19.05.05/CD	move the creation of the perhost pid file after
			the fork so we retrieve the correct pid
 4.08   14.03.08/AVD 	add environment variable RLS_LOGDIR to perhost. user can
                     	use it to config where create and save the file 'errors.logs'
 4.09 30.05.2008/CRE Change DEBUG_PREFIX
 4.10 30.07.2008/CRE Error on malloc : incorrect use of sizeof instead of strlen
 5.01/5.02 12.06.14/SCH(CG) TX-2372 : Trick the Linux compiler to avoid optimization on __tx_version unused variable 
                            SONAR violations Correction

 5.03	21.07.2014/TCE(CG) TX-2563 : allow to disable window event log write via rls.cfg 

 5.04   18.05.2017/TCE(CG) EI-325 ; correct some memory leaks	
===========================================================================*/
/* files leak to peruser ! */

#ifdef MACHINE_WNT
#include <windows.h>
/* #include <assert.h>*/
#include <lm.h>
#include <Aclapi.h>
/* #include <sddl.h>              for ConvertSidToStringSid function  */
#define _POSIX_
#else
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <syslog.h>
#include <fcntl.h>
#include <grp.h>
#include <unistd.h>
extern char **environ;
#include <pwd.h>
#endif

#ifndef PATH_MAX
#include <limits.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#include "rlsfileformat.h"
#include "logsysrem.h"
#include "perr.h"

#include "misc/config.h"

#ifdef MACHINE_LINUX
#include <setjmp.h>
static jmp_buf jump_point;
#endif

/* Atenzioooooone Itaaaalian statsioooone sileeenzioooo */
#define SILENCIO (void *)

#define DEBUG_PREFIX	" [perhost] "

char *EVENT_NAME = "logd";

char logging_file[1024];
extern int debugged;

#ifdef MACHINE_WNT
int EVENT_CATEGORY = 0;

char *xlogging = "\\\\.\\PIPE\\errors.logd";
char *logging = "\\tmp\\errors.logd";

#define PIDTYPE HANDLE
#else
char *logging = "/tmp/errors.logd";

#define PIDTYPE pid_t
#endif

static void remove_child(PIDTYPE pid);

static int signaled_to_exit = 0;

static char *peruser = "peruser";
static char *user_args[64];

static char *exec_path;        /* argv[0] saved */

struct usermap {
	struct usermap *next;
	int port;
	PIDTYPE pid;
	char name[1];         /* extends */
};

static struct usermap *usermap, *reuse;

static int reqsocket;

static char *
timestamp(void)
{
        static char buf[10] = "";
        time_t  curtime;
        struct tm *p_tm;

        time(&curtime);
        p_tm = localtime(&curtime);
        sprintf(buf, "%02d:%02d:%02d",
                p_tm->tm_hour,
                p_tm->tm_min,
                p_tm->tm_sec);

        return buf;
}

static struct usermap *
lookup(char *name, char *username) {
	char *sysuser=NULL;
	char *log_user;
	struct usermap *p;
	int s, alen, on = 1;
	PIDTYPE pid;
	struct sockaddr_in sin;
	int nRet;
	struct passwd *pw;
#ifdef MACHINE_WNT
    LPUSER_INFO_0 pBufInfo_0 = NULL;
    LPUSER_INFO_3 pBufInfo_3 = NULL;
    LPUSER_INFO_0 pTmpBufInfo_0 = NULL;
	NET_API_STATUS nStatus;
#else
	int port, maxport;
#endif
	if (debugged) {
		debug("%sEntering lookup\n", DEBUG_PREFIX);
	}
	
	s = -1;
	log_user = malloc(strlen(name) + strlen(username) + 2);
	/* TODO - Test retour du sprintf */
	sprintf(log_user, "%s@%s", username, name);

	/* This could lift a found entry to top... */
#ifdef MACHINE_WNT
again:
#endif
	for (p = usermap; p; p = p->next) {
		if (!strcmp(p->name, log_user)) {
#ifdef MACHINE_WNT
			{
				DWORD x;

				x = WaitForSingleObject(p->pid, 0);
				if (x == WAIT_OBJECT_0) {
					remove_child(p->pid);
					goto again;
				}
			}
#endif
			if (debugged) {
				debug("%s%s old\n", DEBUG_PREFIX, NOTNUL(p->name));
			}
			return (p);
		}
	}
#ifndef MACHINE_WNT

	if ((pw = getpwnam(name)) == NULL) {
		if (debugged){
			debug("%sERROR : system user [%s] not found\n", DEBUG_PREFIX, NOTNUL(name));
		}
		return (NULL); /* No such user. Fail silently. */
	}
	
	if (debugged) {
		debug("%sFOUND user [%s] \n", DEBUG_PREFIX, NOTNUL(pw->pw_name));
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_family      = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	if ((s = socket(sin.sin_family, SOCK_STREAM, 0)) == -1)
	{
		warn("%ssocket: %s", DEBUG_PREFIX, NOTNUL(syserr()));
		return (NULL);
	}

	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, SILENCIO &on, sizeof(on)) == -1) {
		warn("%ssetsockopt(TCP_NODELAY): %s", DEBUG_PREFIX, NOTNUL(syserr()));
	}

	if (getenv("RLS_PORT_MIN")) {
		sscanf(getenv("RLS_PORT_MIN"), "%d", &port);
	} else {
		port = LD_DEFPORT + 1;
	}

	if (getenv("RLS_PORT_MAX")) {
		sscanf(getenv("RLS_PORT_MAX"), "%d", &maxport);
	} else {
		maxport = -1;
	}
	do
	{
		if (maxport > 0 && port > maxport)
		{
			warn("%sbind: exceed maximum port number : %s", DEBUG_PREFIX, NOTNUL(syserr()));
			close(s);
			return (NULL);
		} 
		sin.sin_port = htons(port++);
	}
	while (bind(s, SILENCIO &sin, sizeof(sin)));
	
	if (listen(s, 10))
	{
		warn("%slisten: %s", DEBUG_PREFIX, NOTNUL(syserr()));
		close(s);
		return (NULL);
	}

	alen = sizeof(sin);

	if (getsockname(s, SILENCIO &sin, (socklen_t *) &alen))
	{
		warn("%sgetsockname: %s", DEBUG_PREFIX, NOTNUL(syserr()));
		close(s);
		return (NULL);
	}
#endif
	if ((p = reuse)) {
		reuse = p->next;
	} else if ((p = malloc(sizeof(*p) + strlen(log_user))) == NULL)
	{
		warn("%smalloc: %s", DEBUG_PREFIX, NOTNUL(syserr()));
		if (s>=0) { close(s); }
		return (NULL);
	}
#ifdef MACHINE_WNT
	{
		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		SECURITY_ATTRIBUTES sa;
		HANDLE prd, pwr;
		DWORD n;
		DWORD dwEntriesRead = 0;
		DWORD dwTotalEntries = 0;
		DWORD dwResumeHandle = 0;
		BOOL bRtnBool = TRUE;
		
		PSID pSidOwner2;
		LPTSTR AcctName = "", DomainName = "";
		DWORD dwAcctName = 1, dwDomainName = 1;
		/* Dummy initial value, just use the defined one but unknown */
		SID_NAME_USE eUse = SidTypeUnknown;
		PSECURITY_DESCRIPTOR pSD;
		
		int i;
		char buf[1024];
		char buf2[1024];
		int gid;
		wchar_t UnicodeBuf[1024];

		static char homebuf[1024];
		static char namebuf[512];  /* into env */
		static char sysnamebuf[512];
		
		DWORD dwSize=0;
		SID_NAME_USE SidType;
		char* pSidOwner = (char * ) malloc (sizeof(PSID));
		char* pSidGroup = (char * ) malloc (sizeof(PSID));
		
		/* Allocate memory for the SID structure. */
		pSidOwner = (PSID)GlobalAlloc(GMEM_FIXED, sizeof(PSID));

		/* Allocate memory for the security descriptor structure. */
		pSD = (PSECURITY_DESCRIPTOR)GlobalAlloc(GMEM_FIXED, sizeof(PSECURITY_DESCRIPTOR));

 		if ((pSidOwner == NULL) || (pSidGroup == NULL)) {
			warn("%s*** Error on malloc", DEBUG_PREFIX);
		}

		sprintf(namebuf, "USERNAME=%.500s", name);
		putenv(namebuf);

		sprintf(homebuf, "HOME=%.500s\\users\\%.500s", NOTNUL(getenv("EDIHOME")), NOTNUL(name));
		putenv(homebuf);
		
			/* Recherche du sysuser */
		nRet = GetSysUser(name, username, &sysuser);
		if (nRet != BLOCK_FOUND) {
			/* sysuser field not found : get the user */
			debug("%ssystem user field not found. Use %s (GetSysUser(%s,%s) return %d)", 
			      DEBUG_PREFIX, NOTNUL(name), 
				  NOTNUL(username), 
				  NOTNUL(name), 
				  nRet);
			sysuser = name;
			MultiByteToWideChar(CP_ACP, 0, name, -1, UnicodeBuf, 1024);
			nStatus = NetUserGetInfo(NULL, UnicodeBuf, 0, (LPBYTE *) &pBufInfo_0);
		} else {
			log("%ssystem user field found. Use %s", DEBUG_PREFIX, NOTNUL(sysuser));
			MultiByteToWideChar(CP_ACP, 0, sysuser, -1, UnicodeBuf, 1024);
			nStatus = NetUserGetInfo(NULL, UnicodeBuf, 0, (LPBYTE *) &pBufInfo_0);
		}
		if (nStatus == NERR_Success) {
			log("%ssystem user found.", DEBUG_PREFIX);
			/* Free the allocated buffer. */
			if (pBufInfo_0 != NULL)  {
				NetApiBufferFree(pBufInfo_0);
				pBufInfo_0 = NULL;
			}
		} else {
			warn("%ssystem user not found. (return %d)", DEBUG_PREFIX, nStatus);
			/* Free the allocated buffer. */
			if (pBufInfo_0 != NULL)  {
				NetApiBufferFree(pBufInfo_0);
				pBufInfo_0 = NULL;
			}
			switch (nStatus) {
				case ERROR_ACCESS_DENIED :
					warn("%sAccess is denied : you do not have the right to look of the User existance", DEBUG_PREFIX);
					break;
				case ERROR_BAD_NETPATH :
					warn("%sERROR_BAD_NETPATH", DEBUG_PREFIX);
					break;
				case ERROR_INVALID_LEVEL :
					warn("%sERROR_INVALID_LEVEL", DEBUG_PREFIX);
					break;
				case NERR_InvalidComputer :
					warn("%sNERR_InvalidComputer", DEBUG_PREFIX);
					break;
				case NERR_UserNotFound :
					warn("%sThe defined user (%s) was not found on the computer", DEBUG_PREFIX, NOTNUL(sysuser));
					break;
				default :
					warn("%sError ID (%d) is unknown ", DEBUG_PREFIX, nStatus);
					break;
			}
			warn("%sTrying to list the known users.", DEBUG_PREFIX);
			nStatus = NetUserEnum(NULL, 0, 0, (LPBYTE *) &pBufInfo_0, MAX_PREFERRED_LENGTH, &dwEntriesRead, &dwTotalEntries, &dwResumeHandle);
			if ((nStatus == NERR_Success) || (nStatus == ERROR_MORE_DATA) ){
				warn("%sList of system user found.", DEBUG_PREFIX);
			} else {
				warn("%sList of system user not found. (return %d)", DEBUG_PREFIX, nStatus);
			}
			 if ((pTmpBufInfo_0 = pBufInfo_0) != NULL) {
				/* Loop through the entries. */
				for (i = 0; (i < dwEntriesRead); i++) {
				   if (pTmpBufInfo_0 == NULL) {
					  fprintf(stderr, "An access violation has occurred\n");
					  break;
				   }
					/* Print the name of the user account. */
					WideCharToMultiByte(CP_ACP, 0, pTmpBufInfo_0->usri0_name, -1, buf, 1024, NULL, NULL);
					warn("%s*** Local user found : %s", DEBUG_PREFIX, NOTNUL(buf));

					pTmpBufInfo_0++;
				}
			} else {
				warn("%sList of system user is empty !", DEBUG_PREFIX);
			}
	     	/* Free the allocated buffer. */
			if (pBufInfo_0 != NULL)  {
				NetApiBufferFree(pBufInfo_0);
				pBufInfo_0 = NULL;
			}
		}
	
		if (!SetCurrentDirectory(getenv("HOME"))) {
			warn("%s%s: %s", DEBUG_PREFIX, NOTNUL(getenv("HOME")), NOTNUL(syserr()));
			free(p);
			return (NULL);
		}

		sa.nLength = sizeof(sa);
		sa.lpSecurityDescriptor = NULL;
		sa.bInheritHandle = TRUE;

		if (!CreatePipe(&prd, &pwr, &sa, 0)) {
			warn("%sCreatePipe : %s", DEBUG_PREFIX, NOTNUL(syserr()));
			free(p);
			return (NULL);
		}

		memset(&si, 0, sizeof(si));
		si.cb = sizeof(si);
		si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
		si.hStdOutput = pwr;
		si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
		si.dwFlags = STARTF_USESTDHANDLES;

		buf[0] = 0;
		for (i = 0; user_args[i]; ++i)
		{
			strcat(buf, user_args[i]);
			strcat(buf, " ");
		}

		strcat(buf, name);

		/* TODO : use of CreateProcessAsUser() function (http://msdn.microsoft.com/en-us/library/ms682429.aspx) */
		if (!CreateProcess(NULL, buf, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
			warn("%s%s: %s", DEBUG_PREFIX, NOTNUL(buf), NOTNUL(syserr()));
			free(p);
			CloseHandle(prd);
			CloseHandle(pwr);
			return (NULL);
		}

		CloseHandle(pwr);

		if (debugged) {
			debug("%sGoing to read port from peruser\n", DEBUG_PREFIX);
		}

		if (!ReadFile(prd, buf, 5, &n, NULL))
		{
			warn("%sReadFile: %s", DEBUG_PREFIX, NOTNUL(syserr()));
			free(p);
			CloseHandle(prd);
			return (NULL);
		}

		CloseHandle(prd);

		buf[5] = 0;

		if (debugged) {
			debug("%sGot '%s' from peruser\n", DEBUG_PREFIX, NOTNUL(buf));
		}

		sin.sin_port = htons((u_short) atoi(buf));

		CloseHandle(pi.hThread);
		pid = pi.hProcess;
		nStatus = NetUserGetInfo(NULL, UnicodeBuf, 3, (LPBYTE *) &pBufInfo_3);

		if ((nStatus == NERR_Success) && (pBufInfo_3 != NULL)  && (pid != 0)) {
			/* set the rights */
			WideCharToMultiByte(CP_ACP, 0, pBufInfo_3->usri3_name, -1, buf, 1024, NULL, NULL);
			debug("%sUser used for setting rights of process : %s (group : %d)/n", DEBUG_PREFIX, NOTNUL(buf), pBufInfo_3->usri3_primary_group_id);
			nRet = GetSecurityInfo(pid, 
			                       SE_KERNEL_OBJECT, 
								   (OWNER_SECURITY_INFORMATION + GROUP_SECURITY_INFORMATION), 
								   (PSID *)(pSidOwner), 
								   (PSID *)(pSidGroup), 
								   NULL, 
								   NULL, 
								   NULL);
			if (nRet != ERROR_SUCCESS) {
				warn("%sError on GetSecurityInfo : %d", DEBUG_PREFIX, GetLastError());
			}
			
			nRet = GetSecurityInfo(
				pid,                                       /* Handle to the file */
				SE_KERNEL_OBJECT,             /* Directory or file */
				OWNER_SECURITY_INFORMATION, /* Owner information of the (file) object */
				&pSidOwner2,                        /* Pointer to the owner of the (file) object */
				NULL,
				NULL,
				NULL,
				&pSD);                                  /* Pointer to the security descriptor of the (process) object */
 

			/* Check GetLastError for GetSecurityInfo error condition. */
			if (nRet != ERROR_SUCCESS) {
				warn("%sError on GetSecurityInfo (2) : %d", DEBUG_PREFIX, GetLastError());
			} else {
				/* First call to LookupAccountSid() to get the buffer sizes AcctName. */
				bRtnBool = LookupAccountSid(
					NULL,                      /* Local computer */
					pSidOwner2,            /* Pointer to the SID to lookup for */
					AcctName,             /* The account name of the SID (pSIDOwner) */
					(LPDWORD)&dwAcctName, /* Size of the AcctName in TCHAR */
					DomainName,      /* Pointer to the name of the Domain where the account name was found */
					(LPDWORD)&dwDomainName,  /* Size of the DomainName in TCHAR */
					&eUse);                /* Value of the SID_NAME_USE enum type that specify the SID type */

				/* Reallocate memory for the buffers. */
				AcctName = (char *)GlobalAlloc(GMEM_FIXED, dwAcctName);

				/* Check GetLastError() for GlobalAlloc() error condition. */
				if(AcctName == NULL) {
					warn("%sGlobalAlloc() error = %d", DEBUG_PREFIX, GetLastError());
				} else {
					/* Second call to LookupAccountSid() to get the account name. */
					bRtnBool = LookupAccountSid(
						NULL,                    /* name of local or remote computer */
						pSidOwner2,              /* security identifier, SID */
						AcctName,                /* account name buffer */
						(LPDWORD)&dwAcctName,    /* size of account name buffer */
						DomainName,              /* domain name */
						(LPDWORD)&dwDomainName,  /* size of domain name buffer */
						&eUse);                  /* SID type */

					/* Check GetLastError() for LookupAccountSid() error condition. */
					if(bRtnBool == FALSE) {
						DWORD dwErrorCode = 0;
						dwErrorCode = GetLastError();

						if(dwErrorCode == ERROR_NONE_MAPPED) {
							warn("%sAccount owner not found for specified SID.", DEBUG_PREFIX);
						} else {
							warn("%sError in LookupAccountSid() : %d.", DEBUG_PREFIX, dwErrorCode);
						}
					}  else {
							debug("%sActual Process User Name = %s.", DEBUG_PREFIX, NOTNUL(AcctName));
					}
				}
			}

			
			dwSize = 1024;
/*        if( !LookupAccountSid(NULL, pSidOwner, NULL, &dwSize , NULL, &dwSize, &SidType) ) {
			warn("%s*** DEBUG *** Error on LookupAccountSid : %d", DEBUG_PREFIX, GetLastError());
        } else {
			warn("%s *** DEBUG *** GetSecurityInfo(pid=%d, type=%d, rights=%d, Owner=%s, Group=%d, NULL, NULL)", 
			     DEBUG_PREFIX, pid, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION + GROUP_SECURITY_INFORMATION , 
				 NOTNUL(pSidOwner), pSidGroup);
		}
*/			

/*			WideCharToMultiByte(CP_ACP, 0, , -1, buf2, 1024, NULL, NULL); */
/*			warn("%s *** DEBUG *** GetSecurityInfo(pid=%d, type=%d, rights=%d, Owner=%s, Group=%d, NULL, NULL)", 
                 DEBUG_PREFIX, pid, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION + GROUP_SECURITY_INFORMATION , 
				 NOTNUL(buf2), pSidGroup);
			warn("%s *** DEBUG *** SetSecurityInfo(%d, %d, %d, %s, %d, NULL, NULL)", DEBUG_PREFIX, pid, SE_KERNEL_OBJECT, 
			     OWNER_SECURITY_INFORMATION , NOTNUL(buf), pBufInfo_3->usri3_primary_group_id); */
			/* SE_UNKNOWN_OBJECT_TYPE = 0 */
			/*nRet = SetSecurityInfo(pid, SE_UNKNOWN_OBJECT_TYPE, (OWNER_SECURITY_INFORMATION + GROUP_SECURITY_INFORMATION), 
			                         pBufInfo_3->usri3_user_id, pBufInfo_3->usri3_primary_group_id, NULL, NULL); */
/*			nRet = SetSecurityInfo(pid, SE_KERNEL_OBJECT, (OWNER_SECURITY_INFORMATION), buf, NULL, NULL, NULL);
			if (nRet == ERROR_SUCCESS) {
				warn("%sRights setted for process successfully.", DEBUG_PREFIX);
			} else {
				warn("%s*** Error on setting rights : %d", DEBUG_PREFIX, nRet);
			} */
		} else {
			warn("%s*** Error on getting user info (nStatus= %d)", DEBUG_PREFIX, nStatus);
		}
		
	}
#else
	if ((pid = fork()) == -1)
	{
		warn("%sfork: %s", DEBUG_PREFIX, NOTNUL(syserr()));
		close(s);
		free(p);
		return (NULL);
	}

	if (pid == 0)
	{
		endpwent();
		closelog();
		dup2(s, 0);
		dup2(s, 1);
		if (s > 1) {
			close(s);
		}

		if (reqsocket > 1) {
			close(reqsocket);
		}

		{
			static char env[2048];
			int i;
			char *ep = env;
			sigset_t msk;

			sigemptyset(&msk);
			sigprocmask(SIGCHLD, &msk, NULL);

			chdir(pw->pw_dir);

			sprintf(ep, "HOME=%s", NOTNUL(pw->pw_dir));
			putenv(ep);
			while (*ep++){
				;
			}

			ep++;

			sprintf(ep, "USER=%s", NOTNUL(pw->pw_name));
			putenv(ep);
			while (*ep++){
				;
			}

			if (debugged) {
				debug("%sSetting variables : HOME=%s; USER=%s \n", DEBUG_PREFIX, NOTNUL(pw->pw_dir), NOTNUL(pw->pw_name));
			}
			/* Recherche du sysuser */
			nRet = GetSysUser(name, username, &sysuser);
			if (nRet != BLOCK_FOUND) {
				/* sysuser field not found : get the user */
				debug("%ssystem user field not found. Use %s (GetSysUser() return %d)", DEBUG_PREFIX, NOTNUL(name), nRet);
				/* Jira EI-325 : add a byte for end of string char */
				sysuser = malloc(sizeof(name) +1);
				strcpy(sysuser, name);
			} else {
				if ((pw = getpwnam(sysuser)) == NULL) {
					/* No such system user. Use ediuser instead. */
					pw = getpwnam(name);
					warn("%ssystem user %s not found .Use %s instead", DEBUG_PREFIX, NOTNUL(sysuser), NOTNUL(name));
				}
			}
			if (debugged) {
				debug("%sSysUser for %s : [%s] \n", DEBUG_PREFIX, NOTNUL(username), NOTNUL(pw->pw_name));
			}
			initgroups(pw->pw_name, pw->pw_gid);
			setgid(pw->pw_gid);
			setuid(pw->pw_uid);

			for (i = 0; user_args[i]; ++i){
				;
			}

			user_args[i++] = log_user;
			user_args[i] = NULL;

			execve(peruser, user_args, environ);
			if (exec_path)
			{
				char *cp = strrchr(exec_path, '/');
				if (cp)
				{
					strcpy(cp + 1, "peruser");
					execv(exec_path, user_args);
				}
			}
		}
		fatal("%s%s: %s", DEBUG_PREFIX, NOTNUL(peruser), NOTNUL(syserr()));
	}
#endif
	if (s >= 0) { close(s); }
	strcpy(p->name, log_user);
	p->port = ntohs(sin.sin_port);
	p->next = usermap;
	p->pid  = pid;
	usermap = p;

	if (debugged) {
		debug("%s%s new @ %d\n", DEBUG_PREFIX, NOTNUL(p->name), p->port);
	}

	return (p);
}

static void remove_child(PIDTYPE pid)
{
	struct usermap *p, **pp;

	for (pp = &usermap; (p = *pp); pp = &p->next)
	{
		if (p->pid == pid)
		{
			*pp = p->next;
			p->next = reuse;
			reuse = p;
			if (debugged) {
				debug("%s%s dead\n", DEBUG_PREFIX,NOTNUL(p->name));
			}

			break;
		}
	}
#ifdef MACHINE_WNT
	CloseHandle(pid);
#endif
}

#ifndef MACHINE_WNT
/*
 *  Remember that syslog() might fork also, if syslogd is not running...
 *  Excess childs, that is.
 */
static void reap(int sig)
{
	int save = errno;
	int status = 0;
	pid_t pid;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		remove_child(pid);
	}

	errno = save;
}

static void timber(int sig)
{
	struct usermap *p, *q;

	if (debugged) {
		debug("%sShutting down logd services...\n", DEBUG_PREFIX);
	}

	/* use SIGTERM */
	p = usermap;
	while( p )
	{
		q = p->next;
		kill( p->pid, SIGTERM );
		p = q;
	}

	/* wait for childs to be killed */
	sleep(5);
	signaled_to_exit = 1;

#ifdef MACHINE_LINUX
	/* 4.04/JR 2.8.99
	 * interrupt from select() messes up timeval
	 * we jump right where we want to continue
	 */
	longjmp(jump_point, 1);
#endif
}

#endif

#ifdef MACHINE_WNT
static void WINAPI svc_main(DWORD argc, LPTSTR *argv);
static void real_main();

static SERVICE_STATUS        svc_status;
static SERVICE_STATUS_HANDLE svc_handle;

static HANDLE main_thread;

static int    real_argc;
static char **real_argv;

static char *svc_name = "logd";

void killall()
{
	struct usermap *p;
	int nkilled = 0;

	TerminateThread(main_thread, 0);

	for (p = usermap; p; p = p->next)
	{
		if (p->pid)
		{
			TerminateProcess(p->pid, 0);
			nkilled++;
		}
	}

	if (nkilled) {
		svclog('I', "%s%d logd slaves terminated", DEBUG_PREFIX,nkilled);
	}

}

/* Extra level of kinkyness here. */
int main(int argc, char **argv)
{
	SERVICE_TABLE_ENTRY tbl[2];

	real_argc = argc;
	real_argv = argv;

	if (argc > 1 && !strcmp(argv[1], "-d"))
	{
		debugged = 1;

		real_argv++;
		real_argc--;
		real_argv[0] = real_argv[-1]; /* lift argv[0] */

		real_main();

		exit(69);
	}

	tbl[0].lpServiceName = svc_name;
	tbl[0].lpServiceProc = svc_main;

	tbl[1].lpServiceName = NULL;
	tbl[1].lpServiceProc = NULL;

	if (!StartServiceCtrlDispatcher(tbl)) {
		fatal("%sStartServiceCtrlDispatcher: %s", DEBUG_PREFIX, NOTNUL(syserr()));
	}
}

static void svc_report(int currently)
{
	if (!svc_handle) {
		return;
	}
	
	svc_status.dwCurrentState  = currently;
	svc_status.dwServiceType  = SERVICE_WIN32_OWN_PROCESS;
	svc_status.dwWin32ExitCode  = NO_ERROR;
	svc_status.dwWaitHint  = 15000;
	svc_status.dwCheckPoint  = 0;
	svc_status.dwServiceSpecificExitCode  = 0;
	svc_status.dwControlsAccepted
		= SERVICE_ACCEPT_STOP
		| SERVICE_ACCEPT_SHUTDOWN
		| SERVICE_ACCEPT_PAUSE_CONTINUE;

	if (!SetServiceStatus(svc_handle, &svc_status)) {
		fatal("%sSetServiceStatus: %s", DEBUG_PREFIX, NOTNUL(syserr()));
	}
}

static void WINAPI svc_controller(DWORD code)
{
	switch (code) {
		case SERVICE_CONTROL_INTERROGATE: svc_report(svc_status.dwCurrentState); break;
		case SERVICE_CONTROL_PAUSE:
			SuspendThread(main_thread);
			svc_report(SERVICE_PAUSED);
			break;
		case SERVICE_CONTROL_CONTINUE:
			ResumeThread(main_thread);
			svc_report(SERVICE_RUNNING);
			break;
		case SERVICE_CONTROL_STOP:
			svc_report(SERVICE_STOP_PENDING);
			killall();
			break;
		default: 
			svc_report(svc_status.dwCurrentState); 
			break;
	}
}

static void WINAPI svc_main(DWORD argc, LPTSTR *argv)
{
	DWORD id;

	svc_handle = RegisterServiceCtrlHandler(svc_name, svc_controller);
	if (!svc_handle) {
		fatal("%sRegisterServiceCtrlHandler: %s", DEBUG_PREFIX, NOTNUL(syserr()));
	}
	
	svc_report(SERVICE_START_PENDING);
	main_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) real_main, NULL, 0, &id);
	svc_report(SERVICE_RUNNING);
	WaitForSingleObject(main_thread, INFINITE);
	svc_report(SERVICE_STOPPED);
}

static void real_main()
{
	int    argc = real_argc;
	char **argv = real_argv;
#else
int main(int argc, char **argv)
{
#endif
	u_short port = LD_DEFPORT;
	int optind = 1;
	int n, alen;
	struct usermap *p;
	struct sockaddr_in us, them;
	struct whereis req;
#ifndef MACHINE_WNT
	struct sigaction act;
	sigset_t msk;
	char perhost_pid_file[128];
	FILE* fPidFile = NULL;
#endif
	char *logdir;
	char rlscfg[PATH_MAX] = "";
	

	exec_path = strdup(argv[0]);
	while (optind < argc)
	{
		if (!strcmp(argv[optind], "-d"))
		{
			debugged = 1;
			optind++;
	 	  	continue;
		}
 
		if (!strcmp(argv[optind], "-p"))
		{
			/* 4.06 set the listening port */
			optind++;
			port = atoi(argv[optind]);
			optind++;
			continue;
		}

		if (argv[optind][0]=='-')
		{
			/* 4.06 unknown parameter */
			debug("%sunknown parameter: %s", DEBUG_PREFIX, NOTNUL(argv[optind]));
			exit(1);
		}

		if (optind < argc)
		{
			peruser = argv[optind++];
			break;
		}
	}

	n = 0;
	user_args[n++] = peruser;
	while (optind < argc) {
		user_args[n++] = argv[optind++];
	}
	user_args[n] = NULL;


#ifdef MACHINE_WNT
	{
		static WSADATA w;

		if (WSAStartup(MAKEWORD(1,1), &w)) {
			fatal("%sWinSock startup: %s", DEBUG_PREFIX, NOTNUL(syserr()));
		}

		SetErrorMode(SEM_FAILCRITICALERRORS |
			     SEM_NOOPENFILEERRORBOX);
	}
#endif

	memset(&us, 0, sizeof(us));
	us.sin_family      = AF_INET;
	us.sin_addr.s_addr = INADDR_ANY;
	us.sin_port        = htons(port);

	if ((reqsocket = socket(us.sin_family, SOCK_DGRAM, 0)) == -1) {
		fatal("%ssocket: %s", DEBUG_PREFIX, NOTNUL(syserr()));
	}

	if (bind(reqsocket, SILENCIO &us, sizeof(us))) {
		fatal("%sbind: %s", DEBUG_PREFIX, NOTNUL(syserr()));
	}

#ifndef MACHINE_WNT
	sigemptyset(&msk);
	sigaddset(&msk, SIGCHLD);

	memset(&msk, 0, sizeof(msk));
	act.sa_handler = reap;
	act.sa_flags   = 0;
	act.sa_mask    = msk;

	if (sigaction(SIGCHLD, &act, NULL)) {
		fatal("%ssigaction: %s", DEBUG_PREFIX, NOTNUL(syserr()));
	}

	if (!debugged)
	{
		int i;
		for (i = 0; i < 32; ++i) {
			if (i != reqsocket) {
				close(i);
			}
		}

		if (fork()) {
			exit(0);
		}

		setsid();

		if ((i = open("/dev/null", O_RDWR)) >= 0 && i < 2) {
			dup(i);
		}

		if (i == 0) {
			dup(i);
		}

#ifdef TIOCNOTTY
		if ((i = open("/dev/tty", O_RDWR)) >= 0)
		{
			ioctl(i, TIOCNOTTY, 0);
			close(i);
		}
#endif
	}
#endif

	readconf_debug = debugged;
	/* read configuration file to env */
#ifndef MACHINE_WNT
	sprintf(rlscfg,"%s/config/rls.cfg",getenv("EDIHOME"));
#else
	sprintf(rlscfg,"%s\\config\\rls.cfg",getenv("EDIHOME"));
#endif

	readConfFileToEnv(rlscfg);
	
	/* RLS_LOGDIR */
	/* 4.08 : add environment variable RLS_LOGDIR to perhost */
	logdir = getenv("RLS_LOGDIR");
#ifndef MACHINE_WNT
	if (logdir) {
		sprintf(logging_file, "%.500s/errors.%d.logd", logdir , port);
	} else {
		sprintf(logging_file, "/tmp/errors.%d.logd", port);
	}
	logging = logging_file;
#else
	if (logdir)
	{
		sprintf(logging_file, "%.500s\\errors.logd", logdir);
		logging = logging_file;
	}
	
#endif
	/* daemon pid file */
	/* 4.05 : Write the PID in /var/run to be able to stop the perhost at will */
	/* 4.07 : moved the creation of pid file after the fork */
#ifndef MACHINE_WNT
	sprintf(perhost_pid_file,"/var/run/perhost.%d.pid",port);
	fPidFile = fopen( perhost_pid_file, "w");
	if( fPidFile != NULL )
	{
		fprintf(fPidFile, "%d", getpid());
		fclose(fPidFile);
	} else {
		warn("Warning: %s not created. Check the /var/run rights or start perhost as root.\n",perhost_pid_file);
	}
#endif
	if (getenv("USE_EVENTLOG")){
		sscanf(getenv("USE_EVENTLOG"), "%d", &nolog);
	}

	svclog('I', "%slogd ready", DEBUG_PREFIX);
	
	signaled_to_exit = 0;
#ifndef MACHINE_WNT
	signal(SIGTERM, timber);
	signal(SIGINT, timber);
#endif

	for (;;)
	{
#ifndef MACHINE_WNT
		if (sigprocmask(SIG_UNBLOCK, &msk, NULL)) {
			fatal("%sUNBLOCK: %s", DEBUG_PREFIX, NOTNUL(syserr()));
		}
#endif

		for (;!signaled_to_exit;)
		{
			/* Just for fun. purify silencer. */
			memset(&them, 0, sizeof(them));
			alen = sizeof(them);

#ifdef MACHINE_LINUX
			/* 4.04/JR 5.8.99 Jump point for longjmp. If we have jumped here from signal handling, then exit(). */
			if ((setjmp(jump_point) == 1) && (signaled_to_exit)) {
				exit(0);
			}
#endif
			/* Receiving the request */
			if ((n = recvfrom(reqsocket, SILENCIO &req, sizeof(req), 0, SILENCIO &them, &alen)) != -1) {
				break;
			}

#ifndef MACHINE_WNT
			if (errno != EINTR)
#endif
			{
				warn("%srecvfrom: %s", DEBUG_PREFIX, NOTNUL(syserr()));
#ifndef MACHINE_WNT
				sleep(10);
#endif
			}
		}

		if (signaled_to_exit)
		{
			if (debugged) {
				debug("%sperhost : exiting.\n", DEBUG_PREFIX);
			}
			exit(0);
		}

		if (debugged) {
			debug("%sperhost : (%s) Requesting new client.\n", DEBUG_PREFIX, NOTNUL(timestamp()));
		}

#ifndef MACHINE_WNT
		if (sigprocmask(SIG_BLOCK, &msk, NULL)) {
			fatal("%sBLOCK: %s", DEBUG_PREFIX, NOTNUL(syserr()));
		}

#endif
		/* test to allow old jedi connection on new perhost */
		if (n == WHEREIS_SIZE_5_0_4) {
			debug("%s [logon] first request : ", DEBUG_PREFIX, DEBUG_PREFIX);
			strcpy(req.username, req.name);
		} else {
			warn("%s [logon] first request : misread : %d read while %d expected", DEBUG_PREFIX, n, WHEREIS_SIZE_5_0_4);
			continue;
		}

		req.type = ntohl(req.type);
		req.closure = ntohl(req.closure);
		if (req.closure < RLS_PROTOCOL_VERSION_NEW_SIGNON) {
			/* old request type */
			debug("old client version used.\n%s [logon] => SysUser parameter will be user with environment name instead of login name\n", DEBUG_PREFIX);
			debug("%sreq.username=%s\n", DEBUG_PREFIX, NOTNUL(req.username));
			debug("%sRLS protocole version %d < 15.\n", DEBUG_PREFIX, req.closure);

			req.size = WHEREIS_SIZE_5_0_4;
		} else {
			/* new request type */
			debug("new client version used.\n");
			debug("%sreq.username=%s, req.name=%s\n", DEBUG_PREFIX, NOTNUL(req.username), NOTNUL(req.name));
			debug("%sRLS protocole version = %d.\n", DEBUG_PREFIX, req.closure);

			req.size = ntohl(req.size);
			req.closure = ntohl(RLS_PROTOCOL_VERSION);
			if ((n = sendto(reqsocket, SILENCIO &req, sizeof(req), 0, SILENCIO &them, sizeof(them))) != sizeof(req))
			{
				if (n == -1) {
					warn("%ssendto: %s", DEBUG_PREFIX, NOTNUL(syserr()));
				} else {
					warn("%sshort write", DEBUG_PREFIX);
				}
			}

			signaled_to_exit = 0;
			for (;!signaled_to_exit;)
			{
				/* Just for fun. purify silencer. */
				memset(&them, 0, sizeof(them));
				alen = sizeof(them);

#ifdef MACHINE_LINUX
				/* 4.04/JR 5.8.99 Jump point for longjmp. If we have jumped here from signal handling, then exit(). */
				if ((setjmp(jump_point) == 1) && (signaled_to_exit)) {
					exit(0);
				}
#endif
				/* Receiving the request */
				if ((n = recvfrom(reqsocket, SILENCIO &req, sizeof(req), 0, SILENCIO &them, &alen)) != -1) {
					break;
				}

#ifndef MACHINE_WNT
				if (errno != EINTR)
#endif
				{
					warn("%srecvfrom: %s", DEBUG_PREFIX, NOTNUL(syserr()));
#ifndef MACHINE_WNT
					sleep(10);
#endif
				}
			}

			if (signaled_to_exit)
			{
				if (debugged) {
					debug("%sperhost : exiting (2).\n", DEBUG_PREFIX);
				}
				exit(0);
			}

			debug("%s [logon] second request, this is new jedi.\n", DEBUG_PREFIX);

			req.size = ntohl(req.size);
			req.type = ntohl(req.type);
			req.closure = ntohl(RLS_PROTOCOL_VERSION);
			if (req.size != sizeof(req) || req.name[sizeof(req.name) - 1])
			{
				warn("%smalformed whereis (req.type=%d); req.size=%d (need %d))", DEBUG_PREFIX, req.type, req.size, sizeof(req));
				continue;
			}

		}

		if (req.name[sizeof(req.name) - 1])
		{
			warn("%smalformed whereis req.type=%d; req.size=%d;", DEBUG_PREFIX, req.type, req.size);
			continue;
		}

		switch (req.type)
		{
			case LDT_PORTBYNAME :
				if ((p = lookup(req.name, req.username)) == NULL) {
					req.status = htonl(-1);
				} else {
					req.status = 0;
					req.port   = htonl(p->port);
				}
				break;
			case LDT_RELOADCONF :
				req.status = htonl(readConfFileToEnv(rlscfg));
				req.port = 0;
				break;
			default:
				warn("%smalformed whereis", DEBUG_PREFIX);
				continue;
				break;
		}

		if (debugged) {
			debug("%sperhost : (%s) Responding to new client.\n", DEBUG_PREFIX,NOTNUL(timestamp()));
		}

		req.closure = ntohl(RLS_PROTOCOL_VERSION);
		if ((n = sendto(reqsocket, SILENCIO &req, req.size, 0, SILENCIO &them, sizeof(them))) != sizeof(req))
		{
			if (n == -1) {
				warn("%ssendto: %s", DEBUG_PREFIX, NOTNUL(syserr()));
			} else {
				warn("%sshort write", DEBUG_PREFIX);
			}
		}
	}
}
#include "conf/neededByWhat.c"
