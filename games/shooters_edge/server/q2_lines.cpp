// Quake II's server console lines about the map (q2_lines.hpp).
//
// ge:summary Quake II's server console lines: a map's faults at load, the inhibited and teams count
// ge:keywords q2 console server-console spawn-warnings inhibited teams picktarget position
// ge:skill shooter
// ge:test tests/shooters_edge_q2_lines_test.cpp
#include "shooters_edge/server/q2_lines.hpp"

#include <cmath>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {
namespace {

// One coordinate: to the nearest whole number, halves to the even one (the default rounding
// mode), `-0` kept for a small negative.
std::string coord(float v) {
  const double r = std::nearbyint(double(v));
  if (r == 0) return std::signbit(v) ? "-0" : "0";
  return std::to_string(static_cast<long long>(r));
}

bool dm(const Q2Server& s) { return s.cvars.find("deathmatch")->on(); }

void say(Q2Server& s, std::string line) { s.console.push_back(std::move(line)); }

}  // namespace

std::string q2_pos(sh::Vec3f p) {
  return "(" + coord(p.x) + " " + coord(p.y) + " " + coord(p.z) + ")";
}

void q2_pick_missing(Q2Server& s, const std::string& target) {
  say(s, target.empty() ? std::string("G_PickTarget called with NULL targetname")
                        : "G_PickTarget: target " + target + " not found");
}

void q2_pick_missing(Q2Server& s, const std::string& target, const std::string& then) {
  if (target.empty()) return;
  q2_pick_missing(s, target);
  if (!then.empty()) say(s, then);
}

void q2_spawn_lines(Q2Server& s, const std::vector<char>& inhibited) {
  if (s.map == nullptr) return;
  const sh::Vec3f zero{};
  const size_t n = std::min(s.entities.size(), s.map->entities.size() - 1);
  for (size_t i = 0; i < n; i++) {
    if (i < inhibited.size() && inhibited[i] != 0) continue;
    Q2Entity& e = s.entities[i];
    const ge::brush::Entity& b = s.map->entities[i + 1];
    const auto has = [&b](const char* k) { return !b.get_any_case(k).empty(); };
    const std::string at = q2_pos(e.origin);
    const std::string& c = e.classname;
    if (c == "path_corner" && e.targetname.empty()) {
      say(s, "path_corner with no targetname at " + at);
      e.gone = true;
    } else if (c == "viewthing") {
      say(s, "viewthing spawned");
    } else if (c == "func_wall" && (e.spawnflags & 4) != 0 && (e.spawnflags & 2) == 0) {
      say(s, "func_wall START_ON without TOGGLE");
      e.spawnflags |= 2;
    } else if (c == "misc_teleporter" && e.target.empty()) {
      say(s, "teleporter without a target.");
    } else if (c == "func_train" && e.target.empty()) {
      sh::Vec3f lo = e.origin - sh::Vec3f{1, 1, 1};
      if (e.modelindex > 0 && size_t(e.modelindex) < s.map->models.size()) {
        const float* m = s.map->models[size_t(e.modelindex)].mins;
        lo = lo + sh::Vec3f{m[0], m[1], m[2]};
      }
      say(s, "func_train without a target at " + q2_pos(lo));
    } else if (c == "func_timer") {
      const float wait = has("wait") ? std::strtof(b.get_any_case("wait").c_str(), nullptr) : 1;
      const float random = std::strtof(b.get_any_case("random").c_str(), nullptr);
      if (random >= (wait == 0 ? 1.0f : wait))
        say(s, "func_timer at " + at + " has random >= wait");
    } else if (c == "target_speaker" && !has("noise")) {
      say(s, "target_speaker with no noise set at " + at);
    } else if (c == "target_changelevel" && !has("map")) {
      say(s, "target_changelevel with no map at " + at);
    } else if (c == "target_lightramp") {
      const std::string r = b.get_any_case("message");
      const bool bad =
          r.size() != 2 || r[0] < 'a' || r[0] > 'z' || r[1] < 'a' || r[1] > 'z' || r[0] == r[1];
      if (bad)
        say(s, "target_lightramp has bad ramp (" + (has("message") ? r : "(null)") + ") at " + at);
      else if (!dm(s) && e.target.empty())
        say(s, "target_lightramp with no target at " + at);
    } else if (c == "target_earthquake" && e.targetname.empty()) {
      say(s, "untargeted target_earthquake at " + at);
    } else if (c == "trigger_once" && (e.spawnflags & 1) != 0) {
      // Meant as the box's centre, but said before the box is known.
      say(s, "fixed TRIGGERED flag on trigger_once at " + q2_pos(zero));
    } else if (c == "trigger_key") {
      const std::string item = b.get_any_case("item");
      if (item.empty())
        say(s, "no key item for trigger_key at " + at);
      else if (sq_item(item) < 0)
        say(s,
            std::string("item ").append(item).append(" not found for trigger_key at ").append(at));
      if (e.target.empty()) say(s, "trigger_key at " + at + " has no target");
    } else if (c == "trigger_gravity" && !has("gravity")) {
      say(s, "trigger_gravity without gravity set at " + at);
    }
  }
}

void q2_count_lines(Q2Server& s, int inhibited) {
  say(s, std::to_string(inhibited) + " entities inhibited");
  std::set<std::string> teams;
  int members = 0;
  for (const Q2Entity& e : s.entities) {
    if (e.gone || e.team.empty() || e.block < 0) continue;
    teams.insert(e.team);
    members++;
  }
  say(s, std::to_string(teams.size()) + " teams with " + std::to_string(members) + " entities");
}

}  // namespace shooters_edge::server
