// THE SKIER'S PROFILE (docs/saves.md § Part of a run): what Fall Line carries of a skier from one
// mountain to another, as the `PROF` chunk a character file is made of. His body (the injuries he
// has taken, worst by part; the blow in progress and the load owed are this run's and stay) and
// his career (state.hpp's `Career`: every run his profile was taken from, this one added).
// Taken from a run (`profile_of`, which a whole run's save also writes, so `save::extract` cuts a
// character out of any save) and carried into a run opened on another seed (`carry_in`), where
// it replaces the player's body and career and nothing else.
//
// ge:summary The skier's profile (body and career) carried from one run into another's
// ge:keywords profile character career statistics body injuries carry import part prof
// ge:skill save sim
// ge:test tests/fall_line_profile_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "fall_line/sim/simulate.hpp"
#include "fall_line/sim/state.hpp"
#include "save/file.hpp"
#include "skier/body.hpp"

namespace fall_line::sim {

inline constexpr uint32_t kProfileTag = ge::save::tag("PROF");
inline constexpr uint16_t kProfileVersion = 1;

struct Profile {
  ge::skier::BodyState body;
  Career career;
  template <class Io>
  friend void describe(Io& io, Profile& v) {
    io(v.body);
    io(v.career);
  }
};

// The player's profile as the run stands: his body (its transients cleared) and his career with
// this run added. False with `error` when there is nothing to carry: he died in this run.
bool profile_of(const Driver& d, Profile& out, std::string& error);

// `p` carried into `d`, opened on any seed and never stepped: the player's body and career set.
// False with `error` when the run has been stepped already.
bool carry_in(Driver& d, const Profile& p, std::string& error);

}  // namespace fall_line::sim
