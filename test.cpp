#include "parse_cb.h"
#include <cstdio>

void handler(struct parser* p, const char* name, const char* value) {
	const char* file = (const char*) p->user;
	std::printf("[");
	if(p->nest_len > 0) {
		std::printf("%s, ", p->nest_buf);
	}

	std::printf("%s:%d:%d] ", file, p->location.line + 1, p->location.col + 1);
	if(name) {
		std::printf("'%s': ", name);
	}

	std::printf("'%s'\n", value);
}

int main(int argc, const char** argv) {
	if(argc < 2) {
		std::printf("No input file given\n");
		return EXIT_FAILURE;
	}

	auto res = parse_file(argv[1], handler, (void*) argv[1]);
	if(res.error != error_type_none) {
		struct parser* p = &res.parser;
		std::printf("Error: %d at %s, %d:%d\n", res.error,
			p->nest_buf, p->location.line + 1, p->location.col + 1);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
