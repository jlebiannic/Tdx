/*========================================================================
        File:		rlsformat.c
        Author:		CRE
        Date:		30/05/2008

        Copyright (c) 2008 Generix Group


	Fonctions for handling rlshosts files format

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: rlsfileformat.c 47371 2013-10-21 13:58:37Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
========================================================================
*  1.00 30.05.2008/CRE Created.
========================================================================*/
#ifdef MACHINE_WNT
#include <windows.h>
#include <process.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef MACHINE_WNT
#include <syslog.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef MACHINE_AIX
#include <sys/select.h> /* barf */
#define COMPAT_43
#endif
#include <time.h>


#include "logsysrem.h"
#include "perr.h"
#include "rlserror.h"

#include "rlsfileformat.h"

#ifndef DEBUG_PREFIX
#define DEBUG_PREFIX "            [rlsff] "
#endif

#ifdef MACHINE_WNT
#define GET_USER	getenv("USERNAME")
#else
#define GET_USER	getenv("USER")
#endif

extern int debugged;

/*
	int GetFieldNew ( char B_start, char B_stop , char *Data , char * Section ,char **Field )

	Retrieves the field inside the characters
	B_start and , B_stop from Section.
	for example:
	B_start = <
	B_stop = >
	Section = Some
	Data =
	<Entry
		<Some xxxxxxxx xxxxxxxxxx xxxxxxxxxxx xxxxxxxxx	>
		<Other xxxxxxxxx xxxxxxxx xxxxxxxxxx >
	>

	so Field contains
	<Some xxxxxxxx xxxxxxxxxx xxxxxxxxxxx xxxxxxxxx> and the
	Data =
	<Entry
		<Other xxxxxxxxx xxxxxxxx xxxxxxxxxx >
	>
	ie blanks the data which it has grabbed.

	Notice:
	Field must be a pointer to data so it can be freed and
	allocated dynamically.

	Assumes:
		Data terminated with NULL character

	Creator: J 1/2s

*/
int GetFieldNew ( char B_start, char B_stop , char *Data , char * Section ,char **Field ) {
	char *p;
	char section[80];

	sprintf ( section , "%c%s", B_start , NOTNUL(Section));

	if (( p = strstr ( Data , section )) == NULL ) {
		return NOT_FIELD_FOUND;
	}

	if (GetBlockNew ( B_start , B_stop , p , Field )) {
		return NOT_PAIR_FOUND;
	}

	/* Remove the entry */
	memset ( p , ' ' , strlen (*Field));
	return BLOCK_FOUND;
}

int GetValue ( char B_start, char B_stop , char *Data , char *FieldName ,char **Value ) {
	char *p;

	if (*Value) {
		free (*Value);
		*Value = NULL;
	}

	if ((*Value = calloc (1,strlen(Data) + 1)) == NULL )
		fatal("%sallocate (Auth) GetValue",DEBUG_PREFIX);

	memset ( *Value , 0 , strlen(Data) + 1);
	strcpy ( *Value , Data );
	strcpy ( *Value , *Value + strlen (FieldName) + 1);

	if ( ( p = strchr ( *Value , B_stop )) != NULL )
		*p = '\0';

	CutOfSpaces(*Value);
	return 0;
}

void CutOfSpaces(char *pInput) {
	char *p;
	p = pInput;

	if (strlen(pInput) == 0)
		return;

	while (isspace(*p))
		p++;

	if (strlen(p) == 0) {
		*pInput = '\0';
		return;
	}

	strcpy (pInput, p );

	p = strlen(pInput) + pInput -1;

	while (isspace(*p))
		p--;

	*(p + 1) = '\0';

	return;
}

/*
	see GetFieldNew for further information
*/
int GetBlockNew ( char B_start, char B_stop , char *Data , char **Block ) {
	char *p;
	int Count=0;
	int C_count=0;

	p = Data;

	while (*p) {
		C_count++;

		if ( *p == B_start) {
			Count++;
			p++;
			continue;
		}

		if ( *p == B_stop )
			Count--;

		if ( Count == 0) {
			if (*Block) {
				free (*Block);
				*Block = NULL;
			}

			if ((*Block = calloc (1,C_count + 1)) == NULL )
				fatal("%sallocate (Auth) GetBlockNew",DEBUG_PREFIX);

			memset ( *Block , 0 , C_count + 1);

			strncpy ( *Block , Data , C_count);
			return 0;
		}
		p++;
	}

	if ( Count != 0 )
		return NOT_PAIR_FOUND;

	return 0;
}


