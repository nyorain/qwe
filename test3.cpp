#include "s2/parse.hpp"
#include "s2/print.hpp"
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

std::string print(const Error& err) {
	std::string res;
	res.reserve(50);

	// #1 location
	res += "[";

	if(!err.location.nest.empty()) {
		auto sep = "";
		for(auto& n : err.location.nest) {
			res += sep;
			res += n;
			sep = ".";
		}

		res += ", ";
	}

	res += std::to_string(err.location.line);
	res += ":";
	res += std::to_string(err.location.col);
	res += "]: ";

	// #2 message
	switch(err.type) {
		case ErrorType::unexpectedEnd:
			res += "Unexpected input end";
			break;
		case ErrorType::highIndentation:
			res += "Unexpected high indentation level";
			break;
		case ErrorType::lowIndentation:
			res += "Unexpected low indentation level";
			break;
		case ErrorType::none:
			res += "No error. Data: ";
			res += err.data;
			break;
	}

	return res;
}

int main(int argc, const char** argv) {
	if(argc < 2) {
		std::printf("No input file given\n");
		return EXIT_FAILURE;
	}

	auto file = readFile(argv[1]);
	file.c_str(); // make sure it's null terminated...
	Parser parser{file};

	Error error;
	auto table = parseTable(parser, error);
	assert(parser.input.empty());
	// assert(false);
	if(error.type != ErrorType::none) {
		std::cout << print(error) << "\n";
	} else {
		std::cout << print(table);
	}
}
