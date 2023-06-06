#include "structs.h"

#include <assert.h>
#include <stdlib.h>

structs all_structs = {0};

#define MAX_STRUCTS 1024

void structs_init(void) {
	free(all_structs.s);
	all_structs.s = (structy *)malloc(MAX_STRUCTS * sizeof(structy));
	all_structs.size = 0;
}

structy *add_struct(void) {
	assert(all_structs.size < MAX_STRUCTS);

	structy *s = &all_structs.s[all_structs.size];
	all_structs.size += 1;
	return s;
}

structy *get_struct(name_id name) {
	for (size_t i = 0; i < all_structs.size; ++i) {
		if (all_structs.s[i].name == name) {
			return &all_structs.s[i];
		}
	}

	return NULL;
}
