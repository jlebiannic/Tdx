#include <stdio.h>

/* Dummy function used with non syntax to syntax translators */
int tr_set_out_variables() {
#ifdef DEBUG
    fprintf(stderr," -- Dummy set_out_variables function used.\n");
#endif
    return 1;
}
