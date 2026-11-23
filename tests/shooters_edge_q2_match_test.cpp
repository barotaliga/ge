// games/shooters_edge/net/q2_match and the match switches of Quake II's rules: our Quake II server
// warms up until both players are ready (fraglimit held at 0 meanwhile), `unready` takes a ready
// back, the countdown restarts the map as the match with the limits back; `time` calls a timeout
// that pauses the rules and from the caller ends it; a newcomer to a closed match is kept on the
// sidelines. Then the rules' switches: the faster weapon switch, the weapons and powerups a match
// server removes (their ammunition with them), the warmup's loadout, and the spawn modes. The
// shared rules (server/match_rules.hpp): prewar on a game whose jump is not a button, the phase's
// words.
//
// ge:summary Test: our Quake II server's match: ready, countdown, timeouts, lock, the switches
// ge:keywords test q2-match lobby ready unready countdown timeout time lock-join
// ge:keywords g_fast_weap_switch g_removed_weapons g_removed_powerups g_warmup_loadout g_spawn_mode
// ge:keywords prewar_buttons phase_status warmup_hold
// ge:skill net shooter testing
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "net/sim_network.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/net/q2_match.hpp"
#include "shooters_edge/net/q2_net.hpp"
#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/q2_combat.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace {

namespace br = ge::brush;
namespace sh = ge::shooter;
namespace gn = ge::net;
namespace gs = ge::server;
namespace sv = shooters_edge::server;
namespace cn = shooters_edge::net;

br::WorldBrush box(float x0, float y0, float z0, float x1, float y1, float z1) {
  const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
  return br::box_brush(lo, hi, sh::kSqSolid, "world/wall");
}

br::Entity entity(std::vector<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  e.pairs = std::move(pairs);
  return e;
}

// A walled hall: two spawn spots, and a railgun, its slugs, a rocket launcher, quad damage and a
// power screen on the floor.
std::shared_ptr<const br::Map> hall() {
  br::BrushWorld w;
  w.models.resize(1);
  auto& m = w.models[0];
  m.push_back(box(-1024, -1024, -16, 1024, 1024, 0));
  m.push_back(box(-1040, -1024, -16, -1024, 1024, 256));
  m.push_back(box(1024, -1024, -16, 1040, 1024, 256));
  m.push_back(box(-1040, 1024, -16, 1040, 1040, 256));
  m.push_back(box(-1040, -1040, -16, 1040, -1024, 256));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}, {"angle", "0"}}));
  w.entities.push_back(entity(
      {{"classname", "info_player_deathmatch"}, {"origin", "400 400 24"}, {"angle", "180"}}));
  w.entities.push_back(entity({{"classname", "weapon_railgun"}, {"origin", "0 300 16"}}));
  w.entities.push_back(entity({{"classname", "ammo_slugs"}, {"origin", "0 340 16"}}));
  w.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "0 -300 16"}}));
  w.entities.push_back(entity({{"classname", "item_quad"}, {"origin", "300 0 16"}}));
  w.entities.push_back(entity({{"classname", "item_power_screen"}, {"origin", "-300 0 16"}}));
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK_MSG(br::write_brush_world(w, &bytes, &error), error);
  auto map = std::make_shared<br::Map>();
  CHECK_MSG(br::read_bsp(bytes, map.get(), &error), error);
  return map;
}

struct Rig {
  std::shared_ptr<const br::Map> map;
  sv::RulesRunner rules;
  gn::SimNetwork net;
  gn::Transport st;
  std::vector<std::unique_ptr<gn::Transport>> ct;
  cn::Q2NetServer server;
  cn::Q2Match match;
  std::vector<std::unique_ptr<cn::Q2ClientEnd>> clients;
  int64_t now = 0;
};

std::unique_ptr<Rig> rig(const gs::ServerConfig& c) {
  auto r = std::make_unique<Rig>();
  r->rules = sv::q2_runner(sv::make_q2_server({}));
  r->rules.console("deathmatch 1");
  r->rules.console("fraglimit 10");
  sv::MapSpec map;
  map.name = "hall";
  r->map = hall();
  map.brushes = r->map;
  std::string error;
  CHECK_MSG(r->rules.start(map, error), error);
  r->net = gn::make_sim_network(7, gn::LinkConditions{});
  gn::open_sim_transport(r->st, r->net, gn::sim_address(0, cn::kPortQ2));
  CHECK_MSG(cn::q2_net_start(r->server, r->rules, r->st, {}, error), error);
  cn::q2_match_open(r->match, r->server, r->rules, c);
  return r;
}

