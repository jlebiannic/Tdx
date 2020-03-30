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
char* allocStr2(const char *formatAndParams, ...);
char* uitoa(unsigned int uint);

#endif /* COMMONS_COMMONS_H_ */
