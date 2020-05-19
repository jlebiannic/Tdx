/*
 * test_daservice.c
 *
 *  Created on: 18 mai 2020
 *
 */

#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include <stdarg.h>
#include "commons/commons.h"
#include "dao/dao.h"
#include "commons/daoFactory.h"

static const char *TEST_TABLE1 = "test_table1";
static const char *TX_INDEX = "TX_INDEX";
static const char *STATUS = "STATUS";
static const char *VAL = "VAL";
static const char *CREATED = "CREATED";
static const char *MODIFIED = "MODIFIED";


static void assertTrue(char *functionName, const char *paramName, int res) {
	if (res != TRUE) {
		fprintf(stderr, "ERROR: %s, résultat pour %s attendu %d mais obtenu %d\n", functionName, paramName, TRUE, res);
		exit(1);
	} else {
		printf("OK: %s, assertTrue %s is true\n", functionName, paramName);
	}
}

static void assertIntEquals(char *functionName, const char *paramName, int val1, int val2) {
	if (val1 != val2) {
		fprintf(stderr, "ERROR: %s, %s est différent de %d\n", functionName, paramName, val2);
		exit(1);
	} else {
		printf("OK: %s, assertIntEquals %s=%d\n", functionName, paramName, val2);
	}
}

static void assertIntNotEquals(char *functionName, const char *paramName, int val1, int val2) {
	if (val1 == val2) {
		fprintf(stderr, "ERROR: %s, %s est égal à %d\n", functionName, paramName, val2);
		exit(1);
	} else {
		printf("OK: %s, assertIntNotEquals %s<>%d\n", functionName, paramName, val2);
	}
}

static void assertStrEquals(char *functionName, const char *paramName, const char *val1, const char *val2) {
	if (strcmp(val1, val2) != 0) {
		fprintf(stderr, "ERROR: %s, %s est différent de %s\n", functionName, paramName, val2);
		exit(1);
	} else {
		printf("OK: %s, assertStrEquals %s=%s\n", functionName, paramName, val2);
	}
}

static void assertDblEquals(char *functionName, const char *paramName, double val1, double val2) {
	if (val1 != val2) {
		fprintf(stderr, "ERROR: %s, %s est différent de %f\n", functionName, paramName, val2);
		exit(1);
	} else {
		printf("OK: %s, assertStrEquals %s=%f\n", functionName, paramName, val2);
	}
}

// ==========================================================================

static void test_createTable(Dao *dao) {
	const char *fields[5] = { TX_INDEX, STATUS, VAL, CREATED, MODIFIED };
	const char *types[5] = { "INTEGER", "TEXT", "NUMERIC", "INTEGER", "INTEGER" };
	int res = dao->createTable(dao, TEST_TABLE1, fields, types, 5, 0, TRUE);
	assertTrue("test_createTable", "res", res);
}

static void test_createIndex(Dao *dao) {
	const char *indexeFields[3] = { "TX_INDEX", "VAL" };
	int res = dao->createIndex(dao, TEST_TABLE1, "IDX_TEST_TABLE1", indexeFields, 2);
	assertTrue("test_createIndex", "res", res);
}

static int test_newEntry(Dao *dao) {
	int idx = dao->newEntry(dao, TEST_TABLE1);
	assertIntNotEquals("test_newEntry", "idx", idx, 0);
	return idx;
}

static void test_findEntry(Dao *dao, int idx) {
	const char *fields[1] = { TX_INDEX };
	const char *values[1] = { inttoa(idx) };
	int res = dao->getEntries(dao, TEST_TABLE1, fields, 1, "tx_index=$", values, FALSE);
	assertTrue("test_findEntry", "res", res);
	assertIntEquals("test_findEntry", "getNbResults", dao->getNbResults(dao), 1);
	assertIntEquals("test_findEntry", "TX_INDEX", dao->getFieldValueAsInt(dao, TX_INDEX), idx);
}

static void test_updateEntries(Dao *dao, int idx, const char *status, const char *val) {
	const char *fields[2] = { STATUS, VAL };
	const char *values[2] = { status, val };
	const char *filterValues[1] = { inttoa(idx) };
	int res = dao->updateEntries(dao, TEST_TABLE1, fields, values, 2, "TX_INDEX=$", filterValues);
	assertTrue("test_updateEntries", "res", res);
}

static void test_verifyEntry(Dao *dao, int idx, const char *status, double val) {
	const char *fields[2] = { STATUS, VAL };
	const char *values[1] = { inttoa(idx) };
	int res = dao->getEntries(dao, TEST_TABLE1, fields, 2, "tx_index=$", values, FALSE);
	assertTrue("test_verifyEntry", "res", res);
	assertIntEquals("test_verifyEntry", "getNbResults", dao->getNbResults(dao), 1);
	assertStrEquals("test_verifyEntry", "STATUS", dao->getFieldValue(dao, STATUS), status);
	assertDblEquals("test_verifyEntry", "VAL", dao->getFieldValueAsDouble(dao, VAL), val);
}

static void test_verifyEntry2(Dao *dao, int idx1, int idx2, const char *status1, double val1, const char *status2, double val2) {
	const char *fields[2] = { STATUS, VAL };
	const char *values[2] = { inttoa(idx1), inttoa(idx2) };
	int res = dao->getEntries(dao, TEST_TABLE1, fields, 2, "tx_index=$ or tx_index=$ order by tx_index", values, TRUE);
	assertTrue("test_verifyEntry2", "res", res);
	assertIntEquals("test_verifyEntry2", "getNbResults", dao->getNbResults(dao), 2);
	//assertStrEquals("test_newEntry", "STATUS", dao->getFieldValue(STATUS), status);
	//assertDblEquals("test_newEntry", "VAL", dao->getFieldValueAsDoubleByNum(VAL), val);
	int cpt = 0;
	while (dao->hasNextEntry(dao)) {
		char *status = dao->getFieldValueByNum(dao, 0);
		double val = dao->getFieldValueAsDoubleByNum(dao, 1);
		if (cpt == 0) {
			assertStrEquals("test_verifyEntry2", "STATUS", status, status1);
			assertDblEquals("test_verifyEntry2", "VAL", val, val1);
		} else {
			assertStrEquals("test_verifyEntry2", "STATUS", status, status2);
			assertDblEquals("test_verifyEntry2", "VAL", val, val2);
		}
		dao->getNextEntry(dao);
		cpt++;
	}
	assertIntEquals("test_verifyEntry2", "cpt", cpt, 2);
}

int main(int argc, char **argv) {

	Dao *dao = daoFactory_create(1);
	int res = dao->openDB(dao, NULL);
	assertTrue("openDB", "res", res);

	test_createTable(dao);
	test_createIndex(dao);
	int idx = test_newEntry(dao);
	test_findEntry(dao, idx);
	test_updateEntries(dao, idx, "Modified", "123.456");
	test_verifyEntry(dao, idx, "Modified", 123.456);
	int idx2 = test_newEntry(dao);
	test_updateEntries(dao, idx2, "Modified2", "789.01");
	test_verifyEntry2(dao, idx, idx2, "Modified", 123.456, "Modified2", 789.01);

	return 0;
}
