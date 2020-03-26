/*========================================================================
        T R A D E X P R E S S

        File:		logsystem/util/biglogview.c
        Author:		Luc Meynier (LME)
        Date:		Tue Jan 05 19:40:50 EET 2009

        Copyright (c) 2009 Generix group
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: biglogview.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  1.00	 05.01.2009/LM	creation
  1.10	 08.01.2009/LM	testing menu
  1.11	 08.01.2009/LM	Unix adaptations
  TX-3123 - 10.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "tr_prototypes.h"

#ifdef MACHINE_WNT
#define PSEP '\\'
#else
#define PSEP '/'
#endif
#define FIRST 	0
#define LAST	1
#define NEXT	2
#define PREV	3
#define NOT_FOUND (u_long) -1

extern char *optarg;

long filesize;
int debuglevel;
u_long prevFound = NOT_FOUND ;
char *filter=NULL;
char out_buffer[64000]="";

u_long getPageFromFile( FILE *fp, int linesPerPage, int pageLocation, u_long previousPos, u_long *pPageBottom);
u_long search_page_pos(FILE *fp, u_long top, u_long bottom, int nblines, u_long previous, int direction, u_long *pPageBottom);
int readlines(FILE *fp, int nbLines, int startOfLine, u_long *pfoundPos, u_long *pfoundBottom, u_long previous, int direction);
int printPage(FILE *fp, u_long pagePos, u_long pageBottom);
int get_Next_Command(int *linesPerPage, int *pageLocation, u_long *previousPos, u_long pagePos, u_long pageBottom);
void debug(int level, char * fmt, ... );
void printExpressionFound(FILE *fp, char *expression, u_long expPos);
u_long search(FILE *fp, u_long top, u_long bottom, char *expression);
u_long searchExpression(FILE *fp, u_long top, u_long previousPos, char *expression);

main(int argc, char **argv)
{
      extern int  optind, opterr, optopt;
      FILE *fp;
      u_long pagePos = 0;
      u_long pageBottom = 0;
      u_long previousPos = (u_long)-1;
      u_long expPos = (u_long)-1;
      int pageLocation, c, linesPerPage, result;
      int testmode = 0;
      int maxsearch = 4000;
      char *cmd, *filename;
      char *expression=NULL;
/*      char *filter=NULL;*/
      char input;
#ifdef MACHINE_WNT
      struct _stat buf;
#else	
      struct stat buf;
#endif
      
      cmd = argv[0];
      opterr = 0;

      tr_UseLocaleUTF8();

      while ((c = getopt(argc, argv, "?:f:p:d:l:D:T:r:m:F:")) != -1)
      {
	    switch (c)
	    {
	    default: fprintf(stderr, "%s: Invalid option '%c'.\n", cmd, optopt);
	    case '?': 
		  fprintf(stderr, "\n%s [options]\n",cmd);
		  fprintf(stderr, "\n\
Valid options are:\n\
	-f filename		name of the logfile \n\
	-d direction		direction :\n\
				  FIRST 0\n\
				  LAST	1\n\
				  NEXT	2\n\
				  PREV	3\n\
	-l lines			lines per page\n\
	-p position		position in file where to start searching :\n\
				    - unused with FIRST and LAST\n\
				    - must be the last top position with PREV\n\
				    - must be the last bottom position with NEXT\n\
	-m maxsearch		\n\
	-r expression		expression to search from position forward if direction=NEXT\n\
				or backward if direction=PREV, search scope could be reduced to\n\
				maxsearch octets\n\
	-F filter		expression used to filter output lines\n\
	-T 1|0			test mode\n\
	-D debuglevel		0..8 \n\
");
				exit (2);
	    case 'f': filename = optarg; break;
	    case 'd': pageLocation = atoi(optarg); break; 
	    case 'l': linesPerPage = atoi(optarg); break; 
	    case 'p': previousPos = strtoul(optarg, (char **)NULL, 10);break; 
	    case 'D': debuglevel = atoi(optarg); break; 
	    case 'T': testmode	= atoi(optarg);break;
	    case 'm': maxsearch = atoi(optarg); break; 
	    case 'r': expression = optarg; break;
	    case 'F': filter = optarg; break;
	    }
      }
    
      debug(2,"DEBUG	Arguments :\n\
			file name =%s\n\
			direction =%d\n\
			lines     =%d\n\
			previous  =%ld\n\
			expression=%s\n\
			maxsearch =%d\n\
			filter	  =%s\n\
			debug     =%d\n",\
	      filename,pageLocation,linesPerPage,previousPos,expression?expression:"empty",maxsearch,filter,debuglevel); 
    
      switch (pageLocation) {
      case LAST:
      case FIRST:
      case NEXT:
      case PREV : 
	    break;
      default :
	    fprintf(stderr, "Unknown page location : %d\n", pageLocation);
      }

      if ( ( pageLocation == NEXT || pageLocation == PREV ) && ( previousPos == (u_long)-1)) {
	    fprintf(stderr, "Can't find previous or next page without valid position \n");
	    return -1;	
      }
