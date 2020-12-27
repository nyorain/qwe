#pragma once

#include <vector>
#include <string>

struct Table : std::vector<std::pair<std::string, Table>> {};
