/* TradeXpress $Id: relink_logd.c 47371 2013-10-21 13:58:37Z cdemory $ */
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define BIG 8192
#define PERUSER_NAME	"peruser.exe"

static char decls[BIG];
static char calls[BIG];
static char files[BIG];
static char libes[BIG];
static char link[BIG];
static char tmpbuf[BIG];

static char linkenv[BIG];

static char *objnames[64];

int
comp_objnames( const void *a, const void *b )
{
	char *f1 = (char *)a;
	char *f2 = (char *)b;

	return strcmp( f1, f2 );
}

main(int argc, char **argv)
{
	HANDLE dir;
	WIN32_FIND_DATA dd;
	char *cp;
	char *edihome;
	char *objfile;
	char *cc, *aout, *oslibes;
	int num_bases,
	    counter;
	FILE *fp;
	char workdir[1024]; /* edihome/lib/logd */
	char pathbuf[1024]; 
	char linebuf[1024]; 
	char tag[1024];
	char peruser[1024]; /* $EDIHOME/bin/peruser.exe */
	int  narg;

	aout = "a.exe";

	/*
	Loop through arguments, insert all name=value pairs into the environment
	*/
	for (narg = 1 ; (narg < argc) && argv[narg] ; narg++) {
		if (strchr(argv[narg], '='))
			putenv(argv[narg]);
	}

	if ((cc = getenv("CC")) == NULL)
		cc = "cl -nologo -MT";

	if ((oslibes = getenv("OSLIBEs")) == NULL)
		oslibes = "ADVAPI32.lib WSOCK32.lib USER32.lib";

	if ((edihome = getenv("EDIHOME")) == NULL) {
		fprintf(stderr, "relink_logd: EDIHOME not set.\n");
		exit(1);
	}
	sprintf(workdir, "%.200s\\lib\\logd", edihome);

	if (chdir(workdir)) {
		fprintf(stderr, "relink_logd: %s: %s\n",
			workdir, strerror(errno));
		exit(1);
	}

	sprintf(linkenv, "LIB=");

	if ((cp = getenv("LIB")) != NULL)
		sprintf(linkenv + strlen(linkenv), "%s;", cp);

	sprintf(linkenv + strlen(linkenv), "%s/lib;", edihome);

	putenv(linkenv);

	printf("\n\
Relinking logd from %s...\n\
Bases:",
		workdir);
	fflush(stdout);

	num_bases = 0;

	memset( objnames, 0x00, sizeof(objnames) );

	dir = FindFirstFile("*ops.obj", &dd);
	if (dir != INVALID_HANDLE_VALUE) do {
		objfile = dd.cFileName;

		strcpy(tag, objfile);
		tag[strlen(tag) - 4] = 0;

		if ( strlen(tag) > 1 ) {
			printf(" %s", &tag[1]);
			fflush(stdout);
	
			objnames[num_bases] = strdup( tag );
			num_bases++;
		}

	} while (FindNextFile(dir, &dd));

	qsort( (void *)objnames, num_bases, sizeof(char *), comp_objnames );

	counter = 0;

	while ( objnames[counter] ) {
		char x;

		x = objnames[counter][0];

		if ( isdigit((int) x) ) {

		   strcpy(tag, objnames[counter]+1);

		   sprintf(files + strlen(files), " %s.obj", objnames[counter]);
		   sprintf(decls + strlen(decls), " void *init_%s();", tag);
		   sprintf(calls + strlen(calls), " init_base(init_%s);", tag);

		   sprintf(pathbuf, "%s.nt.libes", tag);

		   if (fp = fopen(pathbuf, "rt")) {
			if (fgets(linebuf, sizeof(linebuf), fp)) {
				cp = linebuf + strlen(linebuf);
				while (--cp >= linebuf && isspace(*cp))
					*cp = 0;
				sprintf(libes + strlen(libes), " %s", linebuf);
			}
			fclose(fp);
		   }
		   else
			fprintf( stderr, "\nError: Can't find %s\n", pathbuf );
		}
		counter++;
	} 

	/*
	 * Expand possible environment variables in libes
	 */
	strcpy(tmpbuf, libes);
	ExpandEnvironmentStrings(tmpbuf, libes, sizeof(libes));

	printf(".\n");

	if (num_bases == 0) {
		fprintf(stderr, "*** oops: zero bases found !\n");
		exit(1);
	}

	remove(aout);

	if ((fp = fopen("_space.c", "wt")) == NULL
	 || fprintf(fp, "%s\nvoid init_bases(){ %s }\n", decls, calls) <= 0
	 || fclose(fp) != 0) {
		perror("relink_logd: _space.c");
		exit(1);
	}
	
	sprintf(link, "%s -o%s _space.c %s liblogd.lib %s libruntime.lib %s > relink_logd.err",
		cc, aout, files, libes, oslibes);

	printf("\
Loading...\n\
%s\n",
		link);
	fflush(stdout);

	if (system(link)) {
		fprintf(stderr, "Failed.\n");
		exit(1);
	}

	sprintf(peruser, "%s/bin/%s", getenv("EDIHOME"), PERUSER_NAME);
	if (CopyFile(aout, peruser, FALSE) == 0) {
		fprintf(stderr, "Copy failed.\n");
		exit(1);
	}

	printf("done.\n");
	fflush(stdout);

	exit(0);
}
