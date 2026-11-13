// Tests: the aim help on a classic mod's server (games/shooters_edge/server/shot_help.hpp). On the
// test arena, a granted player's shot line passing over another player's head is bent onto him;
// the same line is left for a player not granted, for a line that does not start at his eye (not
// his shot), for a target behind a teammate's flag under teamplay, and beyond the leeway; a bend
// is counted for the players list.
//
// ge:summary Tests: the aim help on a classic mod's server: grant, shot, teammates, leeway, count
// ge:keywords test aim-help shot_help help_traced_shot note_aim_help_info team_friends
// ge:skill shooter progs testing
#include <cmath>
#include <memory>
#include <string>

#include "check.hpp"
#include "shooter/server.hpp"
#include "shooter/vec.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/shot_help.hpp"
#include "shooters_edge/server/test_mod.hpp"

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

struct Arena {
  pg::Progs progs;
  std::unique_ptr<sv::QcServer> s;
  bool ok = false;

  Arena() {
    std::string error;
    CHECK_MSG(sv::build_test_mod(progs, error), error);
    sv::QcServerOptions o;
    o.loop.max_edicts = 1024;
    s = sv::make_qc_server(progs, o, error);
    CHECK_MSG(s != nullptr, error);
    if (!s) return;
    sv::MapSpec map;
    map.models = sv::arena_models();
    map.entities = sv::kArenaEntities;
    CHECK_MSG(sv::load_map(*s, map, error), error);
    sv::connect(*s, 0, "ann", false, true);
    sv::connect(*s, 1, "bob", false, true);
    for (int i = 0; i < 10; i++) sv::qc_frame(*s, nullptr, 0);
    sh::set_origin(s->sv, 1, {-300, 0, 24});
    sh::set_origin(s->sv, 2, {200, 0, 24});
    ok = true;
  }
  sh::Vec3f eye() const {
    const sh::Edict& e = s->sv.edicts[1];
    return e.origin + e.view_ofs;
  }
  // ann looks at bob's middle, raised `over` units at his distance; her line out to 2048.
  sh::Vec3f aim_over(float over) {
    const sh::Vec3f from = eye();
    const sh::Edict& bob = s->sv.edicts[2];
    const float z = (bob.absmin.z + bob.absmax.z) * 0.5f + over;
    const double pitch =
        -std::atan2(double(z - from.z), double(bob.origin.x - from.x)) * 57.29577951308232;
    s->sv.edicts[1].v_angle = {float(pitch), 0, 0};
    return from + sh::angle_vectors(s->sv.edicts[1].v_angle).forward * 2048;
  }
};

}  // namespace

TEST(a_granted_players_shot_over_a_head_is_bent_onto_him) {
  Arena a;
  if (!a.ok) return;
  const sh::Edict& bob = a.s->sv.edicts[2];
  const float over = (bob.absmax.z - bob.absmin.z) * 0.5f + 30;  // 30 over his head
  sh::Vec3f end = a.aim_over(over);
  const sh::Vec3f unbent = end;
  CHECK(sv::help_traced_shot(*a.s, 1, a.eye(), end) == -1);  // not granted
  CHECK(end == unbent);
  sv::set_user_info(*a.s, 0, "*aimhelp", "1");
  CHECK(a.s->sv.clients[0].aimhelp);
  CHECK(sv::help_traced_shot(*a.s, 1, a.eye(), end) == 2);
  CHECK(end.z < unbent.z);
  CHECK(a.s->sv.clients[0].aim_helped == 1);
  // The bend is told to him alone, naming whom (his client turns the view onto Bob).
  CHECK(!a.s->out.events.empty());
  const sv::NetEvent& told = a.s->out.events.back();
  CHECK(told.kind == sv::EventKind::aim_bend && told.to == 1u && told.ent == 2);
  // The bent line passes through his box at his distance.
  const sh::Vec3f from = a.eye();
  const float t = (bob.origin.x - from.x) / (end.x - from.x);
  const float z = from.z + (end.z - from.z) * t;
  CHECK(z > bob.absmin.z && z < bob.absmax.z);
  // Beyond the leeway: nothing.
  a.s->sv.set.aimhelp_y = 20;
  end = unbent;
  CHECK(sv::help_traced_shot(*a.s, 1, a.eye(), end) == -1);
  // A line from somewhere else is not his shot.
  a.s->sv.set.aimhelp_y = 48;
  end = unbent;
  CHECK(sv::help_traced_shot(*a.s, 1, a.eye() + sh::Vec3f{0, 0, 200}, end) == -1);
  // Taken back.
  sv::set_user_info(*a.s, 0, "*aimhelp", "");
  CHECK(!a.s->sv.clients[0].aimhelp);
}

