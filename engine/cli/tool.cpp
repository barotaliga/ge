// ge tool, defined (tool.hpp).
//
// ge:summary ge tool's listing, build, run and answer
// ge:keywords cli tool plugin run answer findings
// ge:skill agent-interface
// ge:test tests/cli_tool_test.cpp
#include "cli/tool.hpp"

#include <algorithm>

#include "cli/json.hpp"

namespace ge::cli {

namespace {

// The JSON string value of `key` in `line`, unescaped for quotes and backslashes; "" when absent.
std::string string_field(const std::string& line, const std::string& key) {
  const std::string open = "\"" + key + "\":\"";
  const size_t at = line.find(open);
  if (at == std::string::npos) return "";
  std::string out;
  for (size_t i = at + open.size(); i < line.size() && line[i] != '"'; i++) {
    if (line[i] == '\\' && i + 1 < line.size()) i++;
    out += line[i];
  }
  return out;
}

// The last line of `output` that is a JSON object.
std::string answer_line(const std::string& output) {
  std::string last;
  size_t start = 0;
  while (start < output.size()) {
    const size_t end = std::min(output.find('\n', start), output.size());
    const std::string line = output.substr(start, end - start);
    if (!line.empty() && line.front() == '{' && line.back() == '}') last = line;
    start = end + 1;
  }
  return last;
}

}  // namespace

std::vector<std::string> plugin_tools(const fs::path& repo) {
  std::vector<std::string> out;
  std::error_code ec;
  for (const auto& p : fs::directory_iterator(repo / "plugins", ec)) {
    const fs::path tool = p.path() / "tool";
    if (!fs::is_directory(tool, ec)) continue;
    for (const auto& f : fs::directory_iterator(tool, ec)) {
      if (f.path().extension() == ".cpp") {
        out.push_back(p.path().filename().string());
        break;
      }
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

Outcome tool_answer(const std::string& name, int code, const std::string& output) {
  Outcome out;
  const std::string line = answer_line(output);
  out.result = line;
  if (line.empty()) {
    out.text = output;
    out.findings.push_back(
        {Severity::error,
         "tool " + name + " exited " + std::to_string(code) + " without a JSON answer",
         "plugins/" + name + "/tool", "ge tool " + name + " -- help"});
    return out;
  }
  if (code != 0) {
    const std::string error = string_field(line, "error");
    out.findings.push_back({Severity::error, error.empty() ? "the tool failed" : error,
                            "plugins/" + name + "/tool", "ge tool " + name + " -- help"});
    if (code == 2) out.exit = 2;
    return out;
  }
  out.summary = "tool " + name + " answered; the result is its JSON";
  return out;
}

Outcome run_tool(const BuildOptions& o, const std::string& name,
                 const std::vector<std::string>& passthrough) {
  if (name.empty()) {
    Outcome out;
    const std::vector<std::string> tools = plugin_tools(o.repo);
    out.result = json_strs(tools);
    for (const std::string& t : tools) out.text += "  " + t + "\n";
    out.summary = std::to_string(tools.size()) + " plugin tools; `ge tool <name> -- help` for one";
    out.next = "ge tool <name> -- help";
    return out;
  }
  Outcome out;
  const std::vector<std::string> tools = plugin_tools(o.repo);
  if (std::find(tools.begin(), tools.end(), name) == tools.end()) {
    out.findings.push_back({Severity::error, "no plugin carries a tool named '" + name + "'",
                            "plugins/", "ge tool lists them"});
    out.exit = 2;
    return out;
  }
  if (!build_targets(o, out, name)) return out;
  fs::path exe = o.repo / "build" / o.preset / name;
#ifdef _WIN32
  exe += ".exe";
#endif
  std::string cmd = "\"" + exe.string() + "\"";
  for (const std::string& w : passthrough) cmd += " \"" + w + "\"";
  const fs::path log = build_log(o, "ge-tool-" + name);
  std::error_code ec;
  fs::remove(log, ec);
  const Logged r = run_to_log(cmd, log);
  return tool_answer(name, r.code, r.output);
}

}  // namespace ge::cli
