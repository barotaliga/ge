// What a campaign program's things do (the `se-campaign` lab's `--things`): every entity the map's
// entity text spawned is watched by the class name the map gave it (the program may rename it: a
// pack's func_exploder answers as func_exploder), first while the map runs on its own, then while
// each name the map targets is used in turn (every entity carrying it has its `use` called with
// the player as `activator`, as a trigger's targets are), and each trigger is touched by the
// player standing in it. A thing counts once for each kind of act: moved, turned, shown, hidden,
// animated (its frame stepped), removed, a sound started on it, an effect thrown within reach of
// it, and, while its name was used or it was touched, the things the program made, the player
// hurt and the player's gravity or speed changed. The program's faults are counted. A scripted look
// at every thing, not a playthrough: it says the program's things work on our server, not that the
// map can be got through (the census in `se-q1-campaign` and the campaign bot say that).
// Header-only, beside the labs (a lab's helper never goes into a library: the labs skill).
//
// ge:summary The campaign lab's things: every thing a map placed, used and touched, what it did
// ge:keywords lab things census use-targets triggers rotate movers effects mission-pack
// ge:keywords spawned_as class-names traps campaign-lab
// ge:skill progs shooter labs
#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "campaign_movers.hpp"
#include "lab/lab.hpp"
#include "progs/file.hpp"
#include "progs/vm.hpp"
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

enum Act {
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
  kActs
};
inline const char* const kActNames[kActs] = {"moved",    "turned",  "shown",   "hidden",
                                             "animated", "removed", "sounded", "made",
                                             "effects",  "hurt",    "pushed"};

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
  const int64_t faults_before = s.q.faults;

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
  // One frame's look: what each thing did, and what the classes in `doing` caused.
  const auto look = [&](const std::set<int>& doing) {
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
      if (is && !was_live[e] && t.cls < 0)
        for (int k : doing) rows[static_cast<size_t>(k)].acts[made]++;
      was_live[e] = is;
    }
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
  std::string error;
  const int frames = std::max(1, static_cast<int>(seconds / sv::kCampaignStep));
  // `seconds` of frames, the player kept alive and his hurt and push counted for `doing`.
  const sh::Vec3f start = s.sv.edicts[1].origin;
  // The player back where he joined, still, so one window's fall or teleport is not the next's.
  const auto home = [&] {
    s.sv.edicts[1].origin = start;
    s.sv.edicts[1].velocity = {0, 0, 0};
  };
  const auto run = [&](const std::set<int>& doing) {
    const sh::Vec3f v0 = s.sv.edicts[1].velocity;
    const float g0 = s.sv.edicts[1].gravity;
    bool hit = false, push = false;
    for (int i = 0; i < frames && c.server; i++) {
      if (health_f >= 0) *pg::entity_field(s.q.vm, 1, health_f) = pg::as_word(100.0f);
      if (!sv::campaign_frame(c, nullptr, 0, error)) break;
      look(doing);
      if (health_f >= 0 && float_at(s, 1, health_f) < 100) hit = true;
      const auto& p = s.sv.edicts[1];
      if (p.gravity != g0 || dist(p.velocity, v0) > 200) push = true;
    }
    for (int k : doing) {
      if (hit) rows[static_cast<size_t>(k)].acts[hurt]++;
      if (push) rows[static_cast<size_t>(k)].acts[pushed]++;
    }
  };
  // The player is not a god here: a trap's hurt is seen (his health topped up to 100 each frame).
  if (flags_f >= 0) {
    const int fl = static_cast<int>(float_at(s, 1, flags_f)) & ~64;
    *pg::entity_field(s.q.vm, 1, flags_f) = pg::as_word(static_cast<float>(fl));
  }
  run({});
  // Each name the map targets, used.
  std::map<std::string, std::vector<int>> named;
  for (size_t e = sh::kMaxClients + 1; e < things.size(); e++)
    if (things[e].cls >= 0 && live(static_cast<int>(e)))
      if (std::string n = string_at(s, static_cast<int>(e), targetname); !n.empty())
        named[n].push_back(static_cast<int>(e));
  int used = 0, touched = 0;
  for (const auto& [name, ents] : named) {
    if (!c.server) break;
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
  for (size_t e = sh::kMaxClients + 1; e < things.size() && c.server; e++) {
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
    if (health_f >= 0) *pg::entity_field(s.q.vm, 1, health_f) = pg::as_word(100.0f);
    const std::set<int> doing{t.cls};
    call_as(s, f, word_at(s, static_cast<int>(e), f.touch), static_cast<int>(e));
    look(doing);
    run(doing);
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
  lab.metric("things.faults", static_cast<double>(s.q.faults - faults_before), "count");
  if (s.q.faults > faults_before) lab.note("the program's last fault: " + s.q.last_fault);
}

}  // namespace shooters_edge::labs
