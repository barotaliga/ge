// The calls a hosted game library of the sequel makes, answered over the host's world
// (sequel_native.hpp; each call's duties are porting/shooters-edge/sequel-game-library.md § 2).
//
// ge:summary The hosted game library's calls: prints, sounds, indices, trace, links, move, writes
// ge:keywords game-library native-calls imports trace pointcontents linkentity box-edicts pmove
// ge:keywords multicast unicast write-byte tag-malloc cvar configstring m18v10
// ge:skill shooter mod
// ge:test tests/shooters_edge_sequel_native_test.cpp
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "classic/protocol/directions.hpp"
#include "classic/sequel_move.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_native_host.hpp"

namespace shooters_edge::server {

namespace {

namespace sh = ge::shooter;
namespace br = ge::brush;
namespace cp = ge::classic::protocol;
using native::NativeEntityHead;
using native::NativeMoveBlock;
using native::NativeTrace;

NativeHost* g_host = nullptr;  // the host whose library is being called

constexpr uint8_t kSvcSound = 9, kSvcPrint = 10, kSvcCenterPrint = 15;
constexpr int kCsModels = 32, kCsSounds = 288, kCsImages = 544;
constexpr int32_t kContentsMonster = 0x2000000, kContentsDeadMonster = 0x4000000;
constexpr size_t kMostMessage = 1400;
constexpr int kChanNoHearing = 8, kChanReliable = 16;

// Leaves the library's call (`error` and the faults the interface makes fatal); never returns.
// The text is set before the jump, so no string of the caller's frame is alive across it (a jump
// skips destructors): callers that build one assign `error_text` and call `leave`.
[[noreturn]] void leave(NativeHost& h) {
  h.dead = true;
  if (h.jump != nullptr) std::longjmp(*h.jump, 1);  // NOLINT(cert-err52-cpp): the interface's own
  std::abort();
}

[[noreturn]] void fail(NativeHost& h, const char* text) {
  h.error_text = text;
  leave(h);
}

std::string vformat(const char* fmt, va_list ap, size_t most) {
  std::string out(most, '\0');
  // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized): each caller starts `ap`
  const int n = std::vsnprintf(out.data(), most, fmt != nullptr ? fmt : "", ap);
  out.resize(n < 0 ? 0 : std::min(size_t(n), most - 1));
  return out;
}

int slot_of(const NativeHost& h, const NativeEntityHead* e) {
  const int n = h.number_of(e);
  return n >= 1 && n <= h.max_clients ? n - 1 : -1;
}

void put(std::vector<uint8_t>& to, const std::vector<uint8_t>& bytes) {
  to.insert(to.end(), bytes.begin(), bytes.end());
}

sh::Vec3f vec(const float* v) { return v != nullptr ? sh::Vec3f{v[0], v[1], v[2]} : sh::Vec3f{}; }

// The same effect as an event for the window's own game (rules_runner.hpp's `out`): `to` a bit
// per slot.
NetEvent& add_event(NativeHost& h, EventKind kind, uint32_t to, bool reliable, std::string text) {
  NetEvent& ev = h.s->out.events.emplace_back();
  ev.kind = kind;
  ev.to = to;
  ev.reliable = reliable;
  ev.time = h.clock;
  ev.text = std::move(text);
  h.s->out.events_total++;
  return ev;
}

// Every slot in the game, as event bits.
uint32_t begun_bits(const NativeHost& h) {
  uint32_t to = 0;
  for (int i = 0; i < h.max_clients && i < 32; i++)
    if (h.begun[i]) to |= uint32_t(1) << i;
  return to;
}

// The cluster of the point, or -1 outside every one.
int32_t cluster_at(const NativeHost& h, const float* p) {
  if (!h.map || h.map->leaves.empty()) return 0;
  const uint32_t leaf = br::leaf_at(*h.map, p);
  return leaf < h.map->leaves.size() ? h.map->leaves[leaf].cluster : -1;
}

// Whether a message at `origin` reaches slot `slot` (`see`: the visible set, else the hearable).
bool reaches(const NativeHost& h, int32_t from, int slot, bool see) {
  const NativeEntityHead* e = h.entity(slot + 1);
  const int32_t to = cluster_at(h, e->s.origin);
  if (from < 0 || to < 0) return false;
  if (!h.map) return true;  // a map of clip hulls: every point in every set
  return see ? br::cluster_sees(*h.map, from, to) : br::cluster_hears(*h.map, from, to);
}

// The bytes to every slot `to` reaches (0 all, 1 hearable, 2 visible; reliable or not).
void send_to(NativeHost& h, const float* origin, int to, bool reliable,
             const std::vector<uint8_t>& bytes) {
  const int32_t from = to == 0 ? 0 : cluster_at(h, origin);
  for (int i = 0; i < h.max_clients; i++) {
    if (!h.connected[i] || (!reliable && !h.begun[i])) continue;
    if (to != 0 && !reaches(h, from, i, to == 2)) continue;
    put(reliable ? h.s->raw.reliable[i] : h.s->raw.datagram[i], bytes);
  }
}

void text_message(std::vector<uint8_t>& b, uint8_t id, int level, const std::string& text) {
  b.push_back(id);
  if (level >= 0) b.push_back(static_cast<uint8_t>(level));
  b.insert(b.end(), text.begin(), text.end());
  b.push_back(0);
}

// --- prints ---

void c_bprintf(int level, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const std::string text = vformat(fmt, ap, 2048);
  va_end(ap);
  NativeHost& h = *g_host;
  std::vector<uint8_t> b;
  text_message(b, kSvcPrint, level, text);
  for (int i = 0; i < h.max_clients; i++)
    if (h.begun[i]) put(h.s->raw.reliable[i], b);
  NetEvent& ev = add_event(h, EventKind::print, begun_bits(h), true, text);
  ev.level = level;
  h.s->console.push_back(text);
}

void c_dprintf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  g_host->s->console.push_back(vformat(fmt, ap, 1024));
  va_end(ap);
}

void c_cprintf(NativeEntityHead* ent, int level, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const std::string text = vformat(fmt, ap, 1024);
  va_end(ap);
  NativeHost& h = *g_host;
  if (ent == nullptr) return h.s->console.push_back(text);
  const int slot = slot_of(h, ent);
  if (slot < 0) fail(h, "the game printed to an entity that is no player");
  native_print(h, slot, level, text);
}

void c_centerprintf(NativeEntityHead* ent, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  const std::string text = vformat(fmt, ap, 1024);
  va_end(ap);
  NativeHost& h = *g_host;
  const int slot = slot_of(h, ent);
  if (slot < 0) return;
  std::vector<uint8_t> b;
  text_message(b, kSvcCenterPrint, -1, text);
  put(h.s->raw.reliable[slot], b);
  add_event(h, EventKind::centerprint, uint32_t(1) << slot, true, text);
}

[[noreturn]] void c_error(const char* fmt, ...) {
  {
    va_list ap;
    va_start(ap, fmt);
    g_host->error_text = vformat(fmt, ap, 1024);
    va_end(ap);
  }
  leave(*g_host);
}

// --- sounds ---

void c_positioned_sound(const float* origin, NativeEntityHead* ent, int channel, int index,
                        float volume, float attenuation, float offset) {
  NativeHost& h = *g_host;
  if (ent == nullptr) return;
  const int n = h.number_of(ent);
  if (n < 0 || n > 4095) return;
  if (volume < 0 || volume > 1) volume = 1;
  if (attenuation < 0 || attenuation > 4) attenuation = 0;
  if (offset < 0 || offset > 0.255f) offset = 0;
  float at[3] = {ent->s.origin[0], ent->s.origin[1], ent->s.origin[2]};
  if (origin != nullptr) {
    std::memcpy(at, origin, sizeof at);
  } else if (ent->solid == native::kSolidBsp) {
    for (int k = 0; k < 3; k++) at[k] += 0.5f * (ent->mins[k] + ent->maxs[k]);
  }
  uint8_t flags = 8 | 4;  // the entity and its channel, the place
  if (volume != 1.0f) flags |= 1;
  if (attenuation != 1.0f) flags |= 2;
  if (offset != 0) flags |= 16;
  std::vector<uint8_t> b = {kSvcSound, flags, static_cast<uint8_t>(index)};
  if (flags & 1) b.push_back(static_cast<uint8_t>(volume * 255));
  if (flags & 2) b.push_back(static_cast<uint8_t>(attenuation * 64));
  if (flags & 16) b.push_back(static_cast<uint8_t>(offset * 1000));
  const int entchan = (n << 3) | (channel & 7);
  b.push_back(static_cast<uint8_t>(entchan & 0xFF));
  b.push_back(static_cast<uint8_t>((entchan >> 8) & 0xFF));
  for (const float v : at) {
    const auto c = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int32_t>(v * 8)));
    b.push_back(static_cast<uint8_t>(c & 0xFF));
    b.push_back(static_cast<uint8_t>(c >> 8));
  }
  const bool all = (channel & kChanNoHearing) != 0 || attenuation == 0;
  send_to(h, at, all ? 0 : 1, (channel & kChanReliable) != 0, b);
  if (index > 0 && index < 256) {  // the window's game hears it by the sound's name
    NetEvent& ev = add_event(h, EventKind::sound, begun_bits(h), (channel & kChanReliable) != 0,
                             h.s->raw.config[size_t(kCsSounds) + size_t(index)]);
    ev.ent = n;
    ev.level = channel & 7;
    ev.origin = vec(at);
    ev.values[0] = volume * 255;
    ev.values[1] = attenuation;
  }
}

