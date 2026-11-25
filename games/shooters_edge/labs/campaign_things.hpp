// What a campaign program's things do (the `se-campaign` lab's `--things`): every entity the map's
// entity text spawned is watched by the class name the map gave it (the program may rename it: a
// pack's func_exploder answers as func_exploder). First the map runs on its own; then each weapon
// is picked up by the player and its attack held; then each name the map targets is used in turn
// (every entity carrying it has its `use` called with the player as `activator`, as a trigger's
// targets are); then each trigger is touched by the player standing in it. A level change the
// program asks for is set aside, so the look goes on to the end. A thing counts once for each kind
// of act: moved, turned, shown, hidden, animated (its frame stepped), removed, a sound started on
// it, an effect thrown within reach of it; and, while its name was used or it was touched, the
// things the program made, the player hurt and the player's gravity or speed changed. The
// program's own uses are watched as it runs: a thing that used another's targets FIRED, a thing
// used by another was SET OFF (a counter's ticks, the count reached). A new entity is the made of
// the thing that owns it or that it appeared beside, else of the class at work; a message to every
// client is TOLD, a level change asked LEVEL. Last, each class that showed nothing of its own is
// looked at again from beside its first three things: the player standing there (a trap's reach),
// its name used again (a field shown only to a player who sees it), the nearest monster touched
// into it (STEERED when the monster takes a new goal: a path), and a rocket fired at it when it
// takes damage (a wall or trigger that needs a hit). The program's faults are counted. A scripted
// look at every thing, not a playthrough: it says the program's things work on our server, not that
// the map can be got through (the census in `se-q1-campaign` and the campaign bot say that).
// Header-only, beside the labs (a lab's helper never goes into a library: the labs skill).
//
// ge:summary The campaign lab's things: every thing a map placed, used and touched, what it did
// ge:keywords lab things census use-targets triggers rotate movers effects mission-pack
// ge:keywords spawned_as class-names traps campaign-lab
// ge:skill progs shooter labs
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "campaign_movers.hpp"
#include "core/libm/libm.hpp"
#include "lab/lab.hpp"
#include "progs/file.hpp"
#include "progs/vm.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/server/rules_world.hpp"

