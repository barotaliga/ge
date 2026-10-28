// LAB `traffic`: the village's traffic on a seed's streets. Each seed's map is built at the free
// ride's generator version, its streets laid (`plan_streets`) and its traffic planned
// (`plan_traffic`). The first picture is the plan: every route's line in its role's colour (the
// loop's cars red, the visitors violet, the cars through orange, the ski bus yellow, the
// cyclists green), every bay (taken filled, free outlined) and the bus stop. The second is the
// village at four moments of the period, every vehicle drawn to its size and heading in its
// kind's colour and every parked car grey. The tables list every vehicle (its role, its kind,
// its phase, its top speed) and every seed; the metrics count the vehicles by role and the
// parked cars, and the lab fails when two vehicles (or one and a parked car) ever overlap over
// the period, sampled every half second.
//
// ge:summary Lab: the village's traffic on a seed's streets, its routes and four moments drawn
// ge:keywords lab traffic vehicles routes bus bike parked overlap moments
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "mountain/labs/level_map.hpp"
#include "mountain/village/ground.hpp"
#include "mountain/village/streets.hpp"
#include "mountain/village/traffic.hpp"

namespace {

namespace mg = ge::mountain::mapgen;
namespace vg = ge::mountain::village;
namespace libm = ge::core::libm;
using ge::lab::Rgba;

std::string f1(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.1f", v);
  return b;
}

const char* role_name(vg::Role r) {
  switch (r) {
    case vg::Role::loop:
      return "loop";
    case vg::Role::visitor:
      return "visitor";
    case vg::Role::through:
      return "through";
    case vg::Role::bus:
      return "bus";
    case vg::Role::bike:
      break;
  }
  return "bike";
}

Rgba role_colour(vg::Role r) {
  switch (r) {
    case vg::Role::loop:
      return {230, 60, 40};
    case vg::Role::visitor:
      return {200, 120, 220};
    case vg::Role::through:
      return {250, 150, 40};
    case vg::Role::bus:
      return {250, 230, 60};
    case vg::Role::bike:
      break;
  }
  return {80, 220, 110};
}

Rgba kind_colour(vg::VehicleKind k) {
  switch (k) {
    case vg::VehicleKind::hatch:
      return {230, 60, 40};
    case vg::VehicleKind::estate:
      return {60, 120, 240};
    case vg::VehicleKind::suv:
      return {30, 30, 30};
    case vg::VehicleKind::van:
      return {250, 250, 250};
    case vg::VehicleKind::bus:
      return {250, 210, 40};
    case vg::VehicleKind::bike:
      break;
  }
  return {80, 220, 110};
}

// The picture's frame: the window and the scale.
struct Frame {
  double x0;
  double z0;
  double k;
  double sx(double x) const { return (x - x0) * k; }
  double sz(double z) const { return (z - z0) * k; }
};

// A box `length` by `width` m round (x, z), its length along `heading`, filled.
void fill_box(ge::lab::Picture& pic, const Frame& f, double x, double z, double heading,
              double length, double width, Rgba c) {
  const double fx = libm::sin(heading);
  const double fz = libm::cos(heading);
  const double r = ge::core::hypot(length, width) / 2 * f.k + 1;
  const double cx = f.sx(x);
  const double cz = f.sz(z);
  for (int j = int(cz - r); j <= int(cz + r); j++) {
    for (int i = int(cx - r); i <= int(cx + r); i++) {
      const double dx = (i + 0.5) / f.k + f.x0 - x;
      const double dz = (j + 0.5) / f.k + f.z0 - z;
      const double along = dx * fx + dz * fz;
      const double across = dx * fz - dz * fx;
      if (std::fabs(along) <= length / 2 && std::fabs(across) <= width / 2) {
        ge::lab::set_pixel(pic, i, j, c);
      }
    }
  }
}

void outline_box(ge::lab::Picture& pic, const Frame& f, double x, double z, double heading,
                 double length, double width, Rgba c) {
  const double fx = libm::sin(heading);
  const double fz = libm::cos(heading);
  const auto corner = [&](double a, double b) {
    return std::pair{f.sx(x + fx * a + fz * b), f.sz(z + fz * a - fx * b)};
  };
  const std::array<std::pair<double, double>, 4> c4{
      corner(-length / 2, -width / 2), corner(length / 2, -width / 2),
      corner(length / 2, width / 2), corner(-length / 2, width / 2)};
  for (size_t n = 0; n < 4; n++) {
    ge::lab::draw_line(pic, c4[n].first, c4[n].second, c4[(n + 1) % 4].first,
                       c4[(n + 1) % 4].second, c, 1);
  }
}

// The village from above with its ground: the base every picture here is drawn on.
ge::lab::Picture base_of(const mg::Level& level, const vg::Streets& plan,
                         const vg::GroundMask& mask, int px, double metres, Frame& f) {
  // The middle of the village's junctions (the roads' exits left out).
  double zlo = plan.centre_z;
  double zhi = plan.centre_z;
  for (const vg::Junction& j : plan.junctions) {
    if (j.exit) continue;
    zlo = std::min(zlo, j.z);
    zhi = std::max(zhi, j.z);
  }
  const ge::mountain::picture::MapWindow win{plan.centre_x, (zlo + zhi) / 2, metres};
  ge::lab::Picture pic = ge::mountain::labs::draw_map(level, px, win);
  f = {win.x - metres / 2, win.z - metres / 2, px / metres};
  for (int j = 0; j < px; j++) {
    for (int i = 0; i < px; i++) {
      const vg::Ground g = vg::ground_at(mask, f.x0 + (i + 0.5) / f.k, f.z0 + (j + 0.5) / f.k);
      if (g == vg::Ground::road) ge::lab::set_pixel(pic, i, j, {55, 55, 60, 235});
      if (g == vg::Ground::street) ge::lab::set_pixel(pic, i, j, {200, 200, 195, 225});
    }
  }
  return pic;
}

ge::lab::Picture draw_plan(const mg::Level& level, const vg::Streets& streets,
                           const vg::TrafficPlan& plan, const vg::GroundMask& mask, int px,
                           double metres) {
  Frame f{};
  ge::lab::Picture pic = base_of(level, streets, mask, px, metres, f);
  std::vector<uint8_t> taken(plan.furniture.bays.size(), 0);
  for (const vg::Parked& p : plan.parked) taken[size_t(p.bay)] = 1;
  for (size_t k = 0; k < plan.furniture.bays.size(); k++) {
    const vg::Bay& b = plan.furniture.bays[k];
    if (taken[k]) {
      fill_box(pic, f, b.x, b.z, b.heading, b.length - 0.6, b.width - 0.4, {120, 120, 130});
    } else {
      outline_box(pic, f, b.x, b.z, b.heading, b.length, b.width, {150, 150, 150});
    }
  }
  // Each route once, in the role of the first vehicle on it.
  std::vector<int> drawn(plan.routes.size(), 0);
  for (const vg::Vehicle& v : plan.vehicles) {
    if (drawn[size_t(v.route)]++) continue;
    for (const vg::Driven& d : plan.routes[size_t(v.route)].driven) {
      for (int32_t i = 1; i < d.path.n; i++) {
        ge::lab::draw_line(pic, f.sx(d.path.x[size_t(i) - 1]), f.sz(d.path.z[size_t(i) - 1]),
                           f.sx(d.path.x[size_t(i)]), f.sz(d.path.z[size_t(i)]),
                           role_colour(v.role), 1);
      }
    }
  }
  if (plan.furniture.bus) {
    const vg::BusStop& s = *plan.furniture.bus;
    fill_box(pic, f, s.x, s.z, s.heading + ge::core::kPi / 2, vg::furniture::kBusAlong,
             vg::furniture::kBusDeep, {250, 230, 60});
    ge::lab::draw_text(pic, f.sx(s.x) + 6, f.sz(s.z) - 3, "BUS", {250, 230, 60});
  }
  return pic;
}

ge::lab::Picture draw_moment(const mg::Level& level, const vg::Streets& streets,
                             const vg::TrafficPlan& plan, const vg::GroundMask& mask, int px,
                             double metres, double t) {
  Frame f{};
  ge::lab::Picture pic = base_of(level, streets, mask, px, metres, f);
  for (const vg::Parked& p : plan.parked) {
    const vg::VehicleSize v = vg::vehicle_size(p.kind);
    fill_box(pic, f, p.x, p.z, p.heading, v.length, v.width, {130, 130, 140});
  }
  for (int32_t k = 0; k < int32_t(plan.vehicles.size()); k++) {
    const vg::VehiclePose p = vg::vehicle_at(level, plan, k, t);
    if (!p.shown) continue;
    const vg::VehicleSize v = vg::vehicle_size(p.kind);
    fill_box(pic, f, p.x, p.z, p.heading, v.length, v.width, kind_colour(p.kind));
    outline_box(pic, f, p.x, p.z, p.heading, v.length, v.width, {0, 0, 0});
    if (p.brake) {
      const double bx = p.x - libm::sin(p.heading) * v.length / 2;
      const double bz = p.z - libm::cos(p.heading) * v.length / 2;
      ge::lab::fill_disc(pic, f.sx(bx), f.sz(bz), 1.5, {255, 0, 0});
    }
  }
  const std::string word = "T " + f1(t) + " S";
  ge::lab::fill_rect(pic, 4, 4, ge::lab::text_width(word) + 6, ge::lab::kGlyphH + 5,
                     {0, 0, 0, 180});
  ge::lab::draw_text(pic, 7, 7, word, {255, 255, 255});
  return pic;
}

// The share of the plan's window the moments cover.
constexpr double kCloser = 0.6;

// How many times over the period, sampled every half second, two vehicles (or one and a parked
// car) overlap.
int meetings_of(const mg::Level& level, const vg::TrafficPlan& plan) {
  const auto n = int32_t(plan.vehicles.size());
  int met = 0;
  std::vector<vg::VehiclePose> poses(static_cast<size_t>(n));
  struct Body {
    double x;
    double z;
    double fx;
    double fz;
    double hl;
    double hw;
  };
  const auto body = [](vg::VehicleKind kind, double x, double z, double h) {
    const vg::VehicleSize v = vg::vehicle_size(kind);
    return Body{x, z, libm::sin(h), libm::cos(h), v.length / 2, v.width / 2};
  };
  const auto overlap = [](const Body& a, const Body& b) {
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
  };
  for (int i = 0; i * 0.5 < plan.period; i++) {
    for (int32_t k = 0; k < n; k++) poses[size_t(k)] = vg::vehicle_at(level, plan, k, i * 0.5);
    for (int32_t a = 0; a < n; a++) {
      const vg::VehiclePose& p = poses[size_t(a)];
      if (!p.shown) continue;
      const Body A = body(p.kind, p.x, p.z, p.heading);
      for (int32_t c = a + 1; c < n; c++) {
        const vg::VehiclePose& q = poses[size_t(c)];
        if (q.shown && overlap(A, body(q.kind, q.x, q.z, q.heading))) met++;
      }
      for (const vg::Parked& c : plan.parked) {
        if (std::fabs(c.x - p.x) < 16 && std::fabs(c.z - p.z) < 16 &&
            overlap(A, body(c.kind, c.x, c.z, c.heading))) {
          met++;
        }
      }
    }
  }
  return met;
}

void run(ge::lab::Lab& lab) {
  const int px = static_cast<int>(lab.arg_num("px"));
  const double metres = lab.arg_num("metres");
  const auto version = static_cast<int>(lab.arg_num("version"));
  const std::vector<double> moments = lab.arg_nums("at");
  int with = 0;
  ge::lab::LabTable all{"every seed: its traffic",
                        {"seed", "vehicles", "loop", "visitors", "through", "bus", "bikes",
                         "parked", "bays", "meetings"},
                        {}};
  for (const double seed : lab.arg_nums("seeds")) {
    const std::string seed_word = std::to_string(static_cast<long long>(seed));
    const std::string id = "seed-" + seed_word;
    const auto level = ge::mountain::labs::build_map(lab, seed, version);
    if (!level) continue;
    const std::optional<vg::Streets> streets = vg::plan_streets(*level);
    if (!streets) {
      lab.note(id + ": the map lays no village, so it has no traffic");
      continue;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const std::optional<vg::TrafficPlan> plan = vg::plan_traffic(*level, *streets);
    const auto t1 = std::chrono::steady_clock::now();
    lab.metric(id + ".plan-ms", std::chrono::duration<double, std::milli>(t1 - t0).count(), "ms",
               ge::lab::Det::measured);
    if (!plan) {
      lab.note(id + ": the village's streets carry no lanes, so it has no traffic");
      continue;
    }
    with++;
    std::array<int, 5> roles{};
    ge::lab::LabTable table{
        id + ": the vehicles", {"id", "role", "kind", "phase s", "route period s", "top km/h"}, {}};
    for (const vg::Vehicle& v : plan->vehicles) {
      roles[size_t(v.role)]++;
      double top = 0;
      for (int i = 0; i * 0.5 < plan->period; i++) {
        top = std::max(top, std::fabs(vg::vehicle_at(*level, *plan, v.id, i * 0.5).speed));
      }
      table.rows.push_back({std::to_string(v.id), role_name(v.role), vg::vehicle_kind_name(v.kind),
                            f1(v.phase), f1(plan->routes[size_t(v.route)].period), f1(top * 3.6)});
    }
    lab.table(std::move(table));
    const int met = meetings_of(*level, *plan);
    lab.metric(id + ".vehicles", double(plan->vehicles.size()), "count");
    for (const vg::Role r :
         {vg::Role::loop, vg::Role::visitor, vg::Role::through, vg::Role::bus, vg::Role::bike}) {
      lab.metric(id + "." + role_name(r), roles[size_t(r)], "count");
    }
    lab.metric(id + ".parked", double(plan->parked.size()), "count");
    lab.metric(id + ".bays", double(plan->furniture.bays.size()), "count");
    lab.metric(id + ".meetings", met, "count");
    lab.metric(id + ".hash", double(vg::traffic_hash(*plan, 0xcbf29ce484222325ull) % 1000000007ull),
               "hash");
    if (met > 0) {
      lab.fail(id + ": vehicles overlap " + std::to_string(met) +
                   " times over the period (sampled every half second)",
               "next: check the phases' check in plugins/mountain/village/traffic.cpp "
               "(room_of, overlap). Skill: mountain.");
    }
    all.rows.push_back({seed_word, std::to_string(plan->vehicles.size()), std::to_string(roles[0]),
                        std::to_string(roles[1]), std::to_string(roles[2]),
                        std::to_string(roles[3]), std::to_string(roles[4]),
                        std::to_string(plan->parked.size()),
                        std::to_string(plan->furniture.bays.size()), std::to_string(met)});
    const vg::GroundMask mask = vg::bake_ground(*streets);
    lab.shot({id + "-routes", "SEED " + seed_word + " THE TRAFFIC'S ROUTES",
              "The village from above, " + f1(metres) +
                  " m a side, the carriageways dark and the rest of the streets pale: every "
                  "route's line as its rear axle drives it, the loop's cars red, the visitors to "
                  "the car park violet, the cars through orange, the ski bus yellow, the "
                  "cyclists green; the bays grey (a car parked filled, free outlined) and the bus "
                  "stop yellow.",
              "lines on the right of each street, rounded at the corners; visitors turning into "
              "a bay and out; the bus to its stop and away down a road",
              ge::mountain::labs::map_inputs(lab, *level)},
             draw_plan(*level, *streets, *plan, mask, px, metres));
    // The moments closer in: each panel the picture's side over the middle of the village.
    ge::lab::Picture grid = ge::lab::create_picture(px * 2, px * 2, {0, 0, 0});
    for (size_t m = 0; m < moments.size() && m < 4; m++) {
      const ge::lab::Picture one =
          draw_moment(*level, *streets, *plan, mask, px, metres * kCloser, moments[m]);
      ge::lab::blit(grid, one, int(m % 2) * px, int(m / 2) * px);
    }
    lab.shot(
        {id + "-moments", "SEED " + seed_word + " THE TRAFFIC AT FOUR MOMENTS",
         "The middle of the same village, " + f1(metres * kCloser) +
             " m a side, at four moments of the period (left to right, top to bottom), "
             "every vehicle drawn to its size and heading: a hatch red, an estate blue, an SUV "
             "black, a van white, the bus yellow, a bicycle green, a red dot at its tail when "
             "its brake lights are on; the parked cars grey.",
         "cars spaced along the loop, none on another or on a parked car; the bus at its "
         "stop or on its way; cyclists at the kerb",
         ge::mountain::labs::map_inputs(lab, *level)},
        grid);
  }
  lab.table(std::move(all));
  lab.metric("with-traffic", with, "count");
}

const ge::lab::LabSpec kSpec{
    "traffic",
    ge::lab::LabKind::picture,
    "plugin:mountain",
    "plugins/mountain/village/traffic.cpp",
    "mountain",
    "Who drives a ski village's streets, where and when, and do they ever meet?",
    {
        {"seeds", "38,7,101", "seeds, comma-separated"},
        {"version", "10", "the generator version the map is built at (a ski area: 8 or later)"},
        {"px", "640", "the picture's side, px"},
        {"metres", "480", "the ground the picture covers, m a side"},
        {"at", "60,200,340,480", "the four moments drawn, s into the period"},
        ge::mountain::labs::kRegionFlag,
    },
    "--seeds=38 --px=320",
    run,
};
GE_LAB(kSpec);

}  // namespace
