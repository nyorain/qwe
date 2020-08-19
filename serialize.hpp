#pragma once

#include "common.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <cstdlib>
#include <tuple>
#include <utility>
#include <type_traits>

struct Location {
	unsigned line {};
	unsigned col {}; // NOTE: should probably be (utf-8) char instead of byte
	std::vector<std::string_view> nest {};
};

struct Parser {
	std::string_view input;
	Location location {};
};

struct Printer {
	std::string out;
	unsigned ident;
	bool inArray;
};

enum class ErrorType {
	none = 0,
	cantParseNumber,
	highIndentation,
	unexpectedEnd,
	podInvalidEntry,
	podMissingField,
	podNonTableEntry,
	podDuplicateEntry,
	fixedArrayTooMany,
	fixedArrayNotEnough,
	emptyName,

	/*
	mixedTableArray,
	emptyTableArray
	*/
};

// struct Error {
// 	ErrorType type;
// 	Location location;
// 	std::string_view data {}; // dependent on 'type'
// };

template<typename T> using ParseResult = std::variant<T, ErrorType>;

template<typename T, typename = void> struct Serializer;

template<typename T> ParseResult<T> parse(Parser& parser) {
	return Serializer<T>::parse(parser);
}

template<typename T> void print(Printer& printer, const T& val) {
	return Serializer<T>::print(printer, val);
}

template<typename T>
std::string print(const T& val) {
	Printer printer {};
	print(printer, val);
	return printer.out;
}


// Default implementation for atomic types.
template<typename T, typename>
// struct Serializer<T, std::enable_if_t<
// 		std::is_integral_v<T> || std::is_floating_point_v<T> ||
// 		std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view>>> {
struct Serializer {
	// str must be nullterminated
	static ParseResult<T> parse(Parser& parser) {
		(void) parser;
		auto& str = parser.input;

		if constexpr(std::is_same_v<T, std::string_view>) {
			auto [curr, next] = splitIf(str, str.find('\n'));
			str = next;
			if(!next.empty()) {
				++parser.location.line;
				parser.location.col = 0u;
			}
			return curr;
		} else if constexpr(std::is_same_v<T, std::string>) {
			auto [curr, next] = splitIf(str, str.find('\n'));
			str = next;
			if(!next.empty()) {
				++parser.location.line;
				parser.location.col = 0u;
			}
			return std::string(curr);
		} else if constexpr(std::is_integral_v<T>) {
			auto sep = str.find('\n');
			char* end {};
			auto begin = str.data();
			auto v = std::strtoll(begin, &end, 10u);
			if(end == begin || (sep != str.npos && end - begin != sep)) {
				return ErrorType::cantParseNumber;
			}

			parser.location.col += end - begin;
			str = str.substr(end - begin);
			if(sep != str.npos) {
				str = str.substr(1);
				++parser.location.line;
				parser.location.col = 0u;
			}

			return T(v);
		} else if constexpr(std::is_floating_point_v<T>) {
			auto sep = str.find('\n');
			char* end {};
			auto begin = str.data();
			auto v = std::strtold(begin, &end);
			if(end == begin || (sep != str.npos && end - begin != sep)) {
				return ErrorType::cantParseNumber;
			}

			parser.location.col += end - begin;
			str = str.substr(end - begin);
			if(sep != str.npos) {
				str = str.substr(1);
				++parser.location.line;
				parser.location.col = 0u;
			}

			return T(v);
		} else {
			static_assert(templatize<T>(false), "Can't parse primitive type");
		}
	}

	// Must only append to the given string
	static void print(Printer& printer, const T& val) {
		if constexpr(std::is_same_v<T, std::string> || std::is_same_v<T, std::string>) {
			printer.out += val;
		} else if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T>) {
			printer.out += std::to_string(val);
		} else {
			static_assert(templatize<T>(false), "Can't print primitive type");
		}
	}
};

ErrorType processLine(Parser& parser, bool& done, bool& skip) {
	auto content = parser.input;
	auto first = content.find_first_not_of('\t');
	if(first == content.npos) {
		parser.location.col += first;
		parser.input = {}; // reached end of document
		done = true;
		return ErrorType::none;
	}

	// Comment, skip to next line.
	// Notice how this comes before the indent check.
	// Comments don't have to be aligned, we don't care.
	if(content[first] == '#') {
		auto nl = content.find('\n');
		if(nl == content.npos) {
			// reached end of document
			parser.location.col += parser.input.size();
			parser.input = {};
			done = true;
			return ErrorType::none;
		}

		++parser.location.line;
		parser.location.col = 0u;
		parser.input = content.substr(nl + 1);
		done = false;
		skip = true;
		return ErrorType::none;
	}

	// Empty lines are also always allowed
	if(content[first] == '\n') {
		++parser.location.line;
		parser.location.col = 0u;
		parser.input = content.substr(first + 1);
		done = false;
		skip = true;
		return ErrorType::none;
	}

	auto indent = parser.location.nest.size();
	// indentation is suddenly too high
	if(first > indent) {
		return ErrorType::highIndentation;
	}

	// indentation is too low, line does not belong to this value anymore
	if(first < indent) {
		done = true;
		return ErrorType::none;
	}

	skip = false;
	done = false;
	parser.location.col += first;
	parser.input = content.substr(first);
	return ErrorType::none;
}

