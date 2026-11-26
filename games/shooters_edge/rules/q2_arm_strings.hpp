// QUAKE II'S WEAPONS' STRINGS FOR THE RULES (classic remix's `cr_items q2` on the classic rules):
// every sample, effect name and obituary word the rules' Quake II weapons (common_q2_arms.cpp)
// pass, built from plugins/classic/q2_arms.hpp and never typed again. Both programs add them
// after their own strings (and the campaign's after Quake II's monsters'), at `kQ2ArmStringBase`.
//
// - **The list** (`kQ2ArmStrings`): the hand's one-shot sounds, the bullets' five samples, every
//   other shot sample once, the hits' and blasts' effect names, the blasts', bounce's and click's
//   samples, the bolt's class name, then each weapon's obituary words and tail, straight on and
//   by a blast.
// - **Finding one** (`q2_arm_string`): the index of a name in the list; -1 none. Constant, so a
//   rule names its sample once and the index is worked out when it is built.
//
// ge:summary Quake II's weapons' samples, effects and obituary words as the rules' strings
// ge:keywords q2-arms remix-weapons cr_items strings obituary weapon-sounds effect-names
// ge:skill progs shooter
// ge:test tests/shooters_edge_remix_q2_arms_test.cpp
#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "classic/q2_arms.hpp"

namespace shooters_edge::server::rules {

namespace q2s {

namespace gc = ge::classic;

inline constexpr int kMost = 96;

struct List {
  std::array<const char*, kMost> at{};
  int count = 0;
  constexpr void add(const char* s) {
    for (int i = 0; i < count; i++)
      if (std::string_view(at[static_cast<size_t>(i)]) == s) return;
    at[static_cast<size_t>(count++)] = s;
  }
};

constexpr List build() {
  List l;
  for (const char* s : gc::kQ2ArmSounds) l.add(s);
  for (const char* s : gc::kQ2GunSounds) l.add(s);
  for (int a = 0; a < gc::kQ2ArmCount; a++) {
    gc::Q2ArmSound out[3] = {};
    const int n = gc::q2_arm_shot_sounds(static_cast<gc::Q2ArmId>(a), 3, out);
    for (int k = 0; k < n; k++)
      if (out[k].variants == 0) l.add(out[k].name);
  }
  for (const char* s :
       {gc::kQ2HitBolt, gc::kQ2HitPellet, gc::kQ2HitBullet, gc::kQ2HitBlood, gc::kQ2RailLine,
        gc::kQ2RocketBlast, gc::kQ2GrenadeBlast, gc::kQ2RocketBlastWater, gc::kQ2GrenadeBlastWater,
        gc::kQ2RocketBoom, gc::kQ2GrenadeBoom, gc::kQ2GrenadeBounce, gc::kQ2NoAmmo})
    l.add(s);
  l.add("bolt");  // the blaster's and hyperblaster's shot's class name
  for (int a = 0; a < gc::kQ2ArmCount; a++)
    for (const bool blast : {false, true}) {
      const gc::Q2ArmWords w = gc::q2_arm_words(static_cast<gc::Q2ArmId>(a), blast);
      if (w.words == nullptr) continue;
      l.add(w.words);
      l.add(w.tail);
    }
  return l;
}

inline constexpr List kList = build();

}  // namespace q2s

inline constexpr int kQ2ArmStringCount = q2s::kList.count;
inline constexpr auto kQ2ArmStrings = [] {
  std::array<const char*, static_cast<size_t>(kQ2ArmStringCount)> a{};
  for (int i = 0; i < kQ2ArmStringCount; i++)
    a[static_cast<size_t>(i)] = q2s::kList.at[static_cast<size_t>(i)];
  return a;
}();

// The index of `name` in `kQ2ArmStrings`; -1 none.
constexpr int q2_arm_string(std::string_view name) {
  for (int i = 0; i < kQ2ArmStringCount; i++)
    if (name == kQ2ArmStrings[static_cast<size_t>(i)]) return i;
  return -1;
}

}  // namespace shooters_edge::server::rules