void c_sound(NativeEntityHead* ent, int channel, int index, float volume, float attenuation,
             float offset) {
  c_positioned_sound(nullptr, ent, channel, index, volume, attenuation, offset);
}

// --- the lists ---

void c_configstring(int index, const char* text) {
  NativeHost& h = *g_host;
  if (index < 0 || index >= kSequelConfigStrings) return h.s->console.push_back("bad index");
  h.buf.clear();
  const std::string t = text != nullptr ? text : "";
  const size_t room = index >= 5 && index <= 28 ? size_t(29 - index) * 64 : 64;
  if (t.size() + 1 > room) return h.s->console.push_back("a configstring too long: " + t);
  h.s->raw.config[size_t(index)] = t;
}

int find_index(NativeHost& h, const char* name, int base) {
  if (name == nullptr || name[0] == 0) return 0;
  const std::string n = name;
  if (n.size() >= 64) return 0;
  std::vector<std::string>& cs = h.s->raw.config;
  for (int i = 1; i < 256; i++)
    if (cs[size_t(base) + size_t(i)] == n) return i;
  for (int i = 1; i < 256; i++)
    if (cs[size_t(base) + size_t(i)].empty()) {
      cs[size_t(base) + size_t(i)] = n;
      return i;
    }
  h.s->console.push_back("configstrings overflowed: " + n);
  return 0;
}

int c_modelindex(const char* name) { return find_index(*g_host, name, kCsModels); }
int c_soundindex(const char* name) { return find_index(*g_host, name, kCsSounds); }
int c_imageindex(const char* name) { return find_index(*g_host, name, kCsImages); }

// --- links ---

void c_linkentity(NativeEntityHead* e);

