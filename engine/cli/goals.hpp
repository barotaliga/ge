// `ge goals free [<row>]`: which roadmap numbers are taken, here and on every branch, so a new
// milestone row is never given a number an open pull request already uses.
//
//   ge goals free          the milestone numbers on the base (origin/main's `docs/goals.md`),
//                          each number every other remote branch adds with its branch and title,
//                          and the next free number: one past the highest anywhere
//   ge goals free M17      the same for one milestone's rows: the letters taken under M17 on the
//                          base and on every branch, and the next free letter
//   ge goals free M18k     a row's own sub-rows: numbers under a row ending in a letter (M18k1),
//                          letters under one ending in a digit (M23d1a)
//   ge goals claim <what>  the next free row (--under=M17: the next letter under it) written as its
//                          own file, `docs/goals/<row>.md` (Status, Milestone, Exit and Labs
//                          lines), committed and pushed to this branch at once; then, five seconds
//                          on, the branches read again: a branch that claimed the same row with an
//                          earlier commit keeps it, and this one removes its file and claims the
//                          next
//
// A row is a status file `docs/goals/M<n><suffix>.md`, its name read from the bold text of its
// `Milestone:` line; or, on a branch from before the rows were files, a line of `docs/goals.md`'s
// tables or `docs/goals/reserved.md` that opens `| M<n><suffix> |`. The remote branches are
// fetched first (--no-fetch answers from the branches as last fetched).
//
// ge:summary ge goals free and claim: the roadmap rows taken everywhere, the next free one reserved
// ge:keywords cli ge goals roadmap milestone row next-free number claimed branches pull-request
// ge:keywords goal_rows next_free_row docs-goals claim reserve reserved race row-file
// ge:keywords milestone-line
// ge:skill agent-interface
// ge:test tests/cli_goals_test.cpp
#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "cli/outcome.hpp"

namespace ge::cli {

namespace fs = std::filesystem;

struct GoalRow {
  int number = 0;      // 17 for M17d
  std::string suffix;  // "d" for M17d, "" for M17, ".1" for M0.1
  std::string title;   // the row's bold name, or its first words
};

// Every row a `docs/goals.md` text names, in order.
std::vector<GoalRow> goal_rows(std::string_view goals_md);
// The row a status file's name stands for ("M17d.md"); number 0 when it is none.
GoalRow goal_row_of_file(std::string_view file_name);
// A status file's row name: the bold text of its `Milestone:` line, in the header of `Key: value`
// lines before the first blank line; empty when it has none.
std::string row_file_title(std::string_view status_md);
// The status file a claim writes for a row named `title`: its exit criterion `exit` and the labs it
// lands (`labs`, comma-separated ids, each written as code) when given, "to be written" when not.
std::string claimed_row_file(const std::string& title, const std::string& exit = "",
                             const std::string& labs = "");

// The next free milestone number: one past the highest among `taken`.
int next_free_number(const std::vector<int>& taken);
// The next free letter after the single-letter suffixes among `taken` ("a" when none).
std::string next_free_letter(const std::vector<std::string>& taken);
// The number one past the highest leading number of the suffixes under a row (M18k1, M18k2 ->
// "3"), "1" when there are none: the sub-rows of a row that ends in a letter.
std::string next_free_digit(const std::vector<std::string>& taken);

Outcome run_goals_free(const fs::path& repo, const std::string& row, const std::string& base,
                       bool fetch);

// One branch's claim on a row: the ref, and when the commit that added the row was made.
struct Claimant {
  std::string ref;
  long long time = 0;  // committer time, seconds since the epoch
};
// Which claim stands: the earliest, and on a tie the ref first by name, so every claimant agrees.
std::size_t claim_winner(const std::vector<Claimant>& claimants);

Outcome run_goals_claim(const fs::path& repo, const std::string& title, const std::string& under,
                        const std::string& base, const std::string& exit = "",
                        const std::string& labs = "");

}  // namespace ge::cli
