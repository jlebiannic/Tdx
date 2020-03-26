/*==========================================================================
	V E R S I O N   C O N T R O L
==========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_codeconv.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*==========================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/JN	Created.
  3.01 05.12.96/JN	xclose to tr_xclose
  3.02 02.01.97/JN	NT pathseparators.
==========================================================================*/
#include <stdio.h>
#include <string.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"

static void clear_crlf();

static char *strsplit();
static char *strtoken();
static char *substr();

#define DEBUG 0

static int rowmax = 12;    /* montako rivia otetaan muistiin */

#define NOVALUE ((char *) -1) /* ei etsittya kenttaa vaikka avain olikin */

struct node {
	struct node *next;     /* seuraava koodikonversiosolmu */
	char *name;            /* nimi jonka kayttaja antaa kayttaessaan */
	char *file;            /* asetetaan heti ensimmaisessa avauksessa */
	int seek;              /* mihin asti file on muistissa, fseek (file) tai rivinumero (pipe) */
	int flags;
#define INCORE      0x0001 /* koko tk muistissa, ei tarvi enaa availla */
#define PIPED       0x0002 /* luemme piippua */

	int nrows;
	char *rows[1];         /* vapaanmittainen struktuuri, rowmax:n verran */
};

static struct node *tabs = NULL;

static int forgetnode();
/*
 *	Catch-all kaikenmaailman mahdollisuuksille,
 *	koodikonversion 'ioctl'
 */
int tr_codeconversionoption(int what, int arg)
{
	switch(what){
		case 1:
			/*
			**  MV	Mon Jul 27 13:08:27 EET 1992
			**
			**  Added a check for the allowed range.
			*/
			rowmax = arg > 0 ? arg : 1;           /* vaikuttaa vain seuraaviin avauksiin */
			break;
		case 2:
			return forgetnode(arg);
		default:
			return -1;
	}
	return 0;
}

/*
 *	Etsii koodikonversiotiedostoa,
 *	Eka kerralla pannaan mieleen, mika tiedosto avattiin
 */
static FILE *codecopen(struct node *node, char *mode)
{
	FILE *fp = NULL;

	if (node->name[0] == '|') {
		node->flags |= PIPED;
		return tr_popen(node->name + 1, mode);
	}
	if(node->file && (fp = tr_fopen(node->file, mode)))
		return fp;
	else {
		char path[1025], *p;

		if(node->file){
			tr_free(node->file);
			node->file = NULL;
		}
		do {
			if (
			    strchr(node->name, '/')
#ifdef MACHINE_WNT
			 || strchr(node->name, '\\')
#endif
			) {
				if(fp = tr_fopen(node->name, mode)){
					node->file = tr_strdup(node->name);
					break;
				}
			}
			if(p = getenv("CODEDIR")){
				sprintf(path, "%s/%s", p, node->name);
				if(fp = tr_fopen(path, mode)){
					node->file = tr_strdup(path);
					break;
				}
			}
			if(p = getenv("HOME")){
				sprintf(path, "%s/codes/%s", p, node->name);
				if(fp = tr_fopen(path, mode)){
					node->file = tr_strdup(path);
					break;
				}
			}
			if(p = getenv("EDIHOME")){
				sprintf(path, "%s/codes/%s", p, node->name);
				if(fp = tr_fopen(path, mode)){
					node->file = tr_strdup(path);
					break;
				}
			}
		} while(0);
#if DEBUG
		if(fp)
			printf("*** from %s\n", node->file);
#endif
	}
	return fp;
}

/*
 *	Unohdetaan annettu koodikonversiosolmu,
 *	seuraavalla kerralla se luetaan tyhjalta pohjalta.
 *	(Ei tarpeen, vain jos itse muokaamme luettavaa tk:ta ?).
 */
static int forgetnode(char *name)
{
	int i;
	struct node *over = NULL, *node = tabs;

	while(node && strcmp(node->name, name)){
		over = node;
		node = node->next;
	}
	if(!node)
		return -1;

#if DEBUG
	puts("*** removing node");
#endif

	if(over)
		over->next = node->next;
	else
		tabs = node->next;

	for(i = 0; i < node->nrows; i++)
		if(node->rows[i])
			tr_free(node->rows[i]);
	if(node->name)
		tr_free(node->name);
	if(node->file)
		tr_free(node->file);
	tr_free(node);

	return 0;
}