void c_setmodel(NativeEntityHead* e, const char* name) {
  NativeHost& h = *g_host;
  if (name == nullptr) fail(h, "setmodel with no model");
  e->s.model[0] = c_modelindex(name);
  if (name[0] != '*') return;
  const int n = std::atoi(name + 1);
  if (n < 1 || size_t(n) >= h.model_count()) {
    h.error_text = std::string("the map has no brush model ") + name;
    leave(h);
  }
  if (h.map) {
    const br::Model& m = h.map->models[size_t(n)];
    std::memcpy(e->mins, m.mins, sizeof e->mins);
    std::memcpy(e->maxs, m.maxs, sizeof e->maxs);
  } else {
    const sh::BrushModel& m = h.hulls[size_t(n)];
    const float lo[3] = {m.mins.x, m.mins.y, m.mins.z}, hi[3] = {m.maxs.x, m.maxs.y, m.maxs.z};
    std::memcpy(e->mins, lo, sizeof e->mins);
    std::memcpy(e->maxs, hi, sizeof e->maxs);
  }
  c_linkentity(e);
}

void c_linkentity(NativeEntityHead* e) {
  NativeHost& h = *g_host;
  const int n = h.number_of(e);
  if (n < 0 || size_t(n) >= h.linked.size()) return;
  h.linked[size_t(n)] = 0;
  if (n == 0 || e->inuse == 0) return;
  for (int k = 0; k < 3; k++) e->size[k] = e->maxs[k] - e->mins[k];
  if (e->solid == native::kSolidBox && (e->svflags & native::kSvfDeadMonster) == 0) {
    const int x = std::clamp(static_cast<int>(e->maxs[0] / 8), 1, 31);
    const int down = std::clamp(static_cast<int>(-e->mins[2] / 8), 1, 31);
    const int up = std::clamp(static_cast<int>((e->maxs[2] + 32) / 8), 1, 63);
    e->s.solid = (up << 10) | (down << 5) | x;
  } else {
    e->s.solid = e->solid == native::kSolidBsp ? 31 : 0;
  }
  const bool turned = e->s.angles[0] != 0 || e->s.angles[1] != 0 || e->s.angles[2] != 0;
  if (e->solid == native::kSolidBsp && turned) {
    float r = 0;
    for (int k = 0; k < 3; k++)
      r = std::max({r, e->mins[k] < 0 ? -e->mins[k] : e->mins[k],
                    e->maxs[k] < 0 ? -e->maxs[k] : e->maxs[k]});
    for (int k = 0; k < 3; k++) {
      e->absmin[k] = e->s.origin[k] - r - 1;
      e->absmax[k] = e->s.origin[k] + r + 1;
    }
  } else {
    for (int k = 0; k < 3; k++) {
      e->absmin[k] = e->s.origin[k] + e->mins[k] - 1;
      e->absmax[k] = e->s.origin[k] + e->maxs[k] + 1;
    }
  }
  e->num_clusters = 0;
  e->area_num = e->area_num2 = 0;
  if (e->linkcount == 0) std::memcpy(e->s.old_origin, e->s.origin, sizeof e->s.old_origin);
  e->linkcount++;
  if (e->solid == native::kSolidNot) return;
  h.linked[size_t(n)] = e->solid == native::kSolidTrigger ? 2 : 1;
}

void c_unlinkentity(NativeEntityHead* e) {
  NativeHost& h = *g_host;
  const int n = h.number_of(e);
  if (n >= 0 && size_t(n) < h.linked.size()) h.linked[size_t(n)] = 0;
}

bool touches(const float* lo, const float* hi, const NativeEntityHead* e) {
  for (int k = 0; k < 3; k++)
    if (e->absmin[k] > hi[k] || e->absmax[k] < lo[k]) return false;
  return true;
}

int c_box_edicts(const float* mins, const float* maxs, NativeEntityHead** list, int most,
                 int kind) {
  NativeHost& h = *g_host;
  const uint8_t want = kind == 1 ? 1 : 2;
  int count = 0;
  const int n = std::min(h.game->num_edicts, static_cast<int>(h.linked.size()));
  for (int i = 1; i < n; i++) {
    if (h.linked[size_t(i)] != want) continue;
    NativeEntityHead* e = h.entity(i);
    if (!touches(mins, maxs, e)) continue;
    if (count == most) {
      h.s->console.push_back("MAXCOUNT");
      return most;
    }
    list[count++] = e;
  }
  return count;
}

// --- the world ---

native::NativeSurface* surface_of(NativeHost& h, int32_t texture, int32_t flags, int32_t value) {
  if (texture < 0 || size_t(texture) >= h.surfaces.size()) return &h.no_surface;
  native::NativeSurface& s = h.surfaces[size_t(texture)];
  s.flags = flags;
  s.value = value;
  return &s;
}

void fill_plane(native::NativePlane& p, const float n[3], float d) {
  std::memcpy(p.normal, n, sizeof p.normal);
  p.dist = d;
  p.type = 3;
  for (uint8_t k = 0; k < 3; k++)
    if (n[k] == 1 || n[k] == -1) p.type = k;
  p.signbits = 0;
  for (int k = 0; k < 3; k++)
    if (n[k] < 0) p.signbits = static_cast<uint8_t>(p.signbits | (1 << k));
}

// The sequel's contents bits for a classic contents number (sky and solid both solid, the
// currents water).
int32_t classic_contents(int c) {
  if (c == sh::kContentsEmpty) return 0;
  if (c == sh::kContentsWater || c < -6) return 32;
  if (c == sh::kContentsSlime) return 16;
  if (c == sh::kContentsLava) return 8;
  return 1;
}

