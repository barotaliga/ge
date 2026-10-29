// `ge queue` and `ge pr clash` (engine/cli/queue.hpp), the parts that need no network: merge-tree's
// conflicted files, a queue branch's pull request, a timeline's queue state and the queue's order.
//
// ge:summary Test: ge queue's merge-tree conflicts, queue branches, queue state and order
// ge:keywords test cli queue pr clash merge-tree merge_tree_conflicts queue_pr_of_branch
// ge:keywords queued_since queue_order timeline armed queued
// ge:skill testing agent-interface
#include <string>

#include "check.hpp"
#include "cli/queue.hpp"

using namespace ge::cli;

TEST(merge_tree_names_the_conflicted_files_and_nothing_on_a_clean_merge) {
  CHECK(merge_tree_conflicts("4b825dc642cb6eb9a060e54bf8d69288fbee4904\n", 0).empty());
  const std::string out =
      "9d1c6e0e3c\nCMakeLists.txt\ntests/labs_digests.txt\nCMakeLists.txt\n\n"
      "Auto-merging CMakeLists.txt\nCONFLICT (content): Merge conflict in CMakeLists.txt\n";
  const auto files = merge_tree_conflicts(out, 1);
  CHECK(files.size() == 2);
  CHECK(files[0] == "CMakeLists.txt" && files[1] == "tests/labs_digests.txt");
}

TEST(a_queue_groups_branch_names_its_pull_request) {
  CHECK(queue_pr_of_branch("gh-readonly-queue/main/pr-209-81d4bffe63da2d5c") == 209);
  CHECK(queue_pr_of_branch("gh-readonly-queue/main/pr-7-abc") == 7);
  CHECK(queue_pr_of_branch("claude/project-thread-o3v8ye") == 0);
  CHECK(queue_pr_of_branch("gh-readonly-queue/main/") == 0);
}

TEST(armed_is_not_queued_the_last_queue_event_decides) {
  CHECK(queued_since("").empty());
  CHECK(queued_since("added_to_merge_queue\t2026-10-29T06:52:17Z\n") == "2026-10-29T06:52:17Z");
  CHECK(queued_since("added_to_merge_queue\t2026-10-29T05:17:58Z\n"
                     "removed_from_merge_queue\t2026-10-29T05:20:05Z\n")
            .empty());
  CHECK(queued_since("added_to_merge_queue\t1\nremoved_from_merge_queue\t2\n"
                     "added_to_merge_queue\t3\r\n") == "3");
  CHECK(queued_since("added_to_merge_queue\t1\nmerged\t2\n").empty());
}

TEST(the_queue_merges_by_the_time_each_entered_then_the_armed_by_number) {
  const auto order = queue_order({{246, ""},
                                  {233, "2026-10-29T07:48:11Z"},
                                  {150, ""},
                                  {209, "2026-10-29T06:52:17Z"},
                                  {232, "2026-10-29T06:52:17Z"}});
  CHECK(order.size() == 5);
  CHECK(order[0].number == 209 && order[1].number == 232);  // the same second: by number
  CHECK(order[2].number == 233);
  CHECK(order[3].number == 150 && order[4].number == 246);
}
