// `ge precheck` and `ge ci` (ci.hpp).
//
// ge:summary ge precheck and ge ci's definitions: the gates in CI's order, a job log's findings
// ge:keywords cli ge precheck ci gh pr-checks log-failed github-actions timestamp findings
// ge:skill agent-interface
// ge:test tests/cli_ci_test.cpp
#include "cli/ci.hpp"

#include <algorithm>
#include <chrono>
#include <regex>
#include <thread>

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

std::vector<Finding> blocked_findings(const std::string& base, std::string_view rules,
                                      const std::vector<std::string>& passed) {
  std::vector<Finding> out;
  play::FlatJson m;
  std::string why;
  const size_t open = rules.find('[');
  if (open == std::string_view::npos ||
      !play::read_flat("{\"rules\":" + std::string(rules.substr(open)) + "}", m, why, true)) {
    out.push_back({Severity::error,
                   "GitHub blocks the merge, and the rules on " + base + " could not be read", "",
                   "open the pull request's page to see which rule blocks it"});
    return out;
  }
  bool named = false;
  for (int i = 0; m.contains("rules." + std::to_string(i) + ".type"); i++) {
    const std::string k = "rules." + std::to_string(i) + ".";
    const std::string type = text_of(m, k + "type");
    if (type == "pull_request") {
      const std::string n = text_of(m, k + "parameters.required_approving_review_count");
      if (!n.empty() && n != "0") {
        out.push_back({Severity::error,
                       std::string("the rules on ")
                           .append(base)
                           .append(" require ")
                           .append(n)
                           .append(" approving review(s)"),
                       "", "ask the repo's owner for a review; the checks have passed"});
        named = true;
      }
      continue;
    }
    if (type != "required_status_checks") continue;
    const std::string checks = k + "parameters.required_status_checks.";
    for (int j = 0; m.contains(checks + std::to_string(j) + ".context"); j++) {
      const std::string name = text_of(m, checks + std::to_string(j) + ".context");
      if (std::find(passed.begin(), passed.end(), name) != passed.end()) continue;
      out.push_back(
          {Severity::error,
           std::string("the rules on ")
               .append(base)
               .append(" require the check \"")
               .append(name)
               .append("\", which never passed on this head"),
           "",
           "if no workflow runs a check by that name on a pull request, the repo's owner removes "
           "it from the ruleset (Settings, Rules); otherwise push to run it"});
      named = true;
    }
  }
  if (!named)
    out.push_back({Severity::error, "GitHub blocks the merge for a reason its rules do not name",
                   "",
                   "open the pull request's page (an organisation's rule, or a conversation to "
                   "resolve)"});
  return out;
}

