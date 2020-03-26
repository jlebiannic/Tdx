#ifndef _BUILD_C
#define EXTERN extern
#else 
#undef EXTERN 
#endif

struct opt {
	unsigned :0;
	unsigned rereadCfg:1;
	unsigned checkAffinities:1;
}; 

int logsys_buildsystem(char *sysname, char *cfgfile, struct opt options);
void logsys_createlabel(LogSystem *log);
void logsys_createdirs(char *sysname);
int logsys_copyconfig(char *cfgfile, char *sysname);
void logsys_createsqlindexes(char *cfgfile, LogSystem *log);

#ifndef _BUILD_C
#undef EXTERN 
#endif
