#pragma once

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h> // isspace
#include <assert.h> // TODO

#define NEST_SEP "."
#define ARRAY_SEP "."

#define MAX_LINE_SIZE 512
#define MAX_NEST_SIZE 512

// API
struct parser;
typedef void (*parse_func)(struct parser* parser,
	 const char* name, const char* value);

// fgets-like. Reads into parser->line_buf, can use parser->stream.
// Must return NULL on error.
typedef char*(*read_func)(struct parser* parser);

struct location {
	unsigned line;
	unsigned col;
	unsigned nest_depth;
};

struct parser {
	struct location location;

	bool line_valid;
	char line_buf[MAX_LINE_SIZE];

	unsigned nest_len; // < sizeof(nest_buf)
	char nest_buf[MAX_NEST_SIZE]; // always null terminated

	parse_func cb;
	void* user;

	read_func read;
	void* stream;
};

enum error_type {
	error_type_none = 0,
	error_type_unexpected_end = 1,
	error_type_high_indentation = 2,
	error_type_empty_name = 3,
	error_type_mixed_table_array = 4,
	error_type_empty_table_array = 5,
	error_type_nest_too_long = 6,
	error_type_line_too_long = 7,
};

struct parse_result {
	enum error_type error;
	struct parser parser;
};

struct parse_result parse_from_file(FILE* file, parse_func func, void* user);
struct parse_result parse_file(const char* filename, parse_func func, void* user);
struct parse_result parse_string(const char* str, parse_func func, void* user);

// Implementation
char* parser_read_fgets(struct parser* parser) {
	return fgets(parser->line_buf, sizeof(parser->line_buf), (FILE*) parser->stream);
}

char* parser_read_mem(struct parser* parser) {
	char* buf = (char*) parser->stream;
	if(!buf || buf[0] == '\0') {
		return NULL;
	}

	char* nl = strchr(buf, '\n');
	size_t writable = sizeof(parser->line_buf) - 1;
	if(nl) {
		size_t line = 1 + nl - buf;
		size_t count = line < writable ? line : writable;
		memcpy(parser->line_buf, buf, count);
		parser->line_buf[count] = '\0';
		parser->stream = buf + count;
		return buf;
	}

	size_t len = strlen(buf);
	size_t count = len < writable ? len : writable;
	memcpy(parser->line_buf, buf, count);
	// parser->line_buf[writable] = '\0'; // always there
	parser->stream = buf + count;
	return buf;
}

enum error_type parse_value(struct parser* parser, char* line, int* state);
enum error_type parse_table_or_array(struct parser* parser) {
	int state = 0; // 0: don't know, 1: table, 2: array
	unsigned n_items = 0u;
	while(parser->line_valid || parser->read(parser)) {
		char* input = parser->line_buf;
		char* start = input;
		parser->line_valid = false;
		while(start[0] != '\0' && start[0] == '\t') {
			++start;
		}

		// Comment, skip to next line.
		// Notice how this comes before the indent check.
		// Comments don't have to be aligned, we don't care.
		if(start[0] == '#') {
			const char* nl = strchr(start, '\n');
			if(nl == NULL) {
				// The line filled the whole buffer.
				// If we allocated memory, we could simply allocate
				// a larger buffer now.
				if(strlen(start) == sizeof(parser->line_buf) - 1) {
					return error_type_line_too_long;
				}

				// reached end of document
				parser->location.col += start - input;
				break;
			}

			++parser->location.line;
			parser->location.col = 0u;
			continue;
		}

		// empty lines are also always allowd
		if(start[0] == '\n') {
			assert(start[1] == '\0');
			++parser->location.line;
			parser->location.col = 0u;
			continue;
		}

		unsigned indent = start - input;

		// indentation is suddenly too high
		if(indent > parser->location.nest_depth) {
			return error_type_high_indentation;
		}

		// indentation is too low, line does not belong to this value anymore
		if(indent < parser->location.nest_depth) {
			parser->line_valid = true;
			break;
		}

		// check if input is empty now
		parser->location.col += indent;
		if(start[0] == '\0') {
			return error_type_unexpected_end;
		}

		// NOTE: extended array-nest syntax
		if(start[0] == '-' && start[1] == '\n') {
			if(state == 1) {
				return error_type_mixed_table_array;
			}

			parser->location.col = 0u;
			++parser->location.line;
			++parser->location.nest_depth;

			unsigned prev_len = parser->nest_len;
			size_t bufsz = sizeof(parser->nest_buf) - parser->nest_len;
			int count = snprintf(parser->nest_buf + parser->nest_len,
				bufsz, "%s%d", parser->nest_len ? ARRAY_SEP : "", n_items);

			assert(count >= 0);
			if(count >= (int) bufsz) {
				return error_type_nest_too_long;
			}

			parser->nest_len += count;

			state = 2;
			enum error_type err = parse_table_or_array(parser);
			if(err != error_type_none) {
				return err;
			}

			--parser->location.nest_depth;
			parser->nest_len = prev_len;
			parser->nest_buf[parser->nest_len] = '\0';
		} else {
			enum error_type err = parse_value(parser, start, &state);
			if(err != error_type_none) {
				return err;
			}
		}

		++n_items;
	}

