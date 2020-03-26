/* TradeXpress $Id: rlserror.h 47371 2013-10-21 13:58:37Z cdemory $ */
/*
	Authentication section has reserved error codes from
	10000 to 10100 for it's usage
*/


/*
	AUTHENTICATION SECTION Start
*/
#define USER_ACCEPTED				0
#define SIGNON_PACKET_SYNTAX_ERROR	10001
#define ENV_VAR_ERROR				10002
#define RLSHOSTS_FILE_READ_ERROR	10003
#define NOT_PAIR_FOUND 				10004
#define NOT_FIELD_FOUND 			10005
#define GEN_AUTH_ERROR	 			10006
#define ACCESS_TO_BASE_DENIED 		10007
#define NO_RUN_RIGHTS_IN_SERVER		10008
#define INV_FLAG_IN_FILE_FIELD     	10009
#define INV_FLAG_IN_RUN_FIELD     	10010
#define INV_FLAG_IN_BASE_FIELD      10011
#define ACCESS_TO_FIL_SYS_DENIED	10012
#define LIM_ACCESS_TO_FIL_SYS		10013
#define BASE_ENV_VAR_ERROR			10014
#define INV_VAL_IN_AUTHD_FIELD		10015
#define GEN_AUTHD_ERROR	 			10016
#define AUTHD_MISMATCH_USER         10017
#define INTERNAL_ERROR				10018
#define AUTH_SYSTEM_ERROR			10019
#define INV_PATH_IN_BASE_FIELD      10020
#define LIM_ACCESS_TO_BASE			10021

/*
	Error string which will be sent to the user
*/
#ifdef MACHINE_WNT
#define AUTH_SYSTEM_ERROR_S			"Authentication system socket error, see Event log for more details"
#define ENV_VAR_ERROR_S				"Authentication system environment error, see Event log for more details"
#define RLSHOSTS_FILE_READ_ERROR_S	"Authentication file read error, see Event log for more details"
#define BASE_ENV_VAR_ERROR_S		"Authentication file configuration error, illegal environment variable, see Event log for more details"
#define NOT_PAIR_FOUND_S 			"Authentication file configuration error, block end not found, see Event log for more details"
#define NOT_FIELD_FOUND_S 			"Authentication file configuration error, mandatory tag missing, see Event log for more details"
#define INV_FLAG_IN_FIELD_S   		"Authentication file configuration error, illegal value, see Event log for more details"
#define SIGNON_PACKET_SYNTAX_ERROR_S	"Authentication error, illegal sign on packet, see Event log for more details"
#else
#define AUTH_SYSTEM_ERROR_S			"Authentication system socket error, see System log for more details"
#define ENV_VAR_ERROR_S				"Authentication system environment error, see System log for more details"
#define RLSHOSTS_FILE_READ_ERROR_S	"Authentication file read error, see System log for more details"
#define BASE_ENV_VAR_ERROR_S		"Authentication file configuration error, illegal environment variable, see System log for more details"
#define NOT_PAIR_FOUND_S 			"Authentication file configuration error, block end not found, see System log for more details"
#define NOT_FIELD_FOUND_S 			"Authentication file configuration error, mandatory tag missing, see System log for more details"
#define INV_FLAG_IN_FIELD_S   		"Authentication file configuration error, illegal value, see System log for more details"
#define SIGNON_PACKET_SYNTAX_ERROR_S	"Authentication error, illegal sign on packet, see System log for more details"
#endif
#define GEN_AUTH_ERROR_S 			"Authentication : Access to the server denied, no matching entry found"
#define INTERNAL_ERROR_S			"Authentication : Unknown error"
#define AUTHD_MISMATCH_USER_S       "Authentication : Mismatch username in Authd protocol"
#define ACCESS_TO_BASE_DENIED_S		"Authentication notification: Access to the base not allowed."
#define NO_RUN_RIGHTS_IN_SERVER_S   "Authentication notification: No permission to run programs in server."
#define ACCESS_TO_FIL_SYS_DENIED_S	"Authentication notification: Access to the file system in server not allowed."
#define LIM_ACCESS_TO_FIL_SYS_S		"Authentication notification: Only read access to the file system in server allowed."
#define LIM_ACCESS_TO_BASE_S		"Authentication notification: Only read access to the base in server allowed."

/*
	AUTHENTICATION SECTION End
*/

#define NO_MATCH_FOUND				1
#define MATCH_FOUND					0