/*
 *	Etsitaan annettua solmua listasta.
 *	Luodaan tyhja lista, jos ei loytynyt
 */
static struct node *findcodenode(char *name)
{
	struct node *node = tabs;

	while(node && strcmp(node->name, name))
		node = node->next;

	if(!node){
#if DEBUG
		puts("*** creating new node");
#endif

		if((name = tr_strdup(name))
		&& (node = tr_zalloc(sizeof(*node) + rowmax * sizeof(char *)))){
			node->next = tabs;
			node->name = name;
			node->nrows = rowmax;
			tabs = node;
		}
	}
	return node;
}

/*
 *	Selektori stringin kentittamiseen.
 */
static char *nextcolumn(char *line, char *ifs)
{
	/*
	**  MV	Sun Sep  1 18:50:03 EET 1991
	**  Kaytettaessa oletusarvoerotinta (SPACE ja TAB) kaytetaan
	**  taalla strtoken()-rutiinia, joka hyvaksyy erottimena usean
	**  erotinmerkin yhdistelman.
	**  Mikali kayttaja on maarannyt itse erottimet, kaytetaan
	**  taalla strsplit()-rutiinia, joka palauttaa tyhjan, mikali
	**  erottimia on kaksi perakkain.
	*/
	return (ifs == NULL) ? strtoken(line, " \t") : strsplit(line, ifs);
}

/*
 *	Etsii rivilta sarakkeesta [0...n] avainta.
 *	Jos loytyy, palauttaa arvosarakkeen [0..n] (strdupped).
 *	Jos avain oli, muttei arvosaraketta, palauttaa NOVALUE:n (-1).
 *	Jos avainta ei ollut lain, palauttaa NULL (0).
 */

static char *matchline(char *line, char *key, int keycol, int valuecol, char *ifs)
{
	char *tmp, *field, *tag = NULL, *value = NULL;
	int curcol = 0;

	tmp = line = tr_strdup(line); /* copy line for strtok */

#if DEBUG
fprintf (stderr, "*** ");
#endif
	while(field = nextcolumn(line, ifs)){
#if DEBUG
fprintf (stderr, "%d:\"%s\"  ",curcol,field);
#endif
		line = NULL;
		if(curcol == keycol)
			tag = field;
		if(curcol++ == valuecol)
			value = field;
		if(tag && value)
			break;
	}
#if DEBUG
fprintf (stderr, "\n");
#endif
	/*
	**  MV	Mon Jul 27 13:20:09 EET 1992
	**  The return value has to be TR_EMPTY if nothing
	**  else.
	*/
	if(!tag || strcmp(tag, key)) {
		/*
		**  MV	Mon Dec  7 19:49:05 EET 1992
		**  No error message here, just continue to search.
		*/
		value = NULL;
	} else if(!value) {
		tr_Log (TR_WARN_NO_VALUE_IN_COLUMN, key, keycol, valuecol);
		value = TR_EMPTY;
	} else {
		/*
		**  MV	 Fri Jul 31 11:58:16 EET 1992
		**  Peel off the extra separators in the front that are
		**  possible with the default ifs.
		*/
		if (!ifs)
			value = tr_strdup(value + strspn (value, "\t "));
		else
			value = tr_strdup(value);
	}
	tr_free(tmp);
	return value;
}

/*
 *	Itse isosika
 *
 *	ensin arvoa etsitaan muistiin luetuista alkuriveista,
 *	sitten seekataan alkurivien yli ja skannataan rivi kerrallaan.
 *	alkurivien maaran kertoo static int rowmax.
 *	char *name;      abs. tai suhteellinen nimi katso. ylh.
 *	int keycol;      avainsarake, 0..n
 *	char *key;       avainkentta
 *	int valuecol;    arvosarake, 0..n 
 *	char *ifs;       erottimet, katso ylh. 
 *	int dummy;       rivinumero ohj.tiedostossa 
 *	Palauttaa:
 *		arvon	(tr_strdupped)	Ok.
 *		NOVALUE	(-1)			Ei arvosaraketta
 *		NULL	(0)				Ei avaintakaan
 */
