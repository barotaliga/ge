// The field's deal, its shoulders and its standings (field.hpp).
//
// ge:summary deal_field, meet_racers, standings and place_of
// ge:keywords field deal_field meet_racers standings place_of
// ge:skill sim racing
// ge:test tests/fall_line_field_test.cpp
#include "fall_line/sim/field.hpp"

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"

namespace fall_line::sim {

namespace mg = ge::mountain::mapgen;
namespace core = ge::core;

std::vector<Rider> deal_field(const mg::Level& level, int32_t racers) {
  std::vector<Rider> field(static_cast<size_t>(racers < 1 ? 1 : racers));
  core::Rng rng = core::make_rng(core::num::to_uint32(level.seed) ^ kFieldSalt);
  const mg::Spawn& line = level.spawn;
  const double rx = core::libm::cos(line.heading);
  const double rz = -core::libm::sin(line.heading);
  for (size_t i = 1; i < field.size(); i++) {
    const double pace = core::range(rng, kPaceLeast, kPaceMost);
    if (level.grid.empty()) {
      field[i] = Rider{pace, 0};
      continue;
    }
    const mg::Spawn& slot = level.grid[i % level.grid.size()];
    field[i] = Rider{pace, (slot.x - line.x) * rx + (slot.z - line.z) * rz};
  }
  return field;
}

void meet_racers(GameState& state) {
  const auto n = static_cast<int32_t>(state.racers.size());
  for (int32_t i = 0; i < n; i++) {
    for (int32_t j = i + 1; j < n; j++) {
      auto& a = state.racers[static_cast<size_t>(i)].skier;
      auto& b = state.racers[static_cast<size_t>(j)].skier;
      const double dx = b.x - a.x;
      const double dz = b.z - a.z;
      const double d = core::hypot(dx, dz);
      if (d >= kShoulder || d <= 1e-9) continue;
      const double nx = dx / d;
      const double nz = dz / d;
      const double push = (kShoulder - d) / 2;
      a.x -= nx * push;
      a.z -= nz * push;
      b.x += nx * push;
      b.z += nz * push;
      const double closing = (a.vx - b.vx) * nx + (a.vz - b.vz) * nz;
      if (closing <= 0) continue;
      const double share = closing * (1 + kBounce) / 2;
      a.vx -= nx * share;
      a.vz -= nz * share;
      b.vx += nx * share;
      b.vz += nz * share;
      state.events.push_back(GameEvent{EventKind::shoulder, i, j, closing});
    }
  }
}

void standings(const GameState& state, std::span<ge::racing::Standing> out) {
  const mg::Level& level = *state.level;
  const auto& gates = level.checkpoints;
  for (size_t i = 0; i < state.racers.size() && i < out.size(); i++) {
    const Racer& r = state.racers[i];
    const Progress& p = r.progress;
    ge::racing::Standing& s = out[i];
    s.finished = p.finished;
    s.time = p.finish_time;
    if (p.finished) {
      s.progress = static_cast<double>(gates.size());
      continue;
    }
    const mg::Checkpoint& next = gates[static_cast<size_t>(p.next)];
    const double fx = p.next > 0 ? gates[static_cast<size_t>(p.next - 1)].x : level.spawn.x;
    const double fz = p.next > 0 ? gates[static_cast<size_t>(p.next - 1)].z : level.spawn.z;
    s.progress = ge::racing::leg_progress(p.next, fx, fz, next.x, next.z, r.skier.x, r.skier.z);
  }
}

int32_t place_of(const GameState& state, int32_t i) {
  constexpr size_t kMost = 16;
  ge::racing::Standing field[kMost];
  const size_t n = state.racers.size() < kMost ? state.racers.size() : kMost;
  standings(state, std::span(field, n));
  return ge::racing::place_among(field[static_cast<size_t>(i)], std::span(field, n));
}

}  // namespace fall_line::sim
