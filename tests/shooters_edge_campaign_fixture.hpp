// The campaign mod's tests' fixture: our campaign program (the rules in C++) on the campaign host in the arena's
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
#include <vector>

#include "check.hpp"
#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
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
    pg::Progs out;
    std::string error;
    CHECK_MSG(shooters_edge::server::campaign_mod(out, error), error);
    return out;
  }();
  return p;
}

struct Game {
  sv::Campaign c;
  std::string error;
  uint32_t sequence = 0;
  // `settings`: server lines before the map loads (an arsenal's rows), after `deathmatch 0`.
  // `bounds`: where setmodel finds a brush model file's bounds (none: such a model keeps its size).
  explicit Game(const std::string& things, const std::vector<std::string>& settings = {},
                sv::ModelBounds bounds = nullptr) {
    g_things = things;
    sv::CampaignOptions o;
    o.server.loop.max_edicts = 128;
    o.server.model_bounds = bounds;
    o.load = &load;
    o.settings = {"deathmatch 0"};
    o.settings.insert(o.settings.end(), settings.begin(), settings.end());
    CHECK_MSG(sv::campaign_new_game(c, mod(), o, "first", error), error);
    sv::campaign_join(c, 0, "player");
  }
  sv::QcServer& s() { return *sv::qc_of(c.server); }
  const pg::LayoutFields& f() { return s().q.layout->f; }
  float field(int e, int ofs) { return pg::as_float(*pg::entity_field(s().q.vm, e, ofs)); }
  void set(int e, int ofs, float v) { *pg::entity_field(s().q.vm, e, ofs) = pg::as_word(v); }
  int entity_of(int e, int ofs) {
    return pg::entity_number(s().q.vm, static_cast<int32_t>(*pg::entity_field(s().q.vm, e, ofs)));
  }
  // Entity `e`'s field `name` (the program's own fields, by name).
  // A vector's component by its name with _x, _y or _z after.
  float named(int e, const char* name) {
    const pg::Def* d = pg::find_field(mod(), name);
    if (d != nullptr) return field(e, d->offset);
    const std::string n = name;
    if (n.size() < 3 || n[n.size() - 2] != '_') return -1;
    const char axis = n.back();
    const int k = axis == 'x' ? 0 : axis == 'y' ? 1 : axis == 'z' ? 2 : -1;
    d = pg::find_field(mod(), n.substr(0, n.size() - 2).c_str());
    return d != nullptr && k >= 0 ? field(e, d->offset + k) : -1;
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
  // A builtin by its number called with these parameter words, the way a program calls it.
  bool builtin(int number, std::initializer_list<uint32_t> args) {
    pg::Vm& vm = s().q.vm;
    se::enter_qc(s().q, s().sv);
    int i = 0;
    for (uint32_t a : args) vm.globals[static_cast<size_t>(pg::parm(i++))] = a;
    const bool ok = pg::call_builtin(vm, number, i);
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