char *tr_codeconversion(char *name, int keycol, char *key, int valuecol, char *ifs, int dummy)
{
	FILE *fp;
	struct node *node;
	char **rows, *value;

	if(!(node = findcodenode(name))) {
		tr_OutOfMemory();  /* exit on error */
		return TR_EMPTY;
	}

	/*
	**  MV	Mon Jul 27 13:14:59 EET 1992
	**
	**  In the translator program the users always thinks
	**  of one (1) being the first item in everything, thus
	**  we decrement the column values.
	*/
	keycol--;
	valuecol--;

	for(rows = node->rows; *rows && rows < node->rows + node->nrows; rows++)
		if(substr(*rows, key) && (value = matchline(*rows, key, keycol, valuecol, ifs)))
			return value;

	if(node->flags & INCORE) /* turha enaa yrittaa */
		return TR_EMPTY;

#if DEBUG
	puts("*** opening file");
#endif

	value = TR_EMPTY;

	if(fp = codecopen(node, "r")){
		char *p, buf[8096];
		int i, overflowed = 0;

		if(node->flags & PIPED)
			for(i = 0; i < node->seek; i++)
				fgets(buf, sizeof(buf), fp);
		else
			if(fseek(fp, node->seek, 0)) /* never ? */
				perror("fseek");

		while(fgets(buf, sizeof(buf), fp)){
			clear_crlf(buf);
			if(rows < node->rows + node->nrows){
				*rows++ = tr_strdup(buf);

				if(node->flags & PIPED)
					node->seek++;
				else
					node->seek = ftell(fp);
			} else
				overflowed = 1;

			if(substr(buf, key) && (value = matchline(buf, key, keycol, valuecol, ifs)))
				break;
			else
				value = TR_EMPTY;
		}
		if(!overflowed && !fgets(buf, sizeof(buf), fp)){
			node->flags |= INCORE;
#if DEBUG
			puts("*** table in core");
#endif
		}
		if(node->flags & PIPED)
			tr_pclose(fp);
		else
			tr_fclose(fp, 0);
	} else
		tr_Log (TR_WARN_NO_SUCH_CODETABLE, name);
	return value;
}

static void clear_crlf(char *s)
{
	char *p = s + strlen(s);

	while (--p >= s)
		if (*p == '\n' || *p == '\r')
			*p = 0;
		else
			break;
}

/*
 *	Sama kuin strtok
 */

static char *strtoken(char *s, char *ifs)
{
	static char *save = NULL;

	if (s)
		save = s;
	if (!save)
		return NULL;
	/* s = save; (WAS HERE, MV Mon Jul 19 11:22:36 EET 1993) */
	while (*save && strchr(ifs, *save))
		save++;
	s = save; /* (HAS BEEN MOVED HERE, MV Mon Jul 19 11:22:36 EET 1993 */
	while (*save) {
		if (strchr(ifs, *save)) {
			*save++ = 0;
			return s;
		}
		save++;
	}
	save = NULL;
	return s;
}

/*
 *	strtok:n serkku,
 *	erottimilla ":-.," stringista "eka,toka.:kuka-"
 *	hajoaa sanoihin "eka", "toka, "", "kuka", ""
 *	Siis perakkaisten separaattorien valissa on TYHJA kentta.
 */
static char *strsplit(char *s, char *ifs)
{
	static char *save = NULL;

	if (s)
		save = s;
	if (!save)
		return NULL;
	s = save;
	while (*save) {
		if (strchr(ifs, *save)) {
			*save++ = 0;
			return s;
		}
		save++;
	}
	save = NULL;
	return s;
}

/*
 *	strstr homebrew
 */
static char *substr(char *s, char *sub)
{
	int sublen = strlen(sub);
	int len    = strlen(s);

	while (len >= sublen) {
		if (*s == *sub && !memcmp(s, sub, sublen))
			return s;
		len--;
		s++;
	}
	return NULL;
}