#ifdef MACHINE_WNT
      result = _stat( filename, &buf );
#else
      result = stat( filename, &buf );    
#endif
      if (result != 0) {
	    fprintf(stderr, "Error while getting file %s stats ; error No %d\n", filename, errno);
	    return -1;      
      }
      filesize = buf.st_size;
    
      debug(2,"DEBUG	filesize : %ld\n",filesize); 

      do
      {
	    /* we must open in binary else ftell doesn't work with UNIX text file on windows !!! */
	    if (!(fp = fopen (filename, "rb"))) {
		  fprintf(stderr, "Error while opening file %s : error No %d\n", filename, errno);
		  return -1;
	    } 
	  
	    if (expression)
	    {
		    if (pageLocation == NEXT)
			  expPos = search(fp, previousPos, previousPos + maxsearch, expression);
		    else
			  expPos = searchExpression(fp, maxsearch ? ( previousPos >= maxsearch ? previousPos - maxsearch : 0) : 0, previousPos, expression);
		    
		    if ( expPos != NOT_FOUND )
		    {
			    printExpressionFound(fp, expression, expPos);
			    
		    }else{
			    fprintf(stderr, "expression not found\n\n");
		    }
	    }else{		    
		    pagePos = getPageFromFile( fp, linesPerPage, pageLocation, previousPos, &pageBottom);
		    
		    if ( pagePos == NOT_FOUND ) 
			  if (testmode)
				fprintf(stdout, "page not found\n\n", filename, errno);
			  else
				fprintf(stderr, "page not found\n", filename, errno);
			    
		    else 
			  printPage(fp, pagePos, pageBottom);
		    
		    fclose(fp);
	    }
	    if (testmode)
		  input = (char) get_Next_Command(&linesPerPage, &pageLocation, &previousPos, pagePos, pageBottom);
      }
      while (testmode && (input != 'q') && (input != 'r') );
	
      exit(0);
}

u_long
getPageFromFile( FILE *fp, int linesPerPage, int pageLocation, u_long previousPos, u_long *pPageBottom)
{
      u_long top, bottom, pagePos;
      int found = 0;

      top = bottom = pagePos = 0;
      debug(3,"DEBUG	getPageFromFile : begin\n"); 
      
      switch (pageLocation) {
      case LAST : 
	    pagePos = search_page_pos(fp, 0, filesize - 1, linesPerPage, previousPos, pageLocation, pPageBottom);
	    break;
      case FIRST :
	    fseek(fp, 0, SEEK_SET);
	    found = readlines(fp, linesPerPage, 1, &pagePos, pPageBottom, previousPos, pageLocation);
	    if (found != 0) pagePos = NOT_FOUND;
	    break;
      case NEXT :
	    pagePos = previousPos;
	    fseek(fp, pagePos, SEEK_SET);
	    found = readlines(fp, linesPerPage, 1, &pagePos, pPageBottom, previousPos, pageLocation);
	    if (found != 0) pagePos = NOT_FOUND;
	    break;
      case PREV :
	    pagePos = search_page_pos(fp, 0, previousPos, linesPerPage, previousPos, pageLocation, pPageBottom);
	    break;
      }

	debug(2,"DEBUG	getPageFromFile : pagepos=%ld, found=%d\n", pagePos, found); 
	debug(3,"DEBUG	getPageFromFile : end\n"); 
    
      return pagePos;
}
/*
 */
