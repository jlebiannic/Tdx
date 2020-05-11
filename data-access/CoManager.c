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
#include "commons/commons.h"

int main(void) {
	puts("Main start");

	Dao *dao = daoFactory_create(1);
	dao->openDB(dao, NULL);

	const char *fields[3] = { "Ci1", "Ct2", "Cn3" };
	const char *types[3] = { "INTEGER", "TEXT", "NUMERIC" };
	dao->createTable(dao, "testDaoCreateTable", fields, types, 3, 0);
	
	const char *indexeFields[3] = { "Ct2", "Cn3" };
	dao->createIndex(dao, "testDaoCreateTable", "IDX_TEST", indexeFields, 2);

	dao->execQuery(dao, "select next from syslog where tx_index > 0");
	while (dao->hasNextEntry(dao)) {
		char *str = dao->getFieldValue(dao, "next");
		printf("next is %s\n", str);
		dao->getNextEntry(dao);
	}

	// dao->execQueryParamsMultiResults(dao, "select next from syslog where tx_index > $1 order by tx_index", 0);
	char *fields0[1] = { "next" };
	char *values0[1] = { "0" };
	dao->getEntries(dao, "syslog", (const char**) fields0, 1, "tx_index > $ order by tx_index", (const char**) values0);
	while (dao->hasNextEntry(dao)) {
		char *str = dao->getFieldValue(dao, "next");
		printf("#next is %s\n", str);
		dao->getNextEntry(dao);
	}

	const char *values1[1] = { "2" };
	dao->execQueryParams(dao, "select next from syslog where tx_index = $1 order by tx_index", values1, 1);
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

	char *fields3[2] = { "autack", "info" };
	char *values3[2] = { "a updated", "i updated" };
	dao->updateEntries(dao, "syslog", (const char**) fields3, (const char**) values3, 2, "autack='0'");

	const char *values4[1] = { "a updated" };
	dao->execQueryParams(dao, "select info, autack from syslog where autack=$1 order by tx_index", values4, 1);
	while (dao->hasNextEntry(dao)) {
		char *info = dao->getFieldValueByNum(dao, 0);
		printf("$info is %s\n", info);
		char *autack = dao->getFieldValueByNum(dao, 1);
		printf("$autack is %s\n", autack);

		dao->getNextEntry(dao);
	}

//	int idx = dao->newEntry(dao, "syslog");
//	printf("idx is %d\n", idx);
//	idx = dao->newEntry(dao, "syslog");
//	printf("idx is %d\n", idx);

	dao->closeDB(dao);

	char **arr = NULL;
	arr = (char**) malloc(sizeof(char**));

	int cpt = 0;
	arrayAddElement(arr, "test", cpt, FALSE, TRUE);
	cpt++;
	arrayAddIntElement(arr, 123, cpt, TRUE);
	cpt++;
	arrayAddDoubleElement(arr, 456.789, cpt, TRUE);
	cpt++;
	time_t timestamp = time( NULL);
	arrayAddTimeElement(arr, timestamp, cpt, TRUE);

	int i = 0;
	for (i = 0; i <= cpt; i++) {
		printf("arr[%d]=%s\n", i, arr[i]);
	}

	// factoryEnd();

	puts("Main end");
	return EXIT_SUCCESS;
}
