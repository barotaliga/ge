// The definitions behind json.hpp: string escaping and the joins.
//
// ge:summary The JSON builders' definitions: escaping, objects and arrays joined in order
// ge:keywords cli ge json escape json_str json_obj json_arr
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#include "cli/json.hpp"

#include <cstdio>

namespace ge::cli {

std::string json_str(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\r':
        out += "\\r";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out + "\"";
}

std::string json_num(long long n) { return std::to_string(n); }

std::string json_bool(bool b) { return b ? "true" : "false"; }

std::string json_obj(std::initializer_list<std::pair<std::string_view, std::string>> members) {
  std::string out = "{";
  for (const auto& [key, value] : members) {
    if (out.size() > 1) out += ',';
    out += json_str(key) + ":" + value;
  }
  return out + "}";
}

std::string json_obj(const std::vector<std::pair<std::string, std::string>>& members) {
  std::string out = "{";
  for (const auto& [key, value] : members) {
    if (out.size() > 1) out += ',';
    out += json_str(key) + ":" + value;
  }
  return out + "}";
}

std::string json_arr(const std::vector<std::string>& values) {
  std::string out = "[";
  for (const std::string& v : values) {
    if (out.size() > 1) out += ',';
    out += v;
  }
  return out + "]";
}

std::string json_strs(const std::vector<std::string>& values) {
  std::vector<std::string> quoted;
  quoted.reserve(values.size());
  for (const std::string& v : values) quoted.push_back(json_str(v));
  return json_arr(quoted);
}

}  // namespace ge::cli