	return (state == 0) ? error_type_empty_table_array : error_type_none;
}

enum error_type parse_value(struct parser* parser, char* line, int* state) {
	struct location after_loc = parser->location;
	char* nl = strchr(line, '\n');
	unsigned in_len; // only defined when !nl
	if(nl) {
		after_loc.col = 0u;
		++after_loc.line;
	} else {
		in_len = strlen(line);
		after_loc.col += in_len;
	}

	char* sep = strchr(line, ':');
	assert(!nl || !sep || sep < nl);

	// we just have a single string value
	if(!sep) {
		if(*state == 1) {
			return error_type_mixed_table_array;
		}

		*state = 2;

		// nullterminate input to allow passing it to the callback
		if(nl) {
			*nl = '\0';
		}

		parser->cb(parser, NULL, line);
		parser->location = after_loc;
		return error_type_none;
	}

	if(*state == 2) {
		return error_type_mixed_table_array;
	}
	*state = 1;

	char* name = line;
	char* name_last = sep - 1;

	// remove whitespace suffix in name
	// while(name_last >= name - 1 && isspace(name_last[0])) {
	// 	--name_last;
	// }

	// check that name is not empty
	if(name_last < name) {
		return error_type_empty_name;
	}

	name_last[1] = '\0';
	// unsigned name_len = 1 + name_last - name;

	const char* value = sep + 1;
	// remove whitespace prefix in value
	// while((!nl || value < nl) && value[0] != '\0' && isspace(value[0])) {
	// 	++value;
	// }
	if((!nl || value < nl) && value[0] == ' ') {
		++value;
	}

	unsigned value_len = nl ? nl - value : in_len - (value - line);

	// Value is not empty. We have found a table entry
	if(value_len > 0) {
		// null-terminate name and value
		// name_last[1] = '\0';
		if(nl) {
			*nl = '\0';
		}

		parser->cb(parser, name, value);
		parser->location = after_loc;
		return error_type_none;
	}

	// Nested table/array.
	// parser->input = after;
	parser->location = after_loc;

	unsigned prev_len = parser->nest_len;
	// unsigned need_sep = parser->nest_len > 0 ? 1 : 0;
	// unsigned needed = name_len + 1 + need_sep;
	// if(parser->nest_len + needed >= sizeof(parser->nest_buf)) {
	// 	return error_type_nest_too_long;
	// }

	size_t bufsz = sizeof(parser->nest_buf) - parser->nest_len;
	int count = snprintf(parser->nest_buf + parser->nest_len,
		bufsz, "%s%s", parser->nest_len ? NEST_SEP : "", name);

	assert(count >= 0);
	if(count >= (int) bufsz) {
		return error_type_nest_too_long;
	}

	parser->nest_len += count;

	/*
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
	*/
	++parser->location.nest_depth;

	enum error_type res = parse_table_or_array(parser);
	if(res == error_type_none) {
		--parser->location.nest_depth;
		parser->nest_len = prev_len;
		parser->nest_buf[parser->nest_len] = '\0';
	}

	return res;
}

struct parse_result parse_from_file(FILE* file, parse_func func, void* user) {
	struct parse_result res = {};
	res.parser.stream = file;
	res.parser.read = parser_read_fgets;
	res.parser.cb = func;
	res.parser.user = user;
	res.error = parse_table_or_array(&res.parser);
	return res;
}

struct parse_result parse_string(const char* str, parse_func func, void* user) {
	struct parse_result res = {};
	res.parser.stream = (void*) str;
	res.parser.read = parser_read_mem;
	res.parser.cb = func;
	res.parser.user = user;
	res.error = parse_table_or_array(&res.parser);
	return res;
}

struct parse_result parse_file(const char* filename, parse_func func, void* user) {
	struct parse_result res = {};
	res.parser.stream = fopen(filename, "r");
	res.parser.read = parser_read_fgets;
	res.parser.cb = func;
	res.parser.user = user;
	res.error = parse_table_or_array(&res.parser);
	return res;
}