// A trace through a classic map's model `model` moved by `origin`: the box's hull by its width
// (under 3 units the point hull, up to 32 the player's, else the large one), the points moved by
// the hull's own box against the trace's. Only solid stops it: a mask without solid passes.
NativeTrace hull_trace(NativeHost& h, uint32_t model, const float* origin, const float* start,
                       const float* mins, const float* maxs, const float* end, int mask) {
  NativeTrace r{};
  r.fraction = 1;
  std::memcpy(r.endpos, end, sizeof r.endpos);
  r.surface = &h.no_surface;
  r.ent = h.entity(0);
  if (model >= h.hulls.size() || (mask & 1) == 0) return r;
  const sh::BrushModel& m = h.hulls[model];
  const float width = maxs[0] - mins[0];
  const int hull = width < 3 ? 0 : width <= 32 ? 1 : 2;
  sh::Vec3f off{origin[0], origin[1], origin[2]};
  if (hull > 0) off = off + (m.clip_mins[hull] - vec(mins));
  const sh::HullTrace t = sh::trace_hull(m.hulls[hull], vec(start) - off, vec(end) - off);
  r.allsolid = t.allsolid ? 1 : 0;
  r.startsolid = t.startsolid ? 1 : 0;
  r.fraction = t.fraction;
  if (t.fraction < 1 || t.allsolid) {
    const sh::Vec3f at = t.endpos + off;
    const float p[3] = {at.x, at.y, at.z};
    std::memcpy(r.endpos, p, sizeof r.endpos);
    const float n[3] = {t.plane.normal.x, t.plane.normal.y, t.plane.normal.z};
    fill_plane(r.plane, n, t.plane.dist + n[0] * off.x + n[1] * off.y + n[2] * off.z);
    r.contents = 1;
  }
  return r;
}

// A brush trace through model `model` moved by `origin`, as the interface's answer.
NativeTrace brush_trace(NativeHost& h, uint32_t model, const float* origin, const float* start,
                        const float* mins, const float* maxs, const float* end, int mask) {
  if (!h.tracer) return hull_trace(h, model, origin, start, mins, maxs, end, mask);
  float a[3], b[3];
  for (int k = 0; k < 3; k++) {
    a[k] = start[k] - origin[k];
    b[k] = end[k] - origin[k];
  }
  const br::Trace t = h.tracer->trace(a, b, mins, maxs, mask, model);
  NativeTrace r{};
  r.allsolid = t.all_solid ? 1 : 0;
  r.startsolid = t.start_solid ? 1 : 0;
  r.fraction = t.fraction;
  for (int k = 0; k < 3; k++) r.endpos[k] = t.end[k] + origin[k];
  const float d =
      t.plane.d + t.plane.n[0] * origin[0] + t.plane.n[1] * origin[1] + t.plane.n[2] * origin[2];
  fill_plane(r.plane, t.plane.n, d);
  r.surface = surface_of(h, t.texture, t.flags, t.value);
  r.contents = t.contents;
  r.ent = h.entity(0);
  return r;
}

// A turned brush entity's angles (pitch, yaw, roll); false when all are exactly 0.
bool turned_angles(const NativeEntityHead* e, sh::Vec3f& a) {
  a = {e->s.angles[0], e->s.angles[1], e->s.angles[2]};
  return a.x != 0 || a.y != 0 || a.z != 0;
}

// A point `d` from a model's origin in the model's own frame at `a` (forward, left, up).
sh::Vec3f to_model(sh::Vec3f d, sh::Vec3f a) {
  const sh::ViewAxes x = sh::angle_vectors(a);
  return {sh::dot(d, x.forward), -sh::dot(d, x.right), sh::dot(d, x.up)};
}

// A TRACE AGAINST A TURNED BRUSH MODEL (porting/shooters-edge/sequel-game-library.md § Traces
// against turned brush models): the start and end taken from the origin and turned into the model's
// frame, the box swept there unchanged (so it turns with the model, as the game expects), the end
// rebuilt on the world's segment from the fraction. The hit's normal goes back by the exact
// inverse turn (a stock server turns it back by the negated angles, which is off when two or more
// angles are set), and the plane's distance is the world's at the end.
NativeTrace turned_trace(NativeHost& h, uint32_t model, const float* origin, sh::Vec3f angles,
                         const float* start, const float* mins, const float* maxs, const float* end,
                         int mask) {
  static const float kZero[3] = {0, 0, 0};
  const sh::Vec3f o = vec(origin);
  const sh::Vec3f a = to_model(vec(start) - o, angles), b = to_model(vec(end) - o, angles);
  const float la[3] = {a.x, a.y, a.z}, lb[3] = {b.x, b.y, b.z};
  NativeTrace r = brush_trace(h, model, kZero, la, mins, maxs, lb, mask);
  for (int k = 0; k < 3; k++) r.endpos[k] = start[k] + r.fraction * (end[k] - start[k]);
  if (r.fraction < 1 || r.startsolid != 0) {
    const sh::ViewAxes x = sh::angle_vectors(angles);
    const sh::Vec3f n =
        x.forward * r.plane.normal[0] - x.right * r.plane.normal[1] + x.up * r.plane.normal[2];
    const float w[3] = {n.x, n.y, n.z};
    fill_plane(r.plane, w, w[0] * r.endpos[0] + w[1] * r.endpos[1] + w[2] * r.endpos[2]);
  }
  return r;
}

uint32_t brush_model(const NativeHost& h, const NativeEntityHead* e) {
  const int m = e->s.model[0];
  if (m <= 0 || m >= 256) return 0;
  const std::string& name = h.s->raw.config[size_t(kCsModels) + size_t(m)];
  if (name.size() < 2 || name[0] != '*') return 0;
  const int n = std::atoi(name.c_str() + 1);
  return n > 0 && size_t(n) < h.model_count() ? uint32_t(n) : 0;
}

