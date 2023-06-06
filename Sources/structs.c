#include "structs.h"

#include <stdlib.h>

structs all_structs = {0};

void structs_init(void) {
	size_t size = 1024;

	free(all_structs.s);
	all_structs.s = (structy *)malloc(size * sizeof(structy));
	all_structs.size = 0;
}

structy *add_struct(void) {
	structy *s = &all_structs.s[all_structs.size];
	all_structs.size += 1;
	return s;
}
