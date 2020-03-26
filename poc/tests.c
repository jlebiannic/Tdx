/*
 ============================================================================
 Name        : Test.c
 Author      :
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(void) {


	const char select_format[] = "select %s from %s";
	char *query = NULL;

	int res = 0;

        //res = sprintf(query, select_format, "idx", "table_data");
	//printf("[sprintf]size query=%d\n", res);

	res = snprintf(query, 0, select_format, "idx", "table_data");
	printf("[sNprintf]size query=%d\n", res);
	query = malloc(res);
	snprintf(query, res, select_format, "idx", "table_data");
	printf("[sNprintf]query=%s\n", query);
	printf("[sNprintf]strlen query=%d\n", strlen(query));
	free(query);

	return EXIT_SUCCESS;
}


