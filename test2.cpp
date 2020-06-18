#include "print.hpp"
#include "parse.hpp"
#include "util.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <iostream>

// TODO: test this api
struct Dummy {
	int a;
	float b;
};

template<>
struct ValueParser<Dummy> : public PodParser<Dummy> {
	template<typename DummyCV>
	static constexpr auto map(DummyCV& dummy) {
		return std::tuple {
			MapEntry{"a", dummy.a},
			MapEntry{"b", dummy.b},
		};
	}
};

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

int main(int argc, const char** argv) {
	if(argc < 2) {
		std::printf("No input file given\n");
		return EXIT_FAILURE;
	}

	auto file = readFile(argv[1]);
	file.c_str(); // make sure it's null terminated...
	Parser parser{file};

	auto pr = parseTableOrArray(parser);
	if(auto err = std::get_if<Error>(&pr); err) {
		std::cout << print(*err) << "\n";
	} else {
		std::cout << print(std::get<NamedValue>(pr).value) << "\n";
	}
}
