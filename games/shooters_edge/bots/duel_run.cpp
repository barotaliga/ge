// The watched run (duel_run.hpp): the host with the seer and the runner in two bot slots, and the
// truth measured beside the seer's own senses each frame.
//
// ge:summary The watched run: make_duel and run_duel on the arena's host
// ge:keywords duel make_duel run_duel seer runner aim-error reaction
// ge:skill shooter bots
// ge:test tests/shooter_senses_test.cpp
#include "shooters_edge/bots/duel_run.hpp"

#include <cmath>
#include <string>

#include "shooter/trace.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/sim/arena.hpp"

namespace shooters_edge::bots {

namespace {

constexpr double kHz = 1000.0 / kPilotMsec;
constexpr float kOnTarget = 5;  // degrees: a reaction ends with the view this near

float wrap(float d) {
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return d;
}

}  // namespace

bool make_duel(Duel& d, sh::AimSkill skill, uint32_t seed) {
  server::HostSettings hs;
  hs.seed = seed;
  std::string error;
  if (!server::host_create(d.host, hs, error)) return false;
  const sim::Rules& rules = d.host.rules;
  const sh::Server& sv = d.host.sv;
  sh::MoveWorld world;
  sh::map_world(d.host.models[0], world);
  const sh::Edict& door = sv.edicts[static_cast<size_t>(rules.door)];
  const sh::Edict& plat = sv.edicts[static_cast<size_t>(rules.plat)];
  sh::NavMover movers[2];
  movers[0].ent = rules.door;
  movers[0].absmin = door.absmin;
  movers[0].absmax = door.absmax;
  movers[1].ent = rules.plat;
  movers[1].absmin = plat.absmin;
  movers[1].absmax = plat.absmax;
  movers[1].model = &d.host.models[static_cast<size_t>(sim::kPlatModel) - 1];
  movers[1].travel = {0, 0, sim::kPlatRise};
  movers[1].ride_s = sim::kPlatWait + sim::kPlatRise / sim::kPlatSpeed;
  sh::build_nav(world, movers, sh::MoveSettings{}, sh::NavSettings{}, d.graph);

  sh::bot_init(d.seer, d.graph, sh::PilotSettings{}, sh::PilotHands::human, skill, kHz, seed);
  const std::vector<sh::Vec3f> goals = arena_goals();
  for (const sh::Vec3f& p : goals) d.seer.pilot.goals.push_back(sh::nav_nearest(d.graph, p));
  const std::vector<ge::ai::AxisHands> h = sh::pilot_hands(sh::PilotHands::human);
  sh::PilotSettings ps;
  ps.lead_s = sh::pilot_lead_s(sh::PilotHands::human);
  sh::pilot_init(d.runner.pilot, d.graph, ps, h, kHz, seed + 101);
  for (size_t i = 0; i < goals.size(); i++)
    d.runner.goals.push_back(sh::nav_nearest(d.graph, goals[(i + 2) % goals.size()]));
  d.seer_slot = server::host_add_bot(d.host, sh::bot_source(d.seer));
  d.runner_slot = server::host_add_bot(d.host, sh::pilot_source(d.runner));
  return d.seer_slot >= 0 && d.runner_slot >= 0;
}

DuelResult run_duel(Duel& d, int frames) {
  DuelResult r;
  const sh::Server& sv = d.host.sv;
  const int me = d.seer_slot + 1, him = d.runner_slot + 1;
  const float vh = d.seer.senses.set.view_height;
  bool was_seen = false, reacting = false;
  double acquired_at = 0;
  for (int f = 0; f < frames; f++) {
    server::host_frame(d.host, nullptr, 0);
    r.frames++;
    const sh::Vec3f a = sv.edicts[static_cast<size_t>(me)].origin;
    const sh::Vec3f b = sv.edicts[static_cast<size_t>(him)].origin;
    r.seer.push_back(a);
    r.runner.push_back(b);
    r.view_yaw.push_back(d.seer.yaw);
    const sh::Vec3f ea{a.x, a.y, a.z + vh}, eb{b.x, b.y, b.z + vh};
    const sh::HullTrace tr = sh::trace_move(sv, ea, {}, {}, eb, sh::kTraceNoMonsters, me);
    if (!tr.startsolid && tr.fraction >= 1) r.line_frames++;
    bool seen = false;
    for (const sh::Contact& c : d.seer.senses.contacts)
      if (c.ent == him && c.visible) seen = true;
    r.seen.push_back(seen ? 1 : 0);
    if (seen) {
      r.seen_frames++;
      float yaw = 0, pitch = 0;
      sh::aim_angles(ea, b, yaw, pitch);
      const float dy = wrap(yaw - d.seer.yaw), dp = pitch - d.seer.pitch;
      const float err = std::sqrt(dy * dy + dp * dp);
      r.error_deg.push_back(err);
      if (!was_seen) {
        reacting = true;
        acquired_at = sv.time;
      }
      if (reacting && err <= kOnTarget) {
        r.reaction_s.push_back(static_cast<float>(sv.time - acquired_at));
        reacting = false;
      }
    } else {
      reacting = false;
    }
    was_seen = seen;
    if (d.seer.aiming) {
      r.yaw.push_back(d.seer.pressed[0]);
      r.yaw_wanted.push_back(d.seer.wanted[0]);
    }
  }
  r.heard = d.seer.senses.stats.heard;
  r.senses = d.seer.senses.stats;
  r.bot = d.seer.stats;
  return r;
}

}  // namespace shooters_edge::bots
