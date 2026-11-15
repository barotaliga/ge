// The definitions behind outcome.hpp: the exit code, and the two printed forms.
//
// ge:summary An outcome printed as text or as one JSON object, and its exit code
// ge:keywords split_lines cli ge gamecli outcome emit exit_of finding_json format_finding count_of
// ge:keywords json text
// ge:skill agent-interface
// ge:test tests/cli_commands_test.cpp
#include "toolkit/outcome.hpp"

#include "core/json_write.hpp"

namespace ge::toolkit {

using core::json_arr;
using core::json_bool;
using core::json_num;
using core::json_obj;
using core::json_str;
using core::json_strs;

namespace {

std::string_view severity_word(Severity s) {
  switch (s) {
    case Severity::error:
      return "error";
    case Severity::warn:
      return "warn";
    case Severity::note:
      return "note";
  }
  return "error";
}

}  // namespace

std::string format_finding(std::string_view command, const Finding& f) {
  std::string out;
  out += command;
  out += ": ";
  out += severity_word(f.severity);
  out += ": ";
  out += f.what;
  out += "\n    where: ";
  out += f.where.empty() ? "the repository" : f.where;
  out += "\n    next: ";
  out += f.next;
  out += '\n';
  return out;
}

int count_of(const std::vector<Finding>& findings, Severity severity) {
  int n = 0;
  for (const Finding& f : findings) n += f.severity == severity ? 1 : 0;
  return n;
}

int exit_of(const Outcome& o) {
  if (o.exit >= 0) return o.exit;
  return count_of(o.findings, Severity::error) > 0 ? 1 : 0;
}

std::string finding_json(const Finding& f) {
  return json_obj({{"severity", json_str(severity_word(f.severity))},
                   {"what", json_str(f.what)},
                   {"where", json_str(f.where)},
                   {"next", json_str(f.next)}});
}

int emit(std::string_view command, const Outcome& o, bool json, std::ostream& out) {
  const int code = exit_of(o);
  const int errors = count_of(o.findings, Severity::error);
  const int warns = count_of(o.findings, Severity::warn);
  const int notes = count_of(o.findings, Severity::note);
  std::string summary = o.summary;
  // A failed command's next step is in its findings; its success path's `next` would mislead.
  const std::string next = code == 0 ? o.next : "";
  if (code != 0 && errors > 0) {
    summary = std::to_string(errors) + " error(s), " + std::to_string(warns) + " warn, " +
              std::to_string(notes) + " note. Fix each error from its next line, then rerun.";
  }
  if (json) {
    std::vector<std::string> findings;
    findings.reserve(o.findings.size());
    for (const Finding& f : o.findings) findings.push_back(finding_json(f));
    out << json_obj({{"command", json_str(command)},
                     {"ok", json_bool(code == 0)},
                     {"exit", json_num(code)},
                     {"findings", json_arr(findings)},
                     {"written", json_strs(o.written)},
                     {"result", o.result.empty() ? "null" : o.result},
                     {"summary", json_str(summary)},
                     {"next", json_str(next)}})
        << "\n";
    return code;
  }
  for (const std::string& path : o.written) out << "  wrote " << path << "\n";
  out << o.text;
  for (const Finding& f : o.findings) out << format_finding(command, f);
  if (!summary.empty()) {
    out << command << ": " << summary;
    if (code == 0 && warns + notes > 0) {
      out << " (" << warns << " warn, " << notes << " note)";
    }
    out << "\n";
  }
  if (!next.empty()) out << "next: " << next << "\n";
  return code;
}

std::vector<std::string_view> split_lines(std::string_view text) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    if (end == std::string_view::npos) {
      lines.push_back(text.substr(start));
      break;
    }
    lines.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  return lines;
}

}  // namespace ge::toolkit
