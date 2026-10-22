// `ge precheck` and `ge ci` (ci.hpp).
//
// ge:summary ge precheck and ge ci's definitions: the gates in CI's order, a job log's findings
// ge:keywords cli ge precheck ci gh pr-checks log-failed github-actions timestamp findings
// ge:skill agent-interface
// ge:test tests/cli_ci_test.cpp
#include "cli/ci.hpp"

#include <regex>

#include "cli/json.hpp"
#include "cli/play.hpp"
#include "play/json_read.hpp"

namespace ge::cli {

namespace {

constexpr size_t kMostFindings = 20;  // a job's; the log holds the rest

std::string quoted(const fs::path& p) { return "\"" + p.string() + "\""; }

// A log line without what GitHub puts before it: `gh run view --log-failed`'s "job\tstep\t", the
// runner's "2026-10-22T13:44:36.9046158Z " and a BOM.
std::string_view bare(std::string_view line) {
  if (line.starts_with("\xEF\xBB\xBF")) line.remove_prefix(3);
  for (int tabs = 0; tabs < 2; tabs++) {
    const size_t t = line.find('\t');
    if (t == std::string_view::npos) break;
    line.remove_prefix(t + 1);
  }
  if (line.size() > 28 && line[4] == '-' && line[10] == 'T') {
    const size_t z = line.find("Z ");
    if (z != std::string_view::npos && z < 32) line.remove_prefix(z + 2);
  }
  while (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  return line;
}

Severity severity_of(const std::string& word) {
  return word == "error" ? Severity::error : word == "warn" ? Severity::warn : Severity::note;
}

}  // namespace

std::vector<Finding> ci_log_findings(const fs::path& repo, std::string_view job,
                                     std::string_view log) {
  std::vector<std::string> lines;
  for (std::string_view l : split_lines(log)) lines.emplace_back(bare(l));
  std::string text;
  for (const std::string& l : lines) text += l + "\n";

  std::vector<Finding> found;
  // ge's own findings: "ge <command>: <severity>: what", then "    where:" and "    next:".
  static const std::regex kGe(R"(^ge [a-z ]+: (error|warn|note): (.*)$)");
  for (size_t i = 0; i < lines.size(); i++) {
    std::smatch m;
    if (!std::regex_match(lines[i], m, kGe)) continue;
    Finding f{severity_of(m.str(1)), m.str(2), "", ""};
    for (size_t j = i + 1; j < lines.size() && j <= i + 2; j++) {
      if (lines[j].starts_with("    where: ")) f.where = lines[j].substr(11);
      if (lines[j].starts_with("    next: ")) f.next = lines[j].substr(10);
    }
    if (f.severity != Severity::note) found.push_back(std::move(f));
  }
  // The tools' own lines, when ge was not the one that ran them.
  if (found.empty()) {
    for (Finding& f : compiler_findings(repo, text, static_cast<int>(kMostFindings)))
      found.push_back(std::move(f));
    for (Finding& f : test_findings(repo, text, "debug")) found.push_back(std::move(f));
  }
  if (found.empty()) {
    // The last words before GitHub's "Process completed with exit code N".
    std::string last;
    for (const std::string& l : lines) {
      if (l.empty() || l.starts_with("##[") || l.starts_with("Post job") ||
          l.find("[command]") != std::string::npos)
        continue;
      if (l.starts_with("Cleaning up") || l.starts_with("Removing ")) continue;
      last = l;
    }
    found.push_back({Severity::error,
                     last.empty() ? "the job failed and its log names no cause"
                                  : "the job failed; its log ends: " + last,
                     "", "read the job's log from the end; then reproduce it with ge precheck"});
  }
  if (found.size() > kMostFindings) found.resize(kMostFindings);
  for (Finding& f : found) {
    if (!job.empty()) f.where = std::string(job) + ": " + (f.where.empty() ? "its log" : f.where);
    if (f.next.empty()) f.next = "fix it, then ge precheck before the push";
  }
  return found;
}

Outcome run_precheck(const BuildOptions& o, const std::vector<std::string>& presets) {
  Outcome out;
  std::vector<std::string> rows;
  for (const std::string& preset : presets) {
    BuildOptions p = o;
    p.preset = preset;
    std::vector<std::string> gates;
    auto take = [&](const std::string& gate, const Outcome& got) {
      for (Finding f : got.findings) {
        std::string what = "(";
        what.append(preset).append(", ").append(gate).append(") ").append(f.what);
        f.what = std::move(what);
        out.findings.push_back(std::move(f));
      }
      const bool ok = count_of(got.findings, Severity::error) == 0 && exit_of(got) == 0;
      gates.push_back(json_obj({{"gate", json_str(gate)}, {"ok", json_bool(ok)}}));
      return ok;
    };
    // CI's order: a red gate ends the preset, as it ends CI's job.
    bool ok = take("check", run_check(p));
    std::vector<std::string> logs;
    if (ok) {
      const Outcome sim = run_sim(p, "1,2,3", {});
      ok = take("sim", sim);
      play::FlatJson m;
      std::string why;
      if (ok && play::read_flat(sim.result, m, why, true)) {
        for (int i = 0; m.contains("runs." + std::to_string(i) + ".inputs"); i++) {
          logs.push_back(text_of(m, "runs." + std::to_string(i) + ".inputs"));
        }
      }
    }
    for (const std::string& log : logs) {
      if (!take("replay " + rel_path(o.repo, fs::path(log)), run_replay(p, log))) ok = false;
    }
    rows.push_back(json_obj(
        {{"preset", json_str(preset)}, {"ok", json_bool(ok)}, {"gates", json_arr(gates)}}));
    out.text += "  " + preset + ": " + (ok ? "every gate passes" : "red") + "\n";
  }
  out.result = json_obj({{"presets", json_arr(rows)}});
  out.summary = "CI's gates pass here on " + std::to_string(presets.size()) +
                (presets.size() == 1 ? " preset" : " presets") + "; push";
  out.next = "commit and push (skill: commit)";
  return out;
}

Outcome run_ci(const fs::path& repo, const std::string& pr, const std::string& log_file) {
  Outcome out;
  if (!log_file.empty()) {
    const auto text = read_text(fs::absolute(log_file));
    if (!text) {
      out.findings.push_back({Severity::error, "no log at " + log_file, log_file,
                              "save the failed job's log there, then rerun"});
      out.exit = 2;
      return out;
    }
    out.findings = ci_log_findings(repo, "", *text);
    out.result = json_obj({{"log", json_str(log_file)}});
    out.summary = "the log names no failure";
    out.next = "ge precheck reproduces CI's gates here";
    return out;
  }
  const fs::path log = repo / "build" / "ge-ci.log";
  std::error_code ec;
  fs::remove(log, ec);
  // The first line a command printed with something on it, or "(no output)".
  const auto first_line = [](const std::string& text) {
    for (std::string_view l : split_lines(text)) {
      if (l.find_first_not_of(" \r") != std::string_view::npos) return std::string(bare(l));
    }
    return std::string("(no output)");
  };
  const auto fail = [&](const std::string& what, const std::string& next) {
    out.findings.push_back({Severity::error, what, rel_path(repo, log), next});
    return out;
  };
  if (run_to_log("gh --version", log).code != 0) {
    return fail("GitHub's gh command line is not installed",
                "install gh and run `gh auth login`, or save the failed job's log and run "
                "ge ci --log=<file>");
  }
  // The repository off the origin remote; ge may run outside it (--repo).
  const Logged origin = run_to_log("git -C " + quoted(repo) + " remote get-url origin", log);
  static const std::regex kSlug(R"(github\.com[:/]([^/\s]+)/([^/\s]+?)(?:\.git)?/?$)");
  std::smatch slug;
  const std::string url = first_line(origin.output);
  if (origin.code != 0 || !std::regex_search(url, slug, kSlug)) {
    return fail("the repo has no GitHub origin remote (" + url + ")",
                "add one (git remote add origin <url>), or run ge ci --log=<file>");
  }
  const std::string api = "repos/" + slug.str(1) + "/" + slug.str(2);
  // The commit the checks ran on: the pull request's head, or this checkout's.
  std::string sha;
  if (!pr.empty()) {
    const Logged head = run_to_log("gh api " + api + "/pulls/" + pr + " --jq .head.sha", log);
    if (head.code != 0)
      return fail("gh could not read pull request " + pr + ": " + first_line(head.output),
                  "check the number, or `gh auth login`");
    sha = first_line(head.output);
  } else {
    const Logged head = run_to_log("git -C " + quoted(repo) + " rev-parse HEAD", log);
    sha = first_line(head.output);
  }
  const Logged runs =
      run_to_log("gh api \"" + api + "/commits/" + sha + "/check-runs?per_page=100\"", log);
  play::FlatJson m;
  std::string why;
  const size_t open = runs.output.find('{');
  if (runs.code != 0 || open == std::string::npos ||
      !play::read_flat(runs.output.substr(open), m, why, true)) {
    return fail("gh could not list the checks on " + sha + ": " + first_line(runs.output),
                "push the branch (its checks run on the pushed commit), or `gh auth login`");
  }
  std::vector<std::string> failed, pending, rows;
  int total = 0;
  for (int i = 0; m.contains("check_runs." + std::to_string(i) + ".id"); i++, total++) {
    const std::string k = "check_runs." + std::to_string(i) + ".";
    const std::string name = text_of(m, k + "name");
    const std::string status = text_of(m, k + "status");
    const std::string conclusion = text_of(m, k + "conclusion");
    const std::string link = text_of(m, k + "html_url");
    rows.push_back(json_obj({{"name", json_str(name)},
                             {"status", json_str(status)},
                             {"conclusion", json_str(conclusion)},
                             {"link", json_str(link)}}));
    if (status != "completed") {
      pending.push_back(name);
      continue;
    }
    if (conclusion != "failure" && conclusion != "timed_out") continue;
    failed.push_back(name);
    // A GitHub Actions check run's id is its job's id, so its log is one call away.
    const Logged job =
        run_to_log("gh api " + api + "/actions/jobs/" + text_of(m, k + "id") + "/logs", log);
    if (job.code != 0) {
      out.findings.push_back(
          {Severity::error, "check " + name + " failed", link,
           "open its link: its log could not be read (" + first_line(job.output) + ")"});
      continue;
    }
    for (Finding& f : ci_log_findings(repo, name, job.output)) out.findings.push_back(std::move(f));
  }
  for (const std::string& p : pending) {
    out.findings.push_back({Severity::note, "check " + p + " is still running", "", "ge ci again"});
  }
  out.text = "  " + std::to_string(total) + " checks on " + sha.substr(0, 12) + ": " +
             std::to_string(failed.size()) + " failed, " + std::to_string(pending.size()) +
             " running\n";
  out.result = json_obj({{"sha", json_str(sha)}, {"checks", json_arr(rows)}});
  out.summary = pending.empty() ? "every check passed" : "nothing has failed yet";
  out.next = pending.empty() ? "" : "ge ci again when they finish";
  return out;
}

}  // namespace ge::cli
