// The village's traffic (plugins/mountain/village/traffic.hpp): the cars, the ski bus and the
// bicycles on the village's streets and the cars parked in its bays, as a pure function of the
// plan and the clock: the same every time and a period on; carrying every kind; on the streets,
// never two in one place nor one in a parked car; at a village's speeds; the bus standing at its
// stop; the plan of each reference seed golden.
//
// ge:summary Tests: the village's traffic, its routes, phases, parked cars and its golden
// ge:keywords test traffic vehicles parked bus bike overlap period golden
// ge:skill mountain testing
// ge:test tests/mountain_traffic_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "golden.hpp"
#include "mountain/mapgen/generate.hpp"
#include "mountain/village/ground.hpp"
#include "mountain/village/streets.hpp"
#include "mountain/village/traffic.hpp"

namespace mg = ge::mountain::mapgen;
namespace vg = ge::mountain::village;
namespace libm = ge::core::libm;

namespace {

constexpr const char* kGolden = "tests/golden/mountain_traffic.txt";
constexpr int kVersion = 10;

bool env_set(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  const bool set = _dupenv_s(&v, &n, name) == 0 && v != nullptr;
  std::free(v);
  return set;
#else
  return std::getenv(name) != nullptr;
#endif
}

// The ski area each seed builds, its streets and its traffic, built once.
struct Built {
  mg::Level level;
  vg::Streets streets;
  vg::TrafficPlan plan;
};
const Built& built(double seed) {
  static std::map<double, std::unique_ptr<Built>> all;
  auto& slot = all[seed];
  if (!slot) {
    mg::GenerateOptions opts;
    opts.version = kVersion;
    mg::Level level = mg::generate_level(seed, opts);
    std::optional<vg::Streets> streets = vg::plan_streets(level);
    CHECK_MSG(streets.has_value(), "seed " + std::to_string(seed) + " laid no village");
    std::optional<vg::TrafficPlan> plan =
        streets ? vg::plan_traffic(level, *streets) : std::nullopt;
    CHECK_MSG(plan.has_value(), "seed " + std::to_string(seed) + " has no traffic");
    slot = std::make_unique<Built>(
        Built{std::move(level), streets.value_or(vg::Streets{}), plan.value_or(vg::TrafficPlan{})});
  }
  return *slot;
}

struct Box {
  double x;
  double z;
  double fx;
  double fz;
  double hl;
  double hw;
};

Box body_of(vg::VehicleKind kind, double x, double z, double heading) {
  const vg::VehicleSize v = vg::vehicle_size(kind);
  return {x, z, libm::sin(heading), libm::cos(heading), v.length / 2, v.width / 2};
}

// Whether two boxes overlap, by the separating axes of both.
bool overlap(const Box& a, const Box& b) {
  const double dx = b.x - a.x;
  const double dz = b.z - a.z;
  const std::array<std::array<double, 2>, 4> axes{
      {{a.fx, a.fz}, {a.fz, -a.fx}, {b.fx, b.fz}, {b.fz, -b.fx}}};
  for (const auto& [ax, az] : axes) {
    const double ra =
        a.hl * std::fabs(a.fx * ax + a.fz * az) + a.hw * std::fabs(a.fz * ax - a.fx * az);
    const double rb =
        b.hl * std::fabs(b.fx * ax + b.fz * az) + b.hw * std::fabs(b.fz * ax - b.fx * az);
    if (std::fabs(dx * ax + dz * az) > ra + rb) return false;
  }
  return true;
}

std::string hex(uint64_t u) {
  char b[24];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u));
  return b;
}

}  // namespace

TEST(the_traffic_is_the_same_every_time_and_a_period_on) {
  const Built& b = built(38);
  for (int32_t k = 0; k < int32_t(b.plan.vehicles.size()); k++) {
    for (const double t : {0.0, 37.5, 211.25, 480.0}) {
      const vg::VehiclePose p = vg::vehicle_at(b.level, b.plan, k, t);
      const vg::VehiclePose q = vg::vehicle_at(b.level, b.plan, k, t + b.plan.period);
      CHECK(p.shown == q.shown);
      if (!p.shown) continue;
      CHECK_MSG(std::fabs(p.x - q.x) < 1e-6 && std::fabs(p.z - q.z) < 1e-6 &&
                    std::fabs(p.heading - q.heading) < 1e-6,
                "vehicle " + std::to_string(k) + " is not where it was a period before at t " +
                    std::to_string(t));
    }
  }
}