/*
Client *conn, char **file_block ,char **entry_block ,char **block,char **field_block,char **value,char *ipaddress, char *Path
*/
int 	GetAuthSection(char **file_block ,char **entry_block ,char **block, char *Path) {
	int nRet;
	char *p;
	
	if ((p = getenv (USR_ENV_HOME)) == NULL) {
		svclog('I',  "%sUser = %s , Authentication : Environment variable %s not set. Sys Error Msg: %s System Errno: %d",DEBUG_PREFIX, NOTNUL(GET_USER), USR_ENV_HOME,strerror(errno),errno);
		return ENV_VAR_ERROR;
	}

#ifdef MACHINE_WNT
	sprintf ( Path , "%s\\%s" , p , RLSHOSTS);
#else
	sprintf ( Path , "%s/%s" , p , RLSHOSTS);
#endif

	if ( File_read ( file_block , Path) != USER_ACCEPTED)
	{
		/* Try another place */
		if ((p = getenv (EDI_ENV_HOME)) == NULL)
		{
			svclog('I',  "%sUser = %s , Authentication : Environment variable %s not set. Sys Error Msg: %s System Errno: %d",DEBUG_PREFIX, GET_USER ? GET_USER : "" , EDI_ENV_HOME ,strerror(errno),errno);
			return ENV_VAR_ERROR;
		}

#ifdef MACHINE_WNT
		sprintf ( Path , "%s\\lib\\%s" , p , RLSHOSTS);
#else
		sprintf ( Path , "%s/lib/%s" , p , RLSHOSTS);
#endif

		if ((nRet = File_read ( file_block , Path)) != 0 )
			return nRet;
	}
	
	/* Seeking entry block */
	if (( nRet = GetFieldNew ( START_OF_BLOCK , END_OF_BLOCK , *file_block, ENTRY_BLOCK , entry_block )) != 0 )
	{
		LogError (Path , START_OF_BLOCK , ENTRY_BLOCK , nRet);
		return nRet; /* funtsaa */
	}
	/* Seeking auth block */
	if ( ( nRet = GetFieldNew ( START_OF_BLOCK , END_OF_BLOCK , *entry_block, AUTH_BLOCK , block)) != 0 )
	{
		LogError (Path , START_OF_BLOCK , AUTH_BLOCK , nRet);
		return nRet; /* funtsaa */
	}
	/* Auth block found */
	
	return BLOCK_FOUND;
}

int LogError (char *Path , char Prefix , char *Section , int nRet) {
	switch (nRet) 	{
		case NOT_PAIR_FOUND:
			svclog('I',  "%sUser = %s , Authentication error: File: %s Not pair found for TAG: %c%s  ",DEBUG_PREFIX,NOTNUL(GET_USER), Path, Prefix , Section);
			break;
		case NOT_FIELD_FOUND:
			svclog('I',  "%sUser = %s , Authentication error: File: %s, TAG: %c%s missing ",DEBUG_PREFIX,NOTNUL(GET_USER), Path, Prefix , Section);
			break;
		case INV_FLAG_IN_FILE_FIELD:
		case INV_FLAG_IN_RUN_FIELD:
		case INV_FLAG_IN_BASE_FIELD:
		case INV_VAL_IN_AUTHD_FIELD:
			svclog('I',  "%sUser = %s , Authentication error: File: %s, TAG: %c%s , illegal flag value ",DEBUG_PREFIX,NOTNUL(GET_USER), Path, Prefix , Section);
			break;
		case INV_PATH_IN_BASE_FIELD:
			svclog('I',  "%sUser = %s , Authentication error: File: %s, TAG: %c%s , illegal path value ",DEBUG_PREFIX,NOTNUL(GET_USER), Path, Prefix , Section);
			break;
		default:
			break;
	}

	return 0;
}

/***********************************************************************

	File_read (char **buf, char *file_ptr)

	Opens the file given in file_ptr and reads the content of the
	file to the buf

	Returns :  0  on success
			   RLSHOSTS_FILE_READ_ERROR	(10003) on error

	Notice: frees the *buf before allocating new memory

***********************************************************************/

