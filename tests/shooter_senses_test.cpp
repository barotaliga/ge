// THE BOT'S SENSES AND AIM (plugins/shooter/senses.hpp, aim.hpp, bot.hpp) on the server's arena: a
// client is sent the other players and the brush entities, never itself; sight needs the view's
// cone and a clear line (the shut door hides the east room); a sound is heard within its reach;
// the axes turned under another view keep the wish in the world; and on a watched run the seer
// sees the runner nearly whenever a line joins them (a late view loses it from its cone), aims
// better the better its skill (perfect, expert, average, novice by the median error, on every seed
// tried), reacts as late as its hands, keeps running its route while it looks, and replays to the
// bit (tests/golden/shooter_senses.txt, GE_GOLDEN_WRITE=1 rewrites it; the PR says why).
//
// ge:summary Tests: what a client is sent, sight in a cone by trace, hearing, aim by skill, bits
// ge:keywords test senses client-view sight cone trace hearing aim skill turn_axes duel reaction
// ge:keywords golden
// ge:skill shooter bots testing
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "golden.hpp"
#include "shooter/aim.hpp"
#include "shooter/server.hpp"
#include "shooters_edge/bots/duel_run.hpp"

namespace sh = ge::shooter;
namespace pr = shooters_edge::bots;

namespace {

constexpr const char* kGolden = "tests/golden/shooter_senses.txt";

bool env_set(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

float median(std::vector<float> v) {
  if (v.empty()) return -1;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

uint64_t fold(uint64_t h, sh::Vec3f v) {
  for (int i = 0; i < 3; i++) {
    const float f = v[i];
    uint32_t b;
    std::memcpy(&b, &f, 4);
    h = (h ^ b) * 1099511628211ull;
  }
  return h;
}

}  // namespace

TEST(a_client_is_sent_the_other_players_and_the_movers_never_itself) {
  pr::Duel d;
  CHECK(pr::make_duel(d, sh::AimSkill::perfect));
  pr::run_duel(d, 2);
  sh::ClientView v;
  const int me = d.seer_slot + 1, him = d.runner_slot + 1;
  sh::client_view(d.host.sv, me, v);
  bool runner = false, door = false, plat = false, self = false;
  for (const sh::ViewEntity& e : v.entities) {
    runner |= e.ent == him;
    door |= e.ent == d.host.rules.door;
    plat |= e.ent == d.host.rules.plat;
    self |= e.ent == me;
  }
  CHECK_MSG(runner, "the other player was not sent: read client_view's player rule");
  CHECK_MSG(door && plat, "the door or the plat was not sent");
  CHECK_MSG(!self, "a client was sent its own player");
}

TEST(sight_needs_the_cone_and_a_clear_line) {
  pr::Duel d;
  CHECK(pr::make_duel(d, sh::AimSkill::perfect));
  pr::run_duel(d, 2);
  const sh::Server& sv = d.host.sv;
  const sh::SenseSettings set;
  const int me = d.seer_slot + 1;
  const sh::Vec3f eye{-300, 0, 46};
  CHECK_MSG(sh::sees_player(sv, set, me, eye, {0, 0, 0}, {-100, 30, 24}),
            "a player ahead in the open was not seen");
  CHECK_MSG(!sh::sees_player(sv, set, me, eye, {0, 180, 0}, {-100, 30, 24}),
            "a player behind the view was seen");
  CHECK_MSG(!sh::sees_player(sv, set, me, eye, {0, 0, 0}, {400, 0, 24}),
            "a player behind the shut door was seen");
  CHECK_MSG(!sh::sees_player(sv, set, me, eye, {0, 0, 0}, {300, 300, 24}),
            "a player behind the dividing wall was seen");
}

TEST(a_sound_is_heard_within_its_reach) {
  pr::Duel d;
  CHECK(pr::make_duel(d, sh::AimSkill::perfect));
  pr::run_duel(d, 2);
  sh::Server& sv = d.host.sv;
  const int me = d.seer_slot + 1, him = d.runner_slot + 1;
  sv.sound_count = 0;
  sh::start_sound(sv, him, 1, "weapon/rocket_fire", 1, 1);
  sh::ClientView v;
  sh::client_view(sv, me, v);
  CHECK_MSG(v.sounds.size() == 1, "a shot in the same room was not sent");
  sh::Senses s;
  sh::sense(s, sv, v, me, {0, 0, 0}, sv.time);
  bool heard = false;
  for (const sh::Contact& c : s.contacts)
    if (c.ent == him && ge::ai::known(c.heard)) heard = true;
  CHECK_MSG(heard, "the shot was sent but not heard");
  sv.sound_count = 0;
  sh::start_sound(sv, him, 1, "weapon/rocket_fire", 1, 1000);  // reaches one unit
  sh::client_view(sv, me, v);
  CHECK_MSG(v.sounds.empty(), "a sound past its reach was sent");
  sv.sound_count = 0;
}

TEST(axes_turned_under_another_view_keep_the_wish_in_the_world) {
  int16_t f = 320, s = 0;
  sh::turn_axes(f, s, 0, 90);  // meant along +x, the view along +y: +x is the view's right
  CHECK_MSG(f == 0 && s == 320, "forward along +x pressed from a +y view is not a full right: " +
                                    std::to_string(f) + " " + std::to_string(s));
  f = 200;
  s = -100;
  sh::turn_axes(f, s, 30, 30);
  CHECK(f == 200 && s == -100);
}

TEST(the_seer_sees_aims_by_skill_and_keeps_its_route) {
  const sh::AimSkill order[4] = {sh::AimSkill::perfect, sh::AimSkill::expert, sh::AimSkill::average,
                                 sh::AimSkill::novice};
  for (uint32_t seed = 1; seed <= 2; seed++) {
    float last = -1;
    for (const sh::AimSkill k : order) {
      pr::Duel d;
      CHECK(pr::make_duel(d, k, seed));
      const pr::DuelResult r = pr::run_duel(d, 2400);
      const std::string name = std::string(sh::aim_skill_name(k)) + " seed " + std::to_string(seed);
      CHECK_MSG(r.line_frames > 300, name + ": the two were rarely in sight of each other");
      // Behind the view it is not seen until it comes into the cone; a late view loses it too.
      const int64_t share = k == sh::AimSkill::perfect ? 8 : 5;
      CHECK_MSG(r.seen_frames * 10 >= r.line_frames * share,
                name + " saw the runner on " + std::to_string(r.seen_frames) + " of " +
                    std::to_string(r.line_frames) + " frames a line joined them");
      const float m = median(r.error_deg);
      CHECK_MSG(m > last, name + "'s median aim error " + std::to_string(m) +
                              " is not worse than the better skill's " + std::to_string(last));
      last = m;
      if (k != sh::AimSkill::perfect)
        CHECK_MSG(!r.reaction_s.empty() && median(r.reaction_s) >= 0.08f,
                  name + " turned onto the runner faster than a hand can");
      CHECK_MSG(d.seer.pilot.arrivals >= 3,
                name + " reached " + std::to_string(d.seer.pilot.arrivals) +
                    " goals while looking: read turn_axes in plugins/shooter/aim.cpp");
    }
  }
}

TEST(a_watched_run_replays_to_the_bit_and_matches_its_golden) {
  uint64_t h = 1469598103934665603ull;
  for (const sh::AimSkill k : {sh::AimSkill::novice, sh::AimSkill::expert}) {
    uint64_t a = 1469598103934665603ull, b = a;
    for (int run = 0; run < 2; run++) {
      pr::Duel d;
      CHECK(pr::make_duel(d, k, 3));
      const pr::DuelResult r = pr::run_duel(d, 1200);
      uint64_t& x = run == 0 ? a : b;
      for (size_t i = 0; i < r.seer.size(); i++) {
        x = fold(fold(x, r.seer[i]), r.runner[i]);
        x = fold(x, {r.view_yaw[i], static_cast<float>(r.seen[i]), 0});
      }
    }
    CHECK_MSG(a == b, std::string("two watched runs with ") + sh::aim_skill_name(k) + " differ");
    h = (h ^ a) * 1099511628211ull;
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# plugins/shooter/bot.hpp watched on the arena, novice and expert aim, seed 3, 1200 "
           "frames: both players' origins, the seer's view and its sight folded "
           "(tests/shooter_senses_test.cpp)\n";
    char line[32];
    std::snprintf(line, sizeof line, "%016llx\n", static_cast<unsigned long long>(h));
    out << line;
    std::printf("wrote %s\n", kGolden);
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == 1 && rows[0].size() == 1,
            std::string(kGolden) + " is missing or malformed: write it with GE_GOLDEN_WRITE=1");
  if (rows.size() == 1 && rows[0].size() == 1)
    CHECK_MSG(rows[0][0] == h,
              "the watched runs moved off their golden bits: if the senses, the aim, the pilot or "
              "the move changed on purpose, rewrite with GE_GOLDEN_WRITE=1 and say why in the PR");
}
