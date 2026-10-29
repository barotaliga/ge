// `ge qc` (qc.hpp).
//
// ge:summary ge qc's hand-off to the qc executable and the reading of its answer lines
// ge:keywords cli ge qc progs answer findings summary result hand-off
// ge:skill agent-interface progs
// ge:test tests/cli_qc_test.cpp
#include "cli/qc.hpp"

#include <string_view>

namespace ge::cli {

namespace {

constexpr std::string_view kTag = "@qc ";

fs::path qc_exe(const BuildOptions& o) {
#ifdef _WIN32
  return o.repo / "build" / o.preset / "qc.exe";
#else
  return o.repo / "build" / o.preset / "qc";
#endif
}

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

// A finding line: severity, what, where, next, tab-separated.
Finding finding_of(std::string_view line) {
  std::string parts[4];
  int k = 0;
  for (char c : line) {
    if (c == '\t' && k < 3) {
      k++;
      continue;
    }
    parts[k] += c;
  }
  const Severity s = parts[0] == "error"     ? Severity::error
                     : parts[0] == "warning" ? Severity::warn
                                             : Severity::note;
  return {s, parts[1], parts[2], parts[3]};
}

}  // namespace

Outcome qc_answer(const BuildOptions& o, int code, const std::string& output) {
  Outcome out;
  bool answered = false;
  size_t at = 0;
  while (at < output.size()) {
    size_t end = output.find('\n', at);
    if (end == std::string::npos) end = output.size();
    const std::string_view line(output.data() + at, end - at);
    at = end + 1;
    if (!line.starts_with(kTag)) {
      out.text.append(line).append("\n");
      continue;
    }
    answered = true;
    const std::string_view rest = line.substr(kTag.size());
    if (rest.starts_with("finding ")) {
      out.findings.push_back(finding_of(rest.substr(8)));
    } else if (rest.starts_with("summary ")) {
      out.summary = std::string(rest.substr(8));
    } else if (rest.starts_with("result ")) {
      out.result = std::string(rest.substr(7));
    }
  }
  if (!answered) {
    out.findings.push_back({Severity::error,
                            "the qc executable gave no answer (exit " + std::to_string(code) + ")",
                            rel_path(o.repo, build_log(o, "ge-qc")),
                            "read the log; the qc tool is plugins/progs/tool.hpp (skill: progs)"});
    return out;
  }
  if (code == 2) out.exit = 2;
  if (code == 0) out.next = "ge qc trace <progs> --function=<name> (every call it makes)";
  return out;
}

Outcome run_qc(const BuildOptions& o, const std::string& command, const std::string& file,
               const std::vector<std::string>& flags) {
  Outcome out;
  if (!build_targets(o, out, "qc")) return out;
  std::error_code ec;
  if (!fs::is_regular_file(qc_exe(o), ec)) {
    out.findings.push_back({Severity::error, "this build made no `qc` executable",
                            rel_path(o.repo, qc_exe(o)),
                            "the progs plugin builds it (plugins/progs/bin/qc.cpp); skill: progs"});
    return out;
  }
  std::string cmd = quoted(qc_exe(o).string()) + " " + command + " " + quoted(file);
  for (const std::string& f : flags) cmd += " " + quoted(f);
  const fs::path log = build_log(o, "ge-qc");
  fs::remove(log, ec);
  const Logged r = run_to_log(cmd, log);
  return qc_answer(o, r.code, r.output);
}

}  // namespace ge::cli