cn::Q2ClientEnd& join(Rig& r, const std::string& name) {
  const int n = static_cast<int>(r.clients.size());
  r.ct.push_back(std::make_unique<gn::Transport>());
  gn::open_sim_transport(*r.ct.back(), r.net,
                         gn::sim_address(uint32_t(n + 1), uint16_t(27901 + n)));
  r.clients.push_back(std::make_unique<cn::Q2ClientEnd>());
  cn::Q2ClientConfig cfg;
  cfg.name = name;
  cfg.qport = static_cast<uint16_t>(100 + n);
  cn::q2_client_start(*r.clients.back(), *r.ct.back(), gn::sim_address(0, cn::kPortQ2), cfg,
                      static_cast<double>(r.now) / 1e6);
  return *r.clients.back();
}

void run(Rig& r, int ticks) {
  for (int i = 0; i < ticks; i++) {
    r.now += 13000;
    gn::sim_advance(r.net, r.now);
    for (auto& c : r.clients) cn::q2_client_tick(*c, r.now);
    cn::q2_net_frame(r.server, r.now);
    cn::q2_match_frame(r.match, "hall", static_cast<double>(r.now) / 1e6);
  }
}

bool heard(const cn::Q2ClientEnd& c, const std::string& text) {
  return std::any_of(c.prints.begin(), c.prints.end(),
                     [&](const std::string& p) { return p.find(text) != std::string::npos; });
}

std::string cvar(const Rig& r, const char* name) { return r.rules->cvars.find(name)->value; }

gs::ServerConfig lobby_config() {
  gs::ServerConfig c;
  c.lobby = 1;
  c.countdown_s = 1;
  return c;
}

}  // namespace

TEST(the_match_warms_up_until_both_are_ready_then_restarts_with_its_limits) {
  auto r = rig(lobby_config());
  cn::Q2ClientEnd& a = join(*r, "ana");
  cn::Q2ClientEnd& b = join(*r, "ben");
  run(*r, 200);
  CHECK_MSG(a.stage == cn::ClientStage::active && b.stage == cn::ClientStage::active, a.refused);
  CHECK(r->match.lobby.phase == gs::LobbyPhase::warmup);
  CHECK(cvar(*r, "fraglimit") == "0");  // nothing ends a warmup
  CHECK(sv::phase_status(r->match.lobby.phase) == std::string("Standby"));
  cn::q2_client_command(a, "ready");
  run(*r, 20);
  CHECK(r->match.lobby.seats[0].ready || r->match.lobby.seats[1].ready);
  cn::q2_client_command(a, "unready");  // Quake II's match players' word for a break
  run(*r, 20);
  CHECK(!r->match.lobby.seats[0].ready && !r->match.lobby.seats[1].ready);
  cn::q2_client_command(a, "ready");
  cn::q2_client_command(b, "ready");
  run(*r, 200);  // the second's countdown runs out
  CHECK(r->match.lobby.phase == gs::LobbyPhase::match);
  CHECK(r->rules->pending_map == "hall" && r->rules->pending_change == sv::MapChange::restart);
  CHECK(cvar(*r, "fraglimit") == "10");  // the match gets its limit back
  CHECK(heard(a, "ready"));
}

TEST(time_calls_a_timeout_that_pauses_and_the_caller_ends_it) {
  gs::ServerConfig c;  // no lobby: always the match
  c.timeouts = 1;
  c.timeout_length_s = 60;
  auto r = rig(c);
  cn::Q2ClientEnd& a = join(*r, "ana");
  cn::Q2ClientEnd& b = join(*r, "ben");
  run(*r, 200);
  cn::q2_client_command(a, "time");
  run(*r, 20);
  CHECK(gs::timeout_paused(r->match.timeout));
  CHECK(r->rules->paused);
  CHECK(heard(b, "ana"));            // everyone is told who called it
  cn::q2_client_command(a, "time");  // the caller again: back to play after the count
  run(*r, 20);
  CHECK(r->match.timeout.phase == gs::TimeoutPhase::resuming);
  run(*r, 600);
  CHECK(!gs::timeout_paused(r->match.timeout) && !r->rules->paused);
}

