// `ge goals free` (engine/cli/goals.hpp), the part that needs no git: the roadmap's rows read off
// `docs/goals.md`'s tables and the status files' names, and the next free number and letter.
//
// ge:summary Test: ge goals free's rows parsed off the roadmap, and the next free row
// ge:keywords test cli goals roadmap milestone goal_rows next_free_number next_free_letter
// ge:keywords next_free_digit sub-row row_file_title claimed_row_file milestone-line
// ge:skill testing agent-interface
#include <string>

#include "check.hpp"
#include "cli/goals.hpp"

using namespace ge::cli;

TEST(the_roadmaps_table_rows_are_read_with_their_names) {
  const std::string md =
      "# Goals\n\n"
      "| Row | What | Status |\n"
      "| --- | --- | --- |\n"
      "| M0 | **The core** (`engine/core`): the PRNG | [done](goals/M0.md) |\n"
      "| M0.1 | The libm vendored and pinned | [done](goals/M0.1.md) |\n"
      "| M17 | **Multiplayer** (`engine/net`) | [open](goals/M17.md) |\n"
      "| M17d | **Sessions**: lobby, teams | [open](goals/M17d.md) |\n"
      "| M17 and M18 | prose in a table, not a row | |\n"
      "Text that names M24 outside a table.\n";
  const auto rows = goal_rows(md);
  CHECK(rows.size() == 4);
  CHECK(rows[0].number == 0 && rows[0].suffix.empty() && rows[0].title == "The core");
  CHECK(rows[1].number == 0 && rows[1].suffix == ".1");
  CHECK(rows[1].title == "The libm vendored and pinned");
  CHECK(rows[2].number == 17 && rows[2].suffix.empty() && rows[2].title == "Multiplayer");
  CHECK(rows[3].number == 17 && rows[3].suffix == "d" && rows[3].title == "Sessions");
}

TEST(a_status_files_name_is_its_row) {
  CHECK(goal_row_of_file("M17d.md").number == 17);
  CHECK(goal_row_of_file("M17d.md").suffix == "d");
  CHECK(goal_row_of_file("M0.md").number == 0);
  CHECK(goal_row_of_file("M8e2.md").suffix == "e2");
  CHECK(goal_row_of_file("ge-backlog.md").number == 0);
  CHECK(goal_row_of_file("ge-backlog.md").suffix.empty());
  CHECK(goal_row_of_file("M17.txt").number == 0);
}

TEST(the_next_free_row_is_one_past_the_highest_taken) {
  CHECK(next_free_number({0, 3, 29, 17}) == 30);
  CHECK(next_free_number({}) == 0);
  CHECK(next_free_letter({"", "a", "e2", "c"}) == "f");
  CHECK(next_free_letter({""}) == "a");
  CHECK(next_free_letter({"z"}).empty());
  // Under a row ending in a letter the sub-rows are numbered: M18k -> M18k1, M8f7 -> M8f8.
  CHECK(next_free_digit({}) == "1");
  CHECK(next_free_digit({"1", "7", "3a"}) == "8");
}

TEST(a_row_files_name_is_the_bold_text_of_its_milestone_line) {
  const std::string md =
      "Status: open\nMilestone: **Sessions**: lobby, teams\nExit: a lobby of four\n\n"
      "Milestone: **not the header**\n";
  CHECK(row_file_title(md) == "Sessions");
  CHECK(row_file_title("Status: done\n\nMilestone: **after the blank line**\n").empty());
  CHECK(row_file_title("# M17d\n\nfree notes\n").empty());
  CHECK(row_file_title("Status: open\nMilestone: no bold\n").empty());
  // A claim's file names its row the same way, and leaves the table's other columns to be written.
  const std::string claimed = claimed_row_file("Lava and slime");
  CHECK(claimed.rfind("Status: open\n", 0) == 0);
  CHECK(row_file_title(claimed) == "Lava and slime");
  CHECK(claimed.find("Exit: ") != std::string::npos && claimed.find("Labs: ") != std::string::npos);
}

TEST(the_earliest_claim_wins_and_a_tie_goes_to_the_first_ref_by_name) {
  CHECK(claim_winner({{"origin/b", 100}, {"origin/a", 101}}) == 0);
  CHECK(claim_winner({{"origin/b", 100}, {"origin/a", 100}}) == 1);
  CHECK(claim_winner({{"origin/c", 0}}) == 0);
}
