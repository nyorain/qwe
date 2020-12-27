#pragma once

#include "data.hpp"

inline std::string print(const Table& table, unsigned indent = 0u) {
	std::string ret;

	// TODO: properly escape ':' and backslash again?
	// TODO: support line breaks via multi-line strings?

	std::string indentStr(indent, '\t');
	for(auto& entry : table) {
		ret += indentStr;
		ret += entry.first;
		if(entry.second.empty()) {
			ret += "\n";
			continue;
		}

		ret += ": ";
		if(entry.second.size() == 1 && entry.second[0].second.empty()) {
			ret += entry.second[0].first;
			ret += "\n";
			continue;
		}

		ret += "\n";
		ret += print(entry.second, indent + 1);
	}

	return ret;
}

