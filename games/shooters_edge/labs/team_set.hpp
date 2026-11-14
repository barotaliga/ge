// THE SET OF THE `se-team` LAB (se_team_lab.cpp): the A/B a bot change is judged by, every
// variant's match on every seed in parallel and one table of means. Beside the lab, compiled into
// the `labs` executable.
//
// ge:summary The se-team lab's set: variants on seeds in parallel, their means, the A/B table
// ge:keywords lab se-team set seeds variants a-b means parse_seeds parse_flags run_set jobs
// ge:skill bots labs
// ge:test tests/shooters_edge_team_test.cpp
#pragma once

#include "lab/lab.hpp"

namespace shooters_edge::labs::se_team {

// The set (`--seeds`, `--variants`; team_set.cpp): every variant's match on every seed, each a
// child run of `run` with its own flags, their means and the A/B table reported on `lab`.
void run_set(ge::lab::Lab& lab, void (*run)(ge::lab::Lab&));

}  // namespace shooters_edge::labs::se_team
