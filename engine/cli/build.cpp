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
#include <thread>

#include "cli/doctor.hpp"
#include "cli/json.hpp"
#include "cli/reach.hpp"

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

// The tests `filter` picks, by name, once the preset is configured ("" when none or ctest fails).
std::vector<std::string> tests_matching(const BuildOptions& o, const std::string& filter,
                                        const fs::path& log) {
  static const std::regex kTest(R"(^\s*Test\s+#\d+: (\S+))");
  const Ran r = run_logged("ctest --preset " + o.preset + " -N -R \"" + filter + "\"", log);
  std::vector<std::string> names;
  if (r.code != 0) return names;
  for (const auto view : split_lines(r.output)) {
    const std::string line = regex_line(view);
    std::smatch m;
    if (std::regex_search(line, m, kTest)) names.push_back(m.str(1));
  }
  return names;
}

// Configure and build. With `filter`, only the targets of the tests it picks (a test is its own
// executable, the smoke pass the labs executable); with `only`, those targets (space-separated);
// the whole tree when neither names one or a target is unknown.
bool build_step(const BuildOptions& o, Outcome& out, const std::string& filter = "",
                const std::string& only = "") {
  const fs::path log = log_path(o, "ge-build");
  std::error_code ec;
  fs::remove(log, ec);
  if (!configure(o, log, out)) return false;
  std::string targets = only.empty() ? "" : " " + only;
  if (!filter.empty()) {
    for (const std::string& t : tests_matching(o, filter, log))
      targets.append(" ").append(t == "labs_smoke" || t.starts_with("labs_smoke_") ? "labs" : t);
  }
  Ran r{0, ""};
  if (!targets.empty()) {
    r = run_logged("cmake --build --preset " + o.preset + " --target" + targets, log);
    // A test named for no target of its own: build everything as before.
    const bool unknown =
        r.code != 0 && (r.output.find("unknown target") != std::string::npos ||
                        r.output.find("No rule to make target") != std::string::npos ||
                        r.output.find("MSB1009") != std::string::npos);
    if (unknown) targets.clear();
  }
  if (targets.empty()) r = run_logged("cmake --build --preset " + o.preset, log);
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

Logged run_to_log(const std::string& command, const fs::path& log) {
  const Ran r = run_logged(command, log);
  return {r.code, r.output};
}

bool build_into(const BuildOptions& o, Outcome& out) { return build_step(o, out); }

bool build_targets(const BuildOptions& o, Outcome& out, const std::string& targets) {
  return build_step(o, out, "", targets);
}

fs::path build_log(const BuildOptions& o, std::string_view name) { return log_path(o, name); }

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
    const std::string line = regex_line(lines[i]);
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
    const std::string line = regex_line(view);
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
      const bool baseline = m.str(2).find("the baseline") != std::string::npos ||
                            m.str(2).find("not in ") != std::string::npos;
      out.push_back({Severity::error, m.str(2), "lab " + m.str(1),
                     baseline ? "ge digest " + m.str(1) +
                                    " checks it alone; if the move is meant, ge digest " +
                                    m.str(1) + " --write and say why in the PR"
                              : "run it alone: ge lab " + m.str(1) + " --smoke"});
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

Outcome run_test(const BuildOptions& o, const std::string& filter, const std::string& affected,
                 bool list) {
  Outcome out;
  std::string pick = filter;
  std::vector<std::string> picked;
  size_t of = 0;
  if (!affected.empty()) {
    if (!filter.empty()) {
      out.findings.push_back({Severity::error, "--affected and --filter together", "",
                              "pick the tests one way: what the change reaches (--affected) or "
                              "by name (--filter=<regex>)"});
      out.exit = 2;
      return out;
    }
    const fs::path log = log_path(o, "ge-build");
    if (!configure(o, log, out)) return out;
    const fs::path build = o.repo / "build" / o.preset;
    const std::vector<std::string> tests = tests_matching(o, ".", log);
    of = tests.size();
    Reach reach;
    if (!change_reach(o.repo, build, affected, log, reach)) {
      out.findings.push_back({Severity::warn, "no merge base with " + affected + "; ran every test",
                              "",
                              "fetch it (`git fetch origin main`, or depth 0 in a shallow "
                              "clone), or name the base: `ge test --affected --base=<ref>`"});
    } else {
      picked = pick_affected(tests, reach.targets, reach.changed);
      if (picked.empty()) {
        out.result =
            json_obj({{"preset", json_str(o.preset)},
                      {"total", json_num(0)},
                      {"failed", json_num(0)},
                      {"changed", json_num(static_cast<long long>(reach.changed.size()))}});
        out.summary = "no test is reached by the " + std::to_string(reach.changed.size()) +
                      " file(s) changed since " + affected;
        out.next = "ge test runs them all";
        return out;
      }
      if (list) {
        std::vector<std::string> names;
        names.reserve(picked.size());
        for (const std::string& t : picked) names.push_back(json_str(t));
        out.result = json_obj({{"base", json_str(affected)}, {"tests", json_arr(names)}});
        std::string text;
        for (const std::string& t : picked) text.append(t).append("\n");
        out.summary = std::to_string(picked.size()) + " of " + std::to_string(of) +
                      " test(s) reached by the change since " + affected + ":\n" + text;
        out.next = "ge test --affected runs them";
        return out;
      }
      pick = "^(";
      for (size_t k = 0; k < picked.size(); k++) pick.append(k ? "|" : "").append(picked[k]);
      pick += ")$";
    }
  }
  if (!build_step(o, out, pick)) return out;
  const fs::path log = log_path(o, "ge-test");
  std::error_code ec;
  fs::remove(log, ec);
  std::string cmd = "ctest --preset " + o.preset + " -j " +
                    std::to_string(std::max(1u, std::thread::hardware_concurrency()));
  if (!pick.empty()) cmd += " -R \"" + pick + "\"";
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
  if (!picked.empty()) {
    out.summary = std::to_string(total) + " of " + std::to_string(of) +
                  " test(s), those the change since " + affected + " reaches, passed";
  }
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
  if (!lab.shots.empty()) cmd += " \"--shots=" + lab.shots + "\"";
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

Outcome run_lab_diff(const BuildOptions& o, const LabDiffOptions& diff) {
  Outcome out;
  if (!build_step(o, out)) return out;
  const fs::path exe = o.repo / "build" / o.preset / "labs";
  const fs::path log = log_path(o, "ge-lab-diff");
  const fs::path previews = o.repo / "previews";
  std::error_code ec;
  fs::remove(log, ec);
  std::string cmd = quote_path(exe) + " " + diff.id + " --out=" + quote_path(previews) +
                    (diff.run ? " --quiet --compare=" : " --diff=") + diff.against;
  if (!diff.bands.empty()) cmd += " --diff-bands=" + diff.bands;
  if (!diff.psnr.empty()) cmd += " --diff-psnr=" + diff.psnr;
  for (const std::string& w : diff.passthrough) cmd += " \"" + w + "\"";
  const Ran r = run_logged(cmd, log);
  out.text = r.output;
  if (r.code == 2) {
    out.findings.push_back({Severity::error, "the labs refused the flags: " + tail_of(r.output, 2),
                            "lab " + diff.id, "ge lab " + diff.id + " -- --help lists them"});
    out.exit = 2;
    return out;
  }
  const fs::path json = previews / diff.id / "diff" / "diff.json";
  const auto report = read_text(json);
  const std::string kept = rel_path(o.repo, previews / (diff.id + "@" + diff.against));
  if (!report || report->find("\"error\": \"\"") == std::string::npos) {
    out.findings.push_back({Severity::error, "nothing to diff: " + tail_of(r.output, 1), kept,
                            "keep a run first: ge lab " + diff.id + " --tag=" + diff.against +
                                ", change the code, then ge lab diff " + diff.id + " --run"});
    return out;
  }
  // The labs print a "DIFF <shot> <band>: ..." line for every band under its floor, and a shot
  // that could not be compared as "  <shot>: <why>".
  for (const auto view : split_lines(r.output)) {
    const std::string line(view);
    if (line.starts_with("DIFF ")) {
      const size_t at = line.find("; look at ");
      const std::string sheet = at == std::string::npos ? "" : line.substr(at + 10);
      out.findings.push_back(
          {Severity::error, line.substr(5, at == std::string::npos ? std::string::npos : at - 5),
           sheet.empty() ? "" : rel_path(o.repo, fs::path(sheet)),
           "look at the sheet: a change meant, say so in the PR (or loosen --psnr for that band); "
           "else find what moved it"});
    } else if (line.find(": not in the kept run") != std::string::npos ||
               line.find(": a different size") != std::string::npos ||
               line.find(": its picture could not be read") != std::string::npos) {
      out.findings.push_back(
          {Severity::warn, "shot " + line.substr(line.find_first_not_of(' ')), kept,
           "keep a run with the same flags: ge lab " + diff.id + " --tag=" + diff.against});
    }
  }
  if (r.code != 0 && out.findings.empty()) {
    out.findings.push_back({Severity::error, "the lab failed (exit " + std::to_string(r.code) + ")",
                            rel_path(o.repo, log), "read the log"});
  }
  out.result =
      json_obj({{"diff", report.value_or("null")}, {"log", json_str(rel_path(o.repo, log))}});
  out.summary = "every picture of " + diff.id + " is within its bands' floors against " + kept +
                "; the sheets are in " + rel_path(o.repo, previews / diff.id / "diff");
  out.next = "look at the sheets, then put the diff table in the PR";
  return out;
}

}  // namespace ge::cli
