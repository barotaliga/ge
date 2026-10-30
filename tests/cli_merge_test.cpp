// `ge merge-main` (engine/cli/merge.hpp), the part that needs no repository: the unmerged paths of
// `git status --porcelain` sorted into the retired, the lab digests and the ones left by hand.
//
// ge:summary Test: ge merge-main's plan for each unmerged path
// ge:keywords test cli merge-main plan_merge porcelain unmerged retired digests
// ge:skill testing agent-interface
#include <string>

#include "check.hpp"
#include "cli/merge.hpp"

using namespace ge::cli;

TEST(a_file_main_deleted_takes_the_deletion_and_a_lab_digest_is_rerun) {
  const std::string status =
      "UD tests/labs_digests.txt\n"
      "UU tests/digests/world.txt\n"
      "AA tests/digests/cli-loop.txt\n"
      "UU engine/cli/main.cpp\n"
      "M  engine/cli/merge.cpp\n"
      "DU docs/labs.md\n"
      "UU tests/digests/sub/odd.txt\n";
  const MergePlan p = plan_merge(status);
  CHECK(p.retired.size() == 1 && p.retired[0] == "tests/labs_digests.txt");
  CHECK(p.digests.size() == 2 && p.digests[0] == "tests/digests/world.txt");
  CHECK(p.labs.size() == 2 && p.labs[0] == "world" && p.labs[1] == "cli-loop");
  // A staged change is not a conflict; a file this branch deleted and main changed, and a path
  // that is not one lab's digest file, are left by hand.
  CHECK(p.manual.size() == 3);
  CHECK(p.manual[0] == "engine/cli/main.cpp" && p.manual[1] == "docs/labs.md");
  CHECK(p.manual[2] == "tests/digests/sub/odd.txt");
}

TEST(a_clean_status_plans_nothing) {
  const MergePlan p = plan_merge("M  README.md\n?? scratch.txt\n");
  CHECK(p.retired.empty() && p.digests.empty() && p.manual.empty());
}

TEST(a_quoted_path_loses_its_quotes) {
  const MergePlan p = plan_merge("UU \"docs/a b.md\"\r\n");
  CHECK(p.manual.size() == 1 && p.manual[0] == "docs/a b.md");
}