ErrorType getLine(Parser& parser, bool& done) {
	auto skip = true;
	while(skip) {
		auto err = processLine(parser, done, skip);
		if(err != ErrorType::none) {
			return err;
		}

		if(done) {
			return ErrorType::none;
		}
	}

	return ErrorType::none;
}

template<typename T>
struct Serializer<std::vector<T>> {
	static ParseResult<std::vector<T>> parse(Parser& parser) {
		std::vector<T> res;
		while(!parser.input.empty()) {
			bool done;
			auto err = getLine(parser, done);
			if(err != ErrorType::none) {
				return err;
			}

			if(done) {
				break;
			}

			if(parser.input.empty()) {
				return ErrorType::unexpectedEnd;
			}

			// NOTE: extended array-nest syntax
			bool nested = false;
			if(!parser.input.empty() && parser.input.substr(0, 2) == "-\n") {
				parser.input = parser.input.substr(2);
				parser.location.col = 0u;
				++parser.location.line;
				parser.location.nest.push_back(std::to_string(res.size()));
				nested = true;
			}

			auto r = ::parse<T>(parser);
			if(auto err = std::get_if<ErrorType>(&r)) {
				return *err;
			}

			if(nested) {
				parser.location.nest.pop_back();
			}

			res.emplace_back(std::move(std::get<T>(r)));
		}

		return res;
	}

	static void print(Printer& printer, const std::vector<T>& val) {
		auto inArray = printer.inArray;
		if(inArray) {
			printer.out += "-\n";
			++printer.ident;
		}

		for(auto& e : val) {
			printer.inArray = true;
			printer.out += '\n';
			printer.out.append(printer.ident, '\t');
			::print(printer, e);
		}

		if(inArray) {
			--printer.ident;
		}
		printer.inArray = inArray;
	}
};

template<typename T, std::size_t N>
struct Serializer<std::array<T, N>> {
	static ParseResult<std::array<T, N>> parse(Parser& parser) {
		std::array<T, N> res;
		auto i = 0u;
		while(!parser.input.empty()) {
			bool done;
			auto err = getLine(parser, done);
			if(err != ErrorType::none) {
				return err;
			}

			if(done) {
				break;
			}

			// NOTE: extended array-nest syntax
			bool nested = false;
			if(!parser.input.empty() && parser.input.substr(0, 2) == "-\n") {
				parser.input = parser.input.substr(2);
				parser.location.col = 0u;
				++parser.location.line;
				parser.location.nest.push_back(std::to_string(i));
				nested = true;
			}

			auto r = ::parse<T>(parser);
			if(auto err = std::get_if<ErrorType>(&r)) {
				return *err;
			}

			if(nested) {
				parser.location.nest.pop_back();
			}

			if(i >= res.size()) {
				return ErrorType::fixedArrayTooMany;
			}

			res[i] = std::move(std::get<T>(r));
			++i;
		}

		if(i < res.size()) {
			return ErrorType::fixedArrayNotEnough;
		}

		return res;
	}

	static void print(Printer& printer, const std::array<T, N>& val) {
		auto inArray = printer.inArray;
		if(inArray) {
			printer.out += "-\n";
			++printer.ident;
		}

		for(auto& e : val) {
			printer.inArray = true;
			printer.out += '\n';
			printer.out.append(printer.ident, '\t');
			::print(printer, e);
		}

		if(inArray) {
			--printer.ident;
		}
		printer.inArray = inArray;
	}
};

