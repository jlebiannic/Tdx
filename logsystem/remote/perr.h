/* TradeXpress $Id: perr.h 49172 2014-10-09 15:18:25Z tcellier $ */

extern char *logging;
extern char *EVENT_NAME;

#ifdef MACHINE_WNT

extern int EVENT_CATEGORY;
extern SID *sid_usr;

#endif

void debug(char *fmt, ...);
void warn(char *fmt, ...);
void fatal(char *fmt, ...);
void svclog(char type, char *fmt, ...);

char *syserr();
extern int nolog;