TEST(a_teammate_is_never_aimed_at_under_teamplay) {
  Arena a;
  if (!a.ok) return;
  sv::set_user_info(*a.s, 0, "*aimhelp", "1");
  sv::set_user_info(*a.s, 0, "team", "red");
  sv::set_user_info(*a.s, 1, "team", "red");
  CHECK(sv::server_command(*a.s, "teamplay 1"));
  const sh::Edict& bob = a.s->sv.edicts[2];
  sh::Vec3f end = a.aim_over((bob.absmax.z - bob.absmin.z) * 0.5f + 20);
  CHECK(sv::help_traced_shot(*a.s, 1, a.eye(), end) == -1);
  CHECK(sv::server_command(*a.s, "teamplay 0"));
  CHECK(sv::help_traced_shot(*a.s, 1, a.eye(), end) == 2);
}

TEST(a_granted_players_new_missile_is_led_and_a_denied_weapon_is_not) {
  Arena a;
  if (!a.ok) return;
  sv::set_user_info(*a.s, 0, "*aimhelp", "1");  // sets the loop's missile rule
  CHECK(a.s->sv.missile_watch != nullptr);
  a.s->sv.edicts[2].flags |= sh::kFlagOnGround;  // standing (a bot not stepped never landed)
  const sh::Edict& bob = a.s->sv.edicts[2];
  const sh::Vec3f end = a.aim_over((bob.absmax.z - bob.absmin.z) * 0.5f + 30);
  auto launch = [&] {
    const int m = sh::spawn_edict(a.s->sv);
    sh::Edict& e = a.s->sv.edicts[static_cast<size_t>(m)];
    e.owner = 1;
    e.movetype = sh::kMovetypeFlyMissile;
    e.origin = a.eye();
    sh::Vec3f d = end - a.eye();
    sh::normalize(d);
    e.velocity = d * 1000;
    const sh::Vec3f was = e.velocity;
    a.s->sv.missile_watch(a.s->sv.missile_watch_ctx, a.s->sv, m);
    return !(a.s->sv.edicts[static_cast<size_t>(m)].velocity == was);
  };
  CHECK(launch());  // led down onto bob
  CHECK(a.s->sv.clients[0].aim_helped == 1);
  // Nothing held is "" to the lists; deny everything: the missile flies as launched.
  CHECK(sv::server_command(*a.s, "sv_aimhelp_deny *"));
  CHECK(!sv::aim_help_weapon_allowed(*a.s, "rl"));
  CHECK(!launch());
  CHECK(sv::server_command(*a.s, "sv_aimhelp_deny \"rl lg\""));
  CHECK(!sv::aim_help_weapon_allowed(*a.s, "rl") && !sv::aim_help_weapon_allowed(*a.s, "lg"));
  CHECK(sv::aim_help_weapon_allowed(*a.s, "sg"));
  CHECK(sv::server_command(*a.s, "sv_aimhelp_deny \"\""));
  CHECK(sv::server_command(*a.s, "sv_aimhelp_allow \"sg ssg\""));
  CHECK(sv::aim_help_weapon_allowed(*a.s, "ssg") && !sv::aim_help_weapon_allowed(*a.s, "ng"));
  CHECK(std::string(sv::weapon_name(32)) == "rl" && std::string(sv::weapon_name(64)) == "lg");
}
