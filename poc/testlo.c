/*-------------------------------------------------------------------------
 * *
 * * testlo.c--
 * * test using large objects with libpq
 * *
 * * Copyright (c) 1994, Regents of the University of California
 * *
 * *
 * * IDENTIFICATION
 * * $Header: /usr/local/cvsroot/pgsql/src/man/large_objects.3,v 1.3 1996/12/11 00:27:51 momjian Exp $
 * *
 * *-------------------------------------------------------------------------
 * */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "libpq-fe.h"
#include "libpq/libpq-fs.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


#define BUFSIZE        1024

void
exit_nicely(PGconn* conn)
{
	PQfinish(conn);
	exit(1) ;
}

/*
 * * importFile -
 * * import file "in_filename" into database as large object "lobjOid"
 * *
 * */
Oid importFile(PGconn *conn, char *filename)
{
	Oid lobjId;
	int lobj_fd;
	char buf[BUFSIZE];
	int nbytes, tmp;
	int fd;

	/*
	 * * open the file to be read in
	 * */
	fd = open(filename, O_RDONLY, 0666);
	if (fd < 0) { /* error */
	   fprintf(stderr, "can't open unix file\"%s\"\n", filename);
	}

	/*
	 * * create the large object
	 * */
	lobjId = lo_creat(conn, INV_READ|INV_WRITE);
	if (lobjId == 0) {
	   fprintf(stderr, "can't create large object");
	}

	lobj_fd = lo_open(conn, lobjId, INV_WRITE);
	/*
	 * * read in from the Unix file and write to the inversion file
	 * */
	while ((nbytes = read(fd, buf, BUFSIZE)) > 0) {
	   tmp = lo_write(conn, lobj_fd, buf, nbytes);
	   if (tmp < nbytes) {
	    fprintf(stderr, "error while reading \"%s\"", filename);
	   }
	}

	(void) close(fd);
	(void) lo_close(conn, lobj_fd);

	return lobjId;
}

/*
 * * importFilef -
 * * import file "in_filename" into database as large object "lobjOid"
 * *
 * */
Oid importFilef(PGconn *conn, char *filename)
{
	Oid lobjId;
	int lobj_fd;
	char buf[BUFSIZE];
	int nbytes, tmp;
	int fd;
	FILE *file;

	/*
	 * * open the file to be read in
	 * */
	fd = open(filename, O_RDONLY, 0666);
	if (fd < 0) { /* error */
	   fprintf(stderr, "can't open unix file\"%s\"\n", filename);
	}

	/*
	 * * create the large object
	 * */
	lobjId = lo_creat(conn, INV_READ|INV_WRITE);
	if (lobjId == 0) {
	   fprintf(stderr, "can't create large object");
	}

	lobj_fd = lo_open(conn, lobjId, INV_WRITE);
        file = fdopen(lobj_fd,"w+");
	/*
	 * * read in from the Unix file and write to the inversion file
	 * */
	while ((nbytes = read(fd, buf, BUFSIZE)) > 0) {
	   //tmp = lo_write(conn, lobj_fd, buf, nbytes);
           tmp = fwrite(buf, 1, nbytes, file);
	   if (tmp < nbytes) {
	    fprintf(stderr, "error while reading \"%s\"", filename);
	   }
	}

	(void) close(fd);
	(void) lo_close(conn, lobj_fd);

	return lobjId;
}

void pickout(PGconn *conn, Oid lobjId, int start, int len)
{
	int lobj_fd;
	char* buf;
	int nbytes;
	int nread;

	lobj_fd = lo_open(conn, lobjId, INV_READ);
	if (lobj_fd < 0) {
	   fprintf(stderr,"can't open large object %d",
	       lobjId);
	}

	lo_lseek(conn, lobj_fd, start, SEEK_SET);
	buf = malloc(len+1);

	nread = 0;
	while (len - nread > 0) {
	   nbytes = lo_read(conn, lobj_fd, buf, len - nread);
	   buf[nbytes] = '\0';
	   fprintf(stderr,">>> %s", buf);
	   nread += nbytes;
	}
	fprintf(stderr,"\n");
	lo_close(conn, lobj_fd);
}

void overwrite(PGconn *conn, Oid lobjId, int start, int len)
{
	int lobj_fd;
	char* buf;
	int nbytes;
	int nwritten;
	int i;

	lobj_fd = lo_open(conn, lobjId, INV_READ);
	if (lobj_fd < 0) {
	   fprintf(stderr,"can't open large object %d",
	       lobjId);
	}

	lo_lseek(conn, lobj_fd, start, SEEK_SET);
	buf = malloc(len+1);

	for (i=0;i<len;i++)
	   buf[i] = 'X';
	buf[i] = '\0';

	nwritten = 0;
	while (len - nwritten > 0) {
	   nbytes = lo_write(conn, lobj_fd, buf + nwritten, len - nwritten);
	   nwritten += nbytes;
	}
	fprintf(stderr,"\n");
	lo_close(conn, lobj_fd);
}


void lo_append(PGconn *conn, Oid lobjId, char* buf, int len )
{
	int lobj_fd;
	int nbytes;
	int nwritten;
	int res;

	lobj_fd = lo_open(conn, lobjId, INV_WRITE);
	if (lobj_fd < 0) {
	   fprintf(stderr,"can't open large object %d",
	       lobjId);
	}

	res = lo_lseek(conn, lobj_fd, 0, SEEK_END);
	if (res>=0) { 
		nwritten = 0;
		while (len - nwritten > 0) {
		   nbytes = lo_write(conn, lobj_fd, buf + nwritten, len - nwritten);
		   fprintf(stderr,"%d bytes written, ",nbytes);
		   if (nbytes >= 0) {
			   nwritten += nbytes;
			   fprintf(stderr,"total %d bytes\n",nwritten);
		   } else {
			fprintf(stderr,"%s\n",PQerrorMessage(conn));
			sleep(1);
			res = lo_lseek(conn, lobj_fd, 0, SEEK_END);
			fprintf(stderr,"res %d\n",res);


			// exit_nicely(conn);
		   }
		}
		fprintf(stderr,"\n");
	}
	lo_close(conn, lobj_fd);
}

