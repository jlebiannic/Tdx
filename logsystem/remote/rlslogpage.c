#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: rlslogpage.c 54591 2018-09-14 14:10:20Z cdemory $")
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
#if defined(MACHINE_LINUX) || defined(MARCH_X86)
#include <getopt.h>
#endif
#ifdef MACHINE_WNT
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
#endif

#include "rlslib/rls.h"

#define FILE_FILTER_ACTION_FIND   1
#define FILE_FILTER_ACTION_FILTER 2

#define NOT_FOUND (off_t) -1
#define READ_SIZE 4096

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

int main(int argc, char **argv)
{
	char opt = -1;
	int x;
	char* buf1 = NULL;
	double matchpos = -1.0;
	rls *xrls;

	char *base = "user@host/port#environment#password:path/to/base";
	double offset = 0.0;
	double chunksize = (double) READ_SIZE;
	double tmpchunksize = (double) READ_SIZE;
	int direction = 1;
	char *file = NULL;
	char *search = NULL;
	size_t readsize = 0;
	int action = FILE_FILTER_ACTION_FIND;

	while ((opt = getopt(argc,argv,"hb:o:s:d:r:fF")) != -1)
	{
		switch (opt)
		{
			case 'b': base = optarg; break;
			case 'o': offset = atof(optarg); break;
			case 's': chunksize = atof(optarg); break;
			case 'd': direction = atoi(optarg); break;
			case 'f': action = FILE_FILTER_ACTION_FIND; break;
			case 'F': action = FILE_FILTER_ACTION_FILTER; break;
			case 'r': readsize = atol(optarg); break;
			case 'h': fprintf(stderr, "%s\n\
							  \n\
							  usage : rlslogpage [options] file search\n\
							  -b base\n\
							  -o offset\n\
							  -s chunksize\n\
							  -d direction\n\
							  -f : find\n\
							  -F : filter\n\
							  -r readsize\n\
							  -h : help\n\
							  ",__tx__version);
					  exit(0);
					  break;
		}
	}

	if (optind != argc - 2 ) {
		exit(1);
	}

	file = argv[optind++];
	search = argv[optind++];

	printf("base = \n%s\n", base);

	xrls = rls_open(base);
	if (xrls == NULL) {
		rls_exit(1);
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

	fflush(stdout);

	if ((x = rls_openf_withrls(xrls,0,file,"r",&xrls,PAGINATED)) > 0)
	{
		rls_seek_file(xrls,offset,0);
		rls_clear_file_filter(xrls);
		rls_add_file_filter_type(xrls,0);
		rls_add_file_filter_chunksize(xrls,chunksize);
		rls_add_file_filter_direction(xrls,direction);
		if (action == FILE_FILTER_ACTION_FIND) {
			rls_add_file_find(xrls,search);
		} else {
			rls_add_file_filter(xrls,search);
		}
		tmpchunksize = rls_compile_file_filter(xrls);
		if ((tmpchunksize > 0) && (tmpchunksize < chunksize))
		{
			fprintf(stderr,"Chunk size restricted by server to : %lf\n",tmpchunksize);
			chunksize = tmpchunksize; 
		}
		if (action == FILE_FILTER_ACTION_FIND)
		{
			matchpos = rls_get_next_file_match(xrls);
			if (matchpos != -1.0) {
				rls_seek_file(xrls,matchpos,0);
			} else {
				rls_seek_file(xrls,offset,0);
			}
		}
		buf1 = (char *) malloc(readsize + 1);
		if (buf1)
		{
			rls_read_file(xrls, (double) readsize,0,&buf1);
			if (buf1)
			{ 
				printf("result (%lf / searched %ld on %lf) :\n---\n%s",offset,(long int) readsize,chunksize,buf1);
				if (buf1[strlen(buf1)-1] != '\x0a') {
					printf("\n");
				}
				printf("---\n");
				if (action == FILE_FILTER_ACTION_FIND) {
					printf("Matched : %lf\n",matchpos);
				}
				buf1 = NULL;
			}
		}
		rls_close_file(xrls);
	}
	rls_exit(0);
	return 0;
}

#include "conf/neededByWhat.c"