TEST(a_newcomer_to_a_closed_match_is_kept_on_the_sidelines) {
  gs::ServerConfig c = lobby_config();
  c.lock_join = 2;  // nobody joins a match under way
  auto r = rig(c);
  cn::Q2ClientEnd& a = join(*r, "ana");
  cn::Q2ClientEnd& b = join(*r, "ben");
  run(*r, 200);
  cn::q2_client_command(a, "ready");
  cn::q2_client_command(b, "ready");
  run(*r, 200);
  CHECK(r->match.lobby.phase == gs::LobbyPhase::match);
  cn::Q2ClientEnd& late = join(*r, "cid");
  run(*r, 200);
  CHECK(late.stage == cn::ClientStage::active);
  CHECK((r->match.held_out & (1u << 2)) != 0);
  CHECK(heard(late, "closed to new players"));
  CHECK_MSG(r->rules->clients[2].spectator, "the newcomer watches");
  CHECK(!r->match.lobby.seats[2].used);
}

TEST(prewar_holds_a_jump_that_is_not_a_button) {
  gs::Lobby l;
  l.settings.prewar = 2;  // the unready neither fire nor jump
  gs::lobby_join(l, 0, "ana", false, 0);
  gs::lobby_join(l, 1, "ben", false, 0);  // not ready: the warmup goes on
  double told = 0;
  std::string tell;
  const uint8_t kept = sv::prewar_buttons(l, 0, sh::kButtonAttack, 1, told, tell, false);
  CHECK((kept & sh::kButtonAttack) == 0);
  CHECK(sv::prewar_jump_held(l, 0));
  CHECK(tell == "Say ready to fire and jump.\n");
  CHECK(gs::lobby_command(l, 0, "ready", 2));
  CHECK(!sv::prewar_jump_held(l, 0));
  l.settings.prewar = 0;  // nobody fires before the match, everyone jumps
  CHECK(!sv::prewar_jump_held(l, 0));
  tell.clear();
  CHECK((sv::prewar_buttons(l, 0, sh::kButtonAttack, 3, told, tell, false) & sh::kButtonAttack) ==
        0);
  CHECK(tell == "Nobody may fire before the match.\n");
  CHECK(sv::phase_status(gs::LobbyPhase::match) == std::string("In progress"));
}

// ---- The rules' match switches ----

namespace {

struct Arena {
  sv::RulesRunner r;
  sv::Q2Server& s() const { return *sv::q2_of(r); }
};

std::unique_ptr<Arena> arena(const std::vector<std::string>& lines) {
  auto a = std::make_unique<Arena>();
  a->r = sv::q2_runner(sv::make_q2_server({}));
  a->r.console("deathmatch 1");
  for (const std::string& l : lines) a->r.console(l);
  sv::MapSpec map;
  map.name = "hall";
  map.brushes = hall();
  std::string error;
  CHECK_MSG(a->r.start(map, error), error);
  return a;
}

bool on_map(const Arena& a, const char* classname) {
  for (const sv::Q2Entity& e : a.s().entities)
    if (e.classname == classname && !e.gone) return true;
  return false;
}

}  // namespace

TEST(a_match_server_takes_the_removed_weapons_and_powerups_off_the_map) {
  auto plain = arena({});
  CHECK(on_map(*plain, "weapon_railgun") && on_map(*plain, "ammo_slugs"));
  CHECK(on_map(*plain, "item_quad") && on_map(*plain, "item_power_screen"));
  // The railgun (256) and the quad (1): the slugs go with the railgun, the rest stays.
  auto removed = arena({"g_removed_weapons 256", "g_removed_powerups 1"});
  CHECK(!on_map(*removed, "weapon_railgun") && !on_map(*removed, "ammo_slugs"));
  CHECK(!on_map(*removed, "item_quad"));
  CHECK(on_map(*removed, "weapon_rocketlauncher") && on_map(*removed, "item_power_screen"));
}

TEST(the_faster_weapon_switch_skips_the_lowering_then_the_raising) {
  const auto steps_to_ready = [](int mode) {
    sv::SqBody body = sv::sq_new_body();
    body.counts[sv::sq_item("weapon_shotgun")] = 1;
    body.ammo[int(sv::SqAmmo::shells)] = 10;
    sv::SqHand h;
    for (int i = 0; i < 20 && h.state != sv::SqHandState::ready; i++)
      sv::sq_hand_step(h, body, false, 0);
    CHECK(sv::sq_choose(h, body, int(sv::SqWeapon::shotgun)));
    int n = 0;
    for (; n < 40; n++) {
      sv::sq_hand_hasten(h, mode);
      sv::sq_hand_step(h, body, false, n * 0.1);
      if (h.weapon == int(sv::SqWeapon::shotgun) && h.state == sv::SqHandState::ready) break;
    }
    return n + 1;
  };
  const int normal = steps_to_ready(0), faster = steps_to_ready(1), instant = steps_to_ready(2);
  CHECK_MSG(normal > faster && faster > instant,
            std::to_string(normal) + " " + std::to_string(faster) + " " + std::to_string(instant));
  CHECK_MSG(instant == 2, std::to_string(instant));  // swapped, then ready a tenth after
}

