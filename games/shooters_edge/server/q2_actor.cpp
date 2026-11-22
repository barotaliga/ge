// Quake II's misc_actor and target_actor (q2_actor.hpp).
//
// ge:summary Quake II's misc_actor and target_actor: the scripted figure, its points, its taunts
// ge:keywords q2 misc_actor target_actor actor scripted-figure taunt flipoff path-point jump
// ge:skill shooter
// ge:test tests/shooters_edge_q2_actor_test.cpp
#include "shooters_edge/server/q2_actor.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

#include "brush/map.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/q2_fight.hpp"
#include "shooters_edge/server/q2_lines.hpp"
#include "shooters_edge/server/q2_map.hpp"
#include "shooters_edge/server/q2_server.hpp"

namespace shooters_edge::server {
namespace {

constexpr const char* kNames[8] = {"Hellrot",    "Tokay",   "Killme", "Disruptor",
                                   "Adrianator", "Rambear", "Titus",  "Bitterman"};
// Four words, of which the draw reaches only the first three.
constexpr const char* kWords[4] = {"Watch it", "#$@*&", "Idiot", "Check your targets"};
constexpr int kChat = 3;  // the chat print level

sh::Vec3f unpack(const int16_t v[3]) {
  return {float(v[0]) * 0.125f, float(v[1]) * 0.125f, float(v[2]) * 0.125f};
}

bool is_actor(const Q2Server& s, const Q2Monster& m) {
  return s.entities[size_t(m.entity)].classname == "misc_actor";
}

std::string key(const Q2Server& s, const Q2Entity& e, const char* k) {
  if (s.map == nullptr || e.block < 0 || size_t(e.block) >= s.map->entities.size()) return {};
  return s.map->entities[size_t(e.block)].get_any_case(k);
}

float yaw_to(sh::Vec3f d) {
  constexpr double kDegrees = 180.0 / 3.14159265358979323846;
  if (d.x == 0 && d.y == 0) return 0;
  const float y = static_cast<float>(ge::core::libm::atan2(double(d.y), double(d.x)) * kDegrees);
  return y < 0 ? y + 360 : y;
}

// The first entity named `name` (the game picks among up to eight; maps name one), or -1.
int pick(const Q2Server& s, const std::string& name) {
  for (size_t i = 0; i < s.entities.size(); i++)
    if (!s.entities[i].gone && s.entities[i].targetname == name) return static_cast<int>(i);
  return -1;
}

// A chat line (the chat's print level) to the players in `to`, a bit each.
void chat(Q2Server& s, uint32_t to, std::string text) {
  NetEvent ev;
  ev.kind = EventKind::print;
  ev.to = to;
  ev.reliable = true;
  ev.level = kChat;
  ev.text = std::move(text) + "\n";
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

void jump_sound(Q2Server& s, int e) {
  NetEvent ev;
  ev.kind = EventKind::sound;
  ev.to = 0xffffffffu;
  ev.ent = map_row(e);
  ev.level = 2;  // the voice
  ev.origin = s.entities[size_t(e)].origin;
  ev.values[0] = 255;
  ev.values[1] = 1;
  ev.text = "player/male/jump1.wav";
  ev.time = s.level_time;
  s.out.events.push_back(std::move(ev));
  s.out.events_total++;
}

}  // namespace

std::string actor_name(const Q2Server& s, int e) {
  const int number = std::max(1, s.sv.set.maxclients) + e + 1;  // the world, the clients, the map
  return kNames[number % 8];
}

void actor_spawn(Q2Server& s) {
  for (Q2Monster& m : s.monsters) {
    if (!is_actor(s, m)) continue;
    Q2Entity& e = s.entities[size_t(m.entity)];
    const int health = std::atoi(key(s, e, "health").c_str());
    if (health != 0) e.health = e.max_health = health;
  }
}

bool actor_use(Q2Server& s, Q2Monster& m) {
  Q2Entity& e = s.entities[size_t(m.entity)];
  if (m.dead) return true;
  const std::string target = e.target;
  const int point = target.empty() ? -1 : pick(s, target);
  if (point < 0 || s.entities[size_t(point)].classname != "target_actor") {
    if (target.empty())
      s.console.emplace_back("G_PickTarget called with NULL targetname");
    else if (point < 0)
      q2_pick_missing(s, target);
    s.console.push_back("misc_actor has bad target " + (target.empty() ? "(null)" : target) +
                        " at " + q2_pos(e.origin));
    e.target.clear();
    m.corner = -1;
    return true;
  }
  m.corner = point;
  m.corner_wait = 0;
  e.angles.y = m.ideal_yaw = yaw_to(s.entities[size_t(point)].origin - e.origin);
  e.target.clear();  // a second use finds nothing
  return true;
}

void actor_arrive(Q2Server& s, Q2Monster& m) {
  const int at = m.corner;
  m.corner = -1;
  if (at < 0 || m.enemy != -1) return;
  const Q2Entity point = s.entities[size_t(at)];
  Q2Entity& e = s.entities[size_t(m.entity)];
  const std::string message = key(s, point, "message");
  if (!message.empty()) chat(s, 0xffffffffu, actor_name(s, m.entity) + ": " + message);
  const int flags = point.spawnflags;
  if ((flags & 1) != 0) {  // JUMP: off along the point's facing at `speed`, up at `height`
    const std::string speed = key(s, point, "speed"), height = key(s, point, "height");
    const float along = speed.empty() ? 200 : std::strtof(speed.c_str(), nullptr);
    const float up = height.empty() ? 200 : float(std::atoi(height.c_str()));
    const sh::Vec3f dir =
        sh::angle_vectors({0, point.angles.y == 0 ? 360 : point.angles.y, 0}).forward;
    m.velocity.x = dir.x * along;
    m.velocity.y = dir.y * along;
    if (m.on_ground) {
      m.on_ground = false;
      m.velocity.z = up;
      jump_sound(s, m.entity);
    }
  }
  const std::string pathtarget = key(s, point, "pathtarget");
  if ((flags & 2) != 0) {
    // SHOOT: nothing, and its pathtarget is not fired.
  } else if ((flags & 4) != 0) {  // ATTACK: what the pathtarget names
    const int foe = pathtarget.empty() ? -1 : pick(s, pathtarget);
    if (!pathtarget.empty() && foe < 0) q2_pick_missing(s, pathtarget);
    if (foe >= 0) {
      m.enemy = -2 - foe;
      m.last_sight = s.entities[size_t(foe)].origin;
      m.seen_at = s.level_time;
      if ((flags & 16) != 0) m.hold = true;  // HOLD: it stands its ground
    }
  } else if (!pathtarget.empty()) {
    Q2Entity& p = s.entities[size_t(at)];
    const std::string saved = p.target;
    p.target = pathtarget;
    map_use_targets(s, at, -1);
    s.entities[size_t(at)].target = saved;
  }
  if (point.target.empty()) {
    s.console.emplace_back("G_PickTarget called with NULL targetname");
  } else {
    m.corner = pick(s, point.target);
    if (m.corner < 0) q2_pick_missing(s, point.target);
  }
  if (m.corner >= 0 && m.enemy == -1)
    e.angles.y = m.ideal_yaw = yaw_to(s.entities[size_t(m.corner)].origin - e.origin);
}

bool actor_pain(Q2Server& s, Q2Monster& m, int attacker, bool waiting) {
  if (waiting || attacker < 0 || attacker >= sh::kMaxClients || !s.clients[attacker].active)
    return waiting;
  if (ge::core::next(s.rng) >= 0.4) return false;
  m.pain_at = s.level_time;
  Q2Entity& e = s.entities[size_t(m.entity)];
  m.ideal_yaw = yaw_to(unpack(s.players[attacker].move.origin) - e.origin);
  const bool flipoff = ge::core::next(s.rng) < 0.5;
  m.pose.cue_pain = true;
  m.pose.pain_run = flipoff ? SqSpan{39, 52} : SqSpan{234, 250};
  m.pain_hold = s.level_time + 0.1 * (m.pose.pain_run.last - m.pose.pain_run.first + 1);
  const int word = static_cast<int>(ge::core::next(s.rng) * 3);  // never the fourth
  chat(s, 1u << attacker, actor_name(s, m.entity) + ": " + kWords[word] + "!");
  return true;
}

}  // namespace shooters_edge::server
