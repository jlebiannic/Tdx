/*======================================================================
  Create a LocalSystem-OwnProcess-service.

  First arg is the tag (name) for the service,
  Second is the commandline of the service-process.
======================================================================*/
 
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <errno.h>

#include "conf/local_config.h"
MODULE("@(#)SVCTOOL $Id: svctool.c 54591 2018-09-14 14:10:20Z cdemory $ $Name:  $")

/* Version Control -----------------------------------------------------
	1.00 (JN) First version
	1.01 (VA) Added support for installing service with user account
	1.02 (VA) We no longer show the password
	09/01/2007 - 1.03 (CRE) We just need to use STANDARD_RIGHTS_EXECUTE for start or stop the service
    5.01/5.02 12.06.14/SCH(CG) TX-2372 : Trick the Linux compiler to avoid optimization on __tx_version unused variable 
                               SONAR violations Correction 	
-- Enf of Version Control --------------------------------------------*/

#define NOTNUL(a) a?a:""

static void errx(char *what)
{
	DWORD err;
	char msg[1024];

	err = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ARGUMENT_ARRAY,
		NULL,
		err,
		LANG_NEUTRAL,
		msg,
		sizeof(msg),
		NULL);

	fprintf(stderr, "%s: Error %d: %s", NOTNUL(what), err, NOTNUL(msg));

	exit(1);
}

static void install(char *tag, char *cmd)
{
	SC_HANDLE scmh, sch;

	scmh = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scmh) {
		errx("OpenSCManager");
	}

	sch = CreateService(scmh, tag, tag,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		cmd,
		NULL,
		NULL,
		NULL,
		NULL, /* LocalSystem */
		NULL); /* which has no password */

	if (!sch) {
		errx("CreateService");
	}

	CloseServiceHandle(scmh);
	CloseServiceHandle(sch);
}

/* 1.01 */
static void install_as_user(char *tag, char *cmd,char * user,char * passwd)
{
	SC_HANDLE scmh, sch;
	char	username[64]=""; 

	if (!strchr(user,'\\')) {
		strcpy(username,".\\"); /*specify local user if domain not defined*/
	}
	strcat(username,user);

	scmh = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scmh) {
		errx("OpenSCManager");
	}

	sch = CreateService(scmh, tag, tag,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS,
		SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL,
		cmd,
		NULL,
		NULL,
		NULL,
		username,
		passwd); 

	if (!sch) {
		errx("CreateService");
	}

	CloseServiceHandle(scmh);
	CloseServiceHandle(sch);
}


static void start(char *tag)
{
	SC_HANDLE scmh, sch;

	scmh = OpenSCManager(NULL, NULL, STANDARD_RIGHTS_EXECUTE);
	if (!scmh) {
		errx("OpenSCManager");
	}

	sch = OpenService(scmh, tag, SERVICE_START);

	if (!sch) {
		errx("OpenService");
	}

	if (!StartService(sch, 0, NULL)) {
		errx("StartService");
	}

	CloseServiceHandle(scmh);
	CloseServiceHandle(sch);
}

void stop(char *tag)
{
	SC_HANDLE scmh, sch;
	SERVICE_STATUS status;

	scmh = OpenSCManager(NULL, NULL, STANDARD_RIGHTS_EXECUTE);
	if (!scmh) {
		errx("OpenSCManager");
	}

	sch = OpenService(scmh, tag, SERVICE_STOP);

	if (!sch) {
		errx("OpenService");
	}

	if (!ControlService(sch, SERVICE_CONTROL_STOP, &status)) {
		errx("StopService");
	}

	CloseServiceHandle(scmh);
	CloseServiceHandle(sch);
}

void delete(char *tag)
{
	SC_HANDLE scmh, sch;

	scmh = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (!scmh) {
		errx("OpenSCManager");
	}

	sch = OpenService(scmh, tag, SERVICE_ALL_ACCESS);

	if (!sch) {
		errx("OpenService");
	}

	if (!DeleteService(sch)) {
		errx("DeleteService");
	}

	CloseServiceHandle(scmh);
	CloseServiceHandle(sch);
}

int main(int argc, char **argv)
{
	char **op;
	char **args;
	int n;

	op = argv + 1;
	args = op + 1;
	n = argc - 2;

	/* Support for installing service with user account (1.01) */
	if (n == 5 && !strcmp(*op, "install") && !strcmp(args[0], "-u"))
	{
		/* 1.02 */
		printf("Installing as %s\n",args[1]);
		install_as_user(args[3], args[4],args[1],args[2]);
		exit(0);
	}

	if (n == 2 && !strcmp(*op, "install"))
	{
		install(args[0], args[1]);
		exit(0);
	}
	if (n == 1 && !strcmp(*op, "start"))
	{
		start(args[0]);
		exit(0);
	}
	if (n == 1 && !strcmp(*op, "stop"))
	{
		stop(args[0]);
		exit(0);
	}
	if (n == 1 && !strcmp(*op, "delete"))
	{
		delete(args[0]);
		exit(0);
	}
usage:
	fprintf(stderr, "\
Usage: svctool install [-u name password] tag cmdline\n\
               start tag\n\
               stop tag\n\
               delete tag\n\
");
	exit(2);
}

#include "conf/neededByWhat.c"
