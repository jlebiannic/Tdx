#include<stdio.h>
#include <stdarg.h>
#include <stdlib.h>

char* allocStr(const char *formatAndParams, ...) {
	va_list ap;
	va_start(ap, formatAndParams);
	int size = vsnprintf(NULL, 0, formatAndParams, ap);
	va_end(ap);
	return malloc(size + 1);
}
