
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: rlstest.c 54591 2018-09-14 14:10:20Z cdemory $")
/*===========================================================================
    History

  5.01/5.02 12.06.14/SCH(CG) TX-2372 : Trick the Linux compiler to avoid optimization on __tx_version unused variable 
                             SONAR violations Correction 

===========================================================================*/
#ifdef MACHINE_WNT
#include <winsock.h>
#endif
#include <sys/types.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "rlslib/rls.h"

#ifdef MACHINE_WNT
#undef errno
#define errno GetLastError()
static netrd(int x, char *p, int n) { return (ReadFile((HANDLE)x, p, n, &n, NULL) ? n : -1); }
static netwr(int x, char *p, int n) { return (WriteFile((HANDLE)x, p, n, &n, NULL) ? n : -1); }
static netcl(int x) { return (CloseHandle((HANDLE)x) ? 0 : -1); }
#else
static int netrd(int x, char *p, int n) { return (read(x, p, n)); }
static int netwr(int x, char *p, int n) { return (write(x, p, n)); }
static int netcl(int x) { return (close(x)); }
#endif

void rls_message(rls *rls, int locus, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "rls error: locus %c, errno %d\n", locus, errno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n\n");
	va_end(ap);
}

rls *xrls;

#ifdef MACHINE_WNT

static DWORD tid;

DWORD monitor(LPWORD argh)
{
	int i;
	rls *x = rls_monitor(xrls);

	while (i = rls_getevent(x, NULL)) {
		printf("%d!\n", i);
	}

	printf("%d!\n", 0);
	return (1);
}
#endif

