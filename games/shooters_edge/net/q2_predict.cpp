// The player's own moves predicted ahead of a server of Quake II (q2_predict.hpp).
//
// ge:summary The player's own moves predicted ahead of a server of Quake II, over its brushes
// ge:keywords q2_predict_step q2-prediction movers
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#include "shooters_edge/net/q2_predict.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "classic/teleporters.hpp"
#include "core/num.hpp"
#include "shooter/brush_contents.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter/turned.hpp"
#include "shooters_edge/net/q2_client.hpp"
#include "shooters_edge/server/q2_map.hpp"

namespace shooters_edge::net {

namespace {

namespace gc = ge::classic;

// The level's brushes and its movers where the newest frame put them, each turned by its angles
// (shooter/turned.hpp, as the server traces it).
struct MoverWorld {
  ge::brush::BrushTracer* tracer = nullptr;
  std::vector<Q2Mover> movers;  // traced as entities 2..
  int frames_run = 0;           // the server frames the last run moved them on
  struct Box {
    sh::Vec3f lo, hi;  // the solid box in the world
  };
  std::vector<Box> boxes;
};

gc::Q2Trace mover_trace(void* ctx, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs, sh::Vec3f end) {
  auto* w = static_cast<MoverWorld*>(ctx);
  const float lo[3] = {mins.x, mins.y, mins.z}, hi[3] = {maxs.x, maxs.y, maxs.z};
  gc::Q2Trace best;
  // The world (model 0) first, then each mover moved to where it stands: the nearest hit wins,
  // and a start in any solid is a start in a solid.
  const auto one = [&](uint32_t model, sh::Vec3f at, sh::Vec3f angles, int entity) {
    // A turned mover: the line taken into its frame, the box swept there upright.
    const bool turn = sh::turned(angles);
    sh::Vec3f from = start - at, to = end - at;
    if (turn) from = sh::to_model(from, angles), to = sh::to_model(to, angles);
    const float a[3] = {from.x, from.y, from.z};
    const float b[3] = {to.x, to.y, to.z};
    const ge::brush::Trace t = w->tracer->trace(a, b, lo, hi, sh::kSqMaskPlayerSolid, model);
    best.all_solid = best.all_solid || t.all_solid;
    best.start_solid = best.start_solid || t.start_solid;
    if (model != 0 && t.fraction >= best.fraction) return;
    best.fraction = t.fraction;
    best.end = {t.end[0] + at.x, t.end[1] + at.y, t.end[2] + at.z};
    best.normal = {t.plane.n[0], t.plane.n[1], t.plane.n[2]};
    if (turn) {
      best.end = start + (end - start) * t.fraction;
      best.normal = sh::to_world(best.normal, angles);
    }
    best.surface_flags = t.flags;
    best.contents = t.contents;
    best.entity = entity;
  };
  one(0, {}, {}, 0);
  for (size_t i = 0; i < w->movers.size(); i++)
    one(w->movers[i].model, w->movers[i].at, w->movers[i].angles, 2 + static_cast<int>(i));
  for (const MoverWorld::Box& b : w->boxes) {  // a disc, a player, a monster: its box
    sh::Vec3f n;
    const float f = shooters_edge::server::sweep_box(start, end, mins, maxs, b.lo, b.hi, n);
    if (f < 0) continue;
    if (f == 0) best.start_solid = true;
    if (f >= best.fraction) continue;
    best.fraction = f;
    best.end = start + (end - start) * f;
    best.normal = n;
    best.surface_flags = 0;
    best.contents = sh::kSqSolid;
    best.entity = 1;
  }
  return best;
}

int32_t mover_contents(void* ctx, sh::Vec3f p) {
  auto* w = static_cast<MoverWorld*>(ctx);
  const float q[3] = {p.x, p.y, p.z};
  return w->tracer->contents(q);
}

constexpr int kQ2FrameMs = 100;  // the server's frame: its movers move once in it

// Each mover with a speed run on by `dt` seconds (at most to its stop), carrying the player who
// stands on it (`ground`, as the trace numbers it) or shoving him where it moved into him, when his
// box is clear there (Quake II's standing box; a ducked one is shorter, so clear too).
void run_movers_on(MoverWorld& w, const std::vector<sh::Vec3f>& from, gc::Q2State& s, int ground,
                   float dt) {
  const sh::Vec3f lo{-16, -16, -24}, hi{16, 16, 32};
  for (size_t i = 0; i < w.movers.size(); i++) {
    Q2Mover& m = w.movers[i];
    const float speed = sh::length(m.velocity);
    if (speed <= 0) continue;
    const float step = std::min(speed * dt, m.reach - sh::length(m.at - from[i]));
    if (step <= 0) continue;
    const sh::Vec3f move = m.velocity * (step / speed);
    m.at = m.at + move;
    sh::Vec3f p;
    for (int a = 0; a < 3; a++) (&p.x)[a] = static_cast<float>(s.origin[a]) * 0.125f;
    const bool rider = ground == 2 + static_cast<int>(i);
    if (!rider && !mover_trace(&w, p, lo, hi, p).start_solid) continue;
    const sh::Vec3f to = p + move;
    if (mover_trace(&w, to, lo, hi, to).start_solid) continue;
    for (int a = 0; a < 3; a++)
      s.origin[a] =
          static_cast<int32_t>(ge::core::num::to_int32(static_cast<double>((&to.x)[a]) * 8));
  }
}

// The server's newest state of the player, as the move carries it. `ms`: the extended limits'
// second level (a recording of 3435 on) counts the move's timer in milliseconds; the move's is in
// 8 ms.
gc::Q2State state_of(const client::Q2View& v, bool ms = false) {
  gc::Q2State s;
  s.type = static_cast<gc::Q2Type>(std::clamp(v.pm_type, 0, 4));
  for (int i = 0; i < 3; i++) {
    s.origin[i] = static_cast<int16_t>(v.origin[i]);
    s.velocity[i] = static_cast<int16_t>(v.velocity[i]);
    s.delta_angles[i] = v.delta[i];
  }
  s.flags = static_cast<uint8_t>(v.pm_flags);
  s.time = static_cast<uint8_t>(std::min(ms ? v.pm_time / 8 : v.pm_time, 255));
  s.gravity = static_cast<int16_t>(v.gravity);
  return s;
}

sh::Vec3f origin_of(const gc::Q2State& s) {
  return {static_cast<float>(s.origin[0]) * 0.125f, static_cast<float>(s.origin[1]) * 0.125f,
          static_cast<float>(s.origin[2]) * 0.125f};
}

// The moves sent after `acked` run from `s`; false when the ring no longer holds them.
// A teleporter of `tele` (none when null, `skip` left out) the player's box reaches is taken as
// the server takes it (classic/teleporters.hpp); the first such move's index and teleporter in
// `hit`.
bool run_ahead(gc::Q2State& s, uint32_t acked, const Q2ClientEnd& c, const gc::Q2Feel& feel,
               const gc::Q2World& world, int& replayed, bool* on_ground = nullptr,
               int* water_level = nullptr, float* view_height = nullptr,
               const sh::Teleporters* tele = nullptr, int skip = -1, sh::TeleHit* hit = nullptr,
               uint32_t* hit_move = nullptr, MoverWorld* run_on = nullptr) {
  const uint32_t newest = c.ch.outgoing_sequence;  // the next packet's: not yet sent
  if (newest - acked >= static_cast<uint32_t>(kSqSentRing)) return false;
  // The moves made after the acknowledged packet's, sent or still waiting for the next packet.
  const uint32_t first = c.made_at[acked & (kSqSentRing - 1)];
  if (c.made - first >= static_cast<uint32_t>(kSqSentRing)) return false;
  replayed = 0;
  // The movers run on (`cl_predict_movers`) start where the frame has them and go back after.
  std::vector<sh::Vec3f> from;
  if (run_on != nullptr)
    for (const Q2Mover& m : run_on->movers) from.push_back(m.at);
  int ground = (s.flags & gc::kSqOnGround) != 0 ? 0 : -1;  // what he stands on: 2.. a mover
  // The server moves its movers once a frame (kQ2FrameS) and runs each move as it comes in: the
  // acknowledged move came in as the frame left, each later one as much later as it was made.
  int elapsed_ms = 0, frames_run = 0;
  for (uint32_t k = first; k != c.made; k++) {
    gc::Q2Move pm;
    pm.cmd = gc::q2_command(c.sent[k & (kSqSentRing - 1)]);
    elapsed_ms += pm.cmd.msec;
    for (; run_on != nullptr && (frames_run + 1) * kQ2FrameMs <= elapsed_ms; frames_run++)
      run_movers_on(*run_on, from, s, ground, kQ2FrameMs * 0.001f);
    pm.s = s;
    gc::q2_feel(pm, feel);
    gc::q2_move(pm, world);
    s = pm.s;
    ground = pm.ground_entity;
    if (tele != nullptr && s.type == gc::Q2Type::normal) {
      const int pk = sh::pusher_at(*tele, origin_of(s), pm.mins, pm.maxs, sh::kTeleLinkGrow);
      if (pk >= 0) gc::q2_push_state(s, (*tele)[static_cast<size_t>(pk)]);
    }
    if (tele != nullptr && s.type == gc::Q2Type::normal && (s.flags & gc::kSqTimeTeleport) == 0) {
      const int t =
          sh::teleporter_at(*tele, origin_of(s), pm.mins, pm.maxs, sh::kTeleLinkGrow, skip);
      if (t >= 0) {
        gc::q2_teleport_state(s, (*tele)[static_cast<size_t>(t)], pm.cmd.angles);
        if (hit != nullptr && hit->cmd < 0) {
          *hit = {replayed, t};
          if (hit_move != nullptr) *hit_move = k;
        }
      }
    }
    replayed++;
    if (on_ground != nullptr) *on_ground = pm.ground_entity >= 0;
    if (water_level != nullptr) *water_level = pm.water_level;
    if (view_height != nullptr) *view_height = pm.view_height;
  }
  if (run_on != nullptr) {
    for (size_t i = 0; i < from.size(); i++) run_on->movers[i].at = from[i];
    run_on->frames_run = frames_run;
  }
  return true;
}

// The movers where the newest frame put them.
void movers_of(const Q2Predict& p, const client::DemoState& st, MoverWorld& mw) {
  mw.tracer = p.tracer.get();
  for (size_t n = 0; n < st.entities.size(); n++) {  // the boxes, but for the player's own
    const client::DemoEntity& e = st.entities[n];
    if (!e.present || e.solid == 0 || e.solid == 31 || static_cast<int>(n) == st.player + 1)
      continue;
    const auto x = static_cast<float>(8 * (e.solid & 31));
    const auto down = static_cast<float>(8 * ((e.solid >> 5) & 31));
    const auto up = static_cast<float>(8 * ((e.solid >> 10) & 63) - 32);
    mw.boxes.push_back({e.origin + sh::Vec3f{-x, -x, -down}, e.origin + sh::Vec3f{x, x, up}});
  }
  for (const client::DemoEntity& e : st.entities) {
    if (!e.present || e.model <= 0 || static_cast<size_t>(e.model) >= st.models.size()) continue;
    const std::string& name = st.models[static_cast<size_t>(e.model)];
    if (name.size() < 2 || name[0] != '*') continue;
    const int model = std::atoi(name.c_str() + 1);
    if (model <= 0 || static_cast<size_t>(model) >= p.map->models.size()) continue;
    // Where the newest frame put it: the drawn place is between the last two frames.
    const size_t n = static_cast<size_t>(&e - st.entities.data());
    Q2Mover m{static_cast<uint32_t>(model), e.origin, {}, 1e30f, e.angles};
    if (n < p.movers_track.ents.size() && p.movers_track.ents[n].last_model == e.model)
      m.at = p.movers_track.ents[n].last;
    if (p.movers) sh::mover_motion(p.movers_track, n, m.velocity, m.reach);
    mw.movers.push_back(m);
  }
}

}  // namespace

void q2_predict_map(Q2Predict& p, const ge::brush::Map* map) {
  if (map == p.map) return;
  p.map = map;
  p.tracer = map != nullptr ? std::make_unique<ge::brush::BrushTracer>(*map) : nullptr;
  if (map != nullptr)
    p.tele.fill(gc::q2_teleporters(*map), gc::q2_pushers(*map));
  else
    p.tele = {};
  p.guess = {};
  p.smooth = {};
  p.have_base = false;
  p.movers_track = {};
  p.movers_last.clear();
}

void q2_draw_movers_ahead(const Q2Predict& p, client::DemoState& st) {
  if (!p.movers || !p.predicted) return;
  for (size_t i = 0; i < st.entities.size() && i < p.movers_track.ents.size(); i++) {
    const sh::MoverTrack::One& o = p.movers_track.ents[i];
    if (!st.entities[i].present || o.last_model != st.entities[i].model) continue;
    sh::Vec3f v;
    float reach = 0;
    sh::mover_motion(p.movers_track, i, v, reach);
    if (v.x != 0 || v.y != 0 || v.z != 0)
      st.entities[i].origin = sh::mover_ahead(o.last, v, reach, p.movers_ahead_s);
  }
}

gc::Q2Feel q2_feel_of(const client::Demo& d) {
  if (d.feel_told) return d.feel;
  gc::Q2Feel f;
  f.air_accelerate = d.state.air_accelerate;
  f.land_pause = !d.strafe_hack;  // a later protocol's server data may say it takes no pause
  return f;
}

bool q2_predict_step(Q2Predict& p, const Q2ClientEnd& c, const client::Demo& d, double dt,
                     sh::Vec3f& smoothed) {
  p.predicted = false;
  p.replayed = 0;
  const client::DemoState& st = d.state;
  if (!p.tracer || c.stage != ClientStage::active || d.last_frame < 0 || st.intermission) {
    p.have_base = false;
    return false;
  }
  const client::Q2View& v = d.views[static_cast<size_t>(d.last_frame & 15)];
  const bool ms = st.protocol >= 3435 && st.protocol <= 3436;  // the timer in milliseconds
  if ((v.pm_flags & gc::kSqNoPrediction) != 0 || v.pm_type != 0) {
    p.have_base = false;
    return false;
  }
  if (c.frames != p.track_frames) {  // a new frame: the movers' motion read off it
    p.track_frames = c.frames;
    sh::mover_track_frame(p.movers_track, st.time);
    for (size_t n = 0; n < st.entities.size(); n++)
      if (st.entities[n].present && client::brush_model(st, st.entities[n].model))
        sh::mover_track_seen(p.movers_track, n, st.entities[n].model, st.entities[n].origin);
  }
  MoverWorld mw;
  movers_of(p, st, mw);
  const gc::Q2World world{&mw, mover_trace, mover_contents};
  // The movers as the last prediction had them, for the newest move from the old base.
  MoverWorld then = mw;
  if (p.have_base) then.movers = p.movers_last;
  const gc::Q2World world_then{&then, mover_trace, mover_contents};
  MoverWorld* run_on = p.movers ? &mw : nullptr;
  MoverWorld* run_then = p.movers ? &then : nullptr;
  const gc::Q2Feel feel = q2_feel_of(d);
  const uint32_t acked = c.ch.incoming_acknowledged;
  const sh::Teleporters* tele = p.tele.table(p.teleports, p.pushes);
  // A new frame: the newest move predicted from the old base too, the difference folded into the
  // correction.
  const bool fresh = !p.have_base || c.frames != p.base_frames;
  if (fresh && p.have_base) {
    gc::Q2State old = p.base;
    int n = 0;
    if (run_ahead(old, p.base_acked, c, feel, world_then, n, nullptr, nullptr, nullptr, tele,
                  p.guess.skip, nullptr, nullptr, run_then)) {
      gc::Q2State now = state_of(v, ms);
      if (run_ahead(now, acked, c, feel, world, n, nullptr, nullptr, nullptr, tele, p.guess.skip,
                    nullptr, nullptr, run_on)) {
        ge::shooter::smoothing_frame(p.smooth, origin_of(old), origin_of(now));
        const float miss = ge::shooter::length(origin_of(old) - origin_of(now));
        if (miss > 0) {
          p.misses++;
          p.miss_units = miss;
          p.miss_frame = d.last_frame;
        }
      }
    }
  }
  if (fresh) {
    p.base = state_of(v, ms);
    p.base_acked = acked;
    p.base_frames = c.frames;
    p.have_base = true;
  }
  gc::Q2State s = p.base;
  p.on_ground = (s.flags & gc::kSqOnGround) != 0;
  // The eye's height as the frame has it (standing or ducked), the predicted one replacing it.
  const float framed = (v.pm_flags & gc::kSqDucked) != 0 ? gc::kSqViewDucked : gc::kSqViewStand;
  float ahead = framed;
  sh::TeleHit hit;
  uint32_t hit_move = 0;
  if (!run_ahead(s, p.base_acked, c, feel, world, p.replayed, &p.on_ground, &p.water_level, &ahead,
                 tele, p.guess.skip, &hit, &hit_move, run_on))
    return false;
  p.movers_last = mw.movers;
  p.movers_ahead_s = static_cast<float>(mw.frames_run * kQ2FrameMs) * 0.001f;
  // The teleport foreseen: settled by the server's state (a no leaves that teleporter out while
  // the server has him in it), a new one taken; the view turns with the predicted delta angles.
  if (p.teleports && !p.tele.tele.empty()) {
    sh::Vec3f unused;
    const uint32_t first = c.made_at[p.base_acked & (kSqSentRing - 1)];
    sh::tele_guess_acked(p.guess, p.tele.tele, first, origin_of(p.base), sh::kPlayerMins,
                         sh::kPlayerMaxs, unused);
    if (hit.cmd >= 0) sh::tele_guess_hit(p.guess, p.tele.tele, hit_move + 1, hit.index, {}, unused);
  }
  p.teleported = hit.cmd >= 0;
  for (int i = 0; i < 3; i++) p.delta_angles[i] = s.delta_angles[i];
  const bool crouch = p.crouch == 2 || (p.crouch == 1 && d.feel_told);
  p.eye_shift = crouch ? ahead - framed : 0;
  ge::shooter::smoothing_decay(p.smooth, dt);
  p.origin = origin_of(s);
  p.velocity = {static_cast<float>(s.velocity[0]) * 0.125f,
                static_cast<float>(s.velocity[1]) * 0.125f,
                static_cast<float>(s.velocity[2]) * 0.125f};
  smoothed = ge::shooter::smoothed(p.smooth, p.origin);
  p.predicted = true;
  return true;
}

int q2_predict_others(Q2Predict& p, const Q2ClientEnd& c, client::Demo& d, double now_s,
                      int* most_msec) {
  if (most_msec != nullptr) *most_msec = 0;
  client::DemoState& st = d.state;
  if (!p.tracer || c.stage != ClientStage::active || c.moves_at < 0 || st.intermission) return 0;
  MoverWorld mw;
  movers_of(p, st, mw);
  const gc::Q2World world{&mw, mover_trace, mover_contents};
  const gc::Q2Feel feel = q2_feel_of(d);
  int n = 0;
  for (const client::Demo::OtherMove& o : d.other_moves) {
    if (o.slot == st.player || o.slot < 0 || static_cast<size_t>(o.slot) >= st.players.size() ||
        !st.players[static_cast<size_t>(o.slot)].present || o.state.type != gc::Q2Type::normal)
      continue;
    // the QuakeWorld join's rule (plugins/shooter/predict.hpp): from the state's time to the drawn
    // time, the measured round trip behind now and a little ahead, at most 255 ms.
    const int msec = sh::other_msec(c.moves_at, o.msec, now_s, c.latency);
    gc::Q2State s = o.state;
    if (msec > 0) {
      gc::Q2Move pm;
      pm.s = s;
      pm.cmd = o.cmd;
      pm.cmd.msec = static_cast<uint8_t>(msec);
      gc::q2_feel(pm, feel);
      gc::q2_move(pm, world);
      s = pm.s;
    }
    st.players[static_cast<size_t>(o.slot)].origin = origin_of(s);
    n++;
    if (most_msec != nullptr && msec > *most_msec) *most_msec = msec;
  }
  return n;
}

int q2_run_on_others(const Q2Predict& p, const Q2ClientEnd& c, client::Demo& d,
                     const std::vector<client::DemoEntity>& old,
                     const std::vector<client::DemoEntity>& now, double frame_s, double gap_s,
                     double now_s, int* most_msec) {
  if (most_msec != nullptr) *most_msec = 0;
  client::DemoState& st = d.state;
  if (!p.tracer || c.stage != ClientStage::active || st.intermission || gap_s <= 0) return 0;
  const float lo[3] = {sh::kPlayerMins.x, sh::kPlayerMins.y, sh::kPlayerMins.z};
  const float hi[3] = {sh::kPlayerMaxs.x, sh::kPlayerMaxs.y, sh::kPlayerMaxs.z};
  // The frame was the server's half a round trip before it came; drawn at the server's now.
  const double ahead = (now_s - frame_s + c.latency * 0.5) * 1000.0;
  const int msec = static_cast<int>(std::clamp(ahead + 0.5, 0.0, double(kSqRunOnMostMs)));
  const size_t count = std::min({st.players.size(), old.size(), now.size()});
  int n = 0;
  for (size_t k = 0; k < count; k++) {
    const client::DemoEntity& a = old[k];
    const client::DemoEntity& b = now[k];
    if (static_cast<int>(k) == st.player || !a.present || !b.present || a.model != b.model ||
        !st.players[k].present)
      continue;
    const sh::Vec3f moved = b.origin - a.origin;
    if (sh::length(moved) > kSqRunOnMostMove || (moved.x == 0 && moved.y == 0 && moved.z == 0))
      continue;
    st.players[k].origin = b.origin;  // the newest frame's, not between: run on from it
    n++;
    if (msec == 0) continue;
    const sh::Vec3f to = b.origin + moved * static_cast<float>(msec * 0.001 / gap_s);
    const float from3[3] = {b.origin.x, b.origin.y, b.origin.z}, to3[3] = {to.x, to.y, to.z};
    const ge::brush::Trace t = p.tracer->trace(from3, to3, lo, hi, sh::kSqMaskPlayerSolid, 0);
    if (t.start_solid || t.all_solid) continue;  // a box the brushes do not take: the frame's
    st.players[k].origin = {t.end[0], t.end[1], t.end[2]};
  }
  if (most_msec != nullptr && n > 0) *most_msec = msec;
  return n;
}

}  // namespace shooters_edge::net
