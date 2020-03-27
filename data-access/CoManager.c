/*
 ============================================================================
 Name        : CoManager.c
 Author      : 
 Version     :
 Copyright   : 
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "commons/daoFactory.h"

int main(void) {
	puts("Main start");

	Dao *dao = daoFactory_create(1);
	dao->openDB(dao, NULL);

//	dao->getEntry(dao, "syslog", "tx_index > 0", "NEXT");
//
//	while (dao->hasNextEntry(dao)) {
//		char *str = dao->getFieldValue(dao, "next");
//		printf("next is %s\n", str);
//		dao->getNextEntry(dao);
//	}
//
//	dao->execQuery(dao, "select next from syslog where tx_index > 0");
//	while (dao->hasNextEntry(dao)) {
//		char *str = dao->getFieldValue(dao, "next");
//		printf("next is %s\n", str);
//		dao->getNextEntry(dao);
//	}

	dao->execQueryParamsMultiResults(dao, "select next from syslog where tx_index > $1 order by tx_index", 0);
	while (dao->hasNextEntry(dao)) {
		char *str = dao->getFieldValue(dao, "next");
		printf("#next is %s\n", str);
		dao->getNextEntry(dao);
	}

	const char *values[1] = { "2" };
	dao->execQueryParams(dao, "select next from syslog where tx_index = $1 order by tx_index", values, 1);
	while (dao->hasNextEntry(dao)) {
		char *str = dao->getFieldValue(dao, "next");
		printf("*next is %s\n", str);
		dao->getNextEntry(dao);
	}

	const char *values2[2] = { "0", "test" };
	dao->execQueryParams(dao, "select next, autack from syslog where tx_index > $1 and autack=$2 order by tx_index", values2, 2);
	while (dao->hasNextEntry(dao)) {
		int n = dao->getResultNbFields(dao);
		printf("**getResultNbFields is %d\n", n);
		int next = dao->getFieldValueAsIntByNum(dao, 0);
		printf("**next is %d\n", next);

		char *autack = dao->getFieldValueByNum(dao, 1);
		printf("**autack is %s\n", autack);

		dao->getNextEntry(dao);
	}
//
//	int idx = dao->newEntry(dao, "syslog");
//	printf("idx is %d\n", idx);
//	idx = dao->newEntry(dao, "syslog");
//	printf("idx is %d\n", idx);

	dao->closeDB(dao);

	// factoryEnd();

	puts("Main end");
	return EXIT_SUCCESS;
}
