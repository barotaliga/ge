// JSON AS ge WRITES IT: the builders every `--json` answer is made of, stated once in
// core/json_write.hpp (a game's player CLI writes its answers with the same ones) and named here
// in ge's namespace.
//
// ge:summary The JSON builders behind every ge --json answer, from core/json_write.hpp
// ge:keywords cli ge json machine-readable output json_str json_obj json_arr json_num json_bool
// ge:keywords structured-output
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#pragma once

#include "core/json_write.hpp"

namespace ge::cli {

using core::json_arr;
using core::json_bool;
using core::json_num;
using core::json_obj;
using core::json_str;
using core::json_strs;

}  // namespace ge::cli