TEST(the_warmup_spawns_every_player_armed) {
  auto a = arena({"g_warmup_loadout 1"});
  a->s().localinfo.emplace_back("ge_phase", "warmup");
  a->r.connect(0, "ana", false, false);
  sh::Packet p;
  p.client = 0;
  p.sequence = 1;
  for (sh::MoveCmd& c : p.cmds) c.msec = 13;
  a->r.frame(&p, 1);
  const sv::Q2Player& ana = a->s().players[0];
  CHECK(ana.body.counts[sv::sq_item("weapon_rocketlauncher")] == 1);
  CHECK(ana.body.counts[sv::sq_item("weapon_bfg")] == 0);
  CHECK(ana.body.ammo[int(sv::SqAmmo::rockets)] == 50);
  CHECK(ana.body.armour == sv::SqArmour::body && ana.body.armour_count == 100);
  CHECK(ana.hand.weapon == int(sv::SqWeapon::rocket_launcher));
  // In the match the spawn is the game's own: the blaster alone.
  auto m = arena({"g_warmup_loadout 1"});
  m->s().localinfo.emplace_back("ge_phase", "match");
  m->r.connect(0, "ana", false, false);
  CHECK(m->s().players[0].body.counts[sv::sq_item("weapon_rocketlauncher")] == 0);
}

// ---- Teams, the referee and the stats ----

TEST(a_team_match_seats_captains_dresses_the_teams_and_names_the_winner) {
  gs::ServerConfig c = lobby_config();
  auto r = rig(c);
  r->rules.console("dmflags 64");  // teams by skin: a team match
  r->rules.console("g_team_a_name Wolves");
  cn::Q2ClientEnd& a = join(*r, "ana");
  cn::Q2ClientEnd& b = join(*r, "ben");
  cn::Q2ClientEnd& d = join(*r, "dee");
  run(*r, 200);
  CHECK(r->match.teams_on);
  CHECK(r->match.lobby.seats[0].team == gs::Team::red &&
        r->match.lobby.seats[1].team == gs::Team::blue);
  CHECK(heard(b, "ana became captain of 'Wolves'"));
  CHECK(heard(a, "ben became captain of 'Visitors'"));
  const auto skin = [&](int slot) {
    for (const auto& [k, v] : r->rules->clients[slot].userinfo)
      if (k == "skin") return v;
    return std::string();
  };
  CHECK_MSG(skin(0) == "male/grunt" && skin(1) == "female/athena", skin(0) + " " + skin(1));
  CHECK(sv::fight_teammates(*sv::q2_of(r->rules), 0, 2));  // dee joined the smaller: the Wolves
  cn::q2_client_command(b, "lock");
  run(*r, 20);
  cn::q2_client_command(d, "team 2");
  run(*r, 20);
  CHECK(heard(d, "Team 'Visitors' is locked."));
  CHECK(r->match.lobby.seats[2].team == gs::Team::red);
  for (cn::Q2ClientEnd* e : {&a, &b, &d}) cn::q2_client_command(*e, "ready");
  run(*r, 200);
  CHECK(r->match.lobby.phase == gs::LobbyPhase::match);
  sv::Q2Server& s = *sv::q2_of(r->rules);
  s.players[0].frags = 2;
  s.players[2].frags = 1;
  s.players[1].frags = 4;
  const int rl = int(sv::SqWeapon::rocket_launcher);
  sv::stats_shot(s, 0, rl, 2);
  s.blow_how = sv::kSqHowDirect;
  s.players[1].on_ground = true;
  sv::stats_blow(s, 0, 1, rl, 100);
  run(*r, 2);
  cn::q2_match_map_change(r->match, false, 0);
  run(*r, 20);
  CHECK(heard(a, "Visitors wins, 4 to 3."));
  CHECK(heard(b, "Player statistics:"));  // every classic game's lines, at the match's end
  CHECK(heard(b, "ana [Wolves]: "));
  CHECK(heard(b, "  Weapons: rl 50.0% (1/2)"));
  CHECK(!r->match.teams.side[1].locked);  // the locks end with the match
}