u_long
search_page_pos(FILE *fp, u_long top, u_long bottom, int nblines, u_long previous, int direction, u_long *pPageBottom)
{
	u_long midpos,foundpos;
	int found;

	debug(3,"DEBUG	search_page_pos : begin\n"); 
	debug(3,"DEBUG	search_page_pos : top=%ld, bottom=%ld, nblines=%d \n", top, bottom, nblines); 

	if (fp == NULL || nblines == 0 || top >= bottom)
		return ( NOT_FOUND );

	midpos = top + ((bottom - top) >> 1 );
	fseek(fp, midpos, SEEK_SET);
	
	foundpos = midpos;
	found = readlines(fp, nblines, midpos == 0, &foundpos, pPageBottom, previous, direction );
	
	debug(2,"DEBUG	search_page_pos : readlines return %d, foundpos=%ld, midpos=%ld\n", found, foundpos, midpos); 
	
	switch (found) {
	case 0 : return (foundpos);
	case 1 : return ( search_page_pos(fp, midpos, bottom, nblines, previous, direction, pPageBottom ) );
	case -1: return ( search_page_pos(fp, top, midpos, nblines, previous, direction, pPageBottom  ) );	
	default: return ( NOT_FOUND );
	}
}

int
readlines(FILE *fp, int nbLines, int startOfLine, u_long *pfoundPos, u_long *pfoundBottom, u_long previous, int direction)
{
      u_long startpos;
      int noLines, bEOF, bEOL, bCR, pagefound;
      int bStart = startOfLine;
      int c, prevc = 0;
      char s[4096] = "";
      char *ps = s;

      startpos = bStart ? *pfoundPos : 0;
      noLines = bEOF = bEOL = bCR = pagefound = 0;
      out_buffer[0] = 0;

      debug(3,"DEBUG	readlines : begin \n");
      debug(3,"DEBUG	readlines : nbLines=%d, startOfLine=%d, foundPos=%ld, startpos=%ld, bStart=%d, noLines=%d \n"\
	    , nbLines, startOfLine, *pfoundPos, startpos, bStart, noLines);

      do
      {
	      switch (c = getc(fp)) {
	      case EOF:
		      bEOF = 1;
		      /* if no linebreak before EOF take care of last line */
		      if (prevc != 10) 
			      noLines++;
		      debug(3,"DEBUG	readlines : EOF\n", startpos);
		      break;
	      case 10:
		      if (bStart)
		      {	    
			      if ( (!filter)||( s && ( tr_wildmat(s, filter) ) ) )
			      {
				      noLines++;
				      strcat(out_buffer, s);
				      strcat(out_buffer, "\n");
				      debug(10,"Line Found:%s\n", s);
			      }
		      }
		      else{
			    startpos = ftell(fp);
			    out_buffer[0] = 0;
			    debug(2,"DEBUG	readlines : startpos found at %ld\n", startpos);
		      }
		      bStart = 1;
		      bCR = 0;
		      s[0] = 0;
		      ps = s;
		      break;
	      case 13:
		      bCR = 1;
		      break;
	      default:
		      if (bCR) {
			      debug(3,"DEBUG	readlines : default with bCR true macintosh format ? \n");

			      if (bStart)  
				    noLines++;
			      else
				    startpos = ftell(fp);
			      
			      bStart = 1;
			      bCR = 0;
		      }
		      *ps++ = c;
		      *ps = 0;
		      break;
	      }
	      (*pfoundPos)++;
	      if (noLines == nbLines)
	      {
		      if (!pagefound)
			      *pfoundBottom = ftell(fp);
		      pagefound = 1;
	      }
	    
	      if (pagefound)
	      {
		      if ((direction == FIRST)||(direction == NEXT))
			      break;
	      }
	      prevc = c;
      }
      while(!bEOF && (noLines <= nbLines) && ! ((direction == PREV)&&(*pfoundPos >= previous)) );
      
      
      
      debug(3,"DEBUG	readlines : noLines=%d, startpos=%ld, direction=%d, previous=%ld, bottom=%ld \n",\
		noLines, startpos, direction, previous,*pfoundBottom);
      debug(3,"DEBUG	readlines : end \n");
      
      if (noLines == nbLines)
      {		
	      *pfoundPos = startpos;
	      return (0);
      }
      if (noLines > nbLines)
	      return (1);
      if (noLines < nbLines)
	      return (-1);
}

