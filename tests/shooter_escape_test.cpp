// OUT OF A PIT BY ROCKET: a lava pit 48 deep beside a ledge 32 over its surface (dm2's pits are
// 56 and 64 deep, their ledges 32 and 48 over the lava; lava has no swim way out). The nav proves
// escapes out of it onto the ledge, every one landing on a ledge node, each pit node gets a link
// to one, and the build is the same twice; the shot's keys wait for the backing and the rise; and
// the pilot, put on the pit's floor with the launcher, flies one: swum to the takeoff, backed,
// fired as it rises, thrown onto the ledge. (The rocket bursts on the pit's floor: in a pit 88
// deep the blast is too far below a swimmer at the surface to throw him 32 up.)
//
// ge:summary Tests: rockets out of a lava pit proven, linked, keyed and flown by the pilot
// ge:keywords test escape lava pit rocket-escape prove_escape nav_add_escapes escape_back_keys
// ge:skill shooter testing
#include "check.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/escape.hpp"
#include "shooter/pilot.hpp"
#include "shooter/pmove.hpp"

namespace sh = ge::shooter;

namespace {

sh::Brush box(int contents, sh::Vec3f lo, sh::Vec3f hi) {
  sh::Brush b;
  b.contents = contents;
  b.planes = {{{1, 0, 0}, hi.x},   {{-1, 0, 0}, -lo.x}, {{0, 1, 0}, hi.y},
              {{0, -1, 0}, -lo.y}, {{0, 0, 1}, hi.z},   {{0, 0, -1}, -lo.z}};
  return b;
}

// The ledge (x < 200, its top at z 0), the pit's floor (x 200..480, at z -80) under lava to
// z -32, walled round.
struct Pit {
  sh::HullData point, clip;
  sh::MoveWorld world;
  sh::NavGraph g;
  std::vector<uint8_t> land;
  sh::NavEscapeCounts made;
  Pit() {
    const std::vector<sh::Brush> brushes = {
        box(sh::kContentsSolid, {-160, -160, -200}, {200, 160, 0}),
        box(sh::kContentsSolid, {200, -160, -200}, {480, 160, -80}),
        box(sh::kContentsSolid, {480, -200, -200}, {520, 200, 200}),
        box(sh::kContentsSolid, {-200, -200, -200}, {-160, 200, 200}),
        box(sh::kContentsSolid, {-160, 160, -200}, {480, 200, 200}),
        box(sh::kContentsSolid, {-160, -200, -200}, {480, -160, 200}),
        box(sh::kContentsLava, {200, -160, -80}, {480, 160, -32})};
    point = sh::build_point_hull(brushes);
    clip = sh::build_box_hull(brushes, sh::kPlayerMins, sh::kPlayerMaxs);
    world.solids[0].point_hull = point.hull();
    world.solids[0].clip_hull = clip.hull();
    world.solids[0].mins = {-200, -200, -200};
    world.solids[0].maxs = {520, 200, 200};
    world.count = 1;
    sh::build_nav(world, {}, sh::MoveSettings{}, sh::NavSettings{}, g);
    land.assign(g.nodes.size(), 0);
    for (size_t n = 0; n < g.nodes.size(); n++)
      land[n] = g.nodes[n].origin.z > 0 && sh::water_level(world, g.nodes[n].origin) == 0;
    made = sh::nav_add_escapes(world, sh::MoveSettings{}, g, land);
  }
};

const Pit& pit() {
  static const Pit p;
  return p;
}

bool in_pit(const sh::NavGraph& g, const sh::MoveWorld& w, size_t n) {
  return sh::water_level(w, g.nodes[n].origin) >= 2;
}

}  // namespace

TEST(a_lava_pit_gets_escapes_onto_its_ledge) {
  const Pit& p = pit();
  CHECK_MSG(p.made.held >= 1, "no rocket out of the lava pit was proven (columns " +
                                  std::to_string(p.made.columns) + ", proofs " +
                                  std::to_string(p.made.proofs) + ")");
  CHECK(p.made.links >= 1);
  for (const sh::NavEscape& e : p.g.escapes) {
    CHECK(e.to >= 0 && p.land[static_cast<size_t>(e.to)]);
    CHECK(e.pitch <= 80);  // a player's view stops at 80 down
    CHECK(sh::water_level(p.world, e.from) == 3);
    const sh::NavProof pr = sh::prove_escape(p.world, sh::MoveSettings{}, e, p.g.set);
    CHECK_MSG(pr.reached, "an escape kept that its own proof does not land");
  }
  // Every node on the pit's floor has a way out by one.
  int floor = 0, linked = 0;
  for (size_t n = 0; n < p.g.nodes.size(); n++) {
    if (!in_pit(p.g, p.world, n)) continue;
    floor++;
    const sh::NavNode& nd = p.g.nodes[n];
    for (int32_t k = 0; k < nd.links; k++)
      if (p.g.links[static_cast<size_t>(nd.first_link) + static_cast<size_t>(k)].escape >= 0) {
        linked++;
        break;
      }
  }
  CHECK(floor > 10);
  CHECK_MSG(linked == floor, std::to_string(floor - linked) + " pit nodes have no escape link");
}

