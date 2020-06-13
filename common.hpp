#pragma once

#include <variant>
#include <string_view>
#include <cassert>

template<typename ...Ts>
struct Visitor : Ts...  {
    Visitor(const Ts&... args) : Ts(args)...  {}
    using Ts::operator()...;
};

inline std::pair<std::string_view, std::string_view> split(
		std::string_view src, std::string_view::size_type pos) {
	assert(pos != src.npos && pos < src.size()); // TODO
	auto first = src;
	auto second = src;
	second.remove_prefix(pos + 1);
	first.remove_suffix(src.size() - pos);
	return {first, second};
}
