// `ge merge-main` (merge.hpp): the merge done by git, the unmerged paths read off `git status
// --porcelain`, the retired files and the digests settled, and the digests rewritten by
// `ge digest`'s own run.
//
// ge:summary ge merge-main: the merge, its unmerged paths planned and the mechanical ones settled
// ge:keywords cli ge merge-main git-merge porcelain git-rm checkout-theirs digest-write
// ge:skill agent-interface
// ge:test tests/cli_merge_test.cpp
#include "cli/merge.hpp"

#include "cli/finding.hpp"
#include "cli/golden.hpp"
#include "cli/json.hpp"

namespace ge::cli {

namespace {

std::string quoted(const fs::path& p) { return "\"" + p.string() + "\""; }

// The pieces joined into one string, in one allocation's worth of appends.
template <typename... Piece>
std::string cat(const Piece&... piece) {
  std::string out;
  (out += ... += piece);
  return out;
}

std::string git(const fs::path& repo) { return "git -C " + quoted(repo) + " "; }

std::string joined(const std::vector<std::string>& v, std::string_view sep) {
  std::string out;
  for (const std::string& s : v) {
    if (!out.empty()) out += sep;
    out += s;
  }
  return out;
}

// "tests/digests/<lab>.txt" -> "<lab>", "" for any other path.
std::string digest_lab(std::string_view path) {
  const std::string prefix = std::string(kDigestBaseline) + "/";
  if (!path.starts_with(prefix) || !path.ends_with(".txt")) return "";
  const std::string_view name = path.substr(prefix.size(), path.size() - prefix.size() - 4);
  if (name.empty() || name.find('/') != std::string_view::npos) return "";
  return std::string(name);
}

}  // namespace

MergePlan plan_merge(std::string_view porcelain) {
  MergePlan plan;
  for (std::string_view line : split_lines(porcelain)) {
    while (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.size() < 4 || line[2] != ' ') continue;
    const std::string_view code = line.substr(0, 2);
    const bool unmerged = code == "DD" || code == "AU" || code == "UD" || code == "UA" ||
                          code == "DU" || code == "AA" || code == "UU";
    if (!unmerged) continue;
    std::string path(line.substr(3));
    if (path.size() > 1 && path.front() == '"' && path.back() == '"')
      path = path.substr(1, path.size() - 2);
    const std::string lab = digest_lab(path);
    if (code == "UD") {
      plan.retired.push_back(std::move(path));
    } else if (!lab.empty() && (code == "UU" || code == "AA")) {
      plan.digests.push_back(std::move(path));
      plan.labs.push_back(lab);
    } else {
      plan.manual.push_back(std::move(path));
    }
  }
  return plan;
}

Outcome run_merge_main(const BuildOptions& o, const std::string& base) {
  Outcome out;
  const fs::path& repo = o.repo;
  const fs::path log = build_log(o, "ge-merge-main");
  std::error_code ec;
  fs::create_directories(log.parent_path(), ec);
  if (run_to_log(git(repo) + "rev-parse -q --verify MERGE_HEAD", log).code == 0) {
    out.findings.push_back({Severity::error, "a merge is already in progress", "the repository",
                            "finish it (resolve, git add, git commit --no-edit) or git merge "
                            "--abort, then run ge merge-main again"});
    return out;
  }
  const Logged dirty = run_to_log(git(repo) + "status --porcelain --untracked-files=no", log);
  if (dirty.code != 0 || !split_lines(dirty.output).empty()) {
    out.findings.push_back({Severity::error,
                            dirty.code != 0 ? "git status failed" : "uncommitted changes",
                            "the repository", "commit or set aside the changes, then rerun"});
    return out;
  }
  // origin/main is fetched first; any other ref is merged as it stands.
  if (base.rfind("origin/", 0) == 0) {
    const std::string branch = base.substr(7);
    if (run_to_log(git(repo) + "fetch --quiet origin " + branch, log).code != 0) {
      out.findings.push_back({Severity::error, "git fetch origin " + branch + " failed",
                              rel_path(repo, log), "check the remote and the network, then rerun"});
      return out;
    }
  }
  const Logged merged = run_to_log(git(repo) + "merge --no-edit " + base, log);
  if (merged.code == 0) {
    const bool none = merged.output.find("Already up to date") != std::string::npos;
    out.summary = none ? "already up to date with " + base : "merged " + base + " cleanly";
    out.next = none ? "" : "make precheck (ge precheck in a game repo), then push";
    out.result = json_obj({{"base", json_str(base)}, {"merged", json_bool(!none)}});
    return out;
  }
  const Logged status = run_to_log(git(repo) + "status --porcelain", log);
  const MergePlan plan = plan_merge(status.output);
  if (plan.retired.empty() && plan.digests.empty() && plan.manual.empty()) {
    out.findings.push_back({Severity::error, "git merge " + base + " failed with nothing unmerged",
                            rel_path(repo, log), "read the log; git merge --abort undoes it"});
    return out;
  }
  for (const std::string& p : plan.retired) {
    run_to_log(git(repo) + "rm --quiet -- " + quoted(fs::path(p)), log);
    out.text += cat("  retired on ", base, ", its deletion taken: ", p, "\n");
    out.findings.push_back({Severity::warn, cat(p, " was deleted on ", base, " and changed here"),
                            p,
                            cat("carry what this branch wrote in it to where ", base,
                                " moved it (git show HEAD:", p, ")")});
  }
  for (const std::string& p : plan.digests) {
    run_to_log(git(repo) + "checkout --theirs -- " + quoted(fs::path(p)), log);
    run_to_log(git(repo) + "add -- " + quoted(fs::path(p)), log);
    out.text += cat("  both sides moved, ", base, "'s taken until the lab reruns: ", p, "\n");
  }
  for (const std::string& p : plan.manual) {
    out.findings.push_back({Severity::error, "conflict to resolve by hand", p,
                            "resolve it, git add it, then git commit --no-edit"});
  }
  const std::string rerun =
      plan.labs.empty() ? "" : "ge digest " + joined(plan.labs, " ") + " --write";
  if (!plan.manual.empty()) {
    out.summary = std::to_string(plan.manual.size()) + " conflict(s) left by hand";
    out.next = "resolve them, git add, git commit --no-edit" +
               (rerun.empty() ? std::string() : "; then " + rerun) + "; then make precheck";
    return out;
  }
  if (!plan.labs.empty()) {
    DigestOptions d;
    d.labs = plan.labs;
    d.write = true;
    const Outcome digests = run_digest(o, d);
    for (const Finding& f : digests.findings) out.findings.push_back(f);
    if (exit_of(digests) != 0) {
      out.next = "fix the digest run (above), " + rerun + ", git add " +
                 std::string(kDigestBaseline) + ", then git commit --no-edit";
      return out;
    }
    run_to_log(git(repo) + "add -- " + std::string(kDigestBaseline), log);
    for (const std::string& p : plan.digests) out.written.push_back(p);
  }
  if (run_to_log(git(repo) + "commit --quiet --no-edit", log).code != 0) {
    out.findings.push_back({Severity::error, "git commit of the merge failed", rel_path(repo, log),
                            "read the log, then git commit --no-edit"});
    return out;
  }
  out.summary = "merged " + base + ": " + std::to_string(plan.retired.size()) +
                " retired file(s) and " + std::to_string(plan.digests.size()) +
                " digest file(s) settled";
  out.next = "make precheck (ge precheck in a game repo), then push";
  out.result = json_obj({{"base", json_str(base)},
                         {"merged", json_bool(true)},
                         {"retired", json_strs(plan.retired)},
                         {"digests", json_strs(plan.labs)}});
  return out;
}

}  // namespace ge::cli
