// The definitions behind build.hpp: running a tool with its output logged, and reading
// compilers', CMake's, CTest's and the labs' output back as findings.
//
// ge:summary Running cmake, ctest, clang-format and the labs logged; their errors read as findings
// ge:keywords cli ge build test check lab run_logged compiler_findings test_findings log
// ge:skill agent-interface
// ge:test tests/cli_build_test.cpp
#include "cli/build.hpp"

#include <algorithm>
#include <cstdlib>
#include <regex>
#include <system_error>

#include "cli/doctor.hpp"
#include "cli/json.hpp"

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace ge::cli {

namespace {

constexpr int kMostFindings = 20;  // the rest are counted, and the log holds them

// The line handed to the platform's shell. cmd.exe strips the first and the last quote of a
// line that starts with one, so on Windows the whole line is wrapped in one more pair.
std::string shell_line(const std::string& command) {
#ifdef _WIN32
  return "\"" + command + "\"";
#else
  return command;
#endif
}

struct Ran {
  int code;
  std::string output;
};

// Runs `command` with stdout and stderr appended to `log`; the exit code and what it printed.
Ran run_logged(const std::string& command, const fs::path& log) {
  std::error_code ec;
  fs::create_directories(log.parent_path(), ec);
  const fs::path part = log.string() + ".part";
  const std::string line = command + " > \"" + part.string() + "\" 2>&1";
  int code = std::system(shell_line(line).c_str());
#ifndef _WIN32
  if (code != -1) code = WEXITSTATUS(code);
#endif
  Ran r{code, read_text(part).value_or("")};
  std::string prior = read_text(log).value_or("");
  std::string error;
  write_text(log, prior + "$ " + command + "\n" + r.output, error);
  fs::remove(part, ec);
  return r;
}

std::string quote_path(const fs::path& p) { return "\"" + p.string() + "\""; }

fs::path log_path(const BuildOptions& o, std::string_view name) {
  return o.repo / "build" / o.preset / (std::string(name) + ".log");
}

std::string where_in(const fs::path& repo, const std::string& file, const std::string& line) {
  std::error_code ec;
  if (!fs::path(file).is_absolute()) return line.empty() ? file : file + ":" + line;
  const fs::path p = fs::weakly_canonical(fs::path(file), ec);
  const fs::path root = fs::weakly_canonical(repo, ec);
  std::string rel = p.generic_string();
  const std::string r = root.generic_string() + "/";
  if (rel.starts_with(r)) rel = rel.substr(r.size());
  return line.empty() ? rel : rel + ":" + line;
}

// The last few non-empty lines of a tool's output, for a failure nothing else explains.
std::string tail_of(std::string_view output, int lines) {
  const auto all = split_lines(output);
  std::vector<std::string_view> kept;
  for (size_t i = all.size(); i > 0 && static_cast<int>(kept.size()) < lines; i--) {
    if (!all[i - 1].empty()) kept.push_back(all[i - 1]);
  }
  std::string out;
  for (size_t i = kept.size(); i > 0; i--) {
    if (!out.empty()) out += " | ";
    out += kept[i - 1];
  }
  return out;
}

bool configure(const BuildOptions& o, const fs::path& log, Outcome& out) {
  std::string cmd = "cmake --preset " + o.preset;
  if (!o.engine.empty()) {
    std::error_code ec;
    cmd += " -DFETCHCONTENT_SOURCE_DIR_GAMEENGINE2=" +
           quote_path(fs::path(fs::absolute(o.engine, ec).generic_string()));
  }
  const Ran r = run_logged(cmd, log);
  if (r.code == 0) return true;
  out.findings = compiler_findings(o.repo, r.output, kMostFindings);
  if (out.findings.empty()) {
    out.findings.push_back({Severity::error,
                            "CMake could not configure the " + o.preset + " preset (exit " +
                                std::to_string(r.code) + "): " + tail_of(r.output, 3),
                            rel_path(o.repo, log),
                            "read the log; a download failure needs the network or "
                            "--engine=<engine checkout>"});
  }
  return false;
}

bool build_step(const BuildOptions& o, Outcome& out) {
  const fs::path log = log_path(o, "ge-build");
  std::error_code ec;
  fs::remove(log, ec);
  if (!configure(o, log, out)) return false;
  const Ran r = run_logged("cmake --build --preset " + o.preset, log);
  if (r.code == 0) return true;
  out.findings = compiler_findings(o.repo, r.output, kMostFindings);
  if (out.findings.empty()) {
    out.findings.push_back(
        {Severity::error,
         "the build failed (exit " + std::to_string(r.code) + "): " + tail_of(r.output, 3),
         rel_path(o.repo, log), "read the log, fix the cause, then `ge build`"});
  }
  return false;
}

void sources_in(const fs::path& repo, std::vector<fs::path>& out) {
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(repo, ec); !ec && it != fs::end(it);
       it.increment(ec)) {
    if (it->is_symlink(ec)) {
      if (it->is_directory(ec)) it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory(ec)) {
      if (skipped_dir(it->path().filename().string())) it.disable_recursion_pending();
      continue;
    }
    const std::string ext = it->path().extension().string();
    if (ext == ".cpp" || ext == ".hpp") out.push_back(it->path());
  }
  std::sort(out.begin(), out.end());
}

}  // namespace

