// THE MERGE QUEUE'S COMMANDS: what is queued, in what order, and whether a pull request will merge
// onto main and everything queued ahead of it, by a real merge rather than GitHub's cached
// mergeable state (which can read "clean" for a pull request the queue then ejects).
//
//   ge queue               the queue in the order it was entered (each pull request's last
//                          added_to_merge_queue or removed_from_merge_queue event: armed is not
//                          queued), then the armed pull requests not yet in it, then every other
//                          open pull request that conflicts with main by `git merge-tree`; and
//                          each queue group run that failed, with its failing jobs
//   ge pr clash <n>        pull request n merged by `git merge-tree` onto main, then onto main
//                          with every queued and armed pull request ahead of it merged in queue
//                          order: the files that conflict, and the first one ahead that brings
//                          the conflict in
//
// Both read GitHub through `gh api` (REST) and merge with git alone, so nothing is pushed.
//
// ge:summary ge queue and ge pr clash: the merge queue in order, and a real merge onto it
// ge:keywords cli ge queue merge-queue pr clash conflict merge-tree armed queued ejected
// ge:keywords queue_order merge_tree_conflicts queue_pr_of_branch timeline group-run
// ge:skill agent-interface
// ge:test tests/cli_queue_test.cpp
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "cli/outcome.hpp"

namespace ge::cli {

namespace fs = std::filesystem;

// The files `git merge-tree --write-tree --name-only` names as conflicting, off its output and
// exit code (empty when the merge is clean: exit 0).
std::vector<std::string> merge_tree_conflicts(std::string_view output, int code);

// The pull request a merge queue group's branch is for ("gh-readonly-queue/main/pr-209-<sha>" ->
// 209), 0 when the branch is no queue branch.
int queue_pr_of_branch(std::string_view branch);

// One pull request's queue membership, off its timeline's queue events as "<event>\t<time>" lines
// in order: the time it entered when the last event is added_to_merge_queue, "" when it is not
// queued (removed, merged, or never added).
std::string queued_since(std::string_view events);

struct QueuedPr {
  int number = 0;
  std::string since;  // when it entered the queue; "" for armed but not yet queued
};
// The order the queue merges in: the queued by the time they entered (ties by number), then the
// armed-but-not-queued by number, which join behind them once their checks pass.
std::vector<QueuedPr> queue_order(std::vector<QueuedPr> prs);

Outcome run_queue(const fs::path& repo, const std::string& base);
Outcome run_pr_clash(const fs::path& repo, const std::string& pr, const std::string& base);

}  // namespace ge::cli
