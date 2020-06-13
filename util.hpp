#pragma once

#include "data.hpp"
#include "parse.hpp"

std::string* asString(Value& value) {
	return std::get_if<std::string>(&value.value);
}
Table* asTable(Value& value) {
	return std::get_if<Table>(&value.value);
}
Vector* asVector(Value& value) {
	return std::get_if<Vector>(&value.value);
}

const std::string* asString(const Value& value) {
	return std::get_if<std::string>(&value.value);
}
const Table* asTable(const Value& value) {
	return std::get_if<Table>(&value.value);
}
const Vector* asVector(const Value& value) {
	return std::get_if<Vector>(&value.value);
}

const Value* at(const Value& value, std::string_view name) {
	if(name.empty()) {
		return &value;
	}

	auto table = asTable(value);
	if(!table) {
		return nullptr;
	}

	auto p = name.find_first_of('.');
	auto first = name;
	auto rest = std::string_view {};
	if(p != name.npos) {
		std::tie(first, rest) = split(name, p);
	}

	auto it = table->find(first);
	if(it == table->end()) {
		return nullptr;
	}

	return at(*it->second, rest);
}

/*
template<typename T>
std::optional<T> asFromChars(const Value& value) {
	auto str = asString(value);
	if(!str) {
		return std::nullopt;
	}

	T v;
	auto end = str->data() + str->size();
	auto res = std::from_chars(str->data(), end, v);
	if(res.ec != std::errc() || res.ptr != end) {
		return std::nullopt;
	}

	return v;
}

std::optional<std::uint64_t> asUint(const Value& value) {
	return asFromChars<std::uint64_t>(value);
}

std::optional<std::int64_t> asInt(const Value& value) {
	return asFromChars<std::int64_t>(value);
}

std::optional<double> asDouble(const Value& value) {
	return asFromChars<double>(value);
}
*/

// // TODO throwing versions
// std::string& asStringT();
// Table& asTableT();
// Vector& asVectorT();
//
// std::uint64_t asUintT();
// std::int64_t asIntT();
// double asDoubleT();

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
		case ErrorType::duplicateName:
			res += "Duplicate table identifier '";
			res += err.data;
			res += "'";
			break;
		case ErrorType::emptyName:
			res += "Empty name not allowed";
			break;
		case ErrorType::mixedTableArray:
			res += "Mixing table and array not allowed";
			break;
		case ErrorType::emptyTableArray:
			res += "Empty value/table/array not allowed";
			break;
		case ErrorType::none:
			res += "No error. Data: ";
			res += err.data;
			break;
	}

	return res;
}

