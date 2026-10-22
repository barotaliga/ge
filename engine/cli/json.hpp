// JSON AS ge WRITES IT: the few builders every `--json` answer is made of. A value is a
// std::string that already holds valid JSON, so builders nest by plain string passing and nothing
// is parsed back. Keys keep the order they are given in, and the same inputs give the same bytes,
// so an agent can diff two answers.
//
// ge:summary The JSON builders behind every ge --json answer: strings, objects, arrays
// ge:keywords cli ge json machine-readable output escape json_str json_obj json_arr json_num
// ge:keywords json_bool structured-output
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#pragma once

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ge::cli {

// A JSON string: quoted, with '"', '\\' and every control byte escaped. Other bytes pass through,
// so UTF-8 stays UTF-8.
std::string json_str(std::string_view s);
// An integer as JSON.
std::string json_num(long long n);
std::string json_bool(bool b);
// An object from (key, value) pairs; every value is already JSON.
std::string json_obj(std::initializer_list<std::pair<std::string_view, std::string>> members);
std::string json_obj(const std::vector<std::pair<std::string, std::string>>& members);
// An array of values that are already JSON.
std::string json_arr(const std::vector<std::string>& values);
// An array of strings, each quoted.
std::string json_strs(const std::vector<std::string>& values);

}  // namespace ge::cli