u_long
search(FILE *fp, u_long top, u_long bottom, char *expression)
{
      int c, i;
      char *s;
      char searched[4096]="*";
      u_long foundpos = NOT_FOUND;
      u_long curpos;
      int match = 0;
      char line[4096];

      fseek(fp, top, SEEK_SET);
      curpos = top;
      
      if (expression[0] == '*')
	    strcpy(searched, expression);
      else
	    strcpy(searched+1, expression);

      strcat(searched, "*");
      
      debug(3,"DEBUG	search begin\n");
      debug(3,"DEBUG	search : top=%ld, bottom=%ld, expression=%s, searched=%s\n", top, bottom, expression, searched); 

      do
      {
	      if (s = fgets (line, 4096, fp))
	      {
		      if (tr_wildmat(s, searched))
			      return(curpos);
	      }
	    
	      curpos = ftell(fp);
	    
	      debug(6,"DEBUG	searched in \"%s\" failed \n", s); 

      }
      while( ( s ) && (curpos <= bottom) );
	
      return ( NOT_FOUND );
}

u_long
searchExpression(FILE *fp, u_long top, u_long previousPos, char *expression)
{
      u_long startPos, found, newTop;
       
      
      debug(3,"DEBUG	searchExpression : begin\n");
      debug(3,"DEBUG	searchExpression : top=%ld, previousPos=%ld, expression=%s\n", top, previousPos, expression); 

      startPos = previousPos;
      newTop = ( (startPos - top) >> 1) + top;

      found = search(fp, newTop, startPos, expression);

      if ( found != NOT_FOUND )
      {
	    prevFound = found;
	    if ( (found == top) && (startPos == previousPos) )
	    {
		    return (prevFound);
	    }else{
		    debug(3,"DEBUG	searchExpression : exp found at %ld search for next\n", prevFound); 
		    return ( searchExpression(fp, found, startPos, expression) ); 
	    }
      }else{
	    debug(3,"DEBUG	searchExpression : exp not found\n", prevFound); 
	    if ( prevFound != NOT_FOUND )
	    {
	    	    debug(3,"DEBUG	searchExpression : prev search succeed at %ld return it\n", prevFound); 
		    return(prevFound);
	    }else{
		  debug(3,"DEBUG	searchExpression : no search succeed , retry higher \n", prevFound);
		  startPos = ( (startPos - top) >> 1) + top ;
		  if ( startPos < top + strlen(expression) )
			return ( ( prevFound != NOT_FOUND ) ? prevFound : NOT_FOUND );
		  else
			return ( searchExpression(fp, top, startPos, expression) );
	    }
      }
      
      return ( NOT_FOUND );
}

int
printPage(FILE *fp, u_long pagePos, u_long pageBottom)
{

      printf("Position page : top=%ld, bottom=%ld\n", pagePos, pageBottom);

      printf("%s", out_buffer);

      return(0);
}

void
printExpressionFound(FILE *fp,char *expression, u_long expPos)
{
      int noLine = 0;
      char line[4096];
      char *s;
      
      printf("\nExpression \"%s\" trouvee a la position %ld\n\n", expression, expPos);
      
      fseek(fp, expPos, SEEK_SET);
      do {
	    if (s = fgets (line, 4096, fp))
	    {
		  printf("%s", s);
		  noLine++;
	    }
      }
      while ((noLine < 3) && s);
      return;
}

int
get_Next_Command(int *linesPerPage, int *pageLocation, u_long *previousPos, u_long pagePos, u_long pageBottom)
{
      char c ;

      fflush(stdin);
      printf("Menu : f=first, l=last, n=next, p=previous,q=quit\n");
      printf("choix : ");
      scanf("%c", &c);

      switch ((char)c){
      case 'f': *pageLocation = FIRST;break;
      case 'l': *pageLocation = LAST;break;
      case 'n': *pageLocation = NEXT;*previousPos = pageBottom;break;
      case 'p': *pageLocation = PREV;*previousPos = pagePos;break;
      default : break;
      }
	      
      return ((int) c);
}

void 
debug(int level, char * fmt, ... )
{
      va_list ap;

      if (level > debuglevel) return;

      va_start(ap, fmt);
      fprintf(stderr, "DEBUG : ");
      vfprintf(stderr, fmt, ap);
      va_end(ap);
      
      return;
}
