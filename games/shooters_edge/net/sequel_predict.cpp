// The player's own moves predicted ahead of a server of the sequel (sequel_predict.hpp).
//
// ge:summary The player's own moves predicted ahead of a server of the sequel, over its brushes
// ge:keywords sequel_predict_step sequel-prediction movers
// ge:skill net shooter
// ge:test tests/shooters_edge_sequel_net_test.cpp
#include "shooters_edge/net/sequel_predict.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "shooter/brush_contents.hpp"
#include "shooter/brush_hull.hpp"
#include "shooters_edge/net/sequel_client.hpp"

namespace shooters_edge::net {

namespace {

namespace gc = ge::classic;

// The level's brushes and its movers where the newest frame put them (a mover's turn not traced).
struct MoverWorld {
  ge::brush::BrushTracer* tracer = nullptr;
  struct Mover {
    uint32_t model = 0;
    sh::Vec3f at;
  };
  std::vector<Mover> movers;
};

gc::SequelTrace mover_trace(void* ctx, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs,
                            sh::Vec3f end) {
  auto* w = static_cast<MoverWorld*>(ctx);
  const float lo[3] = {mins.x, mins.y, mins.z}, hi[3] = {maxs.x, maxs.y, maxs.z};
  gc::SequelTrace best;
  // The world (model 0) first, then each mover moved to where it stands: the nearest hit wins,
  // and a start in any solid is a start in a solid.
  const auto one = [&](uint32_t model, sh::Vec3f at, int entity) {
    const float a[3] = {start.x - at.x, start.y - at.y, start.z - at.z};
    const float b[3] = {end.x - at.x, end.y - at.y, end.z - at.z};
    const ge::brush::Trace t = w->tracer->trace(a, b, lo, hi, sh::kSqMaskPlayerSolid, model);
    best.all_solid = best.all_solid || t.all_solid;
    best.start_solid = best.start_solid || t.start_solid;
    if (model != 0 && t.fraction >= best.fraction) return;
    best.fraction = t.fraction;
    best.end = {t.end[0] + at.x, t.end[1] + at.y, t.end[2] + at.z};
    best.normal = {t.plane.n[0], t.plane.n[1], t.plane.n[2]};
    best.surface_flags = t.flags;
    best.contents = t.contents;
    best.entity = entity;
  };
  one(0, {}, 0);
  for (const MoverWorld::Mover& m : w->movers) one(m.model, m.at, 1);
  return best;
}

int32_t mover_contents(void* ctx, sh::Vec3f p) {
  auto* w = static_cast<MoverWorld*>(ctx);
  const float q[3] = {p.x, p.y, p.z};
  return w->tracer->contents(q);
}

// The server's newest state of the player, as the move carries it.
gc::SequelState state_of(const client::SequelView& v) {
  gc::SequelState s;
  s.type = static_cast<gc::SequelType>(std::clamp(v.pm_type, 0, 4));
  for (int i = 0; i < 3; i++) {
    s.origin[i] = static_cast<int16_t>(v.origin[i]);
    s.velocity[i] = static_cast<int16_t>(v.velocity[i]);
    s.delta_angles[i] = v.delta[i];
  }
  s.flags = static_cast<uint8_t>(v.pm_flags);
  s.time = static_cast<uint8_t>(v.pm_time);
  s.gravity = static_cast<int16_t>(v.gravity);
  return s;
}

sh::Vec3f origin_of(const gc::SequelState& s) {
  return {static_cast<float>(s.origin[0]) * 0.125f, static_cast<float>(s.origin[1]) * 0.125f,
          static_cast<float>(s.origin[2]) * 0.125f};
}

// The moves sent after `acked` run from `s`; false when the ring no longer holds them.
bool run_ahead(gc::SequelState& s, uint32_t acked, const SequelClientEnd& c, float air,
               const gc::SequelWorld& world, int& replayed, bool* on_ground = nullptr) {
  const uint32_t newest = c.ch.outgoing_sequence;  // the next packet's: not yet sent
  if (newest - acked >= static_cast<uint32_t>(kSqSentRing)) return false;
  replayed = 0;
  for (uint32_t seq = acked + 1; seq != newest; seq++) {
    gc::SequelMove pm;
    pm.s = s;
    pm.cmd = gc::sequel_command(c.sent[seq & (kSqSentRing - 1)]);
    pm.air_accelerate = air;
    gc::sequel_move(pm, world);
    s = pm.s;
    replayed++;
    if (on_ground != nullptr) *on_ground = pm.ground_entity >= 0;
  }
  return true;
}

// The movers where the newest frame put them.
void movers_of(const SequelPredict& p, const client::DemoState& st, MoverWorld& mw) {
  mw.tracer = p.tracer.get();
  for (const client::DemoEntity& e : st.entities) {
    if (!e.present || e.model <= 0 || static_cast<size_t>(e.model) >= st.models.size()) continue;
    const std::string& name = st.models[static_cast<size_t>(e.model)];
    if (name.size() < 2 || name[0] != '*') continue;
    const int model = std::atoi(name.c_str() + 1);
    if (model <= 0 || static_cast<size_t>(model) >= p.map->models.size()) continue;
    mw.movers.push_back({static_cast<uint32_t>(model), e.origin});
  }
}

}  // namespace

void sequel_predict_map(SequelPredict& p, const ge::brush::Map* map) {
  if (map == p.map) return;
  p.map = map;
  p.tracer = map != nullptr ? std::make_unique<ge::brush::BrushTracer>(*map) : nullptr;
  p.smooth = {};
  p.have_base = false;
}

bool sequel_predict_step(SequelPredict& p, const SequelClientEnd& c, const client::Demo& d,
                         double dt, sh::Vec3f& smoothed) {
  p.predicted = false;
  p.replayed = 0;
  const client::DemoState& st = d.state;
  if (!p.tracer || c.stage != ClientStage::active || d.last_frame < 0 || st.intermission) {
    p.have_base = false;
    return false;
  }
  const client::SequelView& v = d.views[static_cast<size_t>(d.last_frame & 15)];
  if ((v.pm_flags & gc::kSqNoPrediction) != 0 || v.pm_type != 0) {
    p.have_base = false;
    return false;
  }
  MoverWorld mw;
  movers_of(p, st, mw);
  const gc::SequelWorld world{&mw, mover_trace, mover_contents};
  const float air = st.air_accelerate;
  const uint32_t acked = c.ch.incoming_acknowledged;
  // A new frame: the newest move predicted from the old base too, the difference folded into the
  // correction.
  const bool fresh = !p.have_base || c.frames != p.base_frames;
  if (fresh && p.have_base) {
    gc::SequelState old = p.base;
    int n = 0;
    if (run_ahead(old, p.base_acked, c, air, world, n)) {
      gc::SequelState now = state_of(v);
      if (run_ahead(now, acked, c, air, world, n))
        ge::shooter::smoothing_frame(p.smooth, origin_of(old), origin_of(now));
    }
  }
  if (fresh) {
    p.base = state_of(v);
    p.base_acked = acked;
    p.base_frames = c.frames;
    p.have_base = true;
  }
  gc::SequelState s = p.base;
  p.on_ground = (s.flags & gc::kSqOnGround) != 0;
  if (!run_ahead(s, p.base_acked, c, air, world, p.replayed, &p.on_ground)) return false;
  ge::shooter::smoothing_decay(p.smooth, dt);
  p.origin = origin_of(s);
  p.velocity = {static_cast<float>(s.velocity[0]) * 0.125f,
                static_cast<float>(s.velocity[1]) * 0.125f,
                static_cast<float>(s.velocity[2]) * 0.125f};
  smoothed = ge::shooter::smoothed(p.smooth, p.origin);
  p.predicted = true;
  return true;
}

int sequel_predict_others(SequelPredict& p, const SequelClientEnd& c, client::Demo& d, double now_s,
                          int* most_msec) {
  if (most_msec != nullptr) *most_msec = 0;
  client::DemoState& st = d.state;
  if (!p.tracer || c.stage != ClientStage::active || c.moves_at < 0 || st.intermission) return 0;
  MoverWorld mw;
  movers_of(p, st, mw);
  const gc::SequelWorld world{&mw, mover_trace, mover_contents};
  int n = 0;
  for (const client::Demo::OtherMove& o : d.other_moves) {
    if (o.slot == st.player || o.slot < 0 || static_cast<size_t>(o.slot) >= st.players.size() ||
        !st.players[static_cast<size_t>(o.slot)].present || o.state.type != gc::SequelType::normal)
      continue;
    // the classic join's rule (plugins/shooter/predict.hpp): from the state's time to the drawn
    // time, the measured round trip behind now and a little ahead, at most 255 ms.
    const int msec = sh::other_msec(c.moves_at, o.msec, now_s, c.latency);
    gc::SequelState s = o.state;
    if (msec > 0) {
      gc::SequelMove pm;
      pm.s = s;
      pm.cmd = o.cmd;
      pm.cmd.msec = static_cast<uint8_t>(msec);
      pm.air_accelerate = st.air_accelerate;
      gc::sequel_move(pm, world);
      s = pm.s;
    }
    st.players[static_cast<size_t>(o.slot)].origin = origin_of(s);
    n++;
    if (most_msec != nullptr && msec > *most_msec) *most_msec = msec;
  }
  return n;
}

int sequel_run_on_others(const SequelPredict& p, const SequelClientEnd& c, client::Demo& d,
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
