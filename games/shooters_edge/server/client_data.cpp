// A single-player client's own data each frame (client_data.hpp).
//
// ge:summary The blow taken and cleared, and the ideal pitch traced off the floor ahead
// ge:keywords take_client_data ideal_pitch dmg_take dmg_save dmg_inflictor slope trace
// ge:skill shooter progs
// ge:test tests/shooters_edge_client_data_test.cpp
#include "shooters_edge/server/client_data.hpp"

#include <algorithm>

#include "core/libm/libm.hpp"
#include "progs/vm.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

namespace {

namespace sh = ge::shooter;

constexpr double kRadians = 3.14159265358979323846 / 180;
constexpr int kPoints = 6;         // the floor's points ahead
constexpr float kFirstAhead = 36;  // units to the first; the rest 12 apart
constexpr float kPointStep = 12;   // units between the points
constexpr float kTraceDown = 160;  // units down from the eye
constexpr float kFlat = 0.1f;      // a height step this near 0 is flat; two this near are one
constexpr int kLeastSlopeSteps = 2;

float vm_f(const pg::Vm& vm, int e, int f) { return pg::as_float(*pg::entity_field(vm, e, f)); }
void vm_set_f(pg::Vm& vm, int e, int f, float v) { *pg::entity_field(vm, e, f) = pg::as_word(v); }

int to_byte(float v) { return v != v ? 0 : std::clamp(static_cast<int>(v), 0, 255); }

}  // namespace

float ideal_pitch(const sh::Server& sv, int e, float current, float scale) {
  const sh::Edict& ed = sv.edicts[static_cast<size_t>(e)];
  if ((ed.flags & sh::kFlagOnGround) == 0) return current;
  const double yaw = static_cast<double>(ed.angles.y) * kRadians;
  const auto cy = static_cast<float>(ge::core::libm::cos(yaw));
  const auto sy = static_cast<float>(ge::core::libm::sin(yaw));
  float z[kPoints];
  for (int i = 0; i < kPoints; i++) {
    const float ahead = kFirstAhead + kPointStep * static_cast<float>(i);
    sh::Vec3f top{ed.origin.x + cy * ahead, ed.origin.y + sy * ahead, ed.origin.z + ed.view_ofs.z};
    sh::Vec3f bottom = top;
    bottom.z -= kTraceDown;
    const sh::HullTrace tr = sh::trace_move(sv, top, {}, {}, bottom, sh::kTraceNoMonsters, e);
    if (tr.allsolid || tr.startsolid || tr.fraction == 1) return current;
    z[i] = top.z + tr.fraction * (bottom.z - top.z);
  }
  float slope = 0;
  int steps = 0;
  for (int j = 1; j < kPoints; j++) {
    const float step = z[j] - z[j - 1];
    if (step > -kFlat && step < kFlat) continue;
    if (steps > 0 && (step - slope > kFlat || step - slope < -kFlat)) return current;
    steps++;
    slope = step;
  }
  if (steps == 0) return 0;
  if (steps < kLeastSlopeSteps) return current;
  return -slope * scale;
}

void take_client_data(QcServer& s) {
  const pg::LayoutFields& f = s.q.layout->f;
  if (f.idealpitch < 0) return;  // the multiplayer form: its own fields, never touched
  pg::Vm& vm = s.q.vm;
  const auto scale = static_cast<float>(s.cvars.find("sv_idealpitchscale")->number);
  for (int i = 0; i < sh::kMaxClients; i++) {
    ClientInfo& c = s.clients[i];
    c.blow = ClientBlow{};
    if (!c.active) continue;
    const int e = i + 1;
    const float take = vm_f(vm, e, f.dmg_take), save = vm_f(vm, e, f.dmg_save);
    if (take != 0 || save != 0) {
      c.blow.hit = true;
      c.blow.blood = to_byte(take);
      c.blow.armor = to_byte(save);
      int from =
          pg::entity_number(vm, static_cast<int32_t>(*pg::entity_field(vm, e, f.dmg_inflictor)));
      if (from < 0 || from >= s.sv.num_edicts) from = 0;
      const sh::Edict& src = s.sv.edicts[static_cast<size_t>(from)];
      c.blow.from = src.origin + (src.mins + src.maxs) * 0.5f;
      vm_set_f(vm, e, f.dmg_take, 0);
      vm_set_f(vm, e, f.dmg_save, 0);
    }
    if (s.clients[i].spectator) continue;
    vm_set_f(vm, e, f.idealpitch, ideal_pitch(s.sv, e, vm_f(vm, e, f.idealpitch), scale));
  }
}

}  // namespace shooters_edge::server