int main(int argc, char **argv)
{
	int x, i, n;
	int ftype;
	char *s, c, *cp, *arg1, *arg2, *arg3;
	union {
		int i;
		char *s;
		time_t t;
		double n;
	} z;
	char buf[80];

	char *base = "demo32@localhost:/ediserver3.2/users/demo32/syslog";

	if (argc > 1) {
		base = argv[1];
	}

	printf("base = \n%s\n", base);

	xrls = rls_open(base);
	if (xrls == NULL) {
		exit(1);
	}

	if (xrls->pu_username) {
		printf("xrls->pu_username = \n%s\n", xrls->pu_username);
	} else {
		printf("xrls->pu_username = \n%s\n", "NULL");
	}

	if (xrls->pu_password) {
		printf("xrls->pu_password = \n%s\n", xrls->pu_password);
	} else {
		printf("xrls->pu_password = \n%s\n", "NULL");
	}

	if (xrls->location) {
		printf("xrls->location = \n%s\n", xrls->location);
	} else {
		printf("xrls->location = \n%s\n", "NULL");
	}

	printf("Press enter ");
	fflush(stdout);
	getchar();

	{
		char **p;

		p = rls_getfieldnames(xrls);
		while (p && *p) {
			printf(" %s", *p++);
		}
		if (!p) {
			printf("???");
		}
		printf("\n");
	}

	if (argc < 3)
	{
		printf("    typ siz name format header\n");
		for (i = 0; ; ++i)
		{
			char *ns;
			int   typ;
			int   sz;
			char *fs;
			char *hs;
			if (rls_ask_nextfield(xrls,i,&ns,&typ,&sz,&fs,&hs))
				break;
			printf("%3d %3d %3d '%s' '%s' '%s'\n",i,typ,sz,ns,fs,hs);
		}
		printf("\n");

#ifdef MACHINE_WNT
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) monitor, NULL, 0, &tid);
#endif
	}

	while (printf("> "), fflush(stdout), fgets(buf,sizeof(buf),stdin))
	{
		cp = strtok(buf, " \t\n");
		arg1 = strtok(NULL, " \t\n");
		arg2 = strtok(NULL, " \t\n");
		arg3 = strtok(NULL, " \t\n");
		if (!cp) {
			continue;
		}

		x = 0;
		switch (*cp++)
		{
			default: break;
			case '$': printf("%s=%s\n", arg1, rls_getenv(xrls, arg1)); break;
			case 'D': rls_request(xrls, SET_DEBUG, _Int, atoi(arg1), _Continues); break;
			case 'p':
				x = rls_point(xrls, atoi(arg1));
				if (x != 0) {
					break;
				}
				n = rls_gettimevalue(xrls, "CREATED");
				printf("%s", ctime((time_t *) &n));
				n = rls_gettimevalue(xrls, "MODIFIED");
				printf("%s", ctime((time_t *) &n));
				printf("%s\n", rls_gettextvalue(xrls, "STATUS"));
				break;
			case 'q':
				ftype = rls_query(xrls, arg1, arg2, NULL);
				while ((i = rls_getnext(xrls, &z)))
				{
					switch (ftype)
					{
						case _Int:
						case _Index:  printf("%d %d\n", i, z.i); break;
						case _Double: printf("%d %lf\n", i, z.n); break;
						case _Time:   printf("%d %s", i, ctime((time_t *) &z.t)); break;
						case _String: printf("%d %s\n", i, z.s); break;
						default:      printf("%d ???\n", i); break;
					}
				}
				break;
			case 'g':
				switch (*cp++)
				{
					case 's': s = rls_gettextvalue(xrls, arg1); puts(s ? s : "(null)"); break;
					case 'n': printf("%lf\n", rls_getnumvalue(xrls, arg1)); break;
					case 't': n = rls_gettimevalue(xrls, arg1); printf("%s", ctime((time_t *) &n)); break;
				}
				break;
			case 's':
				switch (*cp++)
				{
					case 's': rls_settextvalue(xrls, arg1, arg2 ? arg2 : ""); break;
					case 'n': rls_setnumvalue(xrls, arg1, arg2 ? atof(arg2) : 0.0); break;
					case 't': rls_settimevalue(xrls, arg1, arg2 ? atoi(arg2) : 0); break;
					default: break;
				}
				break;
			case 't': s = rls_gettextvalue(xrls, arg1); puts(s ? s : "(null)"); break;
			case 'G': s = rls_gettextvalue_file(xrls, arg1, arg2); puts(s ? s : "(null)"); break;
			case 'T':
				if (*cp == 'f') {
					x = rls_settextvalue_file(xrls, arg1, arg2, arg3);
				} else {
					x = rls_settextvalue(xrls, arg1, arg2);
				}
				break;
			case 'n': x = rls_createnew(xrls); break;
			case 'd': x = rls_remove(xrls, atoi(arg1)); break;
			case 'u': x = rls_removefile(xrls, atoi(arg1), arg2); break;
			case 'b':
				{
					int mx = -1, nu = -1;
					x = rls_baseinfo(xrls, &mx, &nu, NULL, NULL);
					printf("max count %d, num used %d\n", mx, nu);
				}
				break;
			case 'm': x = rls_mkdir(xrls, arg1); break;
			case 'f':
				x = rls_openf(xrls, atoi(arg1), arg2, "r",STREAMED);
				if (x == -1)
				{
					perror("rls_openf");
					break;
				}
				while ((n = netrd(x, &c, 1)) == 1) {
					putchar(c);
				}

				if (n == -1) {
					perror("input");
				}

				netcl(x);
				break;
			case 'S':
				x = rls_open_scanner(xrls, "r",STREAMED);
				if (x == -1)
				{
					perror("rls_open_scanner");
					break;
				}
				while ((n = netrd(x, &c, 1)) == 1) {
					putchar(c);
				}

				if (n == -1) {
					perror("input");
				}

				netcl(x);
				break;
			case 'P': x = rls_poke_scanner(xrls, arg1); break;
			case 'Q': x = rls_peek_scanner(xrls); break;
			case 'F':
				x = rls_openf(xrls, atoi(arg1), arg2, "w",STREAMED);
				if (x == -1)
				{
					perror("rls_openf");
					break;
				}
				while ((n = getchar()) != EOF)
				{
					c = n;
					if (netwr(x, &c, 1) != 1)
					{
						perror("output");
						break;
					}
				}
				netcl(x);
				break;
			case 'A':
				x = rls_openf(xrls, atoi(arg1), arg2, "a",STREAMED);
				if (x == -1)
				{
					perror("rls_openf");
					break;
				}
				while ((n = getchar()) != EOF)
				{
					c = n;
					if (netwr(x, &c, 1) != 1)
					{
						perror("output");
						break;
					}
				}
				netcl(x);
				break;
			case 'x':
				{
					char *cmd[5];
					int j = 0;
					
					cmd[j++] = arg1;
					cmd[j++] = arg2;
					cmd[j++] = arg3;
					while ((cmd[j++] = strtok(NULL, " \t\n")))
						;

					x = rls_cmdexec(xrls, "r", cmd);
					if (x == -1)
					{
						perror("rls_cmdexec");
						break;
					}
					while ((n = netrd(x, &c, 1)) == 1) {
						putchar(c);
					}

					if (n == -1) {
						perror("input");
					}

					x = rls_cmdclose(xrls, x);
				}
				break;
			case 'X': rls_request(xrls, FORCE_QUIT, _Continues); break;
			case 'E':
				if (*cp == 0)
				{
					char **v;
					int j;
					if ((v = rls_enumerate(xrls, arg1)))
					{
						for (j = 0; v[j]; ++j) {
							puts(v[j]);
						}
						rls_free_stringarray(v);
					}
				}
				else if (*cp == 'E')
				{
					char **v;
					int j;
					if ((v = rls_enum_parms(xrls, atoi(arg1), arg2)))
					{
						for (j = 0; v[j]; ++j) {
							puts(v[j]);
						}

						rls_free_stringarray(v);
					}
				}
				else if (*cp == 'F')
				{
					char **v;
					int j;
					if ((v = rls_fetch_parms(xrls, atoi(arg1), arg2)))
					{
						for (j = 0; v[j]; ++j) {
							puts(v[j]);
						}

						rls_free_stringarray(v);
					}
				}
				else
				{
					char *v[5];

					v[0] = strdup("PARAM1=VALUE1");
					v[1] = strdup("PARAM2=VALUE2");
					v[2] = strdup("PARAM3=VALUE3");
					v[3] = NULL;
					rls_save_parms(xrls, atoi(arg1), arg2, v);
					rls_free_stringarray(v);
				}
				break;
			case 'c': x = rls_copyentry(xrls, atoi(arg1)); break;
			case 'w':
				{
					char **v;
					v = rls_whatfiles(xrls, atoi(arg1));
					if (v)
					{
						for (i = 0; v[i]; ++i) {
							puts(v[i]);
						}

						rls_free_stringarray(v);
					}
				}
				break;
		}
		printf("rv=%d\n", x);
		if (x == -1) {
			getchar();
		}
	}
	rls_close(xrls);
	exit(0);
}

#ifdef MACHINE_WNT
void perror(const char *locus)
{
	DWORD err, rv;
	char buf[4096];

	buf[0] = 0;
	err = GetLastError();
	rv = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ARGUMENT_ARRAY, NULL, err, LANG_NEUTRAL, buf, sizeof(buf) - 1, NULL);
	fprintf(stderr, "Error: %s: %s", locus, buf);
}
#endif

#include "conf/neededByWhat.c"
