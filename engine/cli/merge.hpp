// `ge merge-main`: the base branch merged into this branch, with the conflicts that are mechanical
// settled the one way they can be, so a branch the queue ejected is back in one command:
//
//   a file main deleted that this branch changed      main's deletion taken (`git rm`): the file
//                                                      was retired there, and what this branch
//                                                      wrote in it is named to carry by hand
//   a lab's digest file (tests/digests/<lab>.txt)      main's side taken, then the lab rerun and
//                                                      its file rewritten (`ge digest <lab>
//                                                      --write`), since both sides moved it
//
// Every other conflict is left in the tree and named, and nothing is committed until there are
// none; with none left, the merge is committed. It refuses a tree with uncommitted changes and a
// merge already in progress. The next step is the precheck, then the push.
//
// ge:summary ge merge-main: main merged, retired files and lab digests settled, the rest named
// ge:keywords cli ge merge-main merge conflict origin-main retired modify-delete digests
// ge:keywords plan_merge run_merge_main porcelain unmerged queue-ejection
// ge:skill agent-interface
// ge:test tests/cli_merge_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

// What to do with each unmerged path, off `git status --porcelain` during a merge of the base
// into this branch ("us" this branch, "them" the base).
struct MergePlan {
  std::vector<std::string> retired;  // changed here, deleted on the base (UD): take the deletion
  std::vector<std::string> digests;  // tests/digests/<lab>.txt both sides moved: base's, then rerun
  std::vector<std::string> labs;     // the labs of `digests`, in the same order
  std::vector<std::string> manual;   // every other conflict, left for the agent
};
MergePlan plan_merge(std::string_view porcelain);

Outcome run_merge_main(const BuildOptions& o, const std::string& base);

}  // namespace ge::cli
