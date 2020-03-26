#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_exceptions.c 55062 2019-07-22 12:35:02Z sodifrance $")
/*======================================================================
	Tradexpress   T R A N S L A T O R

	File:		tr_exceptions.c
	Programmer:	luc meynier (LME)
	Date:		Fri Oct  9 10:48:15 EET 1992
======================================================================*/
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include "tr_prototypes.h"
#include "tr_strings.h"

/*==============================================================================
  Record all changes here and update the above string accordingly.
  5.00 15.11.2007/LME	First version
  5.01 05.06.2008/LME	Exit gracefully when there's no exception handler
  5.02 05.06.2008/LME	Always initialize pointer to Null...
  5.03 06.03.2009/LME	Exception stack printed on stderr BugZ 9057
  5.04 17.06.2009/LME	...last one too !!!
  5.05 29.07.2009/LME	BugZ 9252 fixed
  5.10 06.08.2011/PNT	The exception exit use the number 128 to prevent a code exectution stop
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
==============================================================================*/

typedef struct {
    char *classe;  
    char *reason;
    char *message;
    double code;
} ExceptionInfo;


typedef struct {
	jmp_buf jbuf;
	void *next;
	void *prev;
} ExceptionAddress;

ExceptionAddress *firstExceptionAddress = NULL;
ExceptionAddress *lastExceptionAddress = NULL;

typedef struct {
	ExceptionInfo Info;
	void *next;
	void *prev;
} ExceptionInfos;

ExceptionInfos *firstExceptionInfos = NULL;
ExceptionInfos *lastExceptionInfos = NULL;

static int tr_PrintExceptionStack(FILE *stream);

static void infocpy(ExceptionInfo *dest, ExceptionInfo *src)
{
    if (!src || !dest) 
	{
	return;
	}
    if (src->classe)
        {
		dest->classe = strdup(src->classe);
		}
    else
		{ 
	    dest->classe = NULL;
		}
    if (src->reason)
        {
		dest->reason = strdup(src->reason);
		}
    else
        {
		dest->reason = NULL;
		}
    if (src->message)
       {
	   dest->message = strdup(src->message);
	   }
    else
       { 
	   dest->message = NULL;
	   }
    dest->code = src->code;

    return;    
}

static void infofill(ExceptionInfo *dest, char *classe, char *reason, char *message, double code)
{
    if (!dest) 
	{
	return;
    }
	if (classe)
        {
		dest->classe = strdup(classe);
		}
    else
        {
		dest->classe = NULL;
		}
    
    if (reason)
        {
		dest->reason = strdup(reason);
		}
    else
        {
		dest->reason =  NULL;
		}
    
    if (message)
        {
		dest->message = strdup(message);
		}
    else
        {
		dest->message =  NULL;
		}

    if (code != -1.0)
        {
		dest->code = code;
		}

    return;    
}

static ExceptionInfos *tr_newExceptionInfos(char *classe, char *reason, char *message, int code)
{
    ExceptionInfos *newExceptInfos = (ExceptionInfos *)malloc(sizeof(ExceptionInfos));

    if (newExceptInfos)
    {
		if ((classe)||(!lastExceptionInfos))
			{
			infofill(&(newExceptInfos->Info), classe, reason, message, code); 
			}
		else
			{
			infocpy(&(newExceptInfos->Info), &(lastExceptionInfos->Info)); 	  
			}
    }
    return newExceptInfos;
}

static void tr_freeExceptionInfos(ExceptionInfos *pInfos)
{
    if (pInfos)
    {
	    if (pInfos->Info.classe)
			{
			free(pInfos->Info.classe);
			}
	    if (pInfos->Info.reason)
			{
			free(pInfos->Info.reason);
			}
	    if (pInfos->Info.message)
			{
			free(pInfos->Info.message);
			}
	    free(pInfos);
    }
}



jmp_buf * pushExceptionAddress (void)
{
	ExceptionAddress *new;

	new = (ExceptionAddress *)malloc(sizeof(ExceptionAddress));
	
	new->prev = NULL;
	new->next = NULL;
	
	if (lastExceptionAddress)
	{
		lastExceptionAddress->next = new;
		new->prev = lastExceptionAddress;
		lastExceptionAddress = new;
	}else{
		firstExceptionAddress = lastExceptionAddress = new;
	}
	return &(new->jbuf);
}

