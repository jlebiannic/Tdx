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
	char *str = NULL;
	char *res = NULL;
	for (i = 0; i < nb; i++) {
		if (str == NULL) {
			str = (char*) malloc(strlen(fields[i]) + 1);
			strcpy(str, fields[i]);
			res = str;
		} else {
			int len = strlen(str);
			str = str + len;
			str = (char*) realloc(str, len + strlen(fields[i]) + 1);
			strcpy(str, fields[i]);
		}

		if (i < nb - 1) {
			strcat(str, sep);
		}
	}
	return res;
}

static void p_arrayAddElement(char **array, char *element, int idx, int allocArray) {
	if (allocArray) {
		*(array + idx) = (char*) malloc(sizeof(char*));
	}
	array[idx] = element;
}

void arrayAddElement(char **array, char *element, int idx, int allocElement, int allocArray) {
	char *newElem = NULL;
	if (allocElement) {
		newElem = malloc((strlen(element) + 1) * sizeof(char));
		strcpy(newElem, element);
	} else {
		newElem = element;
	}
	p_arrayAddElement(array, newElem, idx, allocArray);
}

void arrayAddTimeElement(char **array, time_t element, int idx, int allocArray) {
	char *str = allocStr("%ld", element);
	p_arrayAddElement(array, str, idx, allocArray);
}

void arrayAddDoubleElement(char **array, double element, int idx, int allocArray) {
	char *str = allocStr("%lf", element);
	p_arrayAddElement(array, str, idx, allocArray);
}

void arrayAddIntElement(char **array, int element, int idx, int allocArray) {
	char *str = allocStr("%d", element);
	p_arrayAddElement(array, str, idx, allocArray);
}
