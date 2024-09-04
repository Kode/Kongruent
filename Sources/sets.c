#include "sets.h"

#include "errors.h"

static descriptor_set sets[MAX_SETS];
static size_t sets_count = 0;

descriptor_set *add_set(name_id name) {
	for (size_t set_index = 0; set_index < sets_count; ++set_index) {
		if (sets[set_index].name == name) {
			return &sets[set_index];
		}
	}

	if (sets_count >= MAX_SETS) {
		debug_context context = {0};
		error(context, "Max set count of %i reached", MAX_SETS);
		return NULL;
	}

	descriptor_set *new_set = &sets[sets_count];
	new_set->name = name;
	new_set->index = (uint32_t)sets_count;
	new_set->definitions_count = 0;

	sets_count += 1;

	return new_set;
}

void add_definition_to_set(descriptor_set *set, definition def) {
	assert(def.kind != DEFINITION_FUNCTION && def.kind != DEFINITION_STRUCT);

	for (size_t definition_index = 0; definition_index < set->definitions_count; ++definition_index) {
		if (set->definitions[definition_index].global = def.global) {
			return;
		}
	}

	assert(get_global(def.global)->set == NULL);
	get_global(def.global)->set = set;
	set->definitions[set->definitions_count] = def;
	set->definitions_count += 1;
}