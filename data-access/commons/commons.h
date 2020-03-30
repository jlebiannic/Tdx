/*
 * commons.h
 *
 *  Created on: 13 fevr. 2020
 *      Author:
 */

#ifndef COMMONS_COMMONS_H_
#define COMMONS_COMMONS_H_

#ifndef TRUE
#define FALSE 0
#define TRUE  1
#endif

char* allocStr(const char *formatAndParams, ...);
char* reallocStr(char *str, const char *formatAndParams, ...);
char* uitoa(unsigned int uint);
void freeArray(char **array, int nb);

#endif /* COMMONS_COMMONS_H_ */
