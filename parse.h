#pragma once

#include "data.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h> // isspace
#include <assert.h> // TODO

struct location {
	unsigned line;
	unsigned col;

	unsigned nest_depth;
	const char** nest_tables;// not owned, except for parser.location
};

struct parser {
	const char* input;
	location location;
};

enum error_type {
	error_type_none,
	error_type_unexpected_end,
	error_type_high_indentation,
	error_type_duplicate_name,
	error_type_empty_name,
	error_type_mixed_table_array,
	error_type_empty_table_array,
};

struct error {
	enum error_type type;
	struct location location;
	const char* data; // dependent on 'type'
};

struct parse_result {
	bool success;
	struct table_entry value; // for array values: name is NULL
	struct error error;
};

struct parse_result parse_value(struct parser* parser);

struct parse_result parse_table_or_array(struct parser* parser) {
	struct value parsed = {
		.type = value_type_string, // don't know yet if vector or table
		// rest is zero-initialized
	};

	while(parser->input && parser->input[0] != '\0') {
		const char* start = parser->input;
		while(start[0] != '\0' && start[0] == '\t') {
			++start;
		}

		// Comment, skip to next line.
		// Notice how this comes before the indent check.
		// Comments don't have to be aligned, we don't care.
		if(start[0] == '#') {
			const char* nl = strchr(start, '\n');
			if(nl == NULL) {
				// reached end of document
				parser->location.col += start - parser->input;
				parser->input = "\0";
				break;
			}

			++parser->location.line;
			parser->location.col = 0u;
			parser->input = nl + 1;
			continue;
		}

		// empty lines are also always allowd
		if(start[0] == '\n') {
			++parser->location.line;
			parser->location.col = 0u;
			parser->input = start + 1;
			continue;
		}

		unsigned indent = start - parser->input;

		// indentation is suddenly too high
		if(indent > parser->location.nest_depth) {
			destroy_value(&parsed);
			return (struct parse_result){
				.success = false,
				.error = {
					.type = error_type_high_indentation,
					.location = parser->location,
				}
			};
		}

		// indentation is too low, line does not belong to this value anymore
		if(indent < parser->location.nest_depth) {
			break;
		}

		// check if input is empty now
		parser->location.col += indent;
		parser->input = start;
		if(start[0] == '\0') {
			break;
		}

		location ploc = parser->location; // save it for later
		parse_result res = parse_value(parser);
		if(!res.success) {
			destroy_value(&parsed);
			return res;
		}

		enum value_type type = (res.value.name == NULL) ?
			value_type_vector : value_type_table;
		if(parsed.type == value_type_string) { // dummy value for first entry
			parsed.type = type;
		} else if(parsed.type != type) {
			destroy_value(&parsed);
			destroy_value(&res.value.value);
			free((void*) res.value.name);

			return (struct parse_result){
				.success = false,
				.error = {
					.type = error_type_mixed_table_array,
					.location = ploc,
				}
			};
		}

		if(parsed.type == value_type_vector) {
			vector* v = &parsed.vector;
			++v->n_values;
			size_t nsize = sizeof(*v->values) * v->n_values;
			v->values = (struct value*) realloc(v->values, nsize);
			v->values[v->n_values - 1] = res.value.value;
		} else { // table
			// check for duplicate entry
			table* t = &parsed.table;
			for(size_t i = 0u; i < t->n_entries; ++i) {
				if(!strcmp(t->entries[i].name, res.value.name)) {
					return (struct parse_result) {
						.success = false,
						.error = {
							.type = error_type_duplicate_name,
							.location = parser->location,
							.data = res.value.name,
						}
					};
				}
			}

			// insert
			++t->n_entries;
			size_t nsize = sizeof(*t->entries) * t->n_entries;
			t->entries = (struct table_entry*) realloc(t->entries, nsize);
			t->entries[t->n_entries - 1] = res.value;
		}
	}

	if(parsed.type == value_type_string) { // we couldn't parse a single entry
		// nothing to free in this case, 'parsed' is empty
		return (struct parse_result) {
			.success = false,
			.error = {
				.type = error_type_mixed_table_array,
				.location = parser->location,
			}
		};
	}

	return (struct parse_result) {
		.success = true,
		.value = {NULL, parsed},
	};
}

struct parse_result parse_value(struct parser* parser) {
	if(!parser->input || parser->input[0] == '\0') {
		return (struct parse_result) {
			.success = false,
			.error = {
				.type = error_type_unexpected_end,
				.location = parser->location,
			}
		};
	}

	const char* after = "\0";
	location after_loc = parser->location;
	const char* nl = strchr(parser->input, '\n');
	if(nl) {
		after = nl + 1;
		after_loc.col = 0u;
		++after_loc.line;
	}

	const char* sep = strchr(parser->input, ':');
	if(nl && sep > nl) {
		sep = NULL;
	}

	// we just have a single string value
	if(!sep) {
		size_t buf_size = nl ? nl - parser->input : strlen(parser->input);
		char* buf = (char*) malloc(buf_size + 1);
		memcpy(buf, parser->input, buf_size);
		buf[buf_size] = '\0';

		parser->input = after;
		parser->location = after_loc;
		return (struct parse_result) {
			.success = true,
			.value = {
				.name = NULL,
				.value = {
					.type = value_type_string,
					.string = buf,
				}
			}
		};
	}

	const char* name = parser->input;
	const char* name_last = sep - 1;
	// remove whitespace suffix in name
	// while(name_last >= name - 1 && isspace(name_last[0])) {
	// 	--name_last;
	// }

	// check that name is not empty
	if(name_last < name) {
		return (struct parse_result) {
			.success = false,
			.error = {
				.type = error_type_empty_name,
				.location = parser->location,
			},
		};
	}

	unsigned name_len = 1 + name_last - name;
	char* name_buf = (char*) malloc(name_len + 1);
	memcpy(name_buf, name, name_len);
	name_buf[name_len] = '\0';

	const char* value = sep + 1;
	// remove whitespace prefix in value
	// while((!nl || value < nl) && value[0] != '\0' && isspace(value[0])) {
	// 	++value;
	// }
	if((!nl || value < nl) && value[0] == ' ') {
		++value;
	}

	unsigned value_len = nl ? nl - value : strlen(value);

	// Value is not empty. We have found a table entry
	if(value_len > 0) {
		char* val_buf = (char*) malloc(value_len + 1);
		memcpy(val_buf, value, value_len);
		val_buf[value_len] = '\0';

		parser->input = after;
		parser->location = after_loc;
		return (struct parse_result) {
			.success = true,
			.value = {
				.name = name_buf,
				.value = {
					.type = value_type_string,
					.string = val_buf,
				}
			}
		};
	}

	// value[0] == '\0' mean after[0] == '\0'. This case is already handled
	// as 'empty table or array' error in parse_table_or_array below.

	// if it's neither a table assignment or an array value,
	// we have a nested table.
	parser->input = after;
	parser->location = after_loc;

	struct location* l = &parser->location;
	size_t nd = ++l->nest_depth;
	size_t nsize = sizeof(*l->nest_tables) * l->nest_depth;
	l->nest_tables = (const char**) realloc(l->nest_tables, nsize);
	l->nest_tables[l->nest_depth - 1] = name_buf;

	struct parse_result res = parse_table_or_array(parser);
	if(!res.success) {
		return res;
	}

	assert(l->nest_depth == nd);
	assert(!strcmp(l->nest_tables[l->nest_depth - 1], name_buf));

	// pop, we move ownership of name_buf to res.value.name
	--l->nest_depth;
	assert(!res.value.name);
	res.value.name = name_buf;
	return res;
}
