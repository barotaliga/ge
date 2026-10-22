// R42's checks behind routes.hpp.
//
// ge:summary R42 checks over the published ski routes
// ge:keywords analysis check-routes ski-route r42
// ge:skill mountain
// ge:test tests/mountain_ski_routes_test.cpp
#include "mountain/analysis/routes.hpp"

#include <algorithm>
#include <string>

#include "core/math.hpp"
#include "mountain/mapgen/resort_rules.hpp"
#include "mountain/mapgen/rules.hpp"
#include "mountain/mapgen/ski_routes.hpp"

namespace ge::mountain::analysis {

namespace K = mapgen::resort_rules::route;
using ge::core::hypot;
using mapgen::SkiRoute;

namespace {

// How far a route's measured windows may pass its bands, m per m: the ground's own rounding.
constexpr double kSlack = 0.005;

}  // namespace

void check_routes(const mapgen::Level& level, const Add& add) {
  if (!level.resort) return;
  const mapgen::ResortMap& area = *level.resort;
  if (double(area.routes.size()) > K::count.max) {
    add("R42", Severity::error, str(double(area.routes.size())) + " ski routes");
  }
  for (size_t i = 0; i < area.routes.size(); i++) {
    const SkiRoute& r = area.routes[i];
    const std::string id = "SR" + std::to_string(i + 1);
    if (r.id != id) add("R42", Severity::error, "route " + r.id + " is out of order");
    if (r.points.size() < 2) {
      add("R42", Severity::error, "route " + r.id + " has no line");
      continue;
    }
    const auto lift = std::find_if(area.lifts.begin(), area.lifts.end(),
                                   [&](const mapgen::Lift& l) { return l.id == r.from; });
    if (lift == area.lifts.end() || lift->kind == mapgen::LiftKind::drag) {
      add("R42", Severity::error, "route " + r.id + " leaves no chair's or gondola's top");
    }
    const bool onto = std::any_of(area.runs.begin(), area.runs.end(),
                                  [&](const mapgen::Run& run) { return run.id == r.into.run; });
    if (!onto) add("R42", Severity::error, "route " + r.id + " comes down onto no run");
    const double vertical = r.points.front().y - r.points.back().y;
    if (r.length < K::length.min || r.length > K::length.max) {
      add("R42", Severity::error, "route " + r.id + " is " + fmt(r.length, 0) + " m long");
    }
    if (vertical < K::vertical) {
      add("R42", Severity::error, "route " + r.id + " falls " + fmt(vertical, 0) + " m");
    }
    const double steepest = mapgen::steepest_along(r.points, mapgen::rules::track::colour_window);
    if (steepest <= mapgen::rules::track::max_grade || steepest > K::steepest + kSlack) {
      add("R42", Severity::error,
          "route " + r.id + " is " + fmt(steepest * 100, 0) + " % at its steepest");
    }
    if (r.drops.size() > size_t(K::wild::drops)) {
      add("R42", Severity::error,
          "route " + r.id + " goes over " + str(double(r.drops.size())) + " drops");
    }
    const double pitch = mapgen::steepest_along(r.points, K::pitch_window, r.drops);
    if (pitch > K::cliff + kSlack) {
      add("R42", Severity::error,
          "route " + r.id + " pitches " + fmt(pitch * 100, 0) + " % outside its drops");
    }
    double x0 = r.points.front().x;
    double x1 = x0;
    double z0 = r.points.front().z;
    double z1 = z0;
    for (const mapgen::TrackPoint& p : r.points) {
      x0 = std::min(x0, p.x);
      x1 = std::max(x1, p.x);
      z0 = std::min(z0, p.z);
      z1 = std::max(z1, p.z);
    }
    int trunks = 0;
    for (const mapgen::TreeDef& t : level.trees) {
      if (t.x < x0 - 5 || t.x > x1 + 5 || t.z < z0 - 5 || t.z > z1 + 5) continue;
      for (const mapgen::TrackPoint& p : r.points) {
        if (hypot(p.x - t.x, p.z - t.z) < t.radius + 0.5) {
          trunks++;
          break;
        }
      }
    }
    if (trunks > 0) {
      add("R42", Severity::error, str(trunks) + " trunk(s) stand on route " + r.id);
    }
    const auto jumps = std::count_if(level.kickers.begin(), level.kickers.end(),
                                     [&](const mapgen::Kicker& k) { return k.route == r.id; });
    if (double(jumps) > K::wild::jump::count.max || (!r.wild && jumps > 0)) {
      add("R42", Severity::error, "route " + r.id + " carries " + str(double(jumps)) + " jumps");
    }
  }
  for (const mapgen::Kicker& k : level.kickers) {
    if (!k.route) continue;
    const bool known = std::any_of(area.routes.begin(), area.routes.end(),
                                   [&](const SkiRoute& r) { return r.id == *k.route; });
    if (!known || k.on_track) {
      add("R42", Severity::error, "jump " + k.id + " stands on no route of the map");
    }
  }
}

}  // namespace ge::mountain::analysis