int run_quiet(const std::string& command) {
#ifdef _WIN32
  return std::system(shell_line(command + " > NUL 2>&1").c_str());
#else
  const int code = std::system(shell_line(command + " > /dev/null 2>&1").c_str());
  return code == -1 ? -1 : WEXITSTATUS(code);
#endif
}

int run_passthrough(const std::string& command) {
  const int code = std::system(shell_line(command).c_str());
#ifdef _WIN32
  return code;
#else
  return code == -1 ? -1 : WEXITSTATUS(code);
#endif
}

std::vector<Finding> compiler_findings(const fs::path& repo, std::string_view output, int most) {
  static const std::regex kGcc(R"(^(.+?):(\d+):(?:(\d+):)? (?:fatal )?error: (.*)$)");
  static const std::regex kMsvc(R"(^(.+?)\((\d+)(?:,\d+)?\): (?:fatal )?error ([A-Z]+\d+): (.*)$)");
  static const std::regex kCmake(R"(^CMake Error at (.+?):(\d+))");
  static const std::regex kLink(R"((undefined reference to .*|unresolved external symbol .*))");
  std::vector<Finding> out;
  int more = 0;
  const auto lines = split_lines(output);
  auto add = [&](Finding f) {
    for (const Finding& seen : out) {
      if (seen.what == f.what && seen.where == f.where) return;
    }
    if (static_cast<int>(out.size()) < most) {
      out.push_back(std::move(f));
    } else {
      more++;
    }
  };
  for (size_t i = 0; i < lines.size(); i++) {
    const std::string line(lines[i]);
    std::smatch m;
    if (std::regex_match(line, m, kGcc)) {
      const std::string where = where_in(repo, m.str(1), m.str(2));
      const bool format = m.str(4).find("clang-format") != std::string::npos;
      add({Severity::error, m.str(4), where,
           format ? "clang-format -i " + where_in(repo, m.str(1), "")
                  : "fix it at " + where + ", then `ge build`"});
    } else if (std::regex_match(line, m, kMsvc)) {
      const std::string where = where_in(repo, m.str(1), m.str(2));
      add({Severity::error, m.str(3) + ": " + m.str(4), where,
           "fix it at " + where + ", then `ge build`"});
    } else if (std::regex_search(line, m, kCmake)) {
      std::string what;
      for (size_t j = i + 1; j < lines.size() && j < i + 8; j++) {
        const std::string next(lines[j]);
        if (next.empty() && !what.empty()) break;
        const size_t a = next.find_first_not_of(' ');
        if (a != std::string::npos) what += (what.empty() ? "" : " ") + next.substr(a);
      }
      add({Severity::error, what.empty() ? "CMake failed" : what,
           where_in(repo, m.str(1), m.str(2)), "fix the CMake file, then `ge build`"});
    } else if (std::regex_search(line, m, kLink)) {
      add({Severity::error, "the link failed: " + m.str(1), "the link",
           "define the symbol, or link the target that does (`ge use <plugin>` for the "
           "engine's), then `ge build`"});
    }
  }
  if (more > 0) {
    out.push_back({Severity::note, std::to_string(more) + " more error(s) not listed", "",
                   "fix these first; the log holds the rest"});
  }
  return out;
}

