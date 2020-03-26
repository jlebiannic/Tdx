/*	TradeXpress $Id: rlsfileformat.h 47371 2013-10-21 13:58:37Z cdemory $
========================================================================
*	Fonctions for handling rlshosts files format
========================================================================
*  1.00 30.05.2008/CRE Created.
========================================================================*/

#define BLOCK_FOUND					0

#define USR_ENV_HOME				"HOME"
#define EDI_ENV_HOME				"EDIHOME"
#define RLSHOSTS					".rlshosts"

/*
	Tags for .rlshosts file
*/
#define ENTRY_BLOCK 				"Entry"
#define AUTH_BLOCK 					"Auth"
#define USEAUTHD_BLOCK 				"UseAuthd"
#define USERNAME_FIELD				"Username"
#define PWORD_FIELD 				"Pword"
#define IP_ADDRESS_FIELD 			"IpAddress"
#define IP_ADDRESS_FIELD2 			"IP-address"
#define RESTRICT_BLOCK 				"Restrict"
#define BASE_FIELD 					"Base"
#define RUN_FIELD 					"Run"
#define FILE_FIELD 					"File"

#define SYSTEM_USER_FIELD			"SystemUser"

#define EMPTY_VALUE					""
#define USEAUTHD_BLOCK_ON			"On"
#define DEFAULT_BASE_FLAG_N			'N'


#define START_OF_BLOCK				'<'
#define END_OF_BLOCK				'>'
#define DELIMITER					";"


int GetFieldNew ( char , char , char * , char *  ,char **);
int GetValue ( char , char , char *, char *,char **);
void CutOfSpaces(char *);
int GetBlockNew ( char , char , char *, char **);
int GetAuthSection (char ** ,char ** ,char **, char *);
int LogError (char *, char , char *, int );
int File_read(char **, char *);
int GetUserAndPwd(char *, char **, char **);
int GetIPAdress(char *, char **, char **);
int GetSysUser(char *, char *, char **);

