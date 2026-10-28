// The server's pool, links, touches, program calls and sounds (server.hpp states the rules).
//
// ge:summary The classic server's pool, area links, trigger touches, impacts and program calls
// ge:keywords create_server spawn_edict free_edict link_edict touch_links impact call_program
// ge:keywords start_sound set_origin set_size set_brush_model world_contents
// ge:skill shooter
// ge:test tests/shooter_server_test.cpp
#include "shooter/server.hpp"

namespace ge::shooter {

namespace {

constexpr float kItemGrow = 15;   // an item's trigger box, grown sideways, units
constexpr float kLinkGrow = 1;    // every other box, grown on every axis, units
constexpr double kReuseAfter = 0.5;  // a freed slot is reused after this long, s
constexpr double kReuseAlways = 2;   // ... or at once before the map's time reaches this, s

void reset(Edict& e) {
  Edict fresh;
  fresh.free = false;
  e = fresh;
}

}  // namespace

void create_server(Server& sv, const ServerSettings& set, const std::vector<BrushModel>& models,
                   const Program& program) {
  sv = Server{};
  sv.set = set;
  if (sv.set.max_edicts > kMaxEdicts) sv.set.max_edicts = kMaxEdicts;
  if (sv.set.maxclients > kMaxClients) sv.set.maxclients = kMaxClients;
  sv.edicts.assign(static_cast<size_t>(sv.set.max_edicts), Edict{});
  sv.touched.assign(static_cast<size_t>(sv.set.max_edicts), 0);
  sv.clients.assign(static_cast<size_t>(sv.set.maxclients), ClientSlot{});
  sv.models.reserve(models.size() + 1);
  sv.models.push_back(BrushModel{});  // modelindex 0: none
  for (const BrushModel& m : models) sv.models.push_back(m);
  sv.program = program;
  sv.time = set.start_time;
  sv.num_edicts = sv.set.maxclients + 1;

  const BrushModel& world = sv.models[1];
  sv.area = create_area_tree(world.mins, world.maxs);
  Edict& w = sv.edicts[0];
  reset(w);
  w.solid = kSolidBsp;
  w.movetype = kMovetypePush;
  w.modelindex = 1;
  w.mins = world.mins;
  w.maxs = world.maxs;
  w.size = world.maxs - world.mins;
}

int spawn_edict(Server& sv) {
  for (int i = sv.set.maxclients + 1; i < sv.num_edicts; i++) {
    const Edict& e = sv.edicts[static_cast<size_t>(i)];
    if (e.free && (e.freetime < kReuseAlways || sv.time - e.freetime > kReuseAfter)) {
      reset(sv.edicts[static_cast<size_t>(i)]);
      return i;
    }
  }
  if (sv.num_edicts >= sv.set.max_edicts) return -1;
  const int i = sv.num_edicts++;
  reset(sv.edicts[static_cast<size_t>(i)]);
  return i;
}

void free_edict(Server& sv, int e) {
  unlink_edict(sv, e);
  Edict& ed = sv.edicts[static_cast<size_t>(e)];
  ed = Edict{};
  ed.free = true;
  ed.freetime = static_cast<float>(sv.time);
}

void unlink_edict(Server& sv, int e) { area_unlink(sv.area, sv.edicts.data(), e); }

void link_edict(Server& sv, int e, bool touch_triggers) {
  unlink_edict(sv, e);
  if (e == 0) return;  // the world is never linked
  Edict& ed = sv.edicts[static_cast<size_t>(e)];
  if (ed.free) return;
  ed.absmin = ed.origin + ed.mins;
  ed.absmax = ed.origin + ed.maxs;
  if ((ed.flags & kFlagItem) != 0) {
    ed.absmin.x = ed.absmin.x - kItemGrow;
    ed.absmin.y = ed.absmin.y - kItemGrow;
    ed.absmax.x = ed.absmax.x + kItemGrow;
    ed.absmax.y = ed.absmax.y + kItemGrow;
  } else {
    for (int i = 0; i < 3; i++) {
      ed.absmin[i] = ed.absmin[i] - kLinkGrow;
      ed.absmax[i] = ed.absmax[i] + kLinkGrow;
    }
  }
  if (ed.solid == kSolidNot) return;
  area_link(sv.area, sv.edicts.data(), e);
  if (touch_triggers) touch_links(sv, e);
}

void touch_links(Server& sv, int e) {
  // Listed first, then touched: a touch may relink anything (this entity included).
  int list[kMaxEdicts];
  const Edict& ed = sv.edicts[static_cast<size_t>(e)];
  const int n = area_list(sv.area, sv.edicts.data(), ed.absmin, ed.absmax, AreaKind::triggers,
                          list, kMaxEdicts);
  const int old_self = sv.g.self, old_other = sv.g.other;
  for (int i = 0; i < n; i++) {
    const int t = list[i];
    if (t == e) continue;
    const Edict& trig = sv.edicts[static_cast<size_t>(t)];
    if (trig.free || trig.touch == kNoFunc || trig.solid != kSolidTrigger) continue;
    sv.g.self = t;
    sv.g.other = e;
    sv.g.time = static_cast<float>(sv.time);
    sv.counts.trigger_touches++;
    call_program(sv, trig.touch);
  }
  sv.g.self = old_self;
  sv.g.other = old_other;
}

void impact(Server& sv, int e1, int e2) {
  const int old_self = sv.g.self, old_other = sv.g.other;
  sv.g.time = static_cast<float>(sv.time);
  {
    const Edict& a = sv.edicts[static_cast<size_t>(e1)];
    if (a.touch != kNoFunc && a.solid != kSolidNot) {
      sv.g.self = e1;
      sv.g.other = e2;
      sv.counts.impacts++;
      call_program(sv, a.touch);
    }
  }
  {
    const Edict& b = sv.edicts[static_cast<size_t>(e2)];
    if (b.touch != kNoFunc && b.solid != kSolidNot) {
      sv.g.self = e2;
      sv.g.other = e1;
      sv.counts.impacts++;
      call_program(sv, b.touch);
    }
  }
  sv.g.self = old_self;
  sv.g.other = old_other;
}

void set_origin(Server& sv, int e, Vec3f origin) {
  sv.edicts[static_cast<size_t>(e)].origin = origin;
  link_edict(sv, e, false);
}

void set_size(Server& sv, int e, Vec3f mins, Vec3f maxs) {
  Edict& ed = sv.edicts[static_cast<size_t>(e)];
  ed.mins = mins;
  ed.maxs = maxs;
  ed.size = maxs - mins;
  link_edict(sv, e, false);
}

void set_brush_model(Server& sv, int e, int modelindex) {
  sv.edicts[static_cast<size_t>(e)].modelindex = modelindex;
  const BrushModel& m = sv.models[static_cast<size_t>(modelindex)];
  set_size(sv, e, m.mins, m.maxs);
}

void call_program(Server& sv, Func fn) {
  if (fn == kNoFunc || sv.program.call == nullptr) return;
  sv.program.call(sv.program.ctx, sv, fn);
}

void start_sound(Server& sv, int e, int channel, const char* sample, float volume,
                 float attenuation) {
  if (sv.sound_count >= kMaxSounds) {
    sv.counts.sounds_dropped++;
    return;
  }
  sv.sounds[sv.sound_count++] = {e, channel, sample, volume, attenuation};
}

int world_contents(const Server& sv, Vec3f p) {
  return point_contents(sv.models[1].hulls[0], p);
}

namespace {

struct Fnv {
  uint64_t h = 0xcbf29ce484222325ull;
  void bytes(const void* p, size_t n) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 0x100000001b3ull;
  }
  template <typename T>
  void add(const T& v) {
    bytes(&v, sizeof v);
  }
  void vec(Vec3f v) {
    add(v.x);
    add(v.y);
    add(v.z);
  }
};

}  // namespace