NativeTrace c_trace(const float* start, const float* mins_in, const float* maxs_in,
                    const float* end, NativeEntityHead* pass, int mask) {
  NativeHost& h = *g_host;
  static const float kZero[3] = {0, 0, 0};
  const float* mins = mins_in != nullptr ? mins_in : kZero;
  const float* maxs = maxs_in != nullptr ? maxs_in : kZero;
  NativeTrace best = brush_trace(h, 0, kZero, start, mins, maxs, end, mask);
  if (best.fraction == 0) return best;
  float lo[3], hi[3];
  for (int k = 0; k < 3; k++) {
    lo[k] = std::min(start[k], end[k]) + mins[k] - 1;
    hi[k] = std::max(start[k], end[k]) + maxs[k] + 1;
  }
  const int count = std::min(h.game->num_edicts, static_cast<int>(h.linked.size()));
  for (int i = 1; i < count && best.allsolid == 0; i++) {
    if (h.linked[size_t(i)] != 1) continue;
    NativeEntityHead* e = h.entity(i);
    if (e == pass || !touches(lo, hi, e)) continue;
    if (pass != nullptr && (e->owner == pass || pass->owner == e)) continue;
    const bool dead = (e->svflags & native::kSvfDeadMonster) != 0;
    if (dead && (mask & kContentsDeadMonster) == 0) continue;
    NativeTrace t{};
    if (e->solid == native::kSolidBsp) {
      const uint32_t model = brush_model(h, e);
      if (model == 0) continue;
      sh::Vec3f angles;
      t = turned_angles(e, angles)
              ? turned_trace(h, model, e->s.origin, angles, start, mins, maxs, end, mask)
              : brush_trace(h, model, e->s.origin, start, mins, maxs, end, mask);
    } else {
      const int32_t contents = dead ? kContentsDeadMonster : kContentsMonster;
      if ((mask & contents) == 0) continue;
      const sh::Vec3f o = vec(e->s.origin);
      const sh::Vec3f bl = {o.x + e->mins[0], o.y + e->mins[1], o.z + e->mins[2]};
      const sh::Vec3f bh = {o.x + e->maxs[0], o.y + e->maxs[1], o.z + e->maxs[2]};
      sh::Vec3f normal;
      const float f = sweep_box(vec(start), vec(end), vec(mins), vec(maxs), bl, bh, normal);
      if (f < 0) continue;
      t.fraction = f;
      if (f == 0) {
        t.startsolid = 1;
        sh::Vec3f n2;
        t.allsolid = sweep_box(vec(end), vec(end), vec(mins), vec(maxs), bl, bh, n2) == 0 ? 1 : 0;
      }
      for (int k = 0; k < 3; k++) t.endpos[k] = start[k] + f * (end[k] - start[k]);
      const float n[3] = {normal.x, normal.y, normal.z};
      fill_plane(t.plane, n, n[0] * t.endpos[0] + n[1] * t.endpos[1] + n[2] * t.endpos[2]);
      t.surface = &h.no_surface;
      t.contents = contents;
    }
    if (t.allsolid != 0 || t.startsolid != 0 || t.fraction < best.fraction) {
      t.ent = e;
      const bool was_start = best.startsolid != 0;
      best = t;
      if (was_start) best.startsolid = 1;
    }
  }
  return best;
}

int c_pointcontents(const float* p) {
  NativeHost& h = *g_host;
  // The world's contents (a model's, at `q` in its frame), in either form of map.
  auto contents = [&h](const float* q, uint32_t model) {
    if (h.tracer) return h.tracer->contents(q, model);
    return model < h.hulls.size()
               ? classic_contents(sh::point_contents(h.hulls[model].hulls[0], vec(q)))
               : 0;
  };
  int32_t c = contents(p, 0);
  const int count = std::min(h.game->num_edicts, static_cast<int>(h.linked.size()));
  for (int i = 1; i < count; i++) {
    if (h.linked[size_t(i)] != 1) continue;
    const NativeEntityHead* e = h.entity(i);
    if (!touches(p, p, e)) continue;
    if (e->solid == native::kSolidBsp) {
      const uint32_t model = brush_model(h, e);
      if (model == 0) continue;
      sh::Vec3f d{p[0] - e->s.origin[0], p[1] - e->s.origin[1], p[2] - e->s.origin[2]};
      sh::Vec3f angles;
      if (turned_angles(e, angles)) d = to_model(d, angles);  // in the turned model's frame
      const float q[3] = {d.x, d.y, d.z};
      c |= contents(q, model);
    } else {
      c |= kContentsMonster;
    }
  }
  return c;
}

int c_in_pvs(const float* a, const float* b) {
  NativeHost& h = *g_host;
  if (!h.map) return 1;  // a map of clip hulls: every point in every set
  const int32_t x = cluster_at(h, a), y = cluster_at(h, b);
  return x >= 0 && y >= 0 && br::cluster_sees(*h.map, x, y) ? 1 : 0;
}

int c_in_phs(const float* a, const float* b) {
  NativeHost& h = *g_host;
  if (!h.map) return 1;  // a map of clip hulls: every point in every set
  const int32_t x = cluster_at(h, a), y = cluster_at(h, b);
  return x >= 0 && y >= 0 && br::cluster_hears(*h.map, x, y) ? 1 : 0;
}

void c_set_area_portal(int, int) {}  // every area stays connected (sequel_native.hpp)
int c_areas_connected(int, int) { return 1; }

// --- the move ---