std::vector<Finding> test_findings(const fs::path& repo, std::string_view output,
                                   const std::string& preset) {
  static const std::regex kFailed(R"(^\s*\d+ - (\S+) \(([^)]*)\))");
  static const std::regex kHarness(R"(^(.+?):(\d+): FAIL (.*)$)");
  static const std::regex kLabFail(R"(^\s*FAIL (\S+?): (.*)$)");
  std::vector<Finding> out;
  bool in_list = false;
  for (const auto view : split_lines(output)) {
    const std::string line(view);
    std::smatch m;
    if (line.starts_with("The following tests FAILED")) {
      in_list = true;
      continue;
    }
    if (in_list && std::regex_search(line, m, kFailed)) {
      out.push_back(
          {Severity::error, "test " + m.str(1) + " failed (" + m.str(2) + ")",
           m.str(1) == "labs_smoke" ? "labs" : "tests/" + m.str(1) + ".cpp",
           "rerun it alone: ge test --preset=" + preset + " --filter=^" + m.str(1) + "$"});
      continue;
    }
    if (std::regex_match(line, m, kHarness)) {
      const std::string where = where_in(repo, m.str(1), m.str(2));
      out.push_back({Severity::error, m.str(3), where, "fix the code or the test at " + where});
    } else if (std::regex_match(line, m, kLabFail)) {
      out.push_back({Severity::error, m.str(2), "lab " + m.str(1),
                     "run it alone: ge lab " + m.str(1) + " --smoke"});
    }
  }
  return out;
}

Outcome run_build(const BuildOptions& o) {
  Outcome out;
  if (!build_step(o, out)) return out;
  out.result = json_obj({{"preset", json_str(o.preset)},
                         {"log", json_str(rel_path(o.repo, log_path(o, "ge-build")))}});
  out.summary = "built the " + o.preset + " preset";
  out.next = "ge test";
  return out;
}

Outcome run_test(const BuildOptions& o, const std::string& filter) {
  Outcome out;
  if (!build_step(o, out)) return out;
  const fs::path log = log_path(o, "ge-test");
  std::error_code ec;
  fs::remove(log, ec);
  std::string cmd = "ctest --preset " + o.preset;
  if (!filter.empty()) cmd += " -R \"" + filter + "\"";
  const Ran r = run_logged(cmd, log);
  static const std::regex kTotals(R"((\d+)% tests passed, (\d+) tests? failed out of (\d+))");
  std::smatch m;
  long long failed = 0, total = 0;
  if (std::regex_search(r.output, m, kTotals)) {
    failed = std::stoll(m.str(2));
    total = std::stoll(m.str(3));
  }
  out.findings = test_findings(o.repo, r.output, o.preset);
  if (r.code != 0 && out.findings.empty()) {
    out.findings.push_back({Severity::error,
                            total == 0 ? "no test ran: " + tail_of(r.output, 2)
                                       : "ctest failed (exit " + std::to_string(r.code) + ")",
                            rel_path(o.repo, log), "read the log"});
  }
  out.result = json_obj({{"preset", json_str(o.preset)},
                         {"total", json_num(total)},
                         {"failed", json_num(failed)},
                         {"log", json_str(rel_path(o.repo, log))}});
  out.summary = std::to_string(total) + " test(s) passed";
  out.next = "ge check";
  return out;
}

Outcome run_check(const BuildOptions& o) {
  Outcome out;
  std::vector<std::string> steps;
  auto step = [&](std::string_view name, bool ok) {
    steps.push_back(json_obj({{"step", json_str(name)}, {"ok", json_bool(ok)}}));
  };

  // 1. Format: what CI's fmt-check runs, file by file so each one is a finding.
  const fs::path log = log_path(o, "ge-check");
  std::error_code ec;
  fs::remove(log, ec);
  if (run_logged("clang-format --version", log).code != 0) {
    out.findings.push_back({Severity::warn, "clang-format is not installed; format not checked", "",
                            "install clang-format: CI runs the format check"});
  } else {
    std::vector<fs::path> files;
    sources_in(o.repo, files);
    size_t before = out.findings.size();
    for (size_t at = 0; at < files.size(); at += 40) {
      std::string cmd = "clang-format --dry-run --Werror";
      for (size_t k = at; k < std::min(files.size(), at + 40); k++)
        cmd += " " + quote_path(files[k]);
      const Ran r = run_logged(cmd, log);
      std::vector<std::string> seen;
      for (const Finding& f : compiler_findings(o.repo, r.output, 1000)) {
        const std::string file = f.where.substr(0, f.where.find(':'));
        if (std::find(seen.begin(), seen.end(), file) != seen.end()) continue;
        seen.push_back(file);
        out.findings.push_back(
            {Severity::error, "not clang-formatted", file, "clang-format -i " + file});
      }
    }
    step("format", out.findings.size() == before);
  }

  // 2. The repo against the engine's rules.
  const auto doctored = doctor(o.repo);
  out.findings.insert(out.findings.end(), doctored.begin(), doctored.end());
  step("doctor", count_of(doctored, Severity::error) == 0);

  // 3. and 4. The build and the tests: a broken build ends the check.
  Outcome tested = run_test(o, "");
  const bool built = tested.result.find("\"total\"") != std::string::npos;
  out.findings.insert(out.findings.end(), tested.findings.begin(), tested.findings.end());
  step("build", built);
  if (built) step("test", count_of(tested.findings, Severity::error) == 0);

  out.result = json_obj({{"preset", json_str(o.preset)}, {"steps", json_arr(steps)}});
  out.summary = "format, doctor, build and tests all pass";
  out.next = "commit (skill: commit)";
  return out;
}