TEST(a_referee_sets_the_time_limit_mutes_and_kicks_without_a_vote) {
  auto r = rig(lobby_config());
  r->rules.console("g_admin_password secret");
  r->rules.console("timelimit 20");
  cn::Q2ClientEnd& a = join(*r, "ana");
  cn::Q2ClientEnd& b = join(*r, "ben");
  run(*r, 200);
  cn::q2_client_command(b, "tl 5");
  run(*r, 20);
  CHECK(heard(b, "Only a referee can do that."));
  cn::q2_client_command(a, "referee secret");
  run(*r, 20);
  CHECK(heard(b, "ana became a referee."));
  cn::q2_client_command(a, "tl 15");
  run(*r, 20);
  CHECK(heard(b, "Time limit set to 15."));
  CHECK(r->match.hold.timelimit == "15");  // the warmup holds it for the match
  cn::q2_client_command(a, "mute ben");
  run(*r, 20);
  const size_t before = a.prints.size();
  cn::q2_client_command(b, "say hello");
  run(*r, 20);
  CHECK(heard(b, "You are muted for 5 minutes more."));
  CHECK(std::none_of(a.prints.begin() + static_cast<ptrdiff_t>(before), a.prints.end(),
                     [](const std::string& p) { return p.find("hello") != std::string::npos; }));
  cn::q2_client_command(a, "kick ben");
  run(*r, 60);
  CHECK(heard(a, "ben was kicked by ana."));
  CHECK(r->server.clients[1].stage != cn::Q2Slot::active);
}

TEST(the_stats_count_a_match_into_the_columns_every_classic_game_shares) {
  auto a = arena({});
  sv::Q2Server& s = a->s();
  const sv::StatsGame& g = sv::q2_stats_game();
  const int rl = int(sv::SqWeapon::rocket_launcher), rg = int(sv::SqWeapon::railgun),
            sg = int(sv::SqWeapon::shotgun);
  sv::stats_shot(s, 0, rl, 1);
  CHECK(!s.stats.slot[0].counted);  // a warmup counts nothing
  s.stats.on = true;
  s.players[1].on_ground = s.players[2].on_ground = true;
  s.stats.slot[1].tally.spawned = s.level_time - 10;
  sv::stats_shot(s, 0, rl, 3);
  s.blow_how = sv::kSqHowDirect;
  sv::stats_blow(s, 0, 1, rl, 100);  // a direct touch: a hit and a splash
  s.blow_how = sv::kSqHowBlast;
  sv::stats_blow(s, 0, 2, rl, 40);  // the blast on another: a splash
  s.blow_how = sv::kSqHowDirect;
  sv::stats_shot(s, 0, rg, 1);
  sv::stats_blow(s, 0, 1, rg, 100);  // a slug through two players: two hits
  sv::stats_blow(s, 0, 2, rg, 100);
  sv::stats_shot(s, 0, sg, 1);  // 12 pellets, 3 of them on a player
  for (int i = 0; i < 3; i++) sv::stats_blow(s, 0, 1, sg, 4);
  s.players[2].on_ground = false;  // a rocket on a player in the air: a mid-air
  s.blow_how = sv::kSqHowBlast;
  sv::stats_blow(s, 0, 2, rl, 60);
  sv::stats_item(s, 0, sv::sq_item("item_quad"));
  sv::stats_item(s, 0, sv::sq_item("item_armor_body"));
  sv::stats_death(s, 1, 0, rg, false);
  sv::stats_death(s, 2, 2, -1, false);
  const sv::PlayerStats& p = s.stats.slot[0].tally;
  const auto at = [&](int w, int part) { return p.n[size_t(sv::stat_weapon(w, part))]; };
  const auto rest = [&](sv::StatRest k) { return p.n[size_t(sv::stat_rest(g, k))]; };
  CHECK(at(rl, sv::kStatHits) == 1 && at(rl, sv::kStatAttacks) == 3);
  CHECK(rest(sv::kStatRlSplash) == 3 && rest(sv::kStatRlDamage) == 200);
  CHECK(rest(sv::kStatMidair) == 1);
  CHECK(at(rg, sv::kStatHits) == 2 && at(rg, sv::kStatKills) == 1);
  CHECK(at(sg, sv::kStatAttacks) == 12 && at(sg, sv::kStatHits) == 3);
  CHECK(rest(sv::kStatDmgGiven) == 412);
  CHECK(rest(sv::kStatQuad) == 1 && rest(sv::kStatArmourHeavy) == 1);
  CHECK(s.stats.slot[2].suicides == 1);
  CHECK(s.stats.slot[1].tally.n[size_t(sv::stat_weapon(rg, sv::kStatDeaths))] == 1);
  CHECK(sv::stats_column_name(g, sv::stat_weapon(rg, sv::kStatHits)) == "rg_hits");
  CHECK(sv::stats_column_name(g, sv::stat_rest(g, sv::kStatPowerB)) == "invuln");
}
