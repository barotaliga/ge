// See lint.hpp.
//
// ge:summary ge lint's definitions: the files a change reaches, clang-tidy run, its findings
// ge:keywords cli ge lint clang-tidy ninja-deps merge-base findings
// ge:skill agent-interface
// ge:test tests/cli_lint_test.cpp
#include "cli/lint.hpp"

#include <algorithm>
#include <regex>
#include <set>
#include <system_error>
#include <thread>

#include "cli/json.hpp"
#include "cli/reach.hpp"

namespace ge::cli {

namespace {

std::string quoted(const fs::path& p) { return "\"" + p.string() + "\""; }

// `command`'s output lines, or none when it failed.
std::vector<std::string> lines_from(const std::string& command, const fs::path& log) {
  const Logged r = run_to_log(command, log);
  std::vector<std::string> out;
  if (r.code != 0) return out;
  for (const std::string_view l : split_lines(r.output)) {
    if (!l.empty()) out.emplace_back(l);
  }
  return out;
}

std::string relative(const fs::path& repo, const std::string& file) {
  std::error_code ec;
  const fs::path p = fs::weakly_canonical(fs::path(file), ec);
  const fs::path r = fs::weakly_canonical(repo, ec);
  const std::string ps = p.generic_string(), rs = r.generic_string() + "/";
  return ps.starts_with(rs) ? ps.substr(rs.size()) : fs::path(file).generic_string();
}

}  // namespace

std::string tidy_check_url(std::string_view check) {
  const size_t dash = check.find('-');
  if (check.starts_with("clang-analyzer-")) {
    return "https://clang.llvm.org/extra/clang-tidy/checks/clang-analyzer/" +
           std::string(check.substr(15)) + ".html";
  }
  if (dash == std::string_view::npos) return "https://clang.llvm.org/extra/clang-tidy/checks/";
  return "https://clang.llvm.org/extra/clang-tidy/checks/" + std::string(check.substr(0, dash)) +
         "/" + std::string(check.substr(dash + 1)) + ".html";
}

std::vector<Finding> lint_findings(const fs::path& repo, std::string_view output, int most) {
  static const std::regex kDiag(R"(^(.+?):(\d+):(\d+): (warning|error): (.*) \[([^\]]+)\]$)");
  std::vector<Finding> out;
  std::set<std::string> seen;
  int more = 0;
  for (const std::string_view view : split_lines(output)) {
    const std::string line = regex_line(view);
    std::smatch m;
    if (!std::regex_match(line, m, kDiag)) continue;
    std::string check = m.str(6);
    if (const size_t comma = check.find(','); comma != std::string::npos) check.resize(comma);
    const std::string where = relative(repo, m.str(1)).append(":").append(m.str(2));
    if (!seen.insert(std::string(where).append(" ").append(check)).second) continue;
    if (static_cast<int>(out.size()) >= most) {
      more++;
      continue;
    }
    out.push_back({Severity::error, m.str(5) + " [" + check + "]", where,
                   "fix it at " + where + " (" + tidy_check_url(check) +
                       " says what the check guards against), then `ge lint`"});
  }
  if (more > 0) {
    out.push_back({Severity::note, std::to_string(more) + " more clang-tidy findings", "",
                   "fix the ones above and `ge lint` again"});
  }
  return out;
}

Outcome run_lint(const BuildOptions& o, const LintOptions& l) {
  Outcome out;
  // The compile database and the generated headers come from the build.
  if (!build_into(o, out)) return out;
  const fs::path build = o.repo / "build" / o.preset;
  const fs::path log = build_log(o, "ge-lint");
  std::error_code ec;
  fs::remove(log, ec);
  if (run_quiet("clang-tidy --version") != 0) {
    out.findings.push_back({Severity::error, "clang-tidy is not installed", "",
                            "install it (LLVM's clang-tidy, the version of your clang), then "
                            "`ge lint`; `ge doctor` lists what else is missing"});
    return out;
  }
  const std::string git = "git -C " + quoted(o.repo) + " ";
  std::vector<std::string> all;
  for (const std::string& f :
       lines_from(git + "ls-files --cached --others --exclude-standard", log)) {
    if (f.ends_with(".cpp")) all.push_back(f);
  }
  std::vector<std::string> files;
  std::string scope;
  if (!l.files.empty()) {
    files = l.files;
    scope = "the files named";
  } else if (l.all) {
    files = all;
    scope = "every .cpp";
  } else {
    Reach reach;
    if (!change_reach(o.repo, build, l.base, log, reach)) {
      out.findings.push_back({Severity::warn,
                              "no merge base with " + l.base + "; linted every .cpp", "",
                              "fetch it (`git fetch origin main`, or depth 0 in a shallow "
                              "clone), or name the base: `ge lint --base=<ref>`"});
      files = all;
      scope = "every .cpp";
    } else {
      for (const std::string& f : all) {
        if (reach.sources.count(f)) files.push_back(f);
      }
      scope = "what the change since " + l.base + " reaches";
    }
  }
  if (files.empty()) {
    out.summary = "nothing to lint: no .cpp is " + scope;
    return out;
  }
  // A repository's own .clang-tidy, or the bug-finding set.
  const bool own = fs::is_regular_file(o.repo / ".clang-tidy", ec);
  std::string tidy = "clang-tidy -p " + quoted(build) + " --quiet";
  if (!own) tidy += " --checks='" + std::string(kDefaultTidyChecks) + "' --warnings-as-errors='*'";
  const fs::path list = build / "ge-lint-files.txt";
  std::string listed;
  for (const std::string& f : files) listed += quoted(o.repo / f) + "\n";
  std::string error;
  if (!write_text(list, listed, error)) {
    out.findings.push_back({Severity::error, "could not write the file list: " + error,
                            relative(o.repo, list.string()),
                            "check the build directory is writable"});
    return out;
  }
#ifdef _WIN32
  std::string cmd = tidy;
  for (const std::string& f : files) cmd += " " + quoted(o.repo / f);
#else
  const unsigned jobs = std::max(1u, std::thread::hardware_concurrency());
  const std::string cmd =
      "xargs -P " + std::to_string(jobs) + " -n 4 " + tidy + " < " + quoted(list);
#endif
  const Logged r = run_to_log(cmd, log);
  out.findings = lint_findings(o.repo, r.output, 40);
  if (r.code != 0 && out.findings.empty()) {
    out.findings.push_back({Severity::error,
                            "clang-tidy failed (exit " + std::to_string(r.code) + ")",
                            relative(o.repo, log.string()), "read the log"});
  }
  std::vector<std::string> names;
  names.reserve(files.size());
  for (const std::string& f : files) names.push_back(json_str(f));
  out.result =
      json_obj({{"scope", json_str(scope)},
                {"files", json_arr(names)},
                {"checks", json_str(own ? ".clang-tidy" : std::string(kDefaultTidyChecks))},
                {"log", json_str(relative(o.repo, log.string()))}});
  out.summary = std::to_string(files.size()) + " of " + std::to_string(all.size()) +
                " .cpp files (" + scope + ") clean under " +
                (own ? std::string(".clang-tidy") : std::string("the bug-finding checks"));
  return out;
}

}  // namespace ge::cli