ge::classic::SequelTrace move_trace(void* ctx, sh::Vec3f start, sh::Vec3f mins, sh::Vec3f maxs,
                                    sh::Vec3f end) {
  const auto* pm = static_cast<const NativeMoveBlock*>(ctx);
  const float a[3] = {start.x, start.y, start.z}, lo[3] = {mins.x, mins.y, mins.z};
  const float hi[3] = {maxs.x, maxs.y, maxs.z}, b[3] = {end.x, end.y, end.z};
  const NativeTrace t = pm->trace(a, lo, hi, b);
  ge::classic::SequelTrace r;
  r.all_solid = t.allsolid != 0;
  r.start_solid = t.startsolid != 0;
  r.fraction = t.fraction;
  r.end = vec(t.endpos);
  r.normal = vec(t.plane.normal);
  r.surface_flags = t.surface != nullptr ? t.surface->flags : 0;
  r.contents = t.contents;
  r.entity = g_host->number_of(t.ent);
  return r;
}

int32_t move_contents(void* ctx, sh::Vec3f p) {
  const auto* pm = static_cast<const NativeMoveBlock*>(ctx);
  const float q[3] = {p.x, p.y, p.z};
  return pm->pointcontents(q);
}

void c_pmove(NativeMoveBlock* pm) {
  NativeHost& h = *g_host;
  ge::classic::SequelMove m;
  m.s.type = static_cast<ge::classic::SequelType>(pm->s.type);
  for (int k = 0; k < 3; k++) {
    m.s.origin[k] = pm->s.origin[k];
    m.s.velocity[k] = pm->s.velocity[k];
    m.s.delta_angles[k] = pm->s.delta_angles[k];
    m.cmd.angles[k] = pm->cmd.angles[k];
  }
  m.s.flags = pm->s.flags;
  m.s.time = pm->s.time;
  m.s.gravity = pm->s.gravity;
  m.cmd.msec = pm->cmd.msec;
  m.cmd.buttons = pm->cmd.buttons;
  m.cmd.forward = pm->cmd.forward;
  m.cmd.side = pm->cmd.side;
  m.cmd.up = pm->cmd.up;
  m.cmd.impulse = pm->cmd.impulse;
  m.cmd.light = pm->cmd.light;
  m.changed = pm->snapinitial != 0;
  m.air_accelerate = h.air_accelerate;
  if (h.classic_physics) {  // the classic game's ground and air, as the sequel runner moves them
    const shooters_edge::profiles::MoveFeel& f = shooters_edge::profiles::kClassicFeel;
    m.max_speed = f.max_speed;
    m.friction = f.friction;
    m.air_accelerate = f.air_accelerate;
    m.land_pause = f.land_pause;
    m.swim_scale = f.swim_scale;
    m.water_friction = f.water_friction;
    m.water_jump = f.water_jump;
    m.air_step = f.air_step;
  }
  const ge::classic::SequelWorld world{pm, move_trace, move_contents};
  ge::classic::sequel_move(m, world);
  pm->s.type = static_cast<int32_t>(m.s.type);
  for (int k = 0; k < 3; k++) {
    pm->s.origin[k] = m.s.origin[k];
    pm->s.velocity[k] = m.s.velocity[k];
    pm->s.delta_angles[k] = m.s.delta_angles[k];
  }
  pm->s.flags = m.s.flags;
  pm->s.time = m.s.time;
  pm->s.gravity = m.s.gravity;
  pm->numtouch = std::min(m.touches, native::kNativeTouch);
  for (int i = 0; i < pm->numtouch; i++) pm->touchents[i] = h.entity(std::max(0, m.touch[i]));
  pm->view_angles[0] = m.view_angles.x;
  pm->view_angles[1] = m.view_angles.y;
  pm->view_angles[2] = m.view_angles.z;
  pm->view_height = m.view_height;
  const float lo[3] = {m.mins.x, m.mins.y, m.mins.z}, hi[3] = {m.maxs.x, m.maxs.y, m.maxs.z};
  std::memcpy(pm->mins, lo, sizeof lo);
  std::memcpy(pm->maxs, hi, sizeof hi);
  pm->groundentity = m.ground_entity >= 0 ? h.entity(m.ground_entity) : nullptr;
  pm->watertype = m.water_type;
  pm->waterlevel = m.water_level;
}

// --- messages ---

// A temporary entity the game multicast, as the named effect the window's game draws (the kinds
// and payloads the sequel's recordings carry, client/demo_sequel.cpp): a hit's sparks, blood, a
// blast, a teleport's flash, the rail's line. Other kinds go to real clients alone.
void effect_event(NativeHost& h, const std::vector<uint8_t>& b) {
  if (b.size() < 8 || b[0] != 3) return;
  const auto at = [&b](size_t i) {
    sh::Vec3f v;
    float* c[3] = {&v.x, &v.y, &v.z};
    for (size_t k = 0; k < 3; k++)
      *c[k] =
          static_cast<float>(static_cast<int16_t>(b[i + 2 * k] | (b[i + 2 * k + 1] << 8))) * 0.125f;
    return v;
  };
  const char* name = nullptr;
  switch (b[1]) {
    case 0:
    case 4:
    case 14:
      name = "te_gunshot";
      break;
    case 1:
    case 26:
    case 42:
      name = "te_blood";
      break;
    case 2:
    case 9:
    case 12:
    case 13:
    case 30:
    case 43:
    case 44:
    case 46:
    case 55:
      name = "te_sparks";
      break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 17:
    case 18:
    case 20:
    case 21:
    case 28:
    case 35:
    case 47:
    case 51:
    case 53:
    case 54:
      name = "te_explosion1";
      break;
    case 22:
    case 48:
      name = "te_teleport";
      break;
    case 3: {
      if (b.size() < 14) return;
      const sh::Vec3f to = at(8);
      NetEvent& ev = add_event(h, EventKind::trail_effect, begun_bits(h), false, "te_railtrail");
      ev.origin = at(2);
      ev.values[0] = to.x;
      ev.values[1] = to.y;
      ev.values[2] = to.z;
      return;
    }
    default:
      return;
  }
  NetEvent& ev = add_event(h, EventKind::point_effect, begun_bits(h), false, name);
  ev.origin = at(2);
  ev.values[1] = 1;
}