uint64_t server_hash(const Server& sv) {
  Fnv f;
  f.add(sv.time);
  f.add(sv.num_edicts);
  for (int i = 0; i < sv.num_edicts; i++) {
    const Edict& e = sv.edicts[static_cast<size_t>(i)];
    f.add(static_cast<uint8_t>(e.free ? 1 : 0));
    if (e.free) continue;
    f.vec(e.origin);
    f.vec(e.velocity);
    f.vec(e.angles);
    f.vec(e.avelocity);
    f.vec(e.mins);
    f.vec(e.maxs);
    f.add(e.movetype);
    f.add(e.solid);
    f.add(e.flags);
    f.add(e.groundentity);
    f.add(e.nextthink);
    f.add(e.think);
    f.add(e.ltime);
    f.add(e.waterlevel);
    f.add(e.watertype);
    f.add(e.teleport_time);
    f.vec(e.v_angle);
    f.add(e.health);
    f.add(e.area_node);
  }
  for (const ClientSlot& c : sv.clients) {
    f.add(static_cast<uint8_t>(c.active ? 1 : 0));
    f.add(static_cast<uint8_t>(c.jump_held ? 1 : 0));
    f.add(c.speed_bank_ms);
    f.add(c.last_sequence);
  }
  return f.h;
}

}  // namespace ge::shooter
