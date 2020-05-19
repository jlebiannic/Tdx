#include<stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

char* inttoa(int i) {
	char *str = NULL;
	const int size = snprintf(NULL, 0, "%d", i);
	str = (char*) malloc(size + 1);
	sprintf(str, "%d", i);
	return str;
}

void freeArray(char *array[], int nb) {
	int i=0;
	for (i = 0; i < nb; i++) {
		free(array[i]);
	}
}

char* arrayJoin(const char *fields[], int nb, char *sep) {
	int i = 0;
	int MAX = 500;
	int maxLen = MAX;
	char *str = str = (char*) malloc(maxLen);
	strcpy(str, "");

	for (i = 0; i < nb; i++) {
		int fieldLen = strlen(fields[i]);
		if ((fieldLen + strlen(str)) + 1 >= maxLen) {
			maxLen = strlen(str) + fieldLen + MAX;
			str = (char*) realloc(str, maxLen);
		}
		strcat(str, fields[i]);

		if (i < nb - 1) {
			strcat(str, sep);
		}
	}
	return str;
}

static void p_arrayAddElement(char **array, char *element, int idx) {
	array[idx] = element;
}

void arrayAddElement(char **array, char *element, int idx, int allocElement) {
	char *newElem = NULL;
	if (allocElement) {
		newElem = malloc((strlen(element) + 1) * sizeof(char));
		strcpy(newElem, element);
	} else {
		newElem = element;
	}
	p_arrayAddElement(array, newElem, idx);
}

void arrayAddTimeElement(char **array, time_t element, int idx) {
	char *str = allocStr("%ld", element);
	p_arrayAddElement(array, str, idx);
}

void arrayAddDoubleElement(char **array, double element, int idx) {
	char *str = allocStr("%lf", element);
	p_arrayAddElement(array, str, idx);
}

void arrayAddIntElement(char **array, int element, int idx) {
	char *str = allocStr("%d", element);
	p_arrayAddElement(array, str, idx);
}

char** arrayConcat(char *t1[], int nbT1, char *t2[], int nbT2) {
	int nbResult = nbT1 + nbT2;
	char **tResult = (char**) malloc(nbResult * sizeof(char*));
	int i;
	for (i = 0; i < nbResult; i++) {
		if (i < nbT1) {
			tResult[i] = t1[i];
		} else {
			tResult[i] = t2[i - nbT1];
		}
	}
	return tResult;
}
