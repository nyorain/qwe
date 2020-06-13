#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h> // isspace
#include <assert.h> // TODO

#define NEST_SEP '.'

typedef void (*parse_func)(struct parser* parser,
	 const char* name, unsigned name_len,
	 const char* value, unsigned value_len);

struct location {
	unsigned line;
	unsigned col;
	unsigned nest_depth;
};

struct parser {
	const char* input;
	struct location location;

	unsigned nest_len; // < sizeof(nest_buf)
	char nest_buf[512]; // null terminated

	parse_func cb;
	void* user;
};

enum error_type {
	error_type_none,
	error_type_unexpected_end,
	error_type_high_indentation,
	error_type_empty_name,
	error_type_mixed_table_array,
	error_type_empty_table_array,
	error_type_nest_too_long,
};


enum error_type parse_value(struct parser* parser, int* state);
enum error_type parse_table_or_array(struct parser* parser) {
	int state = 0; // 0: don't know, 1: table, 2: array
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
			return error_type_high_indentation;
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

		enum error_type err = parse_value(parser, &state);
		if(err != error_type_none) {
			return err;
		}
	}

	return (state == 0) ? error_type_empty_table_array : error_type_none;
}

enum error_type parse_value(struct parser* parser, int* state) {
	if(!parser->input || parser->input[0] == '\0') {
		return error_type_unexpected_end;
	}

	auto after = "\0";
	location after_loc = parser->location;
	auto nl = strchr(parser->input, '\n');
	if(nl) {
		after = nl + 1;
		after_loc.col = 0u;
		++after_loc.line;
	}

	auto sep = strchr(parser->input, ':');
	if(nl && sep > nl) {
		sep = NULL;
	}

	// we just have a single string value
	if(!sep) {
		if(*state == 1) {
			return error_type_mixed_table_array;
		}

		*state = 2;
		size_t val_size = nl ? nl - parser->input : strlen(parser->input);
		parser->cb(parser, NULL, 0, parser->input, val_size);
		parser->input = after;
		parser->location = after_loc;
		return error_type_none;
	}

	if(*state == 2) {
		return error_type_mixed_table_array;
	}
	*state = 1;

	// remove whitespace suffix in name
	const char* name = parser->input;
	const char* name_last = sep - 1;
	while(name_last >= name - 1 && isspace(name_last[0])) {
		--name_last;
	}

	// check that name is not empty
	if(name_last < name) {
		return error_type_empty_name;
	}

	unsigned name_len = 1 + name_last - name;
	char* name_buf = (char*) malloc(name_len + 1);
	memcpy(name_buf, name, name_len);
	name_buf[name_len] = '\0';

	// remove whitespace prefix in value
	const char* value = sep + 1;
	while((!nl || value < nl) && value[0] != '\0' && isspace(value[0])) {
		++value;
	}

	unsigned value_len = nl ? nl - value : strlen(value);

	// Value is not empty. We have found a table entry
	if(value_len > 0) {
		char* val_buf = (char*) malloc(value_len + 1);
		memcpy(val_buf, value, value_len);
		val_buf[value_len] = '\0';

		parser->cb(parser, name, name_len, value, value_len);
		parser->input = after;
		parser->location = after_loc;
		return error_type_none;
	}

	// Nested table/array.
	parser->input = after;
	parser->location = after_loc;

	unsigned prev_len = parser->nest_len;
	unsigned need_sep = parser->nest_len > 0 ? 1 : 0;
	if(parser->nest_len + name_len + 1 + need_sep >= sizeof(parser->nest_buf)) {
		return error_type_nest_too_long;
	}

	// separator (if it's not the first one)
	if(need_sep) {
		parser->nest_buf[parser->nest_len] = NEST_SEP;
		++parser->nest_len;
	}

	// new name
	memcpy(parser->nest_buf + parser->nest_len, name, name_len);
	parser->nest_len += name_len;

	// new (moved) terminator
	parser->nest_buf[parser->nest_len] = '\0';
	++parser->location.nest_depth;

	auto res = parse_table_or_array(parser);
	if(res == error_type_none) {
		--parser->location.nest_depth;
		parser->nest_len = prev_len;
		parser->nest_buf[parser->nest_len] = '\0';
	}

	return res;
}