/*
 * * exportFile -
 * * export large object "lobjOid" to file "out_filename"
 * *
 * */
void exportFile(PGconn *conn, Oid lobjId, char *filename)
{
	int lobj_fd;
	char buf[BUFSIZE];
	int nbytes, tmp;
	int fd;

	/*
	 * * create an inversion "object"
	 * */
	lobj_fd = lo_open(conn, lobjId, INV_READ);
	if (lobj_fd < 0) {
	   fprintf(stderr,"can't open large object %d",
	       lobjId);
	}

	/*
	 * * open the file to be written to
	 * */
	fd = open(filename, O_CREAT|O_WRONLY, 0666);
	if (fd < 0) { /* error */
	   fprintf(stderr, "can't open unix file\"%s\"",
	       filename);
	}

	/*
	 * * read in from the Unix file and write to the inversion file
	 * */
	while ((nbytes = lo_read(conn, lobj_fd, buf, BUFSIZE)) > 0) {
	   tmp = write(fd, buf, nbytes);
	if (tmp < nbytes) {
	    fprintf(stderr,"error while writing \"%s\"",
		filename);
	   }
	}

	(void) lo_close(conn, lobj_fd);
	(void) close(fd);

	return;
}


int
main(int argc, char **argv)
{
	char *in_filename = NULL;
	char *out_filename = NULL;
	char *database = NULL;
	char *conninfo;
	extern char *optarg;
	extern int  optind;
	extern int  opterr;
    char linebuf[1024]; 
	int  option;

	int do_append = 0;


	Oid lobjOid = 0;
	PGconn *conn;
	PGresult *res;

    
	while ((option = getopt (argc, argv,
	"a:D:I:O:E:B:")) != -1) {
		switch (option) {
		case 'D':
			/*
			**  Import file to large object.
			*/
			database = optarg;
			break;
		case 'I':
			/*
			**  Import file to large object.
			*/
			in_filename = optarg;
			break;
		case 'O':
			/*
			**  OID for  export.
			*/
			lobjOid = (Oid) atoi(optarg);
			break;
		case 'a':
			sprintf(linebuf,"%s\n",optarg);
			if (!lobjOid){
				fprintf(stderr, "Append string needs OID\n");
				exit(1);
			} 
			do_append=1;
			break;

		case 'E':
			/*
			**  exporting file name.
			*/
			out_filename = optarg;
			if (!lobjOid){
				fprintf(stderr, "Export needs OID\n");
				exit(1);
			} 
			break;
		default:
	   		fprintf(stderr, "Usage: %s [import|export] database_name filename\n",
	    	argv[0]);
	   		exit(1);

		}
	}	
/*	
	if (argc != 4) {
	   fprintf(stderr, "Usage: %s [import|export] database_name filename\n",
	       argv[0]);
	   exit(1) ;
	}

	action = argv[1];
	database = argv[2];
	in_filename = argv[3];
	out_filename = argv[4];
*/
	/*
	 * * set up the connection
	 * */
	/* conn = PQsetdb(NULL, NULL, NULL, NULL, database); */


	conninfo = getenv("PG_CONNINFO");
	if ( ! conninfo ) {
			fprintf(stderr,"Connection to database failed: %s\n", "Please check environment variable PG_CONNINFO");
            exit(1);
	}
	/* Cree une connexion e la base de donnees */
	conn = PQconnectdb(conninfo);

	/* check to see that the backend connection was successfully made */
	if (PQstatus(conn) == CONNECTION_BAD) {
	   fprintf(stderr,"Connection to database '%s' failed.\n", database);
	   fprintf(stderr,"%s",PQerrorMessage(conn));
	   exit_nicely(conn);
	}
	   
	res = PQexec(conn, "begin");
	fprintf(stderr,"RES apres begin %d\n",PQresultStatus(res));
	PQclear(res);
	if (in_filename){
		printf("importing file \"%s\" ...\n", in_filename);
		lobjOid = importFile(conn, in_filename); 
		/*lobjOid = lo_import(conn, in_filename); */
		printf("\tas large object %d.\n", lobjOid);
	}  
	/*
	 * printf("\tas large object %d.\n", lobjOid);
	 *
	 * printf("picking out bytes 1000-2000 of the large object\n");
	 * pickout(conn, lobjOid, 1000, 1000);
	 *
	 * printf("overwriting bytes 1000-2000 of the large object with X's\n");
	 * overwrite(conn, lobjOid, 1000, 1000);
	 * */
	if (out_filename && lobjOid>0){ 
		printf("exporting large object to file \"%s\" ...\n", out_filename);
		/* exportFile(conn, lobjOid, out_filename); */
		lo_export(conn, lobjOid,out_filename);
	}
    if (do_append && lobjOid>0 ){
		printf("append string to OID \"%d\" ...\n", lobjOid);
		/* exportFile(conn, lobjOid, out_filename); */
		lo_append(conn, lobjOid,linebuf,strlen(linebuf));
	} 
	res = PQexec(conn, "end");
	PQclear(res);
	PQfinish(conn);
	exit(0);
} 
