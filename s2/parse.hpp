#pragma once

#include "data.hpp"
#include <vector>
#include <string>
#include <string_view>
#include <cassert>
#include <cstdio> // TODO: remove

struct Location {
	unsigned line {};
	unsigned col {};
	std::vector<std::string_view> nest {};
};

struct Parser {
	std::string_view input;
	Location location {};
};

enum class ErrorType {
	none,
	unexpectedEnd,
	highIndentation,
	lowIndentation, // only for multiline-strings
};

struct Error {
	ErrorType type;
	Location location;
	std::string_view data {}; // dependent on 'type'
};

Table parseTable(Parser&, Error& error);

inline std::string parseString(Parser& parser, Error& error) {
	error = {ErrorType::none};
	auto i = std::size_t(0);
	auto indent = parser.location.nest.size();
	std::string ret;

	while(i < parser.input.size()) {
		if(i == 0u && parser.input[i] == '\t') {
			error = {ErrorType::highIndentation, parser.location};
			return {};
		}

		if(parser.input[i] == '\\') {
			if(parser.input.size() == i + 1) {
				// NOTE: weird case. input ends on backslash
				ret += '\\';
				parser.input = {};
				return ret;
			}

			++i;
			++parser.location.col;

			if(parser.input[i] == '\\')  {
				ret += '\\';
			} else if(parser.input[i] == '\n') {
				// nothing to append
				parser.location.col = 0;
				++parser.location.line;

				if(parser.input.size() < i + indent) {
					error = {ErrorType::unexpectedEnd, parser.location};
					return {};
				}

				if(parser.input.find_first_not_of("\t", i) != i + indent) {
					error = {ErrorType::lowIndentation, parser.location};
					return {};
				}

				parser.location.col += indent;
				i += indent;
			} else if(parser.input[i] == ':') {
				ret += ':';
			}
		} else if(parser.input[i] == ':' || parser.input[i] == '\n') {
			break;
		} else {
			ret += parser.input[i];
		}

		++i;
		++parser.location.col;
	}

	parser.input = parser.input.substr(i);
	return ret;
}

inline std::pair<std::string, Table> parseEntry(Parser& parser, Error& error, bool& success) {
	error = {ErrorType::none};
	success = false;

	auto first = parser.input.npos;
	std::string_view after;

	while(!parser.input.empty()) {
		after = parser.input;
		first = after.find_first_not_of('\t');

		if(first == after.npos) {
			parser.location.col += first;
			parser.input = {}; // reached end of document
			return {};
		}

		// Comment, skip to next line.
		// Notice how this comes before the indent check.
		// Comments don't have to be aligned, we don't care.
		if(after[first] == '#') {
			auto nl = after.find('\n');
			if(nl == after.npos) {
				// reached end of document
				parser.location.col += parser.input.size();
				parser.input = {};
				return {};
			}

			++parser.location.line;
			parser.location.col = 0u;
			parser.input = after.substr(nl + 1);
			continue;
		}

		// Empty lines are also always allowed
		if(after[first] == '\n') {
			++parser.location.line;
			parser.location.col = 0u;
			parser.input = after.substr(first + 1);
			continue;
		}

		break;
	}

	if(parser.input.empty()) {
		return {};
	}

	// indentation is suddenly too high
	if(first > parser.location.nest.size()) {
		error = {ErrorType::highIndentation, parser.location};
		return {};
	}

	// indentation is too low, line does not belong to this value anymore
	if(first < parser.location.nest.size()) {
		return {};
	}

	parser.location.col += first;
	parser.input = after.substr(first);
	if(parser.input.empty()) {
		error = Error{ErrorType::unexpectedEnd, parser.location};
		return {};
	}

	auto name = parseString(parser, error);
	if(error.type != ErrorType::none) {
		return {};
	}

	if(parser.input.empty() || parser.input[0] == '\n') {
		success = true;
		return {std::move(name), {}};
	}

	assert(parser.input[0] == ':');

	auto tablePos = parser.input.find_first_not_of("\t ", 1);
	if(tablePos == parser.input.npos) {
		// empty table of form `name:` not allowed per grammar
		// at least newline is needed afterwards for proper empty table
		error = {ErrorType::unexpectedEnd, parser.location};
		return {};
	}

	parser.location.col += tablePos;
	parser.input = parser.input.substr(tablePos);

	// parse table mapping dst entry
	parser.location.nest.push_back(name);

	Table table;
	if(parser.input[0] == '\n') {
		++parser.location.line;
		parser.location.col = 0;
		parser.input = parser.input.substr(1);

		// std::printf("table at %d{%s}\n", int(parser.location.nest.size()), parser.location.nest.back().data());
		table = parseTable(parser, error);
		// std::printf("%d: entries: %d\n",int(parser.location.nest.size()), int(table.size()));
	} else {
		auto dst = parseString(parser, error);
		table.push_back({std::move(dst), {}});
	}

	assert(parser.location.nest.back() == name);
	parser.location.nest.pop_back();

	if(error.type != ErrorType::none) {
		return {};
	}

	success = true;
	return {std::move(name), std::move(table)};
}

inline Table parseTable(Parser& parser, Error& error) {
	error = {ErrorType::none};

	Table table;
	auto success = true;

	while(true) {
		auto entry = parseEntry(parser, error, success);
		if(!success) {
			break;
		}

		table.push_back(std::move(entry));
	}

	return table;
}