void c_multicast(const float* origin, int to) {
  NativeHost& h = *g_host;
  if (to < 0 || to > 5) {
    h.error_text = "a message sent to no kind of set (" + std::to_string(to) + ")";
    leave(h);
  }
  const std::vector<uint8_t> bytes = std::move(h.buf);
  h.buf.clear();
  send_to(h, origin, to % 3 == 0 ? 0 : to % 3 == 1 ? 1 : 2, to >= 3, bytes);
  effect_event(h, bytes);
}

// A layout program (message 4 and its text) or the inventory (5 and 256 counts) sent to a slot,
// kept for the window's game.
void keep_shown(NativeHost& h, int slot, const std::vector<uint8_t>& b) {
  if (slot < 0 || slot >= 32 || b.empty()) return;
  if (b[0] == 4) {
    const auto end = std::find(b.begin() + 1, b.end(), uint8_t{0});
    h.layout[slot].assign(b.begin() + 1, end);
  } else if (b[0] == 5 && b.size() >= 1 + 2 * 256) {
    h.inventory[slot].resize(256);
    for (size_t k = 0; k < 256; k++)
      h.inventory[slot][k] = static_cast<int16_t>(b[1 + 2 * k] | (b[2 + 2 * k] << 8));
  }
}

void c_unicast(NativeEntityHead* ent, int reliable) {
  NativeHost& h = *g_host;
  const int slot = slot_of(h, ent);
  if (ent == nullptr || slot < 0) return;  // the bytes wait for the next message
  keep_shown(h, slot, h.buf);
  put(reliable != 0 ? h.s->raw.reliable[slot] : h.s->raw.datagram[slot], h.buf);
  h.buf.clear();
}

void write(NativeHost& h, uint32_t v, int bytes) {
  if (h.buf.size() + size_t(bytes) > kMostMessage) fail(h, "the game's message overflowed");
  for (int k = 0; k < bytes; k++) h.buf.push_back(static_cast<uint8_t>((v >> (8 * k)) & 0xFF));
}

void c_write_char(int c) { write(*g_host, static_cast<uint32_t>(c), 1); }
void c_write_byte(int c) { write(*g_host, static_cast<uint32_t>(c), 1); }
void c_write_short(int c) { write(*g_host, static_cast<uint32_t>(c), 2); }
void c_write_long(int c) { write(*g_host, static_cast<uint32_t>(c), 4); }
void c_write_float(float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, sizeof u);
  write(*g_host, u, 4);
}
void c_write_string(const char* s) {
  NativeHost& h = *g_host;
  for (const char* p = s; p != nullptr && *p != 0; p++) write(h, static_cast<uint8_t>(*p), 1);
  write(h, 0, 1);
}
void c_write_position(const float* p) {
  for (int k = 0; k < 3; k++)
    write(*g_host, static_cast<uint32_t>(static_cast<int32_t>(p[k] * 8)), 2);
}
// The direction's index into the protocol's table of 162 directions (a missing one: 0).
void c_write_dir(const float* d) {
  write(*g_host, d != nullptr ? static_cast<uint32_t>(cp::direction_index(d)) : 0u, 1);
}
void c_write_angle(float f) {
  write(*g_host, static_cast<uint32_t>(static_cast<int32_t>(f * 256.0f / 360.0f)), 1);
}

// --- memory ---

void* c_tag_malloc(int size, int tag) {
  NativeHost& h = *g_host;
  if (size < 0) fail(h, "the game asked for a negative amount of memory");
  void* p = std::calloc(size_t(std::max(size, 1)), 1);
  if (p == nullptr) fail(h, "out of memory for the game");
  h.blocks[p] = tag < 0 || tag > 65535 ? 0 : tag;
  return p;
}

void c_tag_free(void* p) {
  NativeHost& h = *g_host;
  if (p == nullptr) return;
  const auto at = h.blocks.find(p);
  if (at == h.blocks.end()) fail(h, "the game freed memory it was never given");
  h.blocks.erase(at);
  std::free(p);
}

void c_free_tags(int tag) {
  NativeHost& h = *g_host;
  if (tag < 1 || tag > 65535) return;
  for (auto it = h.blocks.begin(); it != h.blocks.end();) {
    if (it->second == tag) {
      std::free(it->first);
      it = h.blocks.erase(it);
    } else {
      ++it;
    }
  }
}

// --- settings and commands ---

bool info_safe(const std::string& v) { return v.find_first_of("\\\";") == std::string::npos; }

native::NativeCvar* c_cvar(const char* name, const char* value, int flags) {
  NativeHost& h = *g_host;
  if (name == nullptr || value == nullptr) return nullptr;
  const bool info = (flags & (native::kCvarUserInfo | native::kCvarServerInfo)) != 0;
  if (info && (!info_safe(name) || !info_safe(value))) return nullptr;
  return &native_setting(h, name, value, flags).rec;
}

native::NativeCvar* c_cvar_set(const char* name, const char* value) {
  NativeHost& h = *g_host;
  NativeSetting& c = native_setting(h, name != nullptr ? name : "", "", 0);
  native_set(h, c, value != nullptr ? value : "", false);
  return &c.rec;
}

native::NativeCvar* c_cvar_forceset(const char* name, const char* value) {
  NativeHost& h = *g_host;
  NativeSetting& c = native_setting(h, name != nullptr ? name : "", "", 0);
  native_set(h, c, value != nullptr ? value : "", true);
  return &c.rec;
}

