#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: rlsping.c 54591 2018-09-14 14:10:20Z cdemory $")
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
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../rlslib/rls.h"

#ifdef MACHINE_WNT
#undef errno
#define errno GetLastError()
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

int main(int argc, char **argv)
{
	char *base = "demo32@localhost:/ediserver3.2/users/demo32/syslog";

	if (argc > 1) {
		base = argv[1];
	}

	xrls = rls_open(base);
	if (xrls == NULL)
	{
		fprintf(stderr, "Failed to open base %s.\n", base);
		exit(1);
	}
	else
	{
		rls_close(xrls);
		fprintf(stderr, "Rls ping ok (base=%s).\n", base);
		exit(0);
	}
}

#include "conf/neededByWhat.c"
