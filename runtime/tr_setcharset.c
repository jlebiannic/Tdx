/*============================================================================
        E D I S E R V E R   T R A N S L A T O R
 
        File:           tr_setcharset.c
        Programmer:     Jouni Haanpalo
        Date:           Fri Sep 24 10:59:34 EET DST 1999
 
        Copyright (c) 1996 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: tr_setcharset.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  Record all changes here and update the above string accordingly.
  
  1.00/JH	24.09.99	1st version.
============================================================================*/
	
#include "tr_constants.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_bottom.h"

int bfSetCharset(char *tCharset, char *tCharPath)
{
	char *charSetDirs[2];
    charSetDirs[0]=tCharPath;
    charSetDirs[1]= NULL;
    return tr_loadcharset (tCharset, charSetDirs);
} 

int bfSetOutputCharset(char *tCharset, char *tCharPath)
{
    if (!tr_in_EDISyntax || !tr_out_EDISyntax) {
        return bfSetCharset(tCharset,tCharPath);
    } else {
        int i = 0;
        while ( (i<MAX_CHAR_DIRS+2) && tr_Outgoing_CharSetDirs[i]) i++;
        tr_Outgoing_CharSetDirs[i] = tr_strdup(tCharPath);
        tr_Outgoing_CharSet = tr_strdup(tCharset);
    }
    return 0;
}

int bfSetInputCharset(char *tCharset, char *tCharPath)
{
    if (!tr_in_EDISyntax || !tr_out_EDISyntax) {
        return bfSetCharset(tCharset,tCharPath);
    } else {
        int i = 0;
        while ((i<MAX_CHAR_DIRS+2) && tr_Incoming_CharSetDirs[i]) i++;
        tr_Incoming_CharSetDirs[i] = tr_strdup(tCharPath);
        tr_Incoming_CharSet = tr_strdup(tCharset);
    }
    return 0;
}