int File_read(char **buf, char *file_ptr) {
#ifdef MACHINE_WNT
	struct _stat stbuf;
#else
	struct stat stbuf;
#endif
	FILE *fpbp;

	if (*buf) {
		free(*buf);
		*buf = NULL;
	}

#ifdef MACHINE_WNT
	if (_stat (file_ptr, &stbuf) != 0)
#else
	if (stat (file_ptr, &stbuf) != 0)
#endif
	{
		if (DEBUG(4)) 
			svclog('I',  "%sUser = %s , Authentication : stat failed for file: %s. Sys Error Msg: %s System Errno: %d",DEBUG_PREFIX,NOTNUL(GET_USER), NOTNUL(file_ptr), NOTNUL(strerror(errno)),errno);
		return RLSHOSTS_FILE_READ_ERROR;
	}


	if ((fpbp = fopen (file_ptr, "rb")) == NULL) {
		if (DEBUG(4))
			svclog('I',  "%sUser = %s , Authentication : fopen failed for file: %s. Sys Error Msg: %s System Errno: %d",DEBUG_PREFIX, NOTNUL(GET_USER), NOTNUL(file_ptr), NOTNUL(strerror(errno)),errno);
		return RLSHOSTS_FILE_READ_ERROR;
	}


	/*
	 * Read data.
	 */

	if (stbuf.st_size == 0) {
		svclog('I',  "%sUser = %s , Authentication : Empty file: %s.",DEBUG_PREFIX,NOTNUL(GET_USER), file_ptr);
		fclose(fpbp);
		return RLSHOSTS_FILE_READ_ERROR;
	}

	if (*buf) {
		free (*buf);
		*buf = NULL;
	}

	if ((*buf = calloc(1,(size_t) stbuf.st_size + 1)) == NULL ) {
		fclose(fpbp);
		fatal ("%smalloc File_read",DEBUG_PREFIX);
	}

#ifndef JOUNI
	if ( fread (*buf , (size_t) stbuf.st_size, 1 , fpbp) != 1) {
		svclog('I',  "%sUser = %s , Authentication : fread failed for  file: %s. Sys Error Msg: %s System Errno: %d",DEBUG_PREFIX,NOTNUL(GET_USER), file_ptr, strerror(errno),errno);
		fclose(fpbp);
		return RLSHOSTS_FILE_READ_ERROR;
	}
#else
	fread (*buf , (size_t) stbuf.st_size , 1 , fpbp);
#endif

	fclose (fpbp);
	return USER_ACCEPTED;
}

/***********************************************************************

	GetUserAndPwd(char *block, char **user_found, char **pwd_found)

	Look on the "Username" block  to found user name and password
	
Input :
	block : <Auth block
	user_found : login name. Defined by the function.
	pwd_found : associated password. Defined by the function.

Output :
	int : code of error or success

***********************************************************************/
int GetUserAndPwd(char *block, char **user_found, char **pwd_found) {
	char *p;
	char *value=NULL;
	int nRet;

	if (debugged) { debug("%sSeeking user in [%s] \n",DEBUG_PREFIX, block); }
	

	if ((nRet = GetValue( START_OF_BLOCK , END_OF_BLOCK , block, USERNAME_FIELD, &value )) != 0) {
		return nRet;
	}
	if (debugged) { debug("%s\tvalue=[%s] \n",DEBUG_PREFIX, value); }
			
	if (( p = strstr ( value , DELIMITER)) == NULL ) {
		if (debugged) { debug("%s\tNo delimiter two syntaxes '<Username >' or '<Username XXX>' \n",DEBUG_PREFIX); }
		/* No delimiter two choices <Username > or <Username morko> */
		if (strlen(value) != 0) {
			/* <Username morko> */
			if (debugged) { debug("%s\t'<Username XXX>' \n",DEBUG_PREFIX); }
			*user_found = malloc(strlen(value) + 1);
			strcpy(*user_found , value);
			*pwd_found = malloc(1);
			**pwd_found = '\0';
		}
		/* <Username > */
		/* Nothing to do */
	} else {
		if (strlen(value) == 1) {
			/*	<Username ;> Only delimiter given */
			if (debugged) { debug("%s\t'<Username ;>' Only delimiter given => Nothing to test \n",DEBUG_PREFIX); }
			; /* Nothing to do */
			*user_found = malloc(1);
			**user_found = '\0';
			*pwd_found = malloc(1);
			**pwd_found = '\0';
		} else {
			if ( strncmp(value , DELIMITER , strlen(DELIMITER)) == 0 ) {
				if (debugged) { debug("%s\t'<Username ;YYY>' Only password given \n",DEBUG_PREFIX); }
				/*	<Username ;Morko> Only password given */
				*user_found = malloc(1);
				**user_found = '\0';
				/* removing the ';' */
				*pwd_found = malloc(strlen(value));
				strcpy(*pwd_found , p + 1);
			} else {
				if ((*(p + 1)) == '\0') {
					if (debugged) { debug("%s\t'<Username XXX;>' Only Username given \n",DEBUG_PREFIX); }
					/*	<Username Morko;> Only Username given */
					*user_found = malloc(strlen(value) + 1);
					strcpy (*user_found , value);
					/* Delete the ';' */
					*((*user_found) + strlen(value) -1) = '\0';
					*pwd_found = malloc(strlen(value));
					strcpy(*pwd_found , p + 1);
				} else {
					/*	<Username Morko;Passsword > Both given */
					if (debugged) { debug("%s\t'<Username XXX;YYY>' Both given \n",DEBUG_PREFIX); }
					*user_found = malloc(strlen(value) - strlen(p) + 1);
					strncpy (*user_found , value, (strlen(value) - strlen(p)) );
					*((*user_found) + strlen(value) - strlen(p)) = '\0';
					*pwd_found = malloc(strlen(p));
					strcpy (*pwd_found , p + 1);
				}
			}
		}
	}
	return 0;
}

