// THE ARENA MOVE UNDER A SINGLE-PLAYER PROGRAM: our campaign program (which jumps, swims up and
// water-jumps in its own pre-think) on `sv_playermove arena`, against the same program on the
// original move. The move never jumps for it: a held jump button jumps once, not again on every
// landing; a jump rises as high on both moves; and the program's water jump ends where the
// original move ends it (out of the water, or past its time), so the player jumps again after it.
//
// ge:summary Tests: the arena move under our campaign program: one jump a press, the water jump
// ge:keywords test campaign sv_playermove arena-move program-jumps jump pogo water-jump
// ge:skill shooter progs testing
#include <string>

#include "check.hpp"
#include "shooters_edge_campaign_fixture.hpp"

using campfix::Game;
using campfix::kFrames;
namespace sh = ge::shooter;

namespace {

constexpr float kFlWaterJump = 2048;

struct Jumps {
  int takeoffs = 0;
  float top = 0;  // the highest the player's origin rose above where it stood
};

// `frames` host frames with the jump button held throughout, counting take-offs.
Jumps hold_jump(Game& g, int frames) {
  Jumps j;
  const float floor = g.ed(1).origin.z;
  bool was_on = (g.ed(1).flags & sh::kFlagOnGround) != 0;
  for (int i = 0; i < frames; i++) {
    CHECK_MSG(g.frame(0, sh::kButtonJump), g.error);
    const bool on = (g.ed(1).flags & sh::kFlagOnGround) != 0;
    if (was_on && !on) j.takeoffs++;
    if (g.ed(1).origin.z - floor > j.top) j.top = g.ed(1).origin.z - floor;
    was_on = on;
  }
  return j;
}

void settle(Game& g) {
  for (int i = 0; i < kFrames; i++) CHECK_MSG(g.frame(), g.error);
}

}  // namespace

TEST(a_held_jump_jumps_once_on_either_move) {
  float tops[2] = {0, 0};
  int k = 0;
  for (const char* move : {"original", "arena"}) {
    Game g("", {std::string("sv_playermove ") + move});
    CHECK((g.s().sv.set.playermove == sh::PlayerMove::arena) == (std::string(move) == "arena"));
    settle(g);
    const Jumps j = hold_jump(g, 3 * kFrames);
    CHECK_MSG(j.takeoffs == 1, std::string(move) + ": " + std::to_string(j.takeoffs) + " take-offs");
    tops[k++] = j.top;
  }
  // The program's jump of 270 rises about 45 units on both moves.
  CHECK_MSG(tops[0] > 40 && tops[0] < 50, std::to_string(tops[0]));
  CHECK_MSG(tops[1] > 40 && tops[1] < 50, std::to_string(tops[1]));
}

TEST(a_water_jump_ends_out_of_the_water_on_the_arena_move) {
  Game g("", {"sv_playermove arena"});
  settle(g);
  // The program's water jump, as its check leaves it, with no water under the player.
  g.s().sv.edicts[1].flags = static_cast<int>(g.s().sv.edicts[1].flags) | 2048;
  g.set(1, g.f().flags, static_cast<float>(g.s().sv.edicts[1].flags));
  g.set(1, g.f().teleport_time, static_cast<float>(g.s().sv.time + 2));
  for (int i = 0; i < 4; i++) CHECK_MSG(g.frame(), g.error);
  CHECK_MSG((static_cast<int>(g.field(1, g.f().flags)) & static_cast<int>(kFlWaterJump)) == 0,
            "the water jump's flag still set out of the water");
  const Jumps j = hold_jump(g, kFrames);
  CHECK_MSG(j.takeoffs == 1, std::to_string(j.takeoffs) + " take-offs after the water jump");
}
