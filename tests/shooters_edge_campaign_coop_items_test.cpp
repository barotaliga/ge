// Tests: our campaign's coop settings both classic games share (plugins/classic/coop.hpp), on the
// campaign host: in coop a weapon stays on the map for everyone and each player takes it once; a
// holder may take a copy nobody has taken with `coop_pickup_weapons 1` and never with 0; alone, a
// taken weapon is gone. A plat stepped on in coop waits `coop_elevator_delay` before it rises, and
// at once with 0 or alone.
//
// ge:summary Tests: our campaign's coop weapons that stay and the plat's coop wait
// ge:keywords test coop weapons-stay coop_pickup_weapons coop_elevator_delay plat-delay
// ge:keywords campaign-mod coop-settings
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "classic/coop.hpp"
#include "shooters_edge_campaign_fixture.hpp"

using namespace campfix;

namespace {

constexpr float kBottom = 1;  // STATE_BOTTOM

// Two spikers: one where the player starts, one 96 units north.
const char* const kGuns =
    "{\n\"classname\" \"weapon_nailgun\"\n\"origin\" \"-416 0 24\"\n}\n"
    "{\n\"classname\" \"weapon_nailgun\"\n\"origin\" \"-416 96 24\"\n}\n";

// The player stood on `e`'s spot for a few frames.
void stand_on(Game& g, int e) {
  for (int i = 0; i < 6; i++) {
    sh::Edict& p = g.s().sv.edicts[1];
    p.origin = g.ed(e).origin;
    p.velocity = {};
    CHECK_MSG(g.frame(0), g.error);
  }
}

bool shown(Game& g, int e) { return g.ed(e).modelindex > 0 && g.ed(e).solid != sh::kSolidNot; }

}  // namespace

TEST(both_games_share_the_coop_settings_defaults) {
  Game g("", {"coop 1"});
  for (const ge::classic::CoopSetting& c : ge::classic::kCoopSettings) {
    const ge::console::Cvar* v = g.s().cvars.find(c.name);
    CHECK_MSG(v != nullptr && v->number == std::stof(c.fallback), c.name);
  }
}

TEST(in_coop_a_weapon_stays_and_a_holder_takes_a_fresh_copy_once) {
  Game g(kGuns, {"coop 1"});
  const int a = g.find("weapon_nailgun"), b = g.find("weapon_nailgun", a);
  CHECK(a > 0 && b > 0);
  stand_on(g, a);
  const float nails = g.field(1, g.f().ammo_nails);
  CHECK_MSG(nails > 0, std::to_string(nails));
  CHECK(shown(g, a));  // still there for the others
  CHECK(g.named(a, "coop_taken") == 1);
  stand_on(g, b);  // a copy nobody has taken: his once, for its ammunition
  const float more = g.field(1, g.f().ammo_nails);
  CHECK_MSG(more > nails, std::to_string(more) + " " + std::to_string(nails));
  CHECK(shown(g, b));
  stand_on(g, a);  // both taken now: refused
  stand_on(g, b);
  CHECK(g.field(1, g.f().ammo_nails) == more);
  CHECK_MSG(g.s().q.faults == 0, g.s().q.last_fault);
}

TEST(with_coop_pickup_weapons_0_a_holder_is_refused) {
  Game g(kGuns, {"coop 1", "coop_pickup_weapons 0"});
  const int a = g.find("weapon_nailgun"), b = g.find("weapon_nailgun", a);
  stand_on(g, a);
  const float nails = g.field(1, g.f().ammo_nails);
  stand_on(g, b);
  CHECK(g.field(1, g.f().ammo_nails) == nails);
  CHECK(g.named(b, "coop_taken") == 0);
  CHECK(shown(g, a) && shown(g, b));
}

TEST(alone_a_taken_weapon_is_gone) {
  Game g(kGuns);
  const int a = g.find("weapon_nailgun");
  stand_on(g, a);
  CHECK(g.field(1, g.f().ammo_nails) > 0);
  CHECK(g.find("weapon_nailgun") != a || !shown(g, a));
}

namespace {

const char* const kPlat = "{\n\"classname\" \"func_plat\"\n\"model\" \"*2\"\n\"height\" \"8\"\n}\n";

// The frames until the plat leaves the bottom with the player standing on it (at most `most`).
int frames_to_rise(Game& g, int most) {
  for (int i = 0; i < 12; i++) CHECK_MSG(g.frame(0), g.error);  // settled at the bottom
  const int p = g.find("func_plat");
  CHECK(g.named(p, "state") == kBottom);
  for (int i = 0; i < most; i++) {
    sh::Edict& me = g.s().sv.edicts[1];
    me.origin = {(g.ed(p).absmin.x + g.ed(p).absmax.x) / 2,
                 (g.ed(p).absmin.y + g.ed(p).absmax.y) / 2, g.ed(p).absmax.z + 24.5f};
    me.velocity = {};
    CHECK_MSG(g.frame(0), g.error);
    if (g.named(p, "state") != kBottom) return i + 1;
  }
  return -1;
}

}  // namespace

TEST(in_coop_a_plat_waits_coop_elevator_delay_before_it_rises) {
  Game coop(kPlat, {"coop 1"});
  const int waited = frames_to_rise(coop, kFrames * 3);
  CHECK_MSG(waited >= kFrames * 9 / 10 && waited <= kFrames * 12 / 10, std::to_string(waited));
  Game quick(kPlat, {"coop 1", "coop_elevator_delay 0"});
  const int at_once = frames_to_rise(quick, kFrames);
  CHECK_MSG(at_once > 0 && at_once < kFrames / 4, std::to_string(at_once));
  Game alone(kPlat);
  const int alone_rose = frames_to_rise(alone, kFrames);
  CHECK_MSG(alone_rose > 0 && alone_rose < kFrames / 4, std::to_string(alone_rose));
  CHECK_MSG(coop.s().q.faults == 0, coop.s().q.last_fault);
}
