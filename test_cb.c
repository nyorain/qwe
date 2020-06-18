#include "parse_cb.h"
#include <stdio.h>

void handler(struct parser* p, const char* name, const char* value) {
	const char* file = (const char*) p->user;
	printf("[");
	if(p->nest_len > 0) {
		printf("%s, ", p->nest_buf);
	}

	printf("%s:%d:%d] ", file, p->location.line + 1, p->location.col + 1);
	if(name) {
		printf("'%s': ", name);
	}

	printf("'%s'\n", value);
}

int main(int argc, const char** argv) {
	if(argc < 2) {
		printf("No input file given\n");
		return EXIT_FAILURE;
	}

	struct parse_result res = parse_file(argv[1], handler, (void*) argv[1]);
	if(res.error != error_type_none) {
		struct parser* p = &res.parser;
		printf("Error: %d at %s, %d:%d\n", res.error,
			p->nest_buf, p->location.line + 1, p->location.col + 1);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

