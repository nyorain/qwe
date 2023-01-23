#pragma once

// Alternate parser, using callbacks.
//
// This one does not allocate a single byte of memory dynamically.
// And it only needs the 'std::string_view' and 'std::size_t' features
// from the stl, only features that could be easily replaced with standalones.
//
// Does not support multiline strings or escaping ':' (for now).
//   (We don't support that so we don't ever have to copy strings.
//    But I guess we could leave the un-escaping up to the callee?)

#include <string_view>
#include <cassert>
#include <cstdlib>

// parse callback interface:
// struct {
// 	Handler& enterTable(Parser&, std::string_view name);
// 	void exitTable(Parser&);
// 	void entry(Parser&, std::string_view value);
// };

struct Location {
	unsigned line {};
	unsigned col {};
	unsigned nest {};
};

struct Parser {
	std::string_view input;
	Location location {};
};

enum class ErrorType {
	none,
	unexpectedEnd,
	highIndentation,
};

struct Error {
	ErrorType type;
	Location location {};
	std::string_view data {}; // dependent on 'type'
};

template<typename CB>
void parseTable(CB& cb, Parser&, Error& error);

inline std::string_view parseString(Parser& parser, Error& error) {
	error = {ErrorType::none};
	auto i = std::size_t(0);

	while(i < parser.input.size()) {
		if(i == 0u && parser.input[i] == '\t') {
			error = {ErrorType::highIndentation, parser.location};
			return {};
		}

		if(parser.input[i] == ':' || parser.input[i] == '\n') {
			break;
		}

		++i;
		++parser.location.col;
	}

	auto ret = parser.input.substr(0, i);
	parser.input = parser.input.substr(i);
	return ret;
}

template<typename CB>
inline bool parseEntry(CB& cb, Parser& parser, Error& error) {
	error = {ErrorType::none};

	auto first = parser.input.npos;
	std::string_view after;

	while(!parser.input.empty()) {
		after = parser.input;
		first = after.find_first_not_of('\t');

		if(first == after.npos) {
			parser.location.col += first;
			parser.input = {}; // reached end of document
			return false;
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
				return false;
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
		return false;
	}

	// indentation is suddenly too high
	if(first > parser.location.nest) {
		error = {ErrorType::highIndentation, parser.location};
		return false;
	}

	// indentation is too low, line does not belong to this value anymore
	if(first < parser.location.nest) {
		return false;
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
		cb.entry(parser, name);
		return true;
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
	auto& nextCB = cb.enterTable(parser, name);
	++parser.location.nest;

	if(parser.input[0] == '\n') {
		++parser.location.line;
		parser.location.col = 0;
		parser.input = parser.input.substr(1);

		// std::printf("table at %d{%s}\n", int(parser.location.nest.size()), parser.location.nest.back().data());
		parseTable(nextCB, parser, error);
		// std::printf("%d: entries: %d\n",int(parser.location.nest.size()), int(table.size()));
	} else {
		auto dst = parseString(parser, error);
		cb.entry(parser, dst);
	}

	if(error.type != ErrorType::none) {
		return false;
	}

	cb.exitTable(parser);
	assert(parser.location.nest > 0);
	--parser.location.nest;

	return true;
}

template<typename CB>
inline void parseTable(CB& cb, Parser& parser, Error& error) {
	error = {ErrorType::none};
	while(parseEntry(cb, parser, error)) /*noop*/ ;
}

