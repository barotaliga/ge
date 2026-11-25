// Quake II's spectators (games/shooters_edge/server/q2_spectate.hpp): the switch mid-match with
// its hold, password, limit and words, the respawn that takes the wish, and the chase camera.
//
// ge:summary Tests Quake II's spectators: the switch, its checks and words, the chase camera
// ge:keywords q2 spectator spectating chase chase-camera sidelines password maxspectators test
// ge:skill shooter
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/q2_spectate.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  const float lo[3] = {-1024, -256, -16}, hi[3] = {1024, 256, 0};
  w.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/floor"));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-300 0 24"}, {"angle", "0"}}));
  w.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "300 0 24"}, {"angle", "180"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  auto map = std::make_shared<br::Map>();
  CHECK(br::write_brush_world(w, &bytes, &error) && br::read_bsp(bytes, map.get(), &error));
  return map;
}

struct Arena {
  sv::RulesRunner r;
  uint32_t sequence = 0;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
  sv::Q2Player& p(int i) const { return s().players[i]; }
};

std::unique_ptr<Arena> arena(const char* deathmatch = "1") {
  auto a = std::make_unique<Arena>();
  a->r = sv::q2_runner(sv::make_q2_server({}));
  a->r.console(std::string("deathmatch ") + deathmatch);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(a->r.start(map, error), error);
  a->r.connect(0, "Ana", false, false);
  a->r.connect(1, "Ben", false, false);
  return a;
}

// Ticks of 13 ms; Ben (slot 1) holds `buttons` and the up move `up`.
void run(Arena& a, int ticks, uint8_t buttons = 0, int16_t up = 0) {
  for (int i = 0; i < ticks; i++) {
    sh::Packet packets[2];
    for (int c = 0; c < 2; c++) {
      packets[c].client = c;
      packets[c].sequence = ++a.sequence;
      sh::MoveCmd cmd;
      cmd.msec = 13;
      if (c == 1) {
        cmd.buttons = buttons;
        cmd.up = up;
      }
      packets[c].cmds[0] = packets[c].cmds[1] = packets[c].cmds[2] = cmd;
    }
    a.r.frame(packets, 2);
  }
}

int seconds(double s) { return static_cast<int>(s / 0.013) + 1; }

// Whether a print or a stuffed line with exactly this text went to slot `slot`'s bit.
bool sent(const sv::Q2Server& s, sv::EventKind kind, uint32_t to, const std::string& text) {
  for (const sv::NetEvent& e : s.out.events)
    if (e.kind == kind && (e.to & to) != 0 && e.text == text) return true;
  return false;
}

}  // namespace

TEST(a_spectator_wish_waits_5_s_then_moves_him_to_the_sidelines_with_his_score_reset) {
  auto a = arena();
  run(*a, seconds(1));
  a->p(1).frags = 3;
  a->r.user_info(1, "spectator", "1");
  CHECK(a->p(1).want_spectator);
  run(*a, seconds(2));
  CHECK(!a->s().clients[1].spectator);  // 5 s from the level's start
  run(*a, seconds(2.5));
  CHECK(a->s().clients[1].spectator);
  CHECK(a->p(1).frags == 0 && !a->p(1).alive);
  CHECK(sent(a->s(), sv::EventKind::print, 1u, "Ben has moved to the sidelines\n"));
  CHECK(a->s().world.clients[1].numbers.stats[17] == 1);
}

TEST(the_limit_and_the_spectators_password_refuse_and_put_the_setting_back) {
  auto a = arena();
  a->s().gate.max_spectators = 1;  // his own wish fills it
  a->r.user_info(1, "spectator", "1");
  run(*a, seconds(5.2));
  CHECK(!a->s().clients[1].spectator && !a->p(1).want_spectator);
  CHECK(sent(a->s(), sv::EventKind::print, 2u, "Server spectator limit is full."));
  CHECK(sent(a->s(), sv::EventKind::stuffcmd, 2u, "spectator 0\n"));
  a->s().gate.max_spectators = 4;
  a->s().gate.spectator_password = "eyes";
  a->r.user_info(1, "spectator", "1");
  run(*a, seconds(0.2));
  CHECK(!a->s().clients[1].spectator);
  CHECK(sent(a->s(), sv::EventKind::print, 2u, "Spectator password incorrect.\n"));
  a->r.user_info(1, "spectator", "eyes");  // the password is the setting's value
  run(*a, seconds(0.2));
  CHECK(a->s().clients[1].spectator);
}

TEST(coming_back_asks_the_players_password_and_says_he_joined) {
  auto a = arena();
  a->s().gate.password = "pw";
  a->r.user_info(1, "spectator", "1");
  run(*a, seconds(5.2));
  CHECK(a->s().clients[1].spectator);
  a->r.user_info(1, "spectator", "0");
  run(*a, seconds(5.2));
  CHECK(a->s().clients[1].spectator && a->p(1).want_spectator);
  CHECK(sent(a->s(), sv::EventKind::print, 2u, "Password incorrect.\n"));
  CHECK(sent(a->s(), sv::EventKind::stuffcmd, 2u, "spectator 1\n"));
  a->r.user_info(1, "password", "pw");
  a->r.user_info(1, "spectator", "0");
  run(*a, seconds(0.2));
  CHECK(!a->s().clients[1].spectator && a->p(1).alive);
  CHECK(sent(a->s(), sv::EventKind::print, 1u, "Ben joined the game\n"));
}

TEST(in_coop_the_wish_is_ignored) {
  auto a = arena("0");
  a->r.console("coop 1");
  a->r.user_info(1, "spectator", "1");
  run(*a, seconds(6));
  CHECK(!a->p(1).want_spectator && !a->s().clients[1].spectator);
}

TEST(attack_starts_and_ends_a_chase_behind_the_player_and_shows_his_bar) {
  auto a = arena();
  a->r.user_info(1, "spectator", "1");
  run(*a, seconds(5.2));
  CHECK(a->s().clients[1].spectator);
  run(*a, 2, sh::kButtonAttack);
  run(*a, 2);
  CHECK(a->p(1).chase == 0);
  // Behind Ana's view: 30 back from her eye, 2 forward again, at least 20 over her origin.
  const float ax = float(a->p(0).move.origin[0]) / 8.0f, bx = float(a->p(1).move.origin[0]) / 8.0f;
  const float az = float(a->p(0).move.origin[2]) / 8.0f, bz = float(a->p(1).move.origin[2]) / 8.0f;
  const float back = bx > ax ? bx - ax : ax - bx;  // she faces along x, one way or the other
  CHECK_MSG(back > 27 && back < 29, std::to_string(ax) + " " + std::to_string(bx));
  CHECK(bz >= az + 19.9f);
  const int16_t* st = a->s().world.clients[1].numbers.stats;
  CHECK(st[16] == 1312 && st[17] == 1);
  CHECK(st[1] == a->s().world.clients[0].numbers.stats[1]);  // her health on his bar
  run(*a, 2, sh::kButtonAttack);                             // a second press ends it
  CHECK(a->p(1).chase < 0);
}

TEST(with_nobody_to_follow_the_chase_says_so) {
  auto a = arena();
  a->r.user_info(0, "spectator", "1");
  a->r.user_info(1, "spectator", "1");
  run(*a, seconds(5.2));
  CHECK(a->s().clients[0].spectator && a->s().clients[1].spectator);
  run(*a, 2, 0, 200);  // the up move's press starts one too
  CHECK(a->p(1).chase < 0);
  CHECK(sent(a->s(), sv::EventKind::centerprint, 2u, "No other players to chase."));
}
