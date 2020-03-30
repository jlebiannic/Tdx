#include<stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// FIXME move these functions to a real commons lib
char* allocStr(const char *formatAndParams, ...) {
	va_list ap;
	va_start(ap, formatAndParams);
	int size = vsnprintf(NULL, 0, formatAndParams, ap);
	va_end(ap);
	char *str = malloc(size + 1);
	va_start(ap, formatAndParams);
	vsprintf(str, formatAndParams, ap);
	return str;
}

char* reallocStr(char *str, const char *formatAndParams, ...) {
	va_list ap;
	va_start(ap, formatAndParams);
	int size = vsnprintf(NULL, 0, formatAndParams, ap);
	va_end(ap);

	int len = strlen(str);
	str = (char*) realloc(str, len + size + 1);
	char *strPlusOffset = str + len;
	va_start(ap, formatAndParams);
	vsprintf(strPlusOffset, formatAndParams, ap);
	return str;
}

char* uitoa(unsigned int uint) {
	char *str = NULL;
	const int size = snprintf(NULL, 0, "%u", uint);
	str = malloc(size + 1);
	sprintf(str, "%u", uint);
	return str;
}

void freeArray(char *array[], int nb) {
	int i=0;
	for (i = 0; i < nb; i++) {
		free(array[i]);
	}
}
