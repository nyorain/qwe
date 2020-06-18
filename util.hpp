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

	// TODO: with c++20 we don't have to construct a string here.
	auto it = table->find(std::string(first));
	if(it == table->end()) {
		return nullptr;
	}

	return at(*it->second, rest);
}

// Fallback
template<typename T, typename B>
auto templatize(B&& val) {
	return val;
}

template<typename T>
struct ValueParser {
	static std::optional<T> call(const Value& value) {
		auto str = asString(value);
		if(!str) {
			return std::nullopt;
		}

		if constexpr(std::is_integral_v<T>) {
			char* end {};
			auto cstr = str->c_str();
			auto v = std::strtoll(cstr, &end, 10u);
			return end == cstr ? std::nullopt : std::optional(T(v));
		} else if constexpr(std::is_floating_point_v<T>) {
			char* end {};
			auto cstr = str->c_str();
			auto v = std::strtold(cstr, &end);
			return end == cstr ? std::nullopt : std::optional(T(v));
		}

		static_assert(templatize<T>(false), "Can't parse type");
	}
};

template<>
struct ValueParser<std::string> {
	static std::optional<std::string> call(const Value& value) {
		auto str = asString(value);
		return str ? std::optional(*str) : std::nullopt;
	}
};

template<>
struct ValueParser<std::string_view> {
	static std::optional<std::string_view> call(const Value& value) {
		auto str = asString(value);
		return str ? std::optional(std::string_view(*str)) : std::nullopt;
	}
};

template<typename T>
std::optional<std::vector<T>> asVector(const Vector& vector) {
	std::vector<T> ret;
	ret.reserve(vector.size());
	for(auto& v : vector) {
		auto parsed = ValueParser<T>::call(*v);
		if(!parsed) {
			return std::nullopt;
		}

		ret.emplace_back(std::move(*parsed));
	}

	return ret;
}

template<typename T>
struct ValueParser<std::vector<T>> {
	static std::optional<std::vector<T>> call(const Value& value) {
		auto* vec = asVector(value);
		return vec ? asVector<T>(*vec) : std::nullopt;
	}
};

template<typename T>
std::optional<T> as(const Value& value) {
	return ValueParser<T>::call(value);
}

template<typename T>
std::optional<T> as(const Value& value, std::string_view field) {
	auto v = at(value, field);
	return v ? as<T>(v) : std::nullopt;
}

template<typename T>
Value print(const T& val);

template<typename T, typename D = ValueParser<T>>
struct PodParser {
	static std::optional<T> parse(const Value& value) {
		T res;
		auto map = D::map(res);

		bool error = for_each_or(map, [&](auto& entry) {
			auto& v = entry.val;
			using V = std::decay_t<decltype(v)>;

			auto val = at(value, entry.name);
			if(!val) {
				return entry.required;
			}

			auto pv = as<V>(*val);
			if(!pv) {
				return true;
			}

			v = *pv;
			return false;
		});

		if(error) {
			return std::nullopt;
		}

		return res;
	}

	static Value print(const T& val) {
		auto map = D::map(val);
		Table table;
		for_each_or(map, [&](auto& entry) {
			table.emplace(entry.name, print(entry.val));
		});

		return {table};
	}
};


// TODO: std::from_chars not supported yet in stdlibc++
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