Outcome run_lab(const BuildOptions& o, const LabOptions& lab) {
  Outcome out;
  if (!build_step(o, out)) return out;
  const fs::path exe = o.repo / "build" / o.preset / "labs";
  const fs::path log = log_path(o, "ge-lab");
  std::error_code ec;
  fs::remove(log, ec);
  if (lab.id.empty()) {
    const Ran r = run_logged(quote_path(exe) + " --json", log);
    if (r.code != 0) {
      out.findings.push_back({Severity::error, "the labs executable did not list its labs",
                              rel_path(o.repo, log), "read the log"});
      return out;
    }
    out.result = r.output;
    while (!out.result.empty() && (out.result.back() == '\n' || out.result.back() == ' ')) {
      out.result.pop_back();
    }
    std::smatch m;
    std::string ids;
    static const std::regex kId("\"id\":\"([^\"]+)\"");
    for (auto it = std::sregex_iterator(r.output.begin(), r.output.end(), kId);
         it != std::sregex_iterator(); ++it) {
      ids += "  " + (*it)[1].str() + "\n";
    }
    out.text = ids;
    out.summary = "the labs above; `ge lab <id> -- --help` for one's flags";
    out.next = "ge lab <id>";
    return out;
  }
  const fs::path previews = o.repo / "previews";
  std::string cmd = quote_path(exe) + " " + lab.id + " --out=" + quote_path(previews);
  if (lab.smoke) cmd += " --smoke";
  if (!lab.tag.empty()) cmd += " --tag=" + lab.tag;
  if (!lab.compare.empty()) cmd += " --compare=" + lab.compare;
  for (const std::string& w : lab.passthrough) cmd += " \"" + w + "\"";
  const Ran r = run_logged(cmd, log);
  out.text = r.output;
  const fs::path dir = previews / (lab.id + (lab.tag.empty() ? "" : "@" + lab.tag));
  const auto report = read_text(dir / "report.json");
  if (r.code == 2) {
    out.findings.push_back({Severity::error, "the lab refused its flags: " + tail_of(r.output, 2),
                            "lab " + lab.id, "ge lab " + lab.id + " -- --help lists them"});
    out.exit = 2;
    return out;
  }
  for (const auto view : split_lines(r.output)) {
    const std::string line(view);
    const size_t a = line.find("FAIL ");
    if (a == std::string::npos || line.find_first_not_of(' ') != a) continue;
    out.findings.push_back({Severity::error, line.substr(a + 5),
                            rel_path(o.repo, dir / "report.json"),
                            "read the report and its pictures; skill: labs"});
  }
  if (r.code != 0 && out.findings.empty()) {
    out.findings.push_back({Severity::error, "the lab failed (exit " + std::to_string(r.code) + ")",
                            rel_path(o.repo, log), "read the log"});
  }
  out.result = json_obj({{"dir", json_str(rel_path(o.repo, dir))},
                         {"log", json_str(rel_path(o.repo, log))},
                         {"report", report.value_or("null")}});
  out.summary = "lab " + lab.id + " ran clean; its report is " + rel_path(o.repo, dir) +
                "/report.json and its pictures are beside it";
  out.next = lab.tag.empty()
                 ? "look at the pictures, then tag a baseline: ge lab " + lab.id + " --tag=before"
                 : "change the code, then ge lab " + lab.id + " --compare=" + lab.tag;
  return out;
}

}  // namespace ge::cli