TEST(the_streets_carry_cars_the_bus_and_bikes_and_cars_park_in_the_bays) {
  for (const double seed : {38.0, 7.0, 101.0}) {
    const Built& b = built(seed);
    std::set<vg::VehicleKind> kinds;
    std::array<int, 5> roles{};
    for (const vg::Vehicle& v : b.plan.vehicles) {
      kinds.insert(v.kind);
      roles[size_t(v.role)]++;
    }
    const std::string at = "seed " + std::to_string(int(seed)) + ": ";
    CHECK_MSG(kinds.count(vg::VehicleKind::bus) == 1, at + "no ski bus on the streets");
    CHECK_MSG(kinds.count(vg::VehicleKind::bike) == 1, at + "no bicycle on the streets");
    CHECK_MSG(roles[size_t(vg::Role::loop)] >= vg::traffic::kLoopLeast * 2 - 1,
              at + "too few cars round the loop: " + std::to_string(roles[size_t(vg::Role::loop)]));
    CHECK_MSG(roles[size_t(vg::Role::visitor)] >= 2, at + "too few visitors to the car park");
    CHECK_MSG(b.plan.parked.size() > 10,
              at + "only " + std::to_string(b.plan.parked.size()) + " cars parked");
    std::set<int32_t> seen;
    for (const vg::Parked& p : b.plan.parked) {
      const vg::Bay& bay = b.plan.furniture.bays[size_t(p.bay)];
      CHECK_MSG(seen.insert(p.bay).second, at + bay.id + " holds two cars");
      CHECK(std::hypot(p.x - bay.x, p.z - bay.z) < 0.05);
      const vg::VehicleSize v = vg::vehicle_size(p.kind);
      CHECK_MSG(v.length <= bay.length + 0.01 && v.width <= bay.width,
                at + "a " + vg::vehicle_kind_name(p.kind) + " does not fit " + bay.id);
    }
  }
}

TEST(the_traffic_keeps_to_the_streets_never_two_in_one_place_nor_in_a_parked_car) {
  for (const double seed : {38.0, 5.0}) {
    const Built& b = built(seed);
    const vg::GroundMask mask = vg::bake_ground(b.streets);
    const auto n = int32_t(b.plan.vehicles.size());
    std::vector<vg::VehiclePose> poses(static_cast<size_t>(n));
    int shown = 0;
    int off = 0;
    int met = 0;
    std::string first;
    for (int i = 0; i * 0.5 < b.plan.period; i++) {
      const double t = i * 0.5;
      for (int32_t k = 0; k < n; k++) poses[size_t(k)] = vg::vehicle_at(b.level, b.plan, k, t);
      for (int32_t a = 0; a < n; a++) {
        const vg::VehiclePose& p = poses[size_t(a)];
        if (!p.shown) continue;
        shown++;
        if (!vg::on_street(mask, p.x, p.z)) off++;
        const Box A = body_of(p.kind, p.x, p.z, p.heading);
        for (int32_t c = a + 1; c < n; c++) {
          const vg::VehiclePose& q = poses[size_t(c)];
          if (!q.shown || !overlap(A, body_of(q.kind, q.x, q.z, q.heading))) continue;
          if (met++ == 0) {
            first = std::to_string(a) + " and " + std::to_string(c) + " at " + std::to_string(t);
          }
        }
        for (const vg::Parked& c : b.plan.parked) {
          if (std::fabs(c.x - p.x) > 16 || std::fabs(c.z - p.z) > 16) continue;
          if (!overlap(A, body_of(c.kind, c.x, c.z, c.heading))) continue;
          if (met++ == 0) {
            first = std::to_string(a) + " and parked " + std::to_string(c.id) + " at " +
                    std::to_string(t);
          }
        }
      }
    }
    const std::string at = "seed " + std::to_string(int(seed)) + ": ";
    std::string why = at;
    why += std::to_string(met) + " meetings, the first " + first;
    CHECK_MSG(met == 0, why);
    CHECK_MSG(shown > 1000, at + "only " + std::to_string(shown) + " vehicle-moments shown");
    CHECK_MSG(off * 20 < shown, at + std::to_string(off) + " of " + std::to_string(shown) +
                                    " vehicle-moments off the streets");
  }
}

