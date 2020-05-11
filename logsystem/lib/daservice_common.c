#include <stdlib.h>
#include <string.h>

const char* strPrefixColName(const char *name) {
	if (strcmp(name, "INDEX") == 0) {
		return "TX_";
	} else {
		return "";
	}
}