namespace shooters_edge::labs {

// What the map's load left before the first frame: the level's looping sounds by place.
struct ThingsWatch {
  std::vector<ge::shooter::Vec3f> ambients;
};

namespace th {
namespace gl = ge::lab;
namespace pg = ge::progs;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

enum Act : uint8_t {
  moved,
  turned,
  shown,
  hidden,
  animated,
  removed,
  sounded,
  made,
  effects,
  hurt,
  pushed,
  fired,    // its targets used: another thing's use called from inside its own
  set_off,  // its use called by the program (another thing fired it), not by the lab
  steered,  // a monster touched into it took a new goal
  told,     // a message written to every client (a finale, a track) while it was at work
  level,    // a level change asked while it was at work
  kActs
};
inline const char* const kActNames[kActs] = {
    "moved",   "turned", "shown",  "hidden", "animated", "removed", "sounded", "made",
    "effects", "hurt",   "pushed", "fired",  "set off",  "steered", "told",    "level"};

// Who used whom, seen as the program runs: a function entered that is the `self` thing's own
// `use`, called from inside the program (depth 2 on), sets that thing off; the thing the calling
// frame began as (its `self` on entry) fired it.
struct UseWatch {
  int self_g = -1, use_f = -1;
  int32_t entity_bytes = 1, entityfields = 0;
  std::vector<int> entry_self;  // by depth: the `self` a frame began as
  std::vector<uint8_t> set_off, fired;
  std::vector<int> by;  // by thing: the last thing that set it off, 0 none
};

inline void use_enter(void* ctx, const pg::Vm& vm, int32_t fn, int depth) {
  auto& w = *static_cast<UseWatch*>(ctx);
  const int self = static_cast<int>(vm.globals[static_cast<size_t>(w.self_g)]) / w.entity_bytes;
  if (depth < 0) return;
  if (static_cast<size_t>(depth) >= w.entry_self.size())
    w.entry_self.resize(static_cast<size_t>(depth) + 1, 0);
  w.entry_self[static_cast<size_t>(depth)] = self;
  if (depth < 2 || self <= 0 || self >= vm.num_entities) return;
  const size_t at = static_cast<size_t>(self) * static_cast<size_t>(w.entityfields) +
                    static_cast<size_t>(w.use_f);
  if (static_cast<int32_t>(vm.entities[at]) != fn) return;
  const auto u = static_cast<size_t>(self);
  if (u >= w.set_off.size()) return;
  w.set_off[u] = 1;
  const int by = w.entry_self[static_cast<size_t>(depth) - 1];
  if (by > 0 && by != self && static_cast<size_t>(by) < w.fired.size()) {
    w.fired[static_cast<size_t>(by)] = 1;
    w.by[u] = by;
  }
}

struct Thing {
  int cls = -1;
  sh::Vec3f origin, angles;
  bool shown = false;
  int frame = 0;
  bool gone = false;
  bool did[kActs] = {};
};

struct Row {
  std::string name;
  int placed = 0;
  int acts[kActs] = {};
};

inline float dist(sh::Vec3f a, sh::Vec3f b) {
  const float x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
  return std::sqrt(x * x + y * y + z * z);
}

inline float turn(sh::Vec3f a, sh::Vec3f b) {
  const auto d = [](float x, float y) {
    float t = std::fmod(std::fabs(x - y), 360.0f);
    return std::min(t, 360.0f - t);
  };
  return std::max({d(a.x, b.x), d(a.y, b.y), d(a.z, b.z)});
}

inline void mark(Thing& t, std::vector<Row>& rows, Act a) {
  if (t.did[a]) return;
  t.did[a] = true;
  rows[static_cast<size_t>(t.cls)].acts[a]++;
}
}  // namespace th

// Before the first frame (right after `campaign_new_game`): the looping sounds the load started.
inline void things_loaded(ThingsWatch& w, server::Campaign& c) {
  w.ambients.clear();
  const server::QcServer* s = server::qc_of(c.server);
  if (s == nullptr) return;
  for (const server::NetEvent& ev : s->out.events)
    if (ev.kind == server::EventKind::ambient) w.ambients.push_back(ev.origin);
}

// Every thing of the map watched for `seconds` alone, then each targeted name used and each
// trigger touched in turn, `seconds` each; the table by class name and the totals as metrics.
inline void things_table(ge::lab::Lab& lab, const ThingsWatch& w, server::Campaign& c,
                         const ge::progs::Progs& progs, double seconds) {
  using namespace th;
  using namespace mv;
  if (!c.server) return;
  sv::QcServer& s = *sv::qc_of(c.server);
  // The server the things live on; should it be replaced all the same, the watch stops there.
  const auto same_map = [&] { return c.server && sv::qc_of(c.server) == &s; };
  // A frame of the map's own server, the level kept: a change the program asks for (an exit used,
  // a changelevel touched) is counted and set aside, so every thing is still looked at after it.
  int held_back = 0;
  const auto step = [&](const sh::Packet* packets, int count) {
    c.server.frame(packets, count);
    if (c.server->pending_map.empty()) return true;
    held_back++;
    c.server->pending_map.clear();
    return true;
  };
  Fields f;
  f.classname = field_of(progs, "classname");
  f.use = field_of(progs, "use");
  f.touch = field_of(progs, "touch");
  f.self = global_of(progs, "self");
  f.other = global_of(progs, "other");
  f.activator = global_of(progs, "activator");
  const int targetname = field_of(progs, "targetname");
  const int frame_f = field_of(progs, "frame");
  const int health_f = field_of(progs, "health");
  const int flags_f = field_of(progs, "flags");
  const int owner_f = field_of(progs, "owner");
  const int goal_f = field_of(progs, "goalentity");
  const int takedamage_f = field_of(progs, "takedamage");
  const int64_t faults_before = s.q.faults;
  // The player's health topped up to this every frame, so a hurt is seen as a drop below it (a
  // program may bring a health over its maximum down on its own, which would read as a hurt).
  constexpr float kTopUp = 100;
  // The program's own uses watched while the look runs (the hook goes when it ends).
  UseWatch uw;
  uw.self_g = f.self;
  uw.use_f = f.use;
  uw.entity_bytes = std::max(1, s.q.vm.entity_bytes);
  uw.entityfields = s.q.vm.entityfields;
  uw.set_off.assign(static_cast<size_t>(s.q.vm.max_entities), 0);
  uw.fired.assign(static_cast<size_t>(s.q.vm.max_entities), 0);
  uw.by.assign(static_cast<size_t>(s.q.vm.max_entities), 0);
  pg::TraceHooks hooks;
  hooks.ctx = &uw;
  hooks.enter = &use_enter;
  const bool watching = f.self >= 0 && f.use >= 0 && s.q.vm.trace == nullptr;
  if (watching) s.q.vm.trace = &hooks;
  struct Unhook {
    pg::Vm& vm;
    bool on;
    ~Unhook() {
      if (on) vm.trace = nullptr;
    }
  } unhook{s.q.vm, watching};

  std::vector<Row> rows;
  std::map<std::string, int> index;
  std::vector<Thing> things(s.sv.edicts.size());
  const auto live = [&](int e) {
    return e < s.sv.num_edicts && !s.sv.edicts[static_cast<size_t>(e)].free;
  };
  const auto frame_of = [&](int e) { return static_cast<int>(float_at(s, e, frame_f)); };
  for (int e = sh::kMaxClients + 1; e < s.sv.num_edicts; e++) {
    const auto u = static_cast<size_t>(e);
    if (u >= s.spawned_as.size() || s.spawned_as[u].empty()) continue;
    const auto [it, fresh] = index.emplace(s.spawned_as[u], static_cast<int>(rows.size()));
    if (fresh) rows.push_back({s.spawned_as[u]});
    rows[static_cast<size_t>(it->second)].placed++;
    if (!live(e)) continue;  // removed by its own spawn: counted placed, never seen
    Thing& t = things[u];
    t.cls = it->second;
    t.origin = s.sv.edicts[u].origin;
    t.angles = s.sv.edicts[u].angles;
    t.shown = sv::model_shown(s, e);
    t.frame = frame_of(e);
  }
  // The level's loops: each at the place of the thing that started it.
  for (const sh::Vec3f& at : w.ambients)
    for (Thing& t : things)
      if (t.cls >= 0 && dist(t.origin, at) < 1) {
        mark(t, rows, sounded);
        break;
      }
  std::vector<bool> was_live(things.size());
  for (size_t e = 0; e < things.size(); e++) was_live[e] = live(static_cast<int>(e));

  int64_t events_seen = s.out.events_total;
  int64_t faults_now = s.q.faults;
  // The thing a new entity is of: its owner when that is a thing of the map, else a thing it
  // appeared beside (a shot leaves from its shooter); -1 for neither.
  const auto maker = [&](size_t e) {
    const int o = owner_f < 0 ? 0
                              : static_cast<int>(word_at(s, static_cast<int>(e), owner_f)) /
                                    std::max(1, s.q.vm.entity_bytes);
    if (o > sh::kMaxClients && static_cast<size_t>(o) < things.size() &&
        things[static_cast<size_t>(o)].cls >= 0)
      return o;
    const sh::Vec3f at = s.sv.edicts[e].origin;
    for (size_t k = sh::kMaxClients + 1; k < things.size(); k++)
      if (k != e && things[k].cls >= 0 && live(static_cast<int>(k)) &&
          dist(s.sv.edicts[k].origin, at) < 32)
        return static_cast<int>(k);
    return -1;
  };
  std::vector<bool> was_shown(things.size());
  for (size_t e = 0; e < things.size(); e++)
    was_shown[e] = live(static_cast<int>(e)) && sv::model_shown(s, static_cast<int>(e));
  // One frame's look: what each thing did, and what the classes in `doing` caused.
  const auto look = [&](const std::set<int>& doing) {
    faults_now = s.q.faults;
    for (size_t e = sh::kMaxClients + 1; e < things.size(); e++) {
      Thing& t = things[e];
      const bool is = live(static_cast<int>(e));
      if (t.cls >= 0 && !t.gone) {
        if (!is) {
          t.gone = true;
          mark(t, rows, removed);
        } else {
          const auto& ed = s.sv.edicts[e];
          if (dist(ed.origin, t.origin) > 1) mark(t, rows, moved);
          if (turn(ed.angles, t.angles) > 1) mark(t, rows, turned);
          const bool sh_now = sv::model_shown(s, static_cast<int>(e));
          if (sh_now && !t.shown) mark(t, rows, shown);
          if (!sh_now && t.shown) mark(t, rows, hidden);
          if (frame_of(static_cast<int>(e)) != t.frame) mark(t, rows, animated);
        }
      }
      // Made: a new entity, or one kept hidden since the load brought out (a spawner's).
      const bool shown_now = is && sv::model_shown(s, static_cast<int>(e));
      if (t.cls < 0 && is && (!was_live[e] || (shown_now && !was_shown[e]))) {
        if (const int by = maker(e); by >= 0)
          rows[static_cast<size_t>(things[static_cast<size_t>(by)].cls)].acts[made]++;
        else
          for (int k : doing) rows[static_cast<size_t>(k)].acts[made]++;
      }
      was_live[e] = is;
      was_shown[e] = shown_now;
      if (t.cls >= 0 && e < uw.set_off.size()) {
        if (uw.set_off[e] != 0) mark(t, rows, set_off);
        if (uw.fired[e] != 0) mark(t, rows, th::fired);
      }
    }
    std::fill(uw.set_off.begin(), uw.set_off.end(), 0);
    std::fill(uw.fired.begin(), uw.fired.end(), 0);
    for (int i = 0; i < s.sv.sound_count; i++) {
      const auto ev = static_cast<size_t>(s.sv.sounds[i].ent);
      if (ev < things.size() && things[ev].cls >= 0) mark(things[ev], rows, sounded);
    }
    // The events since the last look (the list may keep a frame's earlier ones).
    const int64_t fresh = std::min<int64_t>(s.out.events_total - events_seen,
                                            static_cast<int64_t>(s.out.events.size()));
    events_seen = s.out.events_total;
    for (size_t i = s.out.events.size() - static_cast<size_t>(fresh); i < s.out.events.size();
         i++) {
      const sv::NetEvent& ev = s.out.events[i];
      if (ev.kind != sv::EventKind::particle && ev.kind != sv::EventKind::multicast &&
          ev.kind != sv::EventKind::point_effect && ev.kind != sv::EventKind::trail_effect)
        continue;
      // An effect is a thing's when it is thrown within reach of its box.
      constexpr float kReach = 96;
      for (size_t e = sh::kMaxClients + 1; e < things.size(); e++) {
        Thing& t = things[e];
        if (t.cls < 0 || t.gone || t.did[effects]) continue;
        const auto& ed = s.sv.edicts[e];
        const sh::Vec3f o = ev.origin;
        if (o.x >= ed.absmin.x - kReach && o.x <= ed.absmax.x + kReach &&
            o.y >= ed.absmin.y - kReach && o.y <= ed.absmax.y + kReach &&
            o.z >= ed.absmin.z - kReach && o.z <= ed.absmax.z + kReach)
          mark(t, rows, effects);
      }
    }
  };
  const int frames = std::max(1, static_cast<int>(seconds / sv::kCampaignStep));
  // `seconds` of frames, the player kept alive and his hurt and push counted for `doing`.
  const sh::Vec3f start = s.sv.edicts[1].origin;
  // The player brought back where he joined, still, so one window's fall or teleport is not the
  // next's; and back to life should a trap have killed him (the standard fields a living player
  // has: walking, solid, taking damage), so the looks after it still see him move and get hurt.
  const int deadflag_f = field_of(progs, "deadflag");
  const int movetype_f = field_of(progs, "movetype");
  const int solid_f = field_of(progs, "solid");
  int revived = 0;
  const auto set_f = [&](int ofs, float v) {
    if (ofs >= 0) *pg::entity_field(s.q.vm, 1, ofs) = pg::as_word(v);
  };
  const auto home = [&] {
    s.sv.edicts[1].origin = start;
    s.sv.edicts[1].velocity = {0, 0, 0};
    if (deadflag_f < 0 || float_at(s, 1, deadflag_f) <= 0) return;
    revived++;
    set_f(deadflag_f, 0);
    set_f(movetype_f, static_cast<float>(sh::kMovetypeWalk));
    set_f(solid_f, static_cast<float>(sh::kSolidSlidebox));
    set_f(takedamage_f, 2);  // takes damage and is aimed at
    set_f(health_f, kTopUp);
  };
  // Where the last window's messages to every client and level changes ended (a use's own come
  // before its window runs, so each window counts from the end of the one before).
  size_t told_from = s.out.all.size();
  int held_from = held_back;
  bool stop_early = false;  // the window ends at the first hurt or push (a trap that kills)
  float last_push = 0;      // the last window's largest change of the player's speed, units/s
  bool last_hit = false;    // whether the player was hurt in it
  const auto run = [&](const std::set<int>& doing) {
    const sh::Vec3f v0 = s.sv.edicts[1].velocity;
    const float g0 = s.sv.edicts[1].gravity;
    const size_t all0 = told_from;
    const int held0 = held_from;
    bool hit = false, push = false;
    last_push = 0;
    for (int i = 0; i < frames && same_map(); i++) {
      if (health_f >= 0) *pg::entity_field(s.q.vm, 1, health_f) = pg::as_word(kTopUp);
      if (!step(nullptr, 0) || !same_map()) break;
      look(doing);
      if (health_f >= 0 && float_at(s, 1, health_f) < kTopUp) hit = true;
      const auto& p = s.sv.edicts[1];
      last_push = std::max(last_push, dist(p.velocity, v0));
      if (p.gravity != g0 || dist(p.velocity, v0) > 200) push = true;
      if (stop_early && (hit || push)) break;
    }
    const bool said = s.out.all.size() > all0, ended = held_back > held0;
    for (int k : doing) {
      if (hit) rows[static_cast<size_t>(k)].acts[hurt]++;
      if (push) rows[static_cast<size_t>(k)].acts[pushed]++;
      if (said) rows[static_cast<size_t>(k)].acts[told]++;
      if (ended) rows[static_cast<size_t>(k)].acts[level]++;
    }
    told_from = s.out.all.size();
    held_from = held_back;
    last_hit = hit;
  };
  // The player is not a god here: a trap's hurt is seen (his health topped up to 100 each frame).
  if (flags_f >= 0) {
    const int fl = static_cast<int>(float_at(s, 1, flags_f)) & ~64;
    *pg::entity_field(s.q.vm, 1, flags_f) = pg::as_word(static_cast<float>(fl));
  }
  run({});
  // Each weapon picked up by the player where he joined, then its attack held: the shots it made,
  // the sounds he played, the effects thrown and the ammunition spent.
  gl::LabTable fired{"each weapon picked up and its attack held (a scripted look)",
                     {"entity", "class (the map's)", "weapon then", "shots made", "sounds",
                      "effects", "ammo spent"},
                     {}};
  const int weapon_f = field_of(progs, "weapon");
  const int ammo_f = field_of(progs, "currentammo");
  int fired_any = 0;
  for (size_t e = sh::kMaxClients + 1; e < things.size() && same_map(); e++) {
    const Thing& t = things[e];
    const int ent = static_cast<int>(e);
    if (t.cls < 0 || !live(ent) || word_at(s, ent, f.touch) <= 0 ||
        rows[static_cast<size_t>(t.cls)].name.rfind("weapon_", 0) != 0)
      continue;
    home();
    if (health_f >= 0) *pg::entity_field(s.q.vm, 1, health_f) = pg::as_word(kTopUp);
    call_as(s, f, word_at(s, ent, f.touch), ent);
    const int weapon = static_cast<int>(float_at(s, 1, weapon_f));
    const float ammo0 = float_at(s, 1, ammo_f);
    std::vector<bool> before(static_cast<size_t>(s.sv.num_edicts));
    for (int k = 0; k < s.sv.num_edicts; k++) before[static_cast<size_t>(k)] = live(k);
    int shots = 0, sounds = 0, thrown = 0;
    int64_t seen = s.out.events_total;
    sh::Packet p;
    for (int i = 0; i < frames * 3 && same_map(); i++) {
      p.sequence = static_cast<uint32_t>(i + 1);
      for (sh::MoveCmd& cmd : p.cmds) {
        cmd.msec = 14;
        cmd.buttons = (i & 1) ? 1 : 0;
      }
      if (health_f >= 0) *pg::entity_field(s.q.vm, 1, health_f) = pg::as_word(kTopUp);
      if (!step(&p, 1) || !same_map()) break;
      for (int k = sh::kMaxClients + 1; k < s.sv.num_edicts; k++) {
        const auto u = static_cast<size_t>(k);
        const bool now = live(k), was = u < before.size() && before[u];
        if (now && !was) shots++;
        if (u >= before.size()) before.resize(u + 1);
        before[u] = now;
      }
      for (int i2 = 0; i2 < s.sv.sound_count; i2++) sounds += s.sv.sounds[i2].ent == 1 ? 1 : 0;
      const int64_t fresh =
          std::min<int64_t>(s.out.events_total - seen, static_cast<int64_t>(s.out.events.size()));
      seen = s.out.events_total;
      for (size_t i2 = s.out.events.size() - static_cast<size_t>(fresh); i2 < s.out.events.size();
           i2++) {
        const sv::EventKind k = s.out.events[i2].kind;
        thrown += k == sv::EventKind::multicast || k == sv::EventKind::particle ||
                          k == sv::EventKind::point_effect || k == sv::EventKind::trail_effect
                      ? 1
                      : 0;
      }
    }
    if (!same_map()) break;
    fired_any += shots + thrown > 0 ? 1 : 0;
    fired.rows.push_back({std::to_string(ent), rows[static_cast<size_t>(t.cls)].name,
                          std::to_string(weapon), std::to_string(shots), std::to_string(sounds),
                          std::to_string(thrown),
                          ge::core::num::to_fixed(ammo0 - float_at(s, 1, ammo_f), 0)});
  }
  if (!fired.rows.empty()) lab.table(fired, gl::Det::measured);
  lab.metric("things.weapons_fired", fired_any, "count");

  // Each name the map targets, used.
  std::map<std::string, std::vector<int>> named;
  for (size_t e = sh::kMaxClients + 1; e < things.size(); e++)
    if (things[e].cls >= 0 && live(static_cast<int>(e)))
      if (std::string n = string_at(s, static_cast<int>(e), targetname); !n.empty())
        named[n].push_back(static_cast<int>(e));
  int used = 0, touched = 0;
  told_from = s.out.all.size();
  held_from = held_back;
  for (const auto& [name, ents] : named) {
    if (!same_map()) break;
    std::set<int> doing;
    home();
    for (int e : ents) {
      if (!live(e) || word_at(s, e, f.use) <= 0) continue;
      doing.insert(things[static_cast<size_t>(e)].cls);
      call_as(s, f, word_at(s, e, f.use), e);
    }
    if (doing.empty()) continue;
    used++;
    look(doing);
    run(doing);
  }
  // Each trigger touched by the player standing in its middle.
  for (size_t e = sh::kMaxClients + 1; e < things.size() && same_map(); e++) {
    const Thing& t = things[e];
    if (t.cls < 0 || !live(static_cast<int>(e)) || word_at(s, static_cast<int>(e), f.touch) <= 0 ||
        rows[static_cast<size_t>(t.cls)].name.rfind("trigger_", 0) != 0)
      continue;
    const auto& ed = s.sv.edicts[e];
    auto& p = s.sv.edicts[1];
    p.origin = {0.5f * (ed.absmin.x + ed.absmax.x), 0.5f * (ed.absmin.y + ed.absmax.y),
                0.5f * (ed.absmin.z + ed.absmax.z)};
    p.velocity = {0, 0, 0};
    touched++;
    if (health_f >= 0) *pg::entity_field(s.q.vm, 1, health_f) = pg::as_word(kTopUp);
    const std::set<int> doing{t.cls};
    call_as(s, f, word_at(s, static_cast<int>(e), f.touch), static_cast<int>(e));
    // A push is the touch's own change to the player's speed (a still player, so any change).
    if (dist(s.sv.edicts[1].velocity, {0, 0, 0}) > 1) mark(things[e], rows, pushed);
    look(doing);
    run(doing);
  }

  // Classes that showed nothing of their own yet: each of their first few things looked at again
  // with the player beside it (a trap's reach, a field shown only to a player who sees it), used
  // again from there, touched by the nearest monster (a path a monster takes), and shot.
  const auto own = [&](const Row& r) {
    for (int a : {moved, turned, shown, hidden, animated, removed, sounded, effects, th::fired,
                  told, level, steered})
      if (r.acts[a] > 0) return true;
    return false;
  };
  const sh::Vec3f pmins{-16, -16, -24}, pmaxs{16, 16, 32};
  // A thing's middle by its own box about its origin (linked or not: a point thing that is not
  // solid has no box in the world).
  const auto middle = [](const sh::Edict& ed) {
    return sh::Vec3f{ed.origin.x + 0.5f * (ed.mins.x + ed.maxs.x),
                     ed.origin.y + 0.5f * (ed.mins.y + ed.maxs.y),
                     ed.origin.z + 0.5f * (ed.mins.z + ed.maxs.z)};
  };
  // A spot about `reach` units from the thing's box where the player stands free and sees its
  // middle; false when none of eight ways has one.
  int in_solid = 0, out_of_sight = 0;  // the spots tried and refused, for the table's note
  const auto beside = [&](int e, float reach, sh::Vec3f& at) {
    const auto& ed = s.sv.edicts[static_cast<size_t>(e)];
    const sh::Vec3f mid = middle(ed);
    const float half = 0.5f * std::max(ed.maxs.x - ed.mins.x, ed.maxs.y - ed.mins.y);
    for (float up : {0.0f, 32.0f, -32.0f, 64.0f})  // heights about its middle
      for (float r : {reach, reach * 0.5f, reach * 0.25f, reach * 1.5f})
        for (int k = 0; k < 8; k++) {
          const float a = static_cast<float>(k) * 0.785398f;
          const sh::Vec3f p{mid.x + static_cast<float>(ge::core::libm::cos(a)) * (r + half),
                            mid.y + static_cast<float>(ge::core::libm::sin(a)) * (r + half),
                            mid.z + up};
          // Free room: a short sweep of the player's box that does not start in solid.
          const sh::HullTrace room =
              sh::trace_move(s.sv, p, pmins, pmaxs, {p.x, p.y, p.z + 1}, sh::kTraceNoMonsters, 1);
          if (room.startsolid || room.allsolid) {
            in_solid++;
            continue;
          }
          const sh::Vec3f eye{p.x, p.y, p.z + 22};
          const sh::HullTrace sight =
              sh::trace_move(s.sv, eye, {0, 0, 0}, {0, 0, 0}, mid, sh::kTraceNoMonsters, 1);
          const sh::Vec3f h = sight.endpos;
          const sh::Vec3f lo = ed.origin + ed.mins, hi = ed.origin + ed.maxs;
          const bool on_it = h.x >= lo.x - 16 && h.x <= hi.x + 16 && h.y >= lo.y - 16 &&
                             h.y <= hi.y + 16 && h.z >= lo.z - 16 && h.z <= hi.z + 16;
          if (sight.fraction < 0.98f && !on_it) {
            out_of_sight++;
            continue;
          }
          at = p;
          return true;
        }
    return false;
  };
  const auto monster_near = [&](int e) {
    int best = -1;
    float d = 1e9f;
    for (int m = sh::kMaxClients + 1; m < s.sv.num_edicts; m++) {
      if (!live(m) || (static_cast<int>(float_at(s, m, flags_f)) & 32) == 0 ||
          float_at(s, m, health_f) <= 0)
        continue;
      const float dm = dist(s.sv.edicts[static_cast<size_t>(m)].origin,
                            s.sv.edicts[static_cast<size_t>(e)].origin);
      if (dm < d) d = dm, best = m;
    }
    return best;
  };
  int besides = 0;
  constexpr int kBesideEach = 3;
  gl::LabTable near{"each idle class's first things looked at from beside (a scripted look)",
                    {"entity", "class (the map's)", "player stood", "most push (units/s)", "hurt",
                     "used", "monster's new goal", "shot"},
                    {}};
  std::vector<int> seen(rows.size(), 0);
  for (size_t e = sh::kMaxClients + 1; e < things.size() && same_map(); e++) {
    const Thing& t = things[e];
    const int ent = static_cast<int>(e);
    if (t.cls < 0 || !live(ent) || own(rows[static_cast<size_t>(t.cls)]) ||
        seen[static_cast<size_t>(t.cls)] >= kBesideEach)
      continue;
    seen[static_cast<size_t>(t.cls)]++;
    const std::set<int> doing{t.cls};
    sh::Vec3f at;
    const bool stand = beside(ent, 160, at);
    std::string push_seen = "-", hurt_seen = "-", used_how = "-", goal_seen = "-", shot_seen = "-";
    if (stand) {
      besides++;
      s.sv.edicts[1].origin = at;
      s.sv.edicts[1].velocity = {0, 0, 0};
      stop_early = true;
      run(doing);
      stop_early = false;
      push_seen = ge::core::num::to_fixed(last_push, 0);
      hurt_seen = last_hit ? "yes" : "no";
      // Used again from here: by the thing that set it off in the look so far when there is one
      // (a counter's value is what a field or an oncount answers to), else by its own name.
      const int by = uw.by[e];
      const int user = by > 0 && live(by) && word_at(s, by, f.use) > 0 ? by : ent;
      if (word_at(s, user, f.use) > 0 && live(ent)) {
        s.sv.edicts[1].origin = at;
        s.sv.edicts[1].velocity = {0, 0, 0};
        call_as(s, f, word_at(s, user, f.use), user);
        look(doing);
        run(doing);
        used_how = user == ent ? "itself" : "its user " + std::to_string(user);
      }
    }
    if (const int m = monster_near(ent); m > 0 && live(ent) && word_at(s, ent, f.touch) > 0) {
      const int32_t goal0 = word_at(s, m, goal_f);
      se::enter_qc(s.q, s.sv);
      s.q.vm.globals[static_cast<size_t>(f.self)] =
          static_cast<uint32_t>(pg::entity_value(s.q.vm, ent));
      s.q.vm.globals[static_cast<size_t>(f.other)] =
          static_cast<uint32_t>(pg::entity_value(s.q.vm, m));
      se::run_qc(s.q, word_at(s, ent, f.touch));
      se::leave_qc(s.q);
      const bool took = goal_f >= 0 && word_at(s, m, goal_f) != goal0;
      if (took) mark(things[e], rows, steered);
      goal_seen = took ? "yes" : "no";
      look(doing);
      run(doing);  // past a monster's wait before another path may take it
    }
    // Shot: a thing that takes damage, a rocket at its middle from beside it (every weapon given
    // the way a single player's cheat gives them, then the rocket launcher chosen).
    if (stand && live(ent) && takedamage_f >= 0 && float_at(s, ent, takedamage_f) > 0) {
      const auto& ed = s.sv.edicts[e];
      const sh::Vec3f mid = middle(ed);
      s.sv.edicts[1].origin = at;
      s.sv.edicts[1].velocity = {0, 0, 0};
      const float dx = mid.x - at.x, dy = mid.y - at.y, dz = mid.z - (at.z + 22);
      const sh::Vec3f aim{
          -static_cast<float>(ge::core::libm::atan2(dz, std::sqrt(dx * dx + dy * dy))) * 57.29578f,
          static_cast<float>(ge::core::libm::atan2(dy, dx)) * 57.29578f, 0};
      sh::Packet p;
      for (int i = 0; i < frames && same_map() && live(ent); i++) {
        p.sequence = static_cast<uint32_t>(i + 1);
        for (sh::MoveCmd& cmd : p.cmds) {
          cmd.msec = 14;
          cmd.angles = aim;
          cmd.impulse = static_cast<uint8_t>(i == 0 ? 9 : i == 2 ? 7 : 0);
          cmd.buttons = i > 4 && (i & 1) ? 1 : 0;
        }
        if (health_f >= 0) *pg::entity_field(s.q.vm, 1, health_f) = pg::as_word(kTopUp);
        s.sv.edicts[1].origin = at;
        if (!step(&p, 1) || !same_map()) break;
        look(doing);
      }
      shot_seen = live(ent) ? "stood" : "gone";
    }
    near.rows.push_back({std::to_string(ent), rows[static_cast<size_t>(t.cls)].name,
                         stand ? "yes" : "no spot", push_seen, hurt_seen, used_how, goal_seen,
                         shot_seen});
    home();
  }
  if (!near.rows.empty()) {
    lab.table(near, gl::Det::measured);
    lab.note("spots beside refused: " + std::to_string(in_solid) + " in solid, " +
             std::to_string(out_of_sight) + " out of sight of the thing's middle");
  }

  std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.name < b.name; });
  std::vector<std::string> head = {"class (the map's)", "placed"};
  for (const char* n : kActNames) head.emplace_back(n);
  gl::LabTable t{
      "every thing the map placed, by what it did (a scripted look, not a playthrough)", head, {}};
  int idle = 0;
  for (const Row& r : rows) {
    std::vector<std::string> cells = {r.name, std::to_string(r.placed)};
    int any = 0;
    for (int a : r.acts) {
      cells.push_back(std::to_string(a));
      any += a;
    }
    idle += any == 0 ? 1 : 0;
    t.rows.push_back(cells);
  }
  lab.table(t, gl::Det::measured);
  lab.metric("things.classes", static_cast<double>(rows.size()), "count");
  lab.metric("things.classes_idle", idle, "count");
  lab.metric("things.names_used", used, "count");
  lab.metric("things.triggers_touched", touched, "count");
  lab.metric("things.looked_at_beside", besides, "count");
  lab.metric("things.player_revived", revived, "count");
  lab.metric("things.faults", static_cast<double>(faults_now - faults_before), "count");
  lab.metric("things.level_changes_held", held_back, "count");
  if (!same_map()) {
    lab.note("the level ended while its things were used: the watch stopped there");
  } else if (s.q.faults > faults_before) {
    lab.note("the program's last fault: " + s.q.last_fault);
  }
}

}  // namespace shooters_edge::labs
