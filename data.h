#pragma once

// NOTE: for efficiency, we should use a hashmap as table
struct table {
	unsigned n_entries;
	struct table_entry* entries; // owned
};

struct vector {
	unsigned n_values;
	struct value* values; // owned
};

enum value_type {
	value_type_string,
	value_type_vector,
	value_type_table,
};

struct value {
	enum value_type type;
	union {
		const char* string; // owned
		struct vector vector;
		struct table table;
	};
};

struct table_entry {
	const char* name; // owned
	struct value value; // owned
};

#include <stdlib.h>

void destroy_value(const value* val) {
	switch(val->type) {
		case value_type_string:
			free((void*) val->string);
			break;
		case value_type_vector:
			for(size_t i = 0u; i < val->vector.n_values; ++i) {
				destroy_value(&val->vector.values[i]);
			}

			free(val->vector.values);
			break;
		case value_type_table:
			for(size_t i = 0u; i < val->table.n_entries; ++i) {
				destroy_value(&val->table.entries[i].value);
				free((void*) val->table.entries[i].name);
			}

			free(val->vector.values);
			break;
	}
}