jmp_buf * popExceptionAddress(void)
{
	static jmp_buf jbuf;
	ExceptionAddress * last;
	
	if (lastExceptionAddress)
	{
		memcpy(&jbuf, &(lastExceptionAddress->jbuf),  sizeof(jmp_buf));
		last  = lastExceptionAddress->prev ? lastExceptionAddress->prev : NULL;
		free(lastExceptionAddress);
		lastExceptionAddress = last;
		if (!lastExceptionAddress)
			{
			firstExceptionAddress = NULL;
			}
	}else{
		return(NULL);
	}
	return &jbuf;
}

static void pushExceptionInfos (ExceptionInfos *pNew)
{
	
	pNew->prev = NULL;
	pNew->next = NULL;
	
	if (lastExceptionInfos)
	{
		lastExceptionInfos->next = pNew;
		pNew->prev = lastExceptionInfos;
		lastExceptionInfos = pNew;
	}else{
		firstExceptionInfos = lastExceptionInfos = pNew;
	}
}

static ExceptionInfos * popExceptionInfos(void)
{
	ExceptionInfos * last = NULL;
	
	if (lastExceptionInfos)
	{
		last  = lastExceptionInfos->prev ? lastExceptionInfos->prev : NULL;
		tr_freeExceptionInfos(lastExceptionInfos);
		lastExceptionInfos = last;
		if (!lastExceptionInfos)
			firstExceptionInfos = NULL;
	}
	return(last);
}

void popExceptionInfosStack(void)
{
    ExceptionInfos * last;

    while ( (last = popExceptionInfos()) );
}


void raisexcept(char *classe, char *reason, char *message, double code)
{
	jmp_buf *pjbuf;
	ExceptionInfos *pNewExceptInfos;

	if ( classe || reason || message || (code != -1) )
	{
		pNewExceptInfos = tr_newExceptionInfos(classe, reason, message, (int)code); /* cast to avoid warning code is used as an integer. */
		pushExceptionInfos(pNewExceptInfos);
	}
      
	pjbuf = popExceptionAddress();
	if (pjbuf)
		{
		longjmp(*pjbuf, 1);
		}
	else
	{
		tr_PrintExceptionStack(stderr);
		/* Modif PNT */
		exit(128);
		/* fin PNT */
	}
}

ExceptionInfo * tr_getLastExceptionInfos(void)
{
    return ( lastExceptionInfos ? &lastExceptionInfos->Info : NULL );     
}

char * tr_getLastExceptionClass(void)
{
    return ( lastExceptionInfos ? lastExceptionInfos->Info.classe : "" );
}

char * tr_getLastExceptionReason(void)
{
    return ( lastExceptionInfos ? lastExceptionInfos->Info.reason : "" );
}

char * tr_getLastExceptionMessage(void)
{
    return ( lastExceptionInfos ? lastExceptionInfos->Info.message : "" );
}

double tr_getLastExceptionCode(void)
{
    return ( lastExceptionInfos ? lastExceptionInfos->Info.code : -1 ); 
}


int tr_matchException( char *args, ...)
{
    va_list ap;
    char *classe = NULL;
    
	/* last exception thrown without class, so any can match */
    if ((!lastExceptionInfos)||(!lastExceptionInfos->Info.classe))
		{
		return 1;
		}

    va_start (ap, args);
	while (*args++)
	{
		if (*args == 's')
		{
			classe = va_arg(ap, char *);
			/* do the current proposed in args match the last exception class thrown ? */
			if ( (classe) && (*classe) && (!strcmp(classe, lastExceptionInfos->Info.classe)) ) 
				{
				return 1;
				}
		}
	}
    va_end (ap);
	
	/* no match found */
	return 0;
}

static int tr_PrintExceptionStack(FILE *stream)
{
	ExceptionInfos *except; 

	if (!firstExceptionInfos) 
	{
	return 0;
	}
	  
	for (except = lastExceptionInfos ; (except != NULL) ; except = except->prev)
	{
		tr_mbFprintf(stream, "Exception classe:%-10s reason:%-35s code:%d message:%s\n",
			except->Info.classe ? except->Info.classe : "",
			except->Info.reason ? except->Info.reason : "",
			(int)except->Info.code,
			except->Info.message ? except->Info.message : "");
			}
	return 1;
}

int bfPrintExceptionStack()
{
	return ( tr_PrintExceptionStack(stdout) );
}

int bfLogExceptionStack()
{
	return ( tr_PrintExceptionStack(stderr) );
}
