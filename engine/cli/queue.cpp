// `ge queue` and `ge pr clash` (queue.hpp): the open pull requests and their queue events read
// through `gh api`, their heads fetched once, and the merges done by `git merge-tree` and
// `git commit-tree`, so no branch moves.
//
// ge:summary ge queue and ge pr clash: the queue read off the timelines, the merges done in git
// ge:keywords cli ge queue pr clash merge-tree commit-tree gh-api timeline fetch pull-head
// ge:skill agent-interface
// ge:test tests/cli_queue_test.cpp
#include "cli/queue.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <regex>

#include "cli/build.hpp"
#include "cli/finding.hpp"
#include "cli/json.hpp"

namespace ge::cli {

std::vector<std::string> merge_tree_conflicts(std::string_view output, int code) {
  std::vector<std::string> files;
  if (code == 0) return files;
  // The tree's id, then the conflicted files one a line, then a blank line and the messages.
  const std::vector<std::string_view> lines = split_lines(output);
  for (std::size_t i = 1; i < lines.size(); ++i) {
    std::string_view l = lines[i];
    while (!l.empty() && l.back() == '\r') l.remove_suffix(1);
    if (l.empty()) break;
    if (std::find(files.begin(), files.end(), l) == files.end()) files.emplace_back(l);
  }
  return files;
}

int queue_pr_of_branch(std::string_view branch) {
  if (branch.rfind("gh-readonly-queue/", 0) != 0) return 0;
  const std::size_t at = branch.find("/pr-");
  if (at == std::string_view::npos) return 0;
  int n = 0;
  for (std::size_t i = at + 4;
       i < branch.size() && std::isdigit(static_cast<unsigned char>(branch[i])); ++i)
    n = n * 10 + (branch[i] - '0');
  return n;
}

std::string queued_since(std::string_view events) {
  std::string since;
  for (std::string_view line : split_lines(events)) {
    const std::size_t tab = line.find('\t');
    if (tab == std::string_view::npos) continue;
    const std::string_view event = line.substr(0, tab);
    std::string_view when = line.substr(tab + 1);
    while (!when.empty() && (when.back() == '\r' || when.back() == ' ')) when.remove_suffix(1);
    since = event == "added_to_merge_queue" ? std::string(when) : std::string();
  }
  return since;
}

std::vector<QueuedPr> queue_order(std::vector<QueuedPr> prs) {
  std::stable_sort(prs.begin(), prs.end(), [](const QueuedPr& a, const QueuedPr& b) {
    if (a.since.empty() != b.since.empty()) return !a.since.empty();
    if (a.since != b.since) return a.since < b.since;
    return a.number < b.number;
  });
  return prs;
}

namespace {

std::string quoted(const fs::path& p) { return "\"" + p.string() + "\""; }

std::string first_line(std::string_view text) {
  return std::string(text.substr(0, text.find('\n')));
}

struct OpenPr {
  int number = 0;
  std::string ref, title, since;
  bool armed = false, draft = false;
};

struct Board {
  std::string api;   // repos/<owner>/<name>
  std::string base;  // origin/main
  std::vector<OpenPr> open;
  std::vector<QueuedPr> order;  // queued, then armed
  fs::path log;
};

// The open pull requests, each one's queue state, and the queue's order; false with a finding.
bool read_board(const fs::path& repo, const std::string& base, Board& b, Outcome& out) {
  b.base = base;
  b.log = repo / "build" / "ge-queue.log";
  std::error_code ec;
  fs::create_directories(b.log.parent_path(), ec);
  const auto fail = [&](const std::string& what, const std::string& next) {
    out.findings.push_back({Severity::error, what, rel_path(repo, b.log), next});
    return false;
  };
  if (run_to_log("gh --version", b.log).code != 0)
    return fail("GitHub's gh command line is not installed", "install gh and run `gh auth login`");
  const std::string url =
      first_line(run_to_log("git -C " + quoted(repo) + " remote get-url origin", b.log).output);
  static const std::regex kSlug(R"(github\.com[:/]([^/\s]+)/([^/\s]+?)(?:\.git)?/?$)");
  std::smatch slug;
  if (!std::regex_search(url, slug, kSlug))
    return fail("the repo has no GitHub origin remote (" + url + ")",
                "add one: git remote add origin <url>");
  b.api = "repos/" + slug.str(1) + "/" + slug.str(2);
  const Logged prs = run_to_log("gh api --paginate \"" + b.api +
                                    "/pulls?state=open&per_page=100\" --jq '.[]|[.number,.head.ref,"
                                    "(.auto_merge!=null),.draft,.title]|@tsv'",
                                b.log);
  if (prs.code != 0)
    return fail("gh could not list the open pull requests: " + first_line(prs.output),
                "check `gh auth status`, then ge queue again");
  for (std::string_view line : split_lines(prs.output)) {
    std::vector<std::string> f;
    std::size_t at = 0;
    while (f.size() < 4) {
      const std::size_t tab = line.find('\t', at);
      if (tab == std::string_view::npos) break;
      f.emplace_back(line.substr(at, tab - at));
      at = tab + 1;
    }
    if (f.size() < 4 || f[0].empty() || !std::isdigit(static_cast<unsigned char>(f[0][0])))
      continue;
    OpenPr p;
    p.number = std::stoi(f[0]);
    p.ref = f[1];
    p.armed = f[2] == "true";
    p.draft = f[3] == "true";
    p.title = std::string(line.substr(at));
    b.open.push_back(std::move(p));
  }
  for (OpenPr& p : b.open) {
    const Logged ev = run_to_log(
        "gh api --paginate \"" + b.api + "/issues/" + std::to_string(p.number) +
            "/timeline?per_page=100\" --jq '.[]|select(.event==\"added_to_merge_queue\" or "
            ".event==\"removed_from_merge_queue\" or "
            ".event==\"merged\")|[.event,.created_at]|@tsv'",
        b.log);
    p.since = queued_since(ev.output);
    if (!p.since.empty() || p.armed) b.order.push_back({p.number, p.since});
  }
  b.order = queue_order(std::move(b.order));
  return true;
}

// Fetches the base and the heads of `numbers` to refs/ge/pull/<n>; false when the fetch failed.
bool fetch_heads(const fs::path& repo, const Board& b, const std::vector<int>& numbers) {
  const std::string branch = b.base.rfind("origin/", 0) == 0 ? b.base.substr(7) : b.base;
  std::string cmd = "git -C " + quoted(repo) + " fetch --quiet origin +refs/heads/" + branch +
                    ":refs/remotes/origin/" + branch;
  for (const int n : numbers) {
    const std::string s = std::to_string(n);
    cmd += " +refs/pull/";
    cmd += s;
    cmd += "/head:refs/ge/pull/";
    cmd += s;
  }
  return run_to_log(cmd, b.log).code == 0;
}

std::string head_ref(int n) { return "refs/ge/pull/" + std::to_string(n); }

struct Merge {
  std::string tree;  // the merged tree, "" on a conflict
  std::vector<std::string> conflicts;
};

Merge merge_tree(const fs::path& repo, const std::string& a, const std::string& b,
                 const fs::path& log) {
  const Logged r = run_to_log(
      "git -C " + quoted(repo) + " merge-tree --write-tree --name-only " + a + " " + b, log);
  Merge m;
  m.conflicts = merge_tree_conflicts(r.output, r.code);
  if (r.code == 0) m.tree = first_line(r.output);
  if (r.code != 0 && m.conflicts.empty()) m.conflicts.push_back("(" + first_line(r.output) + ")");
  return m;
}

// A commit of `tree` with parents `a` and `b`: the chain the next pull request is merged onto.
std::string chain_commit(const fs::path& repo, const std::string& tree, const std::string& a,
                         const std::string& b, const fs::path& log) {
  return first_line(run_to_log("git -C " + quoted(repo) + " commit-tree " + tree + " -p " + a +
                                   " -p " + b + " -m ge-queue-chain",
                               log)
                        .output);
}

std::string joined(const std::vector<std::string>& v, std::string_view sep) {
  std::string out;
  for (const std::string& s : v) out.append(out.empty() ? "" : sep).append(s);
  return out;
}

}  // namespace

Outcome run_queue(const fs::path& repo, const std::string& base) {
  Outcome out;
  Board b;
  if (!read_board(repo, base, b, out)) return out;
  std::vector<int> all;
  all.reserve(b.open.size());
  for (const OpenPr& p : b.open) all.push_back(p.number);
  if (!fetch_heads(repo, b, all)) {
    out.findings.push_back({Severity::error, "git fetch of the pull requests' heads failed",
                            rel_path(repo, b.log), "check the remote: git remote -v"});
    return out;
  }
  std::map<int, const OpenPr*> by;
  for (const OpenPr& p : b.open) by[p.number] = &p;

  // The queue as it will merge: each one onto main and everything ahead of it.
  std::vector<std::string> queue_json;
  std::string chain = b.base;
  out.text += "  queue (merges in this order):\n";
  if (b.order.empty()) out.text += "    empty\n";
  for (const QueuedPr& q : b.order) {
    const OpenPr& p = *by[q.number];
    const Merge m = merge_tree(repo, chain, head_ref(q.number), b.log);
    std::string line =
        "    #" + std::to_string(q.number) + "  " +
        (q.since.empty() ? std::string("armed, not yet queued") : "queued " + q.since);
    if (!m.tree.empty()) {
      chain = chain_commit(repo, m.tree, chain, head_ref(q.number), b.log);
    } else {
      line += "  CLASHES with the queue ahead: " + joined(m.conflicts, " ");
      out.findings.push_back({Severity::warn,
                              "#" + std::to_string(q.number) +
                                  " conflicts with main and the queue ahead of it in " +
                                  joined(m.conflicts, ", "),
                              "",
                              "ge pr clash " + std::to_string(q.number) +
                                  " names the pull request ahead that brings it in"});
    }
    out.text += line + "  " + p.title + "\n";
    queue_json.push_back(json_obj({{"pr", std::to_string(q.number)},
                                   {"queued", json_str(q.since)},
                                   {"clashes", json_strs(m.conflicts)}}));
  }

  // Every other ready pull request that a real merge with main refuses.
  std::vector<std::string> dirty_json;
  std::string dirty;
  for (const OpenPr& p : b.open) {
    if (p.draft || p.armed || !p.since.empty()) continue;
    const Merge m = merge_tree(repo, b.base, head_ref(p.number), b.log);
    if (m.tree.empty()) {
      dirty += "    #" + std::to_string(p.number) + "  " + joined(m.conflicts, " ") + "\n";
      dirty_json.push_back(
          json_obj({{"pr", std::to_string(p.number)}, {"conflicts", json_strs(m.conflicts)}}));
    }
  }
  if (!dirty.empty()) out.text += "  conflicts with " + b.base + " (not armed):\n" + dirty;

  // The queue's latest failed group run for each open pull request, and the jobs that failed.
  std::vector<std::string> failed_json;
  const Logged runs =
      run_to_log("gh api \"" + b.api +
                     "/actions/runs?event=merge_group&per_page=40\" --jq '.workflow_runs[]|[.id,"
                     ".head_branch,.status,.conclusion]|@tsv'",
                 b.log);
  std::map<int, bool> seen;
  std::string failed;
  for (std::string_view line : split_lines(runs.output)) {
    std::vector<std::string> f;
    std::size_t at = 0;
    for (std::size_t tab; (tab = line.find('\t', at)) != std::string_view::npos; at = tab + 1)
      f.emplace_back(line.substr(at, tab - at));
    f.emplace_back(line.substr(at));
    if (f.size() < 4) continue;
    const int n = queue_pr_of_branch(f[1]);
    if (n == 0 || seen[n] || !by.count(n)) continue;
    seen[n] = true;  // the newest run alone: runs come newest first
    if (f[3] != "failure") continue;
    const Logged jobs = run_to_log("gh api \"" + b.api + "/actions/runs/" + f[0] +
                                       "/jobs?per_page=100\" --jq '.jobs[]|select(.conclusion==\""
                                       "failure\" and .name!=\"required\")|.name'",
                                   b.log);
    std::vector<std::string> names;
    for (std::string_view j : split_lines(jobs.output)) {
      if (!j.empty()) names.emplace_back(j);
    }
    failed += "    #" + std::to_string(n) + "  run " + f[0] + ": " +
              (names.empty() ? std::string("no job failed (cancelled or abandoned)")
                             : joined(names, ", ")) +
              "\n";
    failed_json.push_back(
        json_obj({{"pr", std::to_string(n)}, {"run", f[0]}, {"jobs", json_strs(names)}}));
  }
  if (!failed.empty()) out.text += "  last queue run failed:\n" + failed;

  out.result = json_obj({{"base", json_str(b.base)},
                         {"queue", json_arr(queue_json)},
                         {"dirty", json_arr(dirty_json)},
                         {"failed", json_arr(failed_json)}});
  out.summary = std::to_string(b.order.size()) + " in or joining the queue, " +
                std::to_string(dirty_json.size()) + " conflicting with " + b.base;
  out.next = "ge pr clash <n> before arming a pull request: it merges it onto the queue as above";
  return out;
}

Outcome run_pr_clash(const fs::path& repo, const std::string& pr, const std::string& base) {
  Outcome out;
  if (pr.empty() || pr.find_first_not_of("0123456789") != std::string::npos) {
    out.findings.push_back(
        {Severity::error, "\"" + pr + "\" is not a pull request's number", "", "ge pr clash 209"});
    out.exit = 2;
    return out;
  }
  const int n = std::stoi(pr);
  Board b;
  if (!read_board(repo, base, b, out)) return out;
  // Ahead of it: the queue up to its own place, or all of it when it is not in.
  std::vector<int> ahead;
  for (const QueuedPr& q : b.order) {
    if (q.number == n) break;
    ahead.push_back(q.number);
  }
  std::vector<int> heads = ahead;
  heads.push_back(n);
  if (!fetch_heads(repo, b, heads)) {
    out.findings.push_back({Severity::error, "git fetch of #" + pr + "'s head failed",
                            rel_path(repo, b.log), "check the number and the remote"});
    return out;
  }
  const Merge onto_base = merge_tree(repo, b.base, head_ref(n), b.log);
  out.text += "  onto " + b.base + ": " +
              (onto_base.tree.empty() ? "CONFLICTS in " + joined(onto_base.conflicts, " ")
                                      : std::string("clean")) +
              "\n";
  std::string chain = b.base;
  std::string culprit;
  std::vector<std::string> culprit_files, skipped;
  for (const int a : ahead) {
    const Merge step = merge_tree(repo, chain, head_ref(a), b.log);
    if (step.tree.empty()) {
      skipped.push_back("#" + std::to_string(a));  // ejected itself when its turn comes
      continue;
    }
    chain = chain_commit(repo, step.tree, chain, head_ref(a), b.log);
    if (culprit.empty() && !onto_base.tree.empty()) {
      const Merge now = merge_tree(repo, chain, head_ref(n), b.log);
      if (now.tree.empty()) {
        culprit = "#" + std::to_string(a);
        culprit_files = now.conflicts;
      }
    }
  }
  const Merge onto_queue = merge_tree(repo, chain, head_ref(n), b.log);
  out.text += "  onto " + b.base + " and the " + std::to_string(ahead.size() - skipped.size()) +
              " ahead of it: " +
              (onto_queue.tree.empty() ? "CONFLICTS in " + joined(onto_queue.conflicts, " ")
                                       : std::string("clean")) +
              "\n";
  if (!culprit.empty())
    out.text += "  first brought in by " + culprit + " (" + joined(culprit_files, " ") + ")\n";
  if (!skipped.empty())
    out.text += "  left out, clashing with the queue ahead of them: " + joined(skipped, " ") + "\n";

  if (!onto_base.tree.empty() && onto_queue.tree.empty()) {
    out.findings.push_back(
        {Severity::error,
         "#" + pr + " merges onto " + b.base + " but conflicts with the queue ahead of it in " +
             joined(onto_queue.conflicts, ", "),
         "",
         "wait for " + (culprit.empty() ? std::string("the queue ahead") : culprit) +
             " to merge, then merge " + b.base + " and push; or ask for it to be dequeued"});
  } else if (onto_base.tree.empty()) {
    out.findings.push_back(
        {Severity::error,
         "#" + pr + " conflicts with " + b.base + " in " + joined(onto_base.conflicts, ", "), "",
         "git merge " + b.base + ", resolve each file, precheck, push"});
  }
  out.result = json_obj({{"pr", pr},
                         {"base", json_str(b.base)},
                         {"base_conflicts", json_strs(onto_base.conflicts)},
                         {"queue_conflicts", json_strs(onto_queue.conflicts)},
                         {"first_ahead", json_str(culprit)}});
  out.summary = onto_queue.tree.empty() ? "#" + pr + " would be ejected"
                                        : "#" + pr + " merges onto " + b.base + " and the queue";
  out.next = onto_queue.tree.empty() ? "" : "arm it: it merges onto everything ahead of it";
  return out;
}

}  // namespace ge::cli