Outcome run_ci(const fs::path& repo, const std::string& pr, const std::string& log_file,
               bool merge) {
  Outcome out;
  if (merge && (pr.empty() || !log_file.empty())) {
    out.findings.push_back(
        {Severity::error, "--merge needs a pull request's number", "", "ge ci <pr> --merge"});
    out.exit = 2;
    return out;
  }
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
  std::string sha, mergeable, base;
  if (!pr.empty()) {
    // The head, then whether it merges: GitHub's mergeable (null while it is still working it
    // out), its mergeable_state ("dirty" is a conflict), open or closed, merged, and the base.
    const Logged head = run_to_log("gh api " + api + "/pulls/" + pr +
                                       " --jq .head.sha,.mergeable,.mergeable_state,.state,"
                                       ".merged,.base.ref",
                                   log);
    if (head.code != 0) {
      const std::string from = slug.str(1) + "/" + slug.str(2);
      std::string to = first_line(run_to_log("gh api " + api + " --jq .full_name", log).output);
      if (to.find('/') == std::string::npos || to.find(' ') != std::string::npos) to.clear();
      Finding moved;
      if (moved_repo_finding(head.output, from, to, moved)) {
        out.findings.push_back(moved);
        return out;
      }
      return fail("gh could not read pull request " + pr + ": " + first_line(head.output),
                  "check the number, or `gh auth login`");
    }
    std::vector<std::string> w;
    // One line a field; gh prints a null as an empty line.
    for (std::string_view l : split_lines(head.output)) w.emplace_back(bare(l));
    if (w.size() < 6)
      return fail("gh answered pull request " + pr + " oddly: " + first_line(head.output),
                  "rerun; if it persists, open the pull request's page");
    sha = w[0];
    mergeable = w[2];
    base = w[5];
    if (w[4] == "true") {
      out.findings.push_back({Severity::note, "pull request " + pr + " is merged", "",
                              "restart the branch from " + base + " for the next change"});
    } else if (w[3] != "open") {
      out.findings.push_back({Severity::note, "pull request " + pr + " is closed", "",
                              "reopen it, or open a new one"});
    } else if (w[1] == "false" || w[2] == "dirty") {
      out.findings.push_back(
          {Severity::error, "pull request " + pr + " conflicts with " + base, "",
           "git fetch origin " + base + " && git merge origin/" + base +
               ", resolve each conflict, make precheck (ge precheck in a game repo), push"});
    } else if (w[1].empty() || w[1] == "null") {
      out.findings.push_back({Severity::note,
                              "GitHub is still working out whether " + pr + " merges", "",
                              "ge ci again in a minute"});
    } else if (w[2] == "behind") {
      out.findings.push_back({Severity::note, "pull request " + pr + " is behind " + base, "",
                              "merge origin/" + base + " when the base requires it"});
    }
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
  std::vector<std::string> failed, pending, passed, rows;
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
    if (conclusion == "success" || conclusion == "skipped" || conclusion == "neutral")
      passed.push_back(name);
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
  if (mergeable == "blocked" && failed.empty() && pending.empty()) {
    // Why, off the base's rules (the token may not read them: then the page is the answer).
    const Logged rules = run_to_log("gh api " + api + "/rules/branches/" + base, log);
    for (Finding& f : blocked_findings(base, rules.code == 0 ? rules.output : "", passed))
      out.findings.push_back(std::move(f));
  }
  if (!pending.empty()) {
    std::string names;
    for (const std::string& p : pending) names.append(names.empty() ? "" : ", ").append(p);
    out.findings.push_back({Severity::note,
                            std::to_string(pending.size()) + " still running: " + names, "",
                            "ge ci again when they finish"});
  }
  out.text = "  " + std::to_string(total) + " checks on " + sha.substr(0, 12) + ": " +
             std::to_string(failed.size()) + " failed, " + std::to_string(pending.size()) +
             " running\n";
  if (!mergeable.empty()) out.text += "  mergeable: " + mergeable + "\n";
  out.result = json_obj(
      {{"sha", json_str(sha)}, {"mergeable", json_str(mergeable)}, {"checks", json_arr(rows)}});
  out.summary = !pending.empty()       ? std::string(kCiPending)
                : mergeable == "clean" ? "every check passed and it merges cleanly: merge it"
                                       : "every check passed";
  out.next = pending.empty() ? ""
                             : "ge ci again in 15 minutes, until every check is green (or "
                               "ge ci <pr> --wait)";
  if (!merge) return out;
  // --merge: squash only what was just read as ready (every check done, none failed, GitHub's
  // state clean) and only at the head those checks ran on, so a push since is never merged.
  const bool ready = total > 0 && failed.empty() && pending.empty() &&
                     (mergeable == "clean" || mergeable == "has_hooks");
  if (!ready) {
    out.findings.push_back(
        {Severity::note,
         "not merged: " + std::string(!failed.empty()    ? "a check failed"
                                      : !pending.empty() ? "checks still running"
                                      : total == 0       ? "no checks ran"
                                                         : "GitHub's merge state is " +
                                                         (mergeable.empty() ? std::string("unknown")
                                                                            : mergeable)),
         "", "fix what the findings name, then ge ci " + pr + " --merge again"});
    return out;
  }
  const Logged done = run_to_log("gh api -X PUT " + api + "/pulls/" + pr +
                                     "/merge -f merge_method=squash -f sha=" + sha + " --jq .sha",
                                 log);
  if (done.code != 0) {
    return fail("GitHub refused to merge pull request " + pr + ": " + first_line(done.output),
                "ge ci " + pr + " to see why (a push since, or a rule on the base)");
  }
  const std::string merged = first_line(done.output);
  out.text += "  merged: squashed as " + merged.substr(0, 12) + "\n";
  out.result = json_obj({{"sha", json_str(sha)},
                         {"mergeable", json_str(mergeable)},
                         {"merged", json_str(merged)},
                         {"checks", json_arr(rows)}});
  out.summary = "pull request " + pr + " squash-merged";
  out.next = "restart the branch from its base for the next change";
  return out;
}

bool moved_repo_finding(std::string_view error, const std::string& slug,
                        const std::string& moved_to, Finding& out) {
  const bool redirected = error.find("repositories/") != std::string_view::npos ||
                          error.find("Moved Permanently") != std::string_view::npos ||
                          error.find("HTTP 301") != std::string_view::npos;
  const bool renamed = !moved_to.empty() && moved_to != slug;
  if (!redirected && !renamed) return false;
  out = {Severity::error,
         "the repository " + slug + " has moved" + (renamed ? " to " + moved_to : std::string()) +
             ", and its old address answers with a redirect",
         "the origin remote",
         renamed ? "git remote set-url origin https://github.com/" + moved_to + ", then ge ci"
                 : "set the origin remote to the repository's new address (git remote set-url "
                   "origin https://github.com/<owner>/<repo>), then ge ci"};
  return true;
}

bool ci_settled(const Outcome& o) {
  return count_of(o.findings, Severity::error) > 0 || o.summary != kCiPending;
}

Outcome run_ci_wait(const fs::path& repo, const std::string& pr, bool merge, int minutes) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::minutes(minutes);
  for (;;) {
    // Read without merging while checks run: --merge acts once, on the settled answer.
    Outcome o = run_ci(repo, pr, "", false);
    if (ci_settled(o))
      return merge && count_of(o.findings, Severity::error) == 0 ? run_ci(repo, pr, "", true) : o;
    if (std::chrono::steady_clock::now() >= until) {
      o.findings.push_back({Severity::note,
                            "still running after " + std::to_string(minutes) + " minutes", "",
                            "ge ci " + pr + " --wait again, or --wait=<minutes> for longer"});
      return o;
    }
    std::this_thread::sleep_for(std::chrono::seconds(60));
  }
}

}  // namespace ge::cli
