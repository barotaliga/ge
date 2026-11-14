// A SEQUEL DUEL FOR MODS: a scripted match on the sequel's rules that a C++ mod stacks on, played
// the same way every time, for the test and the `mod` lab (sequel_mods.hpp). A hall 2048 long on
// the sequel's map format; Ana and Ben spawn at its start, settle, and are put 200 units apart on
// its axis (Ana west, facing east), still; from frame 100 Ana holds attack with the blaster she
// spawns with, while Ben stands. Every frame's digest (`rules_hash`) and Ana's health are kept.
//
// ge:summary A scripted duel on the sequel's rules that a C++ mod stacks on, for tests and labs
// ge:keywords sequel mod duel scripted-match hall sequel-mods vampire rules_hash fixture
// ge:skill mod shooter
// ge:test tests/shooters_edge_rules_mod_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mod/abi.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

struct SequelDuel {
  bool started = false;
  std::string error;             // why it did not start
  std::vector<uint64_t> hashes;  // rules_hash after every frame
  std::vector<float> health;     // Ana's after every frame
  std::vector<std::string> obituaries;
  std::vector<std::string> mod_lines;    // the server console's lines from the mods
  int64_t hook_calls = 0, refusals = 0;  // the first mod's
};

// The hall as a map the sequel's rules start.
MapSpec sequel_duel_hall();

// `frames` host frames of 13 ms with `table` stacked (null: the rules alone).
SequelDuel run_sequel_duel(const ge_mod_v1* table, int frames);

}  // namespace shooters_edge::server