int c_argc() { return static_cast<int>(g_host->argv.size()); }
const char* c_argv(int n) {
  const NativeHost& h = *g_host;
  static const char kEmpty[] = "";
  return n >= 0 && size_t(n) < h.argv.size() ? h.argv[size_t(n)].c_str() : kEmpty;
}
const char* c_args() { return g_host->args.c_str(); }

void c_add_command_string(const char* text) {
  if (text != nullptr) g_host->commands.emplace_back(text);
}

void c_debug_graph(float, int) {}

}  // namespace

NativeSetting* NativeHost::find_setting(const std::string& name) {
  for (NativeSetting& c : settings)
    if (c.name == name) return &c;
  return nullptr;
}

NativeSetting& native_setting(NativeHost& h, const std::string& name, const std::string& value,
                              int flags) {
  if (NativeSetting* c = h.find_setting(name)) {
    c->rec.flags |= flags;
    return *c;
  }
  NativeSetting& c = h.settings.emplace_back();
  c.name = name;
  c.value = value;
  c.fallback = value;
  c.rec.name = c.name.data();
  c.rec.string = c.value.data();
  c.rec.default_string = c.fallback.data();
  c.rec.flags = flags;
  c.rec.modified = 1;
  c.rec.value = static_cast<float>(std::atof(c.value.c_str()));
  return c;
}

void native_set(NativeHost& h, NativeSetting& c, const std::string& value, bool force) {
  const bool info = (c.rec.flags & (native::kCvarUserInfo | native::kCvarServerInfo)) != 0;
  if (info && !info_safe(value)) return;
  if (!force && (c.rec.flags & native::kCvarNoSet) != 0) {
    h.s->console.push_back(c.name + " is write protected");
    return;
  }
  if (!force && (c.rec.flags & native::kCvarLatch) != 0 && h.game != nullptr) {
    if (value == c.value || (c.has_latched && value == c.latched)) return;
    c.latched = value;
    c.has_latched = true;
    c.rec.latched_string = c.latched.data();
    h.s->console.push_back(c.name + " will be changed for the next game");
    return;
  }
  c.has_latched = false;
  c.rec.latched_string = nullptr;
  if (c.name == "sv_physics") h.classic_physics = value == "classic";
  if (value == c.value) return;
  c.value = value;
  c.rec.string = c.value.data();
  c.rec.value = static_cast<float>(std::atof(c.value.c_str()));
  c.rec.modified = 1;
}

void native_print(NativeHost& h, int slot, int level, const std::string& text) {
  std::vector<uint8_t> b;
  text_message(b, kSvcPrint, level, text);
  put(h.s->raw.reliable[slot], b);
  if (slot >= 0 && slot < 32)
    add_event(h, EventKind::print, uint32_t(1) << slot, true, text).level = level;
}

void native_split(NativeHost& h, const std::string& line) {
  h.argv.clear();
  size_t i = 0;
  bool first = true;
  h.args.clear();
  while (i < line.size()) {
    while (i < line.size() && line[i] <= ' ' && line[i] != '\n') i++;
    if (i >= line.size() || line[i] == '\n') break;
    if (line.compare(i, 2, "//") == 0) break;
    if (!first && h.args.empty()) {
      h.args = line.substr(i);
      while (!h.args.empty() && h.args.back() <= ' ') h.args.pop_back();
    }
    std::string word;
    if (line[i] == '"') {
      i++;
      while (i < line.size() && line[i] != '"') word += line[i++];
      if (i < line.size()) i++;
    } else {
      while (i < line.size() && line[i] > ' ') word += line[i++];
    }
    if (h.argv.size() < 80) h.argv.push_back(word.substr(0, 1023));
    first = false;
  }
}

void native_make_current(NativeHost* h) { g_host = h; }

void native_fill_imports(native::NativeImports& t) {
  t.bprintf = c_bprintf;
  t.dprintf = c_dprintf;
  t.cprintf = c_cprintf;
  t.centerprintf = c_centerprintf;
  t.sound = c_sound;
  t.positioned_sound = c_positioned_sound;
  t.configstring = c_configstring;
  t.error = c_error;
  t.modelindex = c_modelindex;
  t.soundindex = c_soundindex;
  t.imageindex = c_imageindex;
  t.setmodel = c_setmodel;
  t.trace = c_trace;
  t.pointcontents = c_pointcontents;
  t.in_pvs = c_in_pvs;
  t.in_phs = c_in_phs;
  t.set_area_portal = c_set_area_portal;
  t.areas_connected = c_areas_connected;
  t.linkentity = c_linkentity;
  t.unlinkentity = c_unlinkentity;
  t.box_edicts = c_box_edicts;
  t.pmove = c_pmove;
  t.multicast = c_multicast;
  t.unicast = c_unicast;
  t.write_char = c_write_char;
  t.write_byte = c_write_byte;
  t.write_short = c_write_short;
  t.write_long = c_write_long;
  t.write_float = c_write_float;
  t.write_string = c_write_string;
  t.write_position = c_write_position;
  t.write_dir = c_write_dir;
  t.write_angle = c_write_angle;
  t.tag_malloc = c_tag_malloc;
  t.tag_free = c_tag_free;
  t.free_tags = c_free_tags;
  t.cvar = c_cvar;
  t.cvar_set = c_cvar_set;
  t.cvar_forceset = c_cvar_forceset;
  t.argc = c_argc;
  t.argv = c_argv;
  t.args = c_args;
  t.add_command_string = c_add_command_string;
  t.debug_graph = c_debug_graph;
}

}  // namespace shooters_edge::server
