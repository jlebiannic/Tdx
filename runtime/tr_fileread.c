/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_fileread.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_fileread.c 55060 2019-07-18 08:59:51Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	Created.
  3.01 05.12.94/JN	Removed unnecessary memset().
  3.02 28.12.94/JN	Upped the buffer size.
  4.00 01.10.03/JEM	Manage TR_CARRIAGERETURN to indicate CR/LF in read line
  TX-3123 - 14.05.2019 - Olivier REMAUD - UNICODE adaptation

========================================================================*/

#include <stdio.h>
#include <string.h>
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

#define TEXTLINE_MAXLENGTH 8192

#define TR_NULLFILE	((FILE *) 0)

/*============================================================================
============================================================================*/
char *
tr_FileRead (char *filename)
{
	static char buffer[TEXTLINE_MAXLENGTH + 1];
	FILE *fp;

	TR_CARRIAGERETURN = 0;
	
	if (filename == (char *)stdin)
		fp = stdin;
	else {
		if ((fp = tr_GetFp (filename, "r")) == TR_NULLFILE)
			return TR_EOF;
	}
	if (tr_mbFgets (buffer, sizeof(buffer), fp)) {
		/*
		**  Remove all CR and LF characters from
		**  the end of the line.
		*/
		TR_CARRIAGERETURN += tr_stripRC( buffer ); 
		/*
		**  This increments the variable TR_LINE, which in
		**  principle is done only in the GetNextLine ()
		**  routine in tr_pick.c
		*/
		if (fp == stdin)
			TR_LINE++;
		return tr_MemPool (buffer);
	}
	return TR_EOF;
}

