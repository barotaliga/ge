// `ge goals free [<row>]`: which roadmap numbers are taken, here and on every branch, so a new
// milestone row is never given a number an open pull request already uses.
//
//   ge goals free          the milestone numbers on the base (origin/main's `docs/goals.md`),
//                          each number every other remote branch adds with its branch and title,
//                          and the next free number: one past the highest anywhere
//   ge goals free M17      the same for one milestone's rows: the letters taken under M17 on the
//                          base and on every branch, and the next free letter
//
// A row is a line of `docs/goals.md`'s tables that opens `| M<n><suffix> |`, a status file
// `docs/goals/M<n><suffix>.md`, or a line of the same shape in `docs/goals/reserved.md`: a row
// promised to a piece of work before any branch carries it. The remote branches are read as they
// were last fetched (--fetch fetches them first).
//
// ge:summary ge goals free: the roadmap numbers taken on main and every branch, and the next free
// ge:keywords cli ge goals roadmap milestone row next-free number claimed branches pull-request
// ge:keywords goal_rows next_free_row docs-goals
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

// The next free milestone number: one past the highest among `taken`.
int next_free_number(const std::vector<int>& taken);
// The next free letter after the single-letter suffixes among `taken` ("a" when none).
std::string next_free_letter(const std::vector<std::string>& taken);

Outcome run_goals_free(const fs::path& repo, const std::string& row, const std::string& base,
                       bool fetch);

}  // namespace ge::cli
