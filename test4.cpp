#include "s2/parse2.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <iostream>

std::string readFile(std::string_view filename) {
	auto openmode = std::ios::ate;
	std::ifstream ifs(std::string{filename}, openmode);
	ifs.exceptions(std::ostream::failbit | std::ostream::badbit);

	auto size = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	std::string buffer;
	buffer.resize(size);
	auto data = reinterpret_cast<char*>(buffer.data());
	ifs.read(data, size);

	return buffer;
}

struct Handler {
	void enterTable(Parser& p, std::string_view name) {
		for(auto i = 0u; i < p.location.nest; ++i) {
			std::cout << "\t";
		}

		std::cout << name << ":\n";
	}

	void exitTable(Parser&) {
	}

	void entry(Parser& p, std::string_view value) {
		for(auto i = 0u; i < p.location.nest; ++i) {
			std::cout << "\t";
		}

		std::cout << value << "\n";
	}
};

int main(int argc, const char** argv) {
	if(argc < 2) {
		std::printf("No input file given\n");
		return EXIT_FAILURE;
	}

	auto file = readFile(argv[1]);
	file.c_str(); // make sure it's null terminated...
	Parser parser{file};

	Handler h;
	Error err;
	parseTable(h, parser, err);
}
