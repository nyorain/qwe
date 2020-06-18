#pragma once

#include "common.hpp"
#include "data.hpp"

std::string print(const Value& val, unsigned indent = 0u, bool inArray = false) {
	return std::visit(Visitor{
		[](const std::string& sv) {
			return sv;
		}, [&](const Table& table) {
			std::string cat;
			if(inArray) {
				cat += "-";
				++indent;
			}

			auto sep = indent > 0 ? "\n" : "";
			for(auto& val : table) {
				std::string str(indent, '\t');
				str = sep + str;
				str += val.first;
				str += ": ";
				str += print(*val.second, indent + 1);
				cat += str;
				sep = "\n";
			}

			return cat;
		}, [&](const Vector& vec) {
			std::string cat;
			if(inArray) {
				cat += "-";
				++indent;
			}

			auto sep = indent > 0 ? "\n" : "";
			for(auto& val : vec) {
				std::string str(indent, '\t');
				str = sep + str;
				str += print(*val, indent + 1, true);
				cat += str;
				sep = "\n";
			}
			return cat;

		},
	}, val.value);
}