int GetIPAdress(char *block, char **field_block, char **ip_adress) {
	int nRet, other_ipaddr = 0;

	if( (nRet = GetFieldNew ( START_OF_BLOCK , END_OF_BLOCK , block, IP_ADDRESS_FIELD, field_block )) == NOT_FIELD_FOUND)  {
		nRet = GetFieldNew ( START_OF_BLOCK , END_OF_BLOCK , block, IP_ADDRESS_FIELD2, field_block );
		other_ipaddr = 1;
	} else {
		other_ipaddr = 0;
	}

	switch (nRet) {
		/* Success , ipaddress block found */
		case BLOCK_FOUND:
			if(other_ipaddr) {
				GetValue ( START_OF_BLOCK , END_OF_BLOCK , *field_block , IP_ADDRESS_FIELD2 , ip_adress );
			} else {
				GetValue ( START_OF_BLOCK , END_OF_BLOCK , *field_block , IP_ADDRESS_FIELD , ip_adress );
			}
	}
	return nRet;
}

/***********************************************************************

	GetSysUser(char *ediuser, char *login_user, char **system_user)

	Look on the RLSHOST file  on $EDIHOME/lib to found system user associated to ediuser & login_user
	
Input :
	ediuser : Tradexpress user. 
	login_user : login name, defined for the user environement only
	system_user : system user name found. Defined by the function.

Output :
	int : code of error or success

***********************************************************************/
int GetSysUser(char *ediuser, char *login_user, char **system_user) {
	int nRet, nRet2, nRet3;
	char *p;
	char *user_found=NULL;
	char *pwd_found=NULL;
	char *local_user=NULL;
	char *file_block=NULL;
	char *entry_block=NULL;
	char *block=NULL;
	char *login_block=NULL;
	char *sysuser_block=NULL;
	char Path[256];
	
	if (debugged) { debug("%sEntering GetSysUser( %s,%s, %x) \n", DEBUG_PREFIX, NOTNUL(ediuser), NOTNUL(login_user), *system_user); }
	/* Try only on $EDIHOME/.rlshost */
	if ((p = getenv (EDI_ENV_HOME)) == NULL){
		svclog('I',  "%sUser = %s , Authentication : Environment variable %s not set. Sys Error Msg: %s System Errno: %d",DEBUG_PREFIX, GET_USER ? GET_USER : "" , EDI_ENV_HOME ,strerror(errno),errno);
		return ENV_VAR_ERROR;
	}

#ifdef MACHINE_WNT
	sprintf ( Path , "%s\\lib\\%s" , p , RLSHOSTS);
#else
	sprintf ( Path , "%s/lib/%s" , p , RLSHOSTS);
#endif

	if ((nRet = File_read(&file_block , Path)) != 0 ) {
		return nRet;
	}

	/* Seeking entry block */
	while(1) {
		nRet = GetFieldNew( START_OF_BLOCK , END_OF_BLOCK , file_block, ENTRY_BLOCK , &entry_block );
		switch (nRet) {
			case BLOCK_FOUND:
				/* Seeking auth block */
				while(1) {
					nRet = GetFieldNew( START_OF_BLOCK , END_OF_BLOCK , entry_block, AUTH_BLOCK , &block);
					switch (nRet) {
						case BLOCK_FOUND:
							/* Auth block found */
							if (debugged) { debug("%s'<Auth' block found :\n\t%s\n",DEBUG_PREFIX, block); }
							/* Seeking Username block */
							while(1) {
								nRet2 = GetFieldNew( START_OF_BLOCK , END_OF_BLOCK , block , USERNAME_FIELD , &login_block );
								switch (nRet2) {
									/* Success , <Username block found */
									case BLOCK_FOUND: 
										if (debugged) { debug("%s'<Username' block found :[%s]\n",DEBUG_PREFIX, login_block); } 
										nRet3 = GetUserAndPwd(login_block, &user_found, &pwd_found);
										if (nRet3 != BLOCK_FOUND) {
											if (debugged) { debug("%sGetUserAndPwd() return %d \n",DEBUG_PREFIX, nRet3); }
											return nRet3;
										}
										if (user_found != NULL) {
											if (debugged) { debug("%sUser found :  %s \n",DEBUG_PREFIX, user_found); }
											if ( strcmp (user_found , "localuser") == 0 ) {
												if (debugged) { debug("%sLocaluser found (so we must test with %s)\n",DEBUG_PREFIX, NOTNUL(GET_USER) ); }
												local_user = malloc(strlen(GET_USER) + 1);
												if (local_user == NULL) {
													svclog('I',  "%sInternal memory error. Sys Error Msg: %s System Errno: %d",DEBUG_PREFIX,strerror(errno),errno);
													return INTERNAL_ERROR;
												}
												strcpy (local_user , GET_USER);
												if ( strcmp (ediuser , local_user) != 0 ) {
													if (debugged) { debug("%slocaluser : %s != %s \n",DEBUG_PREFIX, ediuser, local_user); }
													/*  le user n'est pas le bon => Suivant ! */
													break;
												}
											} else {
												if ( strcmp (user_found , login_user) != 0 ) {
													if (debugged) { debug("%s%s != %s \n",DEBUG_PREFIX, user_found, login_user); }
													/*  le user n'est pas le bon => Suivant ! */
													break;
												}
											}
											/* OK, user found => recuperer le sysuser */
											if (debugged) { debug("%sUser %s ok, seeking systemUser \n",DEBUG_PREFIX, user_found); }
											nRet3 = GetFieldNew( START_OF_BLOCK , END_OF_BLOCK , block , SYSTEM_USER_FIELD , &sysuser_block );
											if (nRet3 != BLOCK_FOUND) {
												if (debugged) { debug("%s'<SystemUser' block Not found : return %d \n",DEBUG_PREFIX, nRet3); }
												return nRet3;
											}
											nRet3 = GetValue( START_OF_BLOCK , END_OF_BLOCK , sysuser_block, SYSTEM_USER_FIELD, system_user );
											return nRet3;
										} else {
											if (debugged) { debug("%sUser found NULL \n",DEBUG_PREFIX); }
										}
										break;
										/* User Syntax error --> quit */
									case NOT_PAIR_FOUND:
										LogError (Path , START_OF_BLOCK , USERNAME_FIELD , NOT_PAIR_FOUND);
										return NOT_PAIR_FOUND;
									/* Username Field is not given at all */
									case NOT_FIELD_FOUND:
										if (debugged) {  debug("%s'<Username' block not found \n",DEBUG_PREFIX); }
										break;
									default:
										if (debugged) { debug("%sSeek Username block return %d \n",DEBUG_PREFIX, nRet2); }
										return INTERNAL_ERROR;
								} /* end switch */
								if (nRet2 == NOT_FIELD_FOUND) {break;}
							} /* end while */
							break;
						/* User Syntax error --> quit */
						case NOT_PAIR_FOUND:
							LogError (Path , START_OF_BLOCK , USERNAME_FIELD , NOT_PAIR_FOUND);
							return NOT_PAIR_FOUND;
						/* User Field is not given at all */
						case NOT_FIELD_FOUND:
							if (debugged) { debug("%s'<Auth' block not found \n",DEBUG_PREFIX); }
							break;
						default:
							if (debugged) { debug("%sSeek Auth block return %d \n",DEBUG_PREFIX, nRet); }
							return INTERNAL_ERROR;
					} /* end switch */
					if (nRet == NOT_FIELD_FOUND) {
						break;
					}
				} /* end while */
				/* go to next block */
				break;
			/* User Syntax error --> quit */
			case NOT_PAIR_FOUND:
				LogError (Path , START_OF_BLOCK , ENTRY_BLOCK , NOT_PAIR_FOUND);
				return NOT_PAIR_FOUND;
			/* User Field is not given at all*/
			case NOT_FIELD_FOUND:
				if (debugged) { debug("%s'<Entry' block not found \n",DEBUG_PREFIX); }
				return NOT_FIELD_FOUND;
			default:
				if (debugged) { debug("%sSeek Entry block return %d \n",DEBUG_PREFIX, nRet); }
				return INTERNAL_ERROR;
		} /* end switch */
		if (debugged) { debug("%sTry next Entry\n",DEBUG_PREFIX); }
	} /* end while */
	
	if (debugged) { debug("%sWe should never go this way\n",DEBUG_PREFIX); }
	return INTERNAL_ERROR;
}