TEST(the_traffic_drives_at_a_villages_speeds_and_a_bike_at_a_cyclists) {
  const Built& b = built(38);
  for (int32_t k = 0; k < int32_t(b.plan.vehicles.size()); k++) {
    const vg::VehicleKind kind = b.plan.vehicles[size_t(k)].kind;
    const vg::Drive d = vg::drive_of(vg::driver_of(kind));
    double most = 0;
    std::optional<vg::VehiclePose> last;
    for (int i = 0; i * 0.5 < b.plan.period; i++) {
      const vg::VehiclePose p = vg::vehicle_at(b.level, b.plan, k, i * 0.5);
      if (!p.shown) {
        last.reset();
        continue;
      }
      most = std::max(most, std::fabs(p.speed));
      // It moves as fast as it says it does, and no faster.
      if (last) {
        CHECK_MSG(std::hypot(p.x - last->x, p.z - last->z) < 0.5 * d.road + 1.5,
                  "vehicle " + std::to_string(k) + " jumps at t " + std::to_string(i * 0.5));
      }
      last = p;
    }
    const std::string at = std::string(vg::vehicle_kind_name(kind)) + " " + std::to_string(k);
    CHECK_MSG(most <= d.road + 0.05, at + " drives at " + std::to_string(most) + " m/s");
    CHECK_MSG(most > (kind == vg::VehicleKind::bike ? 2 : 4),
              at + " never gets going: " + std::to_string(most) + " m/s");
  }
}

TEST(the_bus_stands_at_its_stop) {
  const Built& b = built(38);
  const auto it = std::find_if(b.plan.vehicles.begin(), b.plan.vehicles.end(),
                               [](const vg::Vehicle& v) { return v.kind == vg::VehicleKind::bus; });
  CHECK(it != b.plan.vehicles.end() && b.plan.furniture.bus.has_value());
  if (it == b.plan.vehicles.end() || !b.plan.furniture.bus.has_value()) return;
  const vg::BusStop stop = b.plan.furniture.bus.value_or(vg::BusStop{});
  double still = 0;
  double at_stop = 0;
  for (int i = 0; i * 0.5 < b.plan.period; i++) {
    const vg::VehiclePose p = vg::vehicle_at(b.level, b.plan, it->id, i * 0.5);
    if (!p.shown || std::fabs(p.speed) > 0.05) continue;
    still += 0.5;
    if (std::hypot(p.x - stop.x, p.z - stop.z) < 12) at_stop += 0.5;
  }
  CHECK_MSG(at_stop >= vg::traffic::kBusStayLeast,
            "the bus stood " + std::to_string(at_stop) + " s at its stop");
  CHECK(at_stop > still * 0.8);
}

TEST(the_traffic_is_a_pure_function_of_the_streets) {
  const Built& b = built(38);
  const std::optional<vg::TrafficPlan> again = vg::plan_traffic(b.level, b.streets);
  CHECK(again.has_value());
  if (!again) return;
  CHECK(vg::traffic_hash(*again, 1) == vg::traffic_hash(b.plan, 1));
}

TEST(every_seed_keeps_its_golden) {
  std::vector<std::string> got;
  for (const double seed : {38.0, 7.0, 101.0}) {
    const Built& b = built(seed);
    uint64_t h = vg::traffic_hash(b.plan, 0xcbf29ce484222325ull);
    // Every vehicle at a few moments, folded in as well.
    for (int32_t k = 0; k < int32_t(b.plan.vehicles.size()); k++) {
      for (const double t : {12.5, 300.0}) {
        const vg::VehiclePose p = vg::vehicle_at(b.level, b.plan, k, t);
        for (const double v : {p.x, p.z, p.heading, p.speed}) {
          uint64_t u = 0;
          std::memcpy(&u, &v, sizeof u);
          h = (h ^ u) * 0x100000001b3ull;
        }
      }
    }
    got.push_back(std::to_string(static_cast<long long>(seed)) + " " + hex(h) + " " +
                  std::to_string(b.plan.vehicles.size()) + " " +
                  std::to_string(b.plan.parked.size()));
  }
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# each seed's village traffic at generator v10: the seed, the plan's hash with every "
           "vehicle at two moments, its vehicles and its parked cars (mountain_traffic_test)\n";
    for (const std::string& row : got) out << row << "\n";
    return;
  }
  const auto want = ge::test::golden_words(kGolden);
  CHECK(want.size() == got.size());
  for (size_t r = 0; r < got.size() && r < want.size(); r++) {
    std::string line;
    for (const auto& w : want[r]) line += (line.empty() ? "" : " ") + w;
    CHECK_MSG(line == got[r], "the traffic moved:\n  got  " + got[r] + "\n  want " + line +
                                  "\nIf the move is meant, rewrite " + kGolden +
                                  " with GE_GOLDEN_WRITE=1 and put the traffic lab's pictures "
                                  "before and after in the PR (skill: mountain).");
  }
}
