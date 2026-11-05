// The campaign mod's tests' fixture: our campaign program on the campaign host in the arena's
// west room (with the door's and the plat's brushes as "*1" and "*2"), the player at its start,
// the test's own entities after; reading and setting fields, calling the program's functions as
// an entity, a blow, a blast and a host frame.
//
// ge:summary Tests' fixture: the campaign mod on the campaign host, fields, calls and frames
// ge:keywords test fixture campaign-mod campaign-host qc-call arena
// ge:skill progs shooter testing
#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>

#include "check.hpp"
#include "progs/layout.hpp"
#include "progs/qc_compile.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/sim/qc_program.hpp"

namespace campfix {

namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;
namespace se = shooters_edge::sim;

inline std::string g_things;  // the map's entities after the world and the start

inline bool load(void* /*ctx*/, const std::string& /*name*/, sv::MapSpec& out,
                 std::string& /*error*/) {
  out.models = sv::arena_models(true);
  out.entities = R"({
"classname" "worldspawn"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
}
)" + g_things;
  return true;
}

inline const pg::Progs& mod() {
  static const pg::Progs p = [] {
    const pg::QcProject built = pg::compile_project(std::filesystem::path(GE_SOURCE_DIR) /
                                                    "games/shooters_edge/qc/campaign/progs.src");
    for (const pg::QcDiagnostic& d : built.result.diagnostics)
      CHECK_MSG(!d.error, pg::diagnostic_text(d));
    return built.result.progs;
  }();
  return p;
}

struct Game {
  sv::Campaign c;
  std::string error;
  uint32_t sequence = 0;
  explicit Game(const std::string& things) {
    g_things = things;
    sv::CampaignOptions o;
    o.server.loop.max_edicts = 128;
    o.load = &load;
    o.settings = {"deathmatch 0"};
    CHECK_MSG(sv::campaign_new_game(c, mod(), o, "first", error), error);
    sv::campaign_join(c, 0, "player");
  }
  sv::QcServer& s() { return *c.server; }
  const pg::LayoutFields& f() { return s().q.layout->f; }
  float field(int e, int ofs) { return pg::as_float(*pg::entity_field(s().q.vm, e, ofs)); }
  void set(int e, int ofs, float v) { *pg::entity_field(s().q.vm, e, ofs) = pg::as_word(v); }
  int entity_of(int e, int ofs) {
    return pg::entity_number(s().q.vm, static_cast<int32_t>(*pg::entity_field(s().q.vm, e, ofs)));
  }
  float global(const char* name) {
    const pg::Def* d = pg::find_global(mod(), name);
    return d ? pg::as_float(s().q.vm.globals[d->offset]) : -1;
  }
  int find(const char* classname, int after = 0) {
    for (int e = after + 1; e < s().sv.num_edicts; e++) {
      const char* cn = pg::vm_string(
          s().q.vm, static_cast<int32_t>(*pg::entity_field(s().q.vm, e, f().classname)));
      if (!s().sv.edicts[static_cast<size_t>(e)].free && cn != nullptr &&
          std::string(cn) == classname)
        return e;
    }
    return 0;
  }
  // A function of the program called with these parameter words, the way the host calls a hook
  // with arguments.
  // `self` (and `activator`) are set first when `as` names an entity.
  bool call(const char* name, std::initializer_list<uint32_t> args, int as = 0) {
    const int fn = pg::find_function(mod(), name);
    if (fn < 0) return false;
    pg::Vm& vm = s().q.vm;
    se::enter_qc(s().q, s().sv);
    if (as > 0) {
      vm.globals[pg::find_global(mod(), "self")->offset] = ent(as);
      vm.globals[pg::find_global(mod(), "activator")->offset] = ent(1);
    }
    int i = 0;
    for (uint32_t a : args) vm.globals[static_cast<size_t>(pg::parm(i++))] = a;
    const bool ok = se::run_qc(s().q, fn);
    se::leave_qc(s().q);
    return ok;
  }
  uint32_t ent(int e) { return static_cast<uint32_t>(pg::entity_value(s().q.vm, e)); }
  // One blow of `points` on `target` from `attacker`, through the program's own `damage`.
  bool hit(int target, int attacker, float points) {
    return call("damage", {ent(target), ent(attacker), ent(attacker), pg::as_word(points)});
  }
  // A blast of `points` centred on `at`'s origin, through the program's own `radius_damage`.
  bool blast(int at, int attacker, float points) {
    return call("radius_damage", {ent(at), ent(attacker), pg::as_word(points), ent(0), 0});
  }
  const sh::Edict& ed(int e) { return s().sv.edicts[static_cast<size_t>(e)]; }
  // One host frame with the player looking along `yaw` and pressing `buttons`.
  bool frame(float yaw = 0, uint8_t buttons = 0, uint8_t impulse = 0) {
    sh::Packet pk;
    pk.client = 0;
    pk.sequence = ++sequence;
    for (sh::MoveCmd& m : pk.cmds) {
      m = {};
      m.msec = 14;
      m.angles = {0, yaw, 0};
      m.buttons = buttons;
      m.impulse = impulse;
    }
    return sv::campaign_frame(c, &pk, 1, error);
  }
};

constexpr int kFrames = 72;  // host frames a second

}  // namespace campfix
