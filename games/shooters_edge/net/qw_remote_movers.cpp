// The movers a server joined predicts over and draws (qw_remote_movers.hpp).
//
// ge:summary The movers a server joined predicts over and draws, and the entities between frames
// ge:keywords remote_movers remote_lerp_entities remote_draw_movers_ahead movers lerp
// ge:skill net shooter
// ge:test tests/shooters_edge_remote_test.cpp
#include "shooters_edge/net/qw_remote_movers.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace shooters_edge::net {

namespace sh = ge::shooter;

// The brush models where the newest frame put them (its movers), with their speed and their stops
// when `cl_predict_movers` is on (`sh::MoverTrack`).
std::vector<sh::ClientModel> remote_movers(const Remote& r) {
  const std::vector<sh::BrushModel>& models = *r.world;
  const client::DemoState& st = r.demo.state;
  // The newest frame's places, not the drawn ones (those are between the last two frames).
  const std::vector<client::DemoEntity>& ents = r.ents_new.empty() ? st.entities : r.ents_new;
  std::vector<sh::ClientModel> movers;
  for (size_t i = 0; i < ents.size(); i++) {
    const client::DemoEntity& e = ents[i];
    if (!e.present || e.model <= 0 || static_cast<size_t>(e.model) >= st.models.size()) continue;
    const std::string& name = st.models[static_cast<size_t>(e.model)];
    if (name.size() < 2 || name[0] != '*') continue;
    const int k = std::atoi(name.c_str() + 1);
    if (k <= 0 || static_cast<size_t>(k) >= models.size()) continue;
    sh::ClientModel m{&models[static_cast<size_t>(k)], e.origin, {}, 1e30f};
    if (r.predict_movers) sh::mover_motion(r.movers_track, i, m.velocity, m.reach);
    movers.push_back(m);
  }
  return movers;
}

const float* remote_mover_times(const Remote& r, uint32_t acked, const uint32_t* seqs,
                                const sh::MoveCmd* cmds, int count, float* mover_at) {
  const QwClientEnd& c = r.end;
  const bool known = c.sent_seq[acked & 63] == acked;
  float sum = 0;
  for (int i = 0; i < count; i++) {
    sum += cmds[i].msec * 0.001f;
    const double at = c.sent_at[seqs[i] & 63] - c.sent_at[acked & 63];
    mover_at[i] = known ? static_cast<float>(at > 0 ? at : 0) : sum;
  }
  return known ? mover_at : nullptr;
}

void remote_movers_keep(Remote& r, const std::vector<sh::ClientModel>& movers,
                        const float* mover_at, int count) {
  r.movers_last = movers;
  r.movers_last_level = r.levels;
  r.movers_ahead_s = count > 0 ? mover_at[count - 1] : 0;
}

void remote_movers_turn(Remote& r) {
  r.movers_prev.swap(r.movers_last);
  r.movers_prev_level = r.movers_last_level;
  r.movers_last.clear();
  r.movers_last_level = -1;
}

// The movers drawn where the player's own prediction ran them (`cl_predict_movers`), so a lift he
// rides is under his feet and a door he runs through is as open as his move found it.
void remote_draw_movers_ahead(Remote& r) {
  if (!r.predict_movers || !r.predicted) return;
  std::vector<client::DemoEntity>& ents = r.demo.state.entities;
  for (size_t i = 0; i < ents.size() && i < r.movers_track.ents.size(); i++) {
    const sh::MoverTrack::One& o = r.movers_track.ents[i];
    if (!ents[i].present || o.last_model != ents[i].model) continue;
    sh::Vec3f v;
    float reach = 0;
    sh::mover_motion(r.movers_track, i, v, reach);
    if (v.x != 0 || v.y != 0 || v.z != 0)
      ents[i].origin = sh::mover_ahead(o.last, v, reach, r.movers_ahead_s);
  }
}

// The entities drawn between the last two frames read, at the drawn time `now_s`.
void remote_lerp_entities(Remote& r, double now_s) {
  r.lerped = 0;
  if (r.nolerp || r.frame_old_s < 0) return;
  const double gap = r.frame_new_s - r.frame_old_s;
  if (gap <= 0) return;
  double f = (now_s - r.frame_new_s) / gap;
  f = f < 0 ? 0 : f > 1 ? 1 : f;
  const auto t = static_cast<float>(f);
  std::vector<client::DemoEntity>& ents = r.demo.state.entities;
  const size_t n = std::min({ents.size(), r.ents_old.size(), r.ents_new.size()});
  for (size_t i = 0; i < n; i++) {
    const client::DemoEntity& a = r.ents_old[i];
    const client::DemoEntity& b = r.ents_new[i];
    if (!a.present || !b.present || a.model != b.model || !ents[i].present) continue;
    const sh::Vec3f d = b.origin - a.origin;
    if (sh::length(d) > kLerpMost || (d.x == 0 && d.y == 0 && d.z == 0)) continue;
    ents[i].origin = a.origin + d * t;
    r.lerped++;
  }
}

}  // namespace shooters_edge::net
