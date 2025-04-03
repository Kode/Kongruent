#include "transformer.h"

#include "functions.h"

#include <assert.h>
#include <string.h>

opcodes new_code;

void transform(uint32_t flags) {
	for (function_id i = 0; get_function(i) != NULL; ++i) {
		function *f = get_function(i);

		if (f->block == NULL) {
			continue;
		}

		uint8_t *data = f->code.o;
		size_t   size = f->code.size;

		f->code;
		new_code.size = 0;

		size_t index = 0;
		while (index < size) {
			opcode *o = (opcode *)&data[index];

			switch (o->type) {
			default: {
				uint8_t *new_data = &new_code.o[new_code.size];

				assert(new_code.size + o->size < OPCODES_SIZE);

				uint8_t *location = &new_code.o[new_code.size];

				memcpy(&new_code.o[new_code.size], o, o->size);

				new_code.size += o->size;
			}
			}

			index += o->size;
		}

		f->code = new_code;
	}
}
