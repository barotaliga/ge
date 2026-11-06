// What a campaign program's doors and plats play (the `se-campaign` lab's `--movers`): on a map
// the program has spawned, every door, secret door and plat is set off in turn (its `use`, or
// for one with none the touch of a field it owns; a mover is any pusher on a brush model but a
// wall or a train, whatever the program calls it, as the player), the frames run until it has
// gone and come back, and every sound the program starts on it is kept in order. A key door is
// also touched by the player without its key, and what it plays then is kept apart. The table
// is one row a mover; the summary folds them by kind, `sounds` and the world's type, so two
// programs (the pack's own and ours) can be held to the same answers.
// Header-only, beside the labs (a lab's helper never goes into a library: the labs skill).
//
// ge:summary The campaign lab's mover sounds: each door and plat set off, the sounds it plays
// ge:keywords lab movers door-sounds plat-sounds sounds-key worldtype locked-sound campaign-lab
// ge:skill progs shooter labs
#pragma once

#include <map>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "progs/file.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "shooters_edge/sim/qc_program.hpp"

namespace shooters_edge::labs {

namespace mv {
namespace gl = ge::lab;
namespace pg = ge::progs;
namespace sv = shooters_edge::server;
namespace se = shooters_edge::sim;

struct Fields {
  int classname = -1, sounds = -1, spawnflags = -1, use = -1, touch = -1, owner = -1,
      worldtype = -1;
  int self = -1, other = -1, activator = -1;
};

inline int field_of(const pg::Progs& p, const char* name) {
  const pg::Def* d = pg::find_field(p, name);
  return d ? d->offset : -1;
}

inline int global_of(const pg::Progs& p, const char* name) {
  const pg::Def* d = pg::find_global(p, name);
  return d ? d->offset : -1;
}

inline float float_at(sv::QcServer& s, int e, int ofs) {
  return ofs < 0 ? -1 : pg::as_float(*pg::entity_field(s.q.vm, e, ofs));
}

inline int32_t word_at(sv::QcServer& s, int e, int ofs) {
  return ofs < 0 ? 0 : static_cast<int32_t>(*pg::entity_field(s.q.vm, e, ofs));
}

inline std::string string_at(sv::QcServer& s, int e, int ofs) {
  if (ofs < 0) return "";
  const char* t = pg::vm_string(s.q.vm, word_at(s, e, ofs));
  return t ? t : "";
}

// `fn` called as `self` with the player as `other` and `activator`.
inline void call_as(sv::QcServer& s, const Fields& f, int32_t fn, int self) {
  if (fn <= 0) return;
  pg::Vm& vm = s.q.vm;
  se::enter_qc(s.q, s.sv);
  if (f.self >= 0)
    vm.globals[static_cast<size_t>(f.self)] = static_cast<uint32_t>(pg::entity_value(vm, self));
  if (f.other >= 0)
    vm.globals[static_cast<size_t>(f.other)] = static_cast<uint32_t>(pg::entity_value(vm, 1));
  if (f.activator >= 0)
    vm.globals[static_cast<size_t>(f.activator)] = static_cast<uint32_t>(pg::entity_value(vm, 1));
  se::run_qc(s.q, fn);
  se::leave_qc(s.q);
}

// The samples started on any of `ents` since the frame began, appended without repeats in a row.
inline void take_sounds(const sv::QcServer& s, const std::vector<int>& ents,
                        std::vector<std::string>& out, std::vector<double>* when = nullptr,
                        double t = 0) {
  for (int i = 0; i < s.sv.sound_count; i++) {
    const auto& ev = s.sv.sounds[i];
    bool ours = false;
    for (int e : ents) ours = ours || ev.ent == e;
    if (!ours) continue;
    if (out.empty() || out.back() != ev.sample) {
      out.emplace_back(ev.sample);
      if (when) when->push_back(t);
    }
  }
}

// The seconds each sample started at, after the mover was set off ("0 0.3 1.3").
inline std::string times(const std::vector<double>& v) {
  std::string t;
  for (double x : v) t += (t.empty() ? "" : " ") + ge::core::num::to_fixed(x, 2);
  return t;
}

inline std::string joined(const std::vector<std::string>& v) {
  std::string t;
  for (const std::string& x : v) t += (t.empty() ? "" : " > ") + x;
  return t.empty() ? "(none)" : t;
}
}  // namespace mv

// Every mover of the map set off in turn, `seconds` each, its sounds tabled and folded.
inline void movers_table(ge::lab::Lab& lab, server::Campaign& c, const ge::progs::Progs& progs,
                         double seconds) {
  using namespace mv;
  if (!c.server) return;
  sv::QcServer& s = *sv::qc_of(c.server);
  Fields f;
  f.classname = field_of(progs, "classname");
  f.sounds = field_of(progs, "sounds");
  f.spawnflags = field_of(progs, "spawnflags");
  f.use = field_of(progs, "use");
  f.touch = field_of(progs, "touch");
  f.owner = field_of(progs, "owner");
  f.worldtype = field_of(progs, "worldtype");
  f.self = global_of(progs, "self");
  f.other = global_of(progs, "other");
  f.activator = global_of(progs, "activator");
  const int frames = static_cast<int>(seconds / sv::kCampaignStep);
  const std::string world =
      f.worldtype < 0 ? "-" : std::to_string(static_cast<int>(float_at(s, 0, f.worldtype)));
  gl::LabTable rows{"every door and plat set off in turn, and what it played",
                    {"entity", "kind", "flags", "sounds", "key", "set off by", "played", "at (s)",
                     "where (from its start)", "back", "locked"},
                    {}};
  std::map<std::string, std::map<std::string, int>> folded;
  std::vector<int> movers;
  for (int e = 1; e < s.sv.num_edicts; e++) {
    if (s.sv.edicts[static_cast<size_t>(e)].free) continue;
    const std::string cn = string_at(s, e, f.classname);
    // A mover is a pusher on a brush of its own; the program names its kind (`door`, `plat`).
    const auto& ed = s.sv.edicts[static_cast<size_t>(e)];
    if (ed.movetype == ge::shooter::kMovetypePush && ed.solid == ge::shooter::kSolidBsp &&
        cn != "func_wall" && cn.find("train") == std::string::npos)
      movers.push_back(e);
  }
  std::string error;
  int silent = 0;
  for (int e : movers) {
    const std::string cn = string_at(s, e, f.classname);
    const int snd = static_cast<int>(float_at(s, e, f.sounds));
    const int key = static_cast<int>(float_at(s, e, f.spawnflags)) & 24;
    // Its fields: the entities it owns that answer a touch.
    std::vector<int> owned;
    for (int o = 1; o < s.sv.num_edicts; o++)
      if (!s.sv.edicts[static_cast<size_t>(o)].free && o != e &&
          pg::entity_number(s.q.vm, word_at(s, o, f.owner)) == e && word_at(s, o, f.touch) > 0)
        owned.push_back(o);
    std::vector<int> ents = owned;
    ents.push_back(e);
    std::vector<std::string> locked, played;
    std::vector<double> when;
    std::string moved;  // where it stood, from its start, at each sample after the first
    size_t seen = 1;
    if (key) {
      call_as(s, f, word_at(s, e, f.touch), e);
      for (int o : owned) call_as(s, f, word_at(s, o, f.touch), o);
      take_sounds(s, ents, locked);
      for (int i = 0; i < 3 && c.server; i++) sv::campaign_frame(c, nullptr, 0, error);
    }
    const auto origin = s.sv.edicts[static_cast<size_t>(e)].origin;
    const auto at_start = [&] {
      const auto now = s.sv.edicts[static_cast<size_t>(e)].origin;
      return now.x == origin.x && now.y == origin.y && now.z == origin.z;
    };
    std::string how = "use";
    if (word_at(s, e, f.use) > 0) {
      call_as(s, f, word_at(s, e, f.use), e);
    } else if (!owned.empty()) {
      how = "its field";
      call_as(s, f, word_at(s, owned[0], f.touch), owned[0]);
    } else {
      how = "(nothing)";
    }
    take_sounds(s, ents, played, &when, 0);
    for (int i = 0; i < frames && c.server; i++) {
      if (!sv::campaign_frame(c, nullptr, 0, error)) break;
      take_sounds(s, ents, played, &when, (i + 1) * sv::kCampaignStep);
      for (; seen < when.size(); seen++) {
        const auto o = s.sv.edicts[static_cast<size_t>(e)].origin;
        moved += (moved.empty() ? "" : " ") + std::string("(") +
                 std::to_string(static_cast<int>(o.x - origin.x)) + " " +
                 std::to_string(static_cast<int>(o.y - origin.y)) + " " +
                 std::to_string(static_cast<int>(o.z - origin.z)) + ")";
      }
    }
    if (!c.server) break;
    // Back where it started? One that stays away is used once more, to see whether that returns it.
    std::string back = at_start() ? "yes" : "no";
    if (back == "no" && word_at(s, e, f.use) > 0) {
      call_as(s, f, word_at(s, e, f.use), e);
      for (int i = 0; i < frames && c.server; i++)
        if (!sv::campaign_frame(c, nullptr, 0, error)) break;
      if (!c.server) break;
      back = at_start() ? "no; a second use returns it" : "no; nor after a second use";
    }
    silent += played.empty();
    rows.rows.push_back({std::to_string(e), cn,
                         std::to_string(static_cast<int>(float_at(s, e, f.spawnflags))),
                         std::to_string(snd),
                         key == 16  ? "silver"
                         : key == 8 ? "gold"
                         : key      ? "both"
                                    : "",
                         how, joined(played), times(when), moved, back, key ? joined(locked) : ""});
    if (static_cast<int>(float_at(s, e, f.spawnflags)) & 32)
      folded[cn + " flag 32"]["back: " + back]++;
    folded[cn + " sounds " + std::to_string(snd)][joined(played)]++;
    if (key)
      folded[std::string(key == 16 ? "silver" : "gold") + " door locked, world " + world]
            [joined(locked)]++;
  }
  lab.table(rows, gl::Det::measured);
  gl::LabTable sum{"the movers' sounds by kind and `sounds` (world type " + world + ")",
                   {"kind", "played", "movers"},
                   {}};
  for (const auto& [k, by] : folded)
    for (const auto& [what, n] : by) sum.rows.push_back({k, what, std::to_string(n)});
  lab.table(sum, gl::Det::measured);
  lab.metric("movers.count", static_cast<double>(movers.size()), "count");
  lab.metric("movers.silent", silent, "count");
}

}  // namespace shooters_edge::labs
