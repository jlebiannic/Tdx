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
		printf("next is %s\n", str);
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