template<typename M>
ErrorType parse(Parser& parser, M& map, std::string_view prefix = "") {
	while(!parser.input.empty()) {
		bool done;
		auto err = getLine(parser, done);
		if(err != ErrorType::none) {
			return err;
		}

		if(done) {
			break;
		}

		auto content = parser.input;
		if(content.empty()) {
			return ErrorType::unexpectedEnd;
		}

		auto nl = content.find("\n");
		auto line = content;
		auto after = std::string_view {};
		if(nl != content.npos) {
			line = content.substr(0, nl);
			after = content.substr(nl + 1);
		}

		auto sep = line.find(":");
		if(sep == line.npos) {
			return ErrorType::podNonTableEntry;
		}

		auto [name, val] = split(line, sep);
		if(name.empty()) {
			return ErrorType::emptyName;
		}

		// removing leading space in val
		if(!val.empty() && val[0] == ' ') {
			val.remove_prefix(1);
		}

		if(val.empty()) {
			parser.input = after;
			parser.location.col = 0u;
			++parser.location.line;
		} else {
			parser.location.col += val.data() - content.data();
			// parser.input = val; // and reset it later
			parser.input = content.substr(val.data() - line.data());
		}

		parser.location.nest.push_back(name);

		// search for binding
		err = ErrorType::none;

		// 'ename = name' needed since capturing structured binding
		// variables not possible (standard c++ bug, basically).
		// See https://stackoverflow.com/questions/46114214/
		auto found = for_each_or(map, [&, ename = name](auto& entry) {
			if(entry.name.length() <= prefix.length() ||
					entry.name.substr(0, prefix.length()) != prefix) {
				return false;
			}

			auto pl = prefix.empty() ? 0u : prefix.length() + 1;
			auto name = entry.name.substr(pl); // remove dot
			if(name != ename) {
				return false;
			}

			if(entry.done) {
				err = ErrorType::podDuplicateEntry;
				return true; // break for each
			}

			entry.done = true;
			auto& v = entry.val;
			using V = std::remove_reference_t<decltype(v)>;

			ParseResult<V> r;
			r = ::parse<V>(parser);
			if(auto nerr = std::get_if<ErrorType>(&r)) {
				err = *nerr;
				return true; // break for each
			}

			v = std::move(std::get<V>(r));
			return true; // break for each
		});

		if(found && err != ErrorType::none) {
			return err;
		}

		if(!found) {
			// try to parse it as sub-object
			// TODO: can be optimized. Check for sub-objects in iteration
			// already, break iteration if found.
			// If not found, don't even attempt it here and return
			// podInvalidEntry
			if(val.empty()) {
				std::string nprefix = std::string(prefix);
				if(!nprefix.empty()) nprefix += ".";
				nprefix += name;

				auto err = ::parse(parser, map, nprefix);
				if(err != ErrorType::none) {
					return err;
				}
			} else {
				return ErrorType::podInvalidEntry;
			}
		}

		parser.location.nest.pop_back();
	}

	return ErrorType::none;
}

template<typename M>
void print(Printer& printer, M& map, std::string_view prefix = "") {
	auto inArray = printer.inArray;
	if(inArray) {
		printer.out += "-\n";
		++printer.ident;
	}

	for_each_or(map, [&](auto& entry) {
		if(entry.done || entry.name.length() <= prefix.length() ||
				entry.name.substr(0, prefix.length()) != prefix) {
			return false;
		}

		auto pl = prefix.empty() ? 0u : prefix.length() + 1;
		auto name = entry.name.substr(pl);

		printer.out += "\n";
		printer.out.append(printer.ident, '\t');

		printer.inArray = false;
		auto sep = name.find('.');
		if(sep != name.npos) {
			if(entry.done) {
				return false;
			}

			name = name.substr(0, sep);
			printer.out += name;
			printer.out += ": ";

			auto nprefix = std::string(prefix);
			if(!nprefix.empty()) nprefix += ".";
			nprefix += name;

			++printer.ident;
			::print(printer, map, nprefix);
			--printer.ident;

			return false;
		}

		entry.done = true;

		auto& v = entry.val;
		using V = std::remove_reference_t<decltype(v)>;

		printer.out += name;
		printer.out += ": ";

		++printer.ident;
		::print(printer, v);
		--printer.ident;

		return false; // never return early; print all elements
	});

	if(inArray) {
		--printer.ident;
	}

	printer.inArray = inArray;
}

template<typename T, typename D = Serializer<T>>
struct PodSerializer {
	static ParseResult<T> parse(Parser& parser) {
		T res {}; // zero-initialized
		auto map = D::map(res);
		auto err = ::parse(parser, map);
		if(err != ErrorType::none) {
			return err;
		}

		// Check that all required entries were present.
		auto missing = for_each_or(map, [](auto& entry) {
			return entry.required && !entry.done;
		});

		if(missing) {
			return ErrorType::podMissingField;
		}

		return ParseResult<T>(res);
	}

	static void print(Printer& printer, const T& val) {
		auto map = D::map(val);
		::print(printer, map);
	}
};
