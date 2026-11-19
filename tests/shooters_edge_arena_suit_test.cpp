// Tests: the hazard suit on the arena's rules (games/shooters_edge/rules/common_items.cpp). A
// player who touches it holds it for 30 s, a second one starting the 30 s again, and keeps a full
// 12 s of air the whole time; it is gone when its time runs out. A dead wearer loses it, and leaves
// it on the map with the time it had left only with `sv_dropsuit 1`; whoever takes that one has
// that time.
//
// ge:summary Tests: the arena's hazard suit: pickup, its 30 s, air kept, sv_dropsuit
// ge:keywords test arena hazard-suit biosuit envirosuit sv_dropsuit dropped-power air_finished
// ge:skill progs shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/file.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge_qc_fixture.hpp"

using qcfix::ent;
using qcfix::Fixture;
namespace sv = shooters_edge::server;
namespace pg = ge::progs;
namespace sh = ge::shooter;

namespace {

constexpr float kItSuit = 2097152;
const sh::Vec3f kSuitAt{160, -96, 24};

const pg::Progs& arena() {
  static const pg::Progs p = [] {
    pg::Progs out;
    std::string error;
    CHECK_MSG(sv::baseline_mod(out, error), error);
    return out;
  }();
  return p;
}

std::string with_suit() {
  return std::string(sv::kArenaEntities) +
         "{\n\"classname\" \"item_artifact_envirosuit\"\n\"origin\" \"160 -96 24\"\n}\n";
}

struct Rig {
  Fixture fx;
  int items, suit, air, health, origin;
  explicit Rig(const std::vector<std::string>& console)
      : fx(with_suit().c_str(), console, &arena()) {
    CHECK_MSG(fx.loaded, fx.error);
    items = fx.field("items");
    suit = fx.field("suit_finished");
    air = fx.field("air_finished");
    health = fx.field("health");
    origin = fx.field("origin");
    CHECK(suit > 0 && air > 0);
  }
  float now() const { return static_cast<float>(fx.s->sv.time); }
  uint32_t sequence = 0;
  // `n` frames with every joined player standing still.
  void frames(int n) {
    for (int i = 0; i < n; i++) {
      sh::Packet pk[2];
      sequence++;
      for (int c = 0; c < joined; c++) {
        pk[c].client = c;
        pk[c].sequence = sequence;
        for (sh::MoveCmd& m : pk[c].cmds) {
          m = {};
          m.msec = 14;
        }
      }
      sv::qc_frame(*fx.s, pk, joined);
    }
  }
  int joined = 0;
  int join(const char* name) { return fx.join(joined++, name); }
  // `name` run as the program's own call with `self`.
  void run(const char* name, int self) {
    sv::QcServer& q = *fx.s;
    q.sv.g.self = self;
    q.sv.g.other = 0;
    q.sv.g.time = static_cast<float>(q.sv.time);
    shooters_edge::sim::enter_qc(q.q, q.sv);
    const int fn = pg::find_function(fx.progs, name);
    CHECK_MSG(fn > 0, name);
    if (fn > 0) pg::call_function(q.q.vm, fn);
    shooters_edge::sim::leave_qc(q.q);
  }
  void put(int p, sh::Vec3f at) {
    fx.call(2, {ent(p), at});
    fx.in_call([&] {
      for (int k = 0; k < 3; k++) fx.set_f(p, fx.field("velocity"), 0, k);
    });
  }
  bool wears(int p) { return (static_cast<int>(fx.f(p, items)) & static_cast<int>(kItSuit)) != 0; }
  // The suits lying on the map: the placed one and any dropped.
  std::vector<int> suits() {
    std::vector<int> out;
    for (int e = 1; e < fx.s->sv.num_edicts; e++)
      if (!fx.s->sv.edicts[static_cast<size_t>(e)].free &&
          fx.str_field(e, fx.field("classname")) == "item_artifact_envirosuit")
        out.push_back(e);
    return out;
  }
  int said(const std::string& head) {
    int n = 0;
    for (const sv::NetEvent* e : fx.events(sv::EventKind::print))
      n += e->text.rfind(head, 0) == 0 ? 1 : 0;
    return n;
  }
};

}  // namespace

TEST(the_suit_lasts_30_s_keeps_the_air_full_and_a_second_starts_it_again) {
  Rig r({"deathmatch 1"});
  const int p = r.join("a");
  r.frames(3);
  r.put(p, kSuitAt);
  r.frames(2);
  CHECK(r.wears(p));
  const float until = r.fx.f(p, r.suit);
  CHECK_MSG(until > r.now() + 29 && until <= r.now() + 30, std::to_string(until));
  r.frames(72 * 5);
  CHECK(r.fx.f(p, r.air) > r.now() + 11.9f);  // air kept full while it is worn
  r.fx.in_call([&] { r.fx.set_f(p, r.suit, r.now() + 10); });
  r.fx.in_call([&] { r.fx.set_f(r.suits()[0], r.fx.field("nextthink"), r.now()); });
  r.frames(2);  // the suit back on the map
  r.put(p, kSuitAt);
  r.frames(2);
  CHECK_MSG(r.fx.f(p, r.suit) > r.now() + 29, "a second suit starts the 30 s again");
  r.fx.in_call([&] { r.fx.set_f(p, r.suit, r.now() + 0.05f); });
  r.frames(12);
  CHECK(!r.wears(p));
  CHECK(r.fx.f(p, r.suit) == 0);
  CHECK_MSG(r.fx.s->q.faults == 0, r.fx.s->q.last_fault);
}

TEST(a_dead_wearer_leaves_the_suit_only_with_sv_dropsuit) {
  for (const bool drop : {false, true}) {
    Rig r({"deathmatch 1", drop ? "sv_dropsuit 1" : "sv_dropsuit 0"});
    const int a = r.join("a");
    const int b = r.join("b");
    r.frames(3);
    r.put(a, kSuitAt);
    r.frames(2);
    CHECK(r.wears(a));
    r.frames(72 * 2);  // past the second after spawning that a suicide waits for
    const float until = r.fx.f(a, r.suit);
    r.run("ClientKill", a);
    r.frames(1);
    CHECK(!r.wears(a));
    CHECK(r.fx.f(a, r.suit) == 0);
    const std::vector<int> lying = r.suits();
    CHECK_MSG(lying.size() == (drop ? 2u : 1u), std::to_string(lying.size()));
    if (!drop || lying.size() != 2) continue;
    const int left =
        r.fx.f(lying[0], r.suit) != 0 ? lying[0] : lying[1];  // the placed one has none
    CHECK(r.fx.f(left, r.suit) == until);
    r.frames(144);  // thrown up, then down on the floor
    const sh::Edict& e = r.fx.s->sv.edicts[static_cast<size_t>(left)];
    r.put(b, {e.origin.x, e.origin.y, e.origin.z + 24});
    r.frames(3);
    CHECK(r.wears(b));
    CHECK(r.fx.f(b, r.suit) == until);
    CHECK(r.suits().size() == 1u);
    CHECK_MSG(r.fx.s->q.faults == 0, r.fx.s->q.last_fault);
  }
}
