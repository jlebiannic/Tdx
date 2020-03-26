#ifndef __BASES_H
#define __BASES_H

#include "logsysrem.h"

baseops_t *init_nilops(void);
baseops_t *init_legacyops(void);
baseops_t *init_odbsysops(void);
baseops_t *init_sdbsysops(void);

void init_bases(void);
void init_base(baseops_t *(*init)());
#endif
