#include "data-access/dao/dao.h"

#include<stdlib.h>
#include<stdio.h>
#include <stdarg.h>

void Dao_init(Dao *This) {
	This->logError = Dao_logError;
	This->logErrorFormat = Dao_logErrorFormat;
	This->logDebug = Dao_logDebug;
	This->logDebugFormat = Dao_logDebugFormat;
}

void Dao_logError(const char *fctName, const char *msg) {
	fprintf(stderr, "%s: %s\n", fctName, msg);
	fflush(stderr);
}

void Dao_logErrorFormat(char *formatAndParams, ...) {
	va_list ap;
	va_start(ap, formatAndParams);
	vprintf(formatAndParams, ap);
	printf("\n");
	fflush(stdout);
	va_end(ap);
}

void Dao_logDebug(const char *fctName, const char *msg) {
#ifdef DEBUG
	printf("%s: %s\n", fctName, msg);
	fflush(stdout);
#endif
}

void Dao_logDebugFormat(char *formatAndParams, ...) {
	if (getenv("DATA_ACCESS_DEBUG") != NULL) {
		va_list ap;
		va_start(ap, formatAndParams);
		vprintf(formatAndParams, ap);
		printf("\n");
		fflush(stdout);
		va_end(ap);
	}
}