TEST(the_escapes_are_the_same_twice) {
  const Pit& a = pit();
  const Pit b;
  CHECK(a.g.escapes.size() == b.g.escapes.size());
  for (size_t i = 0; i < a.g.escapes.size() && i < b.g.escapes.size(); i++) {
    CHECK_BITS(a.g.escapes[i].from.x, b.g.escapes[i].from.x);
    CHECK_BITS(a.g.escapes[i].from.z, b.g.escapes[i].from.z);
    CHECK_BITS(a.g.escapes[i].yaw, b.g.escapes[i].yaw);
    CHECK(a.g.escapes[i].to == b.g.escapes[i].to);
  }
}

TEST(the_shot_waits_for_the_backing_and_the_rise) {
  sh::NavEscape e;
  e.yaw = 180;
  e.pitch = 80;
  e.fire_level = 2;
  const sh::Vec3f rising{0, 0, 50}, sinking{0, 0, -20};
  // Too soon after the backing began.
  CHECK(!sh::escape_back_keys(e, 2, rising, 0.05f).fire);
  // Still under, rising: not at an eyes-out escape's level yet.
  CHECK(!sh::escape_back_keys(e, 3, rising, 0.3f).fire);
  // Eyes out, rising: the shot, the view down, jump held.
  const sh::EscapeKeys k = sh::escape_back_keys(e, 2, rising, 0.3f);
  CHECK(k.fire && k.jump);
  CHECK(k.pitch == 80 && k.forward < 0);
  // Under and sinking: no shot (the eyes are not out).
  CHECK(!sh::escape_back_keys(e, 3, sinking, 0.3f).fire);
  // Out and no longer rising: the shot now, whatever the level.
  e.fire_level = 1;
  CHECK(sh::escape_back_keys(e, 2, sinking, 0.3f).fire);
  // The flight: level, away from the ledge, the back key.
  const sh::EscapeKeys f = sh::escape_flight_keys(e);
  CHECK(f.pitch == 0 && f.forward < 0 && !f.fire);
}

TEST(the_pilot_rockets_out_of_the_pit_onto_the_ledge) {
  const Pit& p = pit();
  // The pit's floor node farthest from the ledge, and the ledge's node by the pit.
  int32_t from = -1, to = -1;
  for (size_t n = 0; n < p.g.nodes.size(); n++) {
    const sh::Vec3f o = p.g.nodes[n].origin;
    if (in_pit(p.g, p.world, n) &&
        (from < 0 || o.x > p.g.nodes[static_cast<size_t>(from)].origin.x))
      from = static_cast<int32_t>(n);
    if (p.land[n] && (to < 0 || o.x > p.g.nodes[static_cast<size_t>(to)].origin.x))
      to = static_cast<int32_t>(n);
  }
  CHECK(from >= 0 && to >= 0);
  if (from < 0 || to < 0) return;
  sh::Pilot pl;
  sh::PilotSettings ps;
  ps.jumper.escapes = true;
  const std::vector<ge::ai::AxisHands> hands = sh::pilot_hands(sh::PilotHands::none);
  sh::pilot_init(pl, p.g, ps, hands, 77, 1);
  const sh::MoveSettings move;
  sh::MoveState st;
  st.origin = p.g.nodes[static_cast<size_t>(from)].origin;
  st.maxspeed = move.maxspeed;
  sh::PilotSense s;
  auto sense = [&] {
    s.origin = st.origin;
    s.velocity = st.velocity;
    s.onground = st.ground >= 0;
    s.waterlevel = sh::water_level(p.world, st.origin);
    s.world = &p.world;
  };
  sense();
  CHECK_MSG(sh::pilot_go(pl, to, s), "no route out of the pit with the launcher");
  sh::Vec3f burst_at;
  float burst_in = -1;
  bool out = false;
  for (int i = 0; i < 77 * 8 && !out; i++) {
    const sh::MoveCmd cmd = sh::pilot_step(pl, s, 13);
    sh::player_move(st, cmd, move, p.world);
    if ((cmd.buttons & sh::kButtonAttack) != 0 && burst_in < 0) {
      const sh::RocketBurst rb = sh::rocket_burst(p.world, st.origin, cmd.angles);
      if (rb.ok) {
        burst_at = rb.at;
        burst_in = rb.flight_s;
      }
    } else if (burst_in >= 0) {
      burst_in -= 0.013f;
      if (burst_in <= 0) {
        sh::rocket_blast(st, burst_at);
        burst_in = -1;
      }
    }
    sense();
    out = st.ground >= 0 && st.origin.z > 0 && s.waterlevel == 0;
  }
  CHECK_MSG(pl.stats.escapes >= 1, "the pilot never fired its rocket out of the pit");
  CHECK_MSG(out, "the pilot did not land on the ledge (at " + std::to_string(st.origin.x) + " " +
                     std::to_string(st.origin.z) + ")");
}
