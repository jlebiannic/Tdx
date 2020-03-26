/*========================================================================
TradeXpressVersion Copyright (c) 1990-2014 Generix Group - All rights reserved.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: monitor.c 54591 2018-09-14 14:10:20Z cdemory $") 
/*========================================================================
  1.4 30.05.2008/CRE         Change debug_prefix
  5.01/5.02 12.06.14/SCH(CG) TX-2372 : Trick the Linux compiler to avoid optimization on __tx_version unused variable
                             SONAR violations Correction
========================================================================*/
#define _HPUX_SOURCE  1
#define _POSIX_SOURCE 1

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include <syslog.h>

#include "logsysrem.h"

#define DEBUG_PREFIX	" [monitor] "

int getresp(int fd, struct respheader *resp)
{
	int x;
	int n = sizeof(*resp);
	char *cp = (char *) resp;

	while (n > 0)
	{
		x = read(fd, cp, n);
		if (x <= 0) {
			return (0);
		}
		cp += x;
		n -= x;
	}

	resp->type   = ntohl(resp->type);
	resp->size   = ntohl(resp->size);
	resp->serial = ntohl(resp->serial);
	resp->status = ntohl(resp->status);

	n = resp->size - sizeof(*resp);

	while (n > 0)
	{
		x = read(fd, cp, n);
		if (x <= 0) {
			return (0);
		}
		cp += x;
		n -= x;
	}
	return (1);
}

int main(int argc, char **argv)
{
	char buf[4096];
	int lc, x, n;
	struct reqheader *req = (struct reqheader *) buf;
	struct respheader *reply = (struct respheader *) buf;
	int *ip = REQ_DATA(reply);

	lc = logsys_connect(argv[1]);

	req->type   = htonl(BASE);
	req->size   = htonl(sizeof(*req) + strlen(argv[2]) + 1);
	req->serial = htonl(0);
	strcpy(REQ_DATA(req), argv[2]);

	write(lc, req, sizeof(*req) + strlen(argv[2]) + 1);

	getresp(lc, reply);

	req->type   = htonl(MONITOR);
	req->size   = htonl(sizeof(*req));
	req->serial = htonl(0);

	write(lc, req, sizeof(*req));

	while (getresp(lc, reply))
	{
		ip[0] = ntohl(ip[0]);
		ip[1] = ntohl(ip[1]);
		debug("%s%d %d\n",DEBUG_PREFIX, ip[0], ip[1]);
	}
	return 0;
}

#include "conf/neededByWhat.c"