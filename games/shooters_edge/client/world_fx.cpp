// The window's effects each frame (world_fx.hpp).
//
// ge:summary The window's effects each frame: trails, explosions, their lights, the screen's blend
// ge:keywords world-fx step_fx trail explosion screen-blend face_hit fx_switches held-beam
// ge:keywords warp ghost
// ge:skill render shooter
#include "shooters_edge/client/world_fx.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "brush/light_probe.hpp"
#include "brush/map.hpp"
#include "camera/pose.hpp"
#include "classicenhance/replace.hpp"
#include "classicenhance/weapons.hpp"
#include "core/math.hpp"
#include "core/prng.hpp"
#include "detail/receivers.hpp"
#include "detail/surfaces.hpp"
#include "detail/vec.hpp"
#include "shooter/hull.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/app/monster_fx.hpp"
#include "shooters_edge/app/weapon_fx.hpp"
#include "shooters_edge/client/face_hit.hpp"
#include "shooters_edge/client/q2_enhance_fx.hpp"
#include "shooters_edge/client/view.hpp"

namespace shooters_edge::client {

namespace fx = shooters_edge::effects;

namespace {

constexpr double kUpWay[3] = {0, 1, 0};  // a scripted burst's way where nothing else gives one

// The specks thrown from here on taken back when this goes out of scope while `on` (a trail
// Quake II's cl_disable_particles leaves out).
struct BirthsCut {
  fx::Particles& births;
  size_t keep = 0;
  bool on = false;
  BirthsCut(fx::Particles& b, bool cut) : births(b), keep(b.size()), on(cut) {}
  BirthsCut(const BirthsCut&) = delete;
  BirthsCut& operator=(const BirthsCut&) = delete;
  ~BirthsCut() {
    // Shrinking never allocates; erase, unlike resize, is not one that may grow.
    if (on && births.size() > keep)
      births.erase(births.begin() + static_cast<std::ptrdiff_t>(keep), births.end());
  }
};

// An explosion's light under Quake II's cl_dlight_hacks: 2 reaches half as far.
fx::Light q2_blast_light(const FxState& s, fx::Light light) {
  if ((s.on.q2.light_hacks & 2) != 0) light.radius *= 0.5;
  return light;
}

// Quake II's rail trail as `cl_railtrail_type` asks (1 the core, 2 the core and its spiral),
// from `from` to `to` (engine metres), fading over `cl_railtrail_time`.
void q2_rail_trail(FxState& s, int entity, const double from[3], const double to[3], double t) {
  const ge::classic::Q2FxSwitches& q = s.on.q2;
  HeldBeam b;
  b.entity = entity;
  b.q2_trail = true;
  b.shot.born_s = t;
  b.shot.end_s = t;  // let go at once: the whole of its life its fade
  b.shot.seed = s.seed++;
  for (int k = 0; k < 3; ++k) b.shot.from[k] = from[k], b.shot.to[k] = to[k];
  b.seen = t;
  b.trail_fade_s = static_cast<float>(q.rail_time);
  b.trail_m = static_cast<float>(q.core_width * ge::brush::kUnitM);
  ge::classic::q2_colour(q.core_colour, b.trail_rgb);  // an unknown name keeps red
  s.beams.push_back(b);
  if (q.rail_type < 2) return;
  b.trail_radius_m = static_cast<float>(q.spiral_radius * ge::brush::kUnitM);
  b.trail_rgb[0] = 0, b.trail_rgb[1] = 0, b.trail_rgb[2] = 1;
  ge::classic::q2_colour(q.spiral_colour, b.trail_rgb);  // an unknown name keeps blue
  if (b.trail_radius_m > 0) s.beams.push_back(b);
}

// The unit way from `p` toward the eye (engine metres): a scripted hit's `along`.
std::array<double, 3> toward(const double p[3], const ge::camera::Vec3& eye) {
  const double d[3] = {eye.x - p[0], eye.y - p[1], eye.z - p[2]};
  const double len = ge::core::hypot3(d[0], d[1], d[2]);
  if (len < 1e-9) return {0, 1, 0};
  return {d[0] / len, d[1] / len, d[2] / len};
}

// The classic explosion sprite's frames and their rate.
constexpr int kSpriteFrames = 6;
constexpr double kSpriteFps = 10;
// The placed effects' entity numbers (below every static's): the sprites', then the bolts'.
constexpr int kSpriteEntities = -(1 << 20), kBoltEntities = -(1 << 21);
constexpr int kPlaceEntities = -(1 << 22);  // loc_draw's markers
constexpr int kArcEntity = -(1 << 22);      // the arc across the player's gun's prongs
// The hot air a nail's flight and a rocket's motor leave (render/haze.hpp's presets).
const ge::render::HazeLook* const kNailHaze = ge::render::haze_look("haze.nail");
const ge::render::HazeLook* const kRocketHaze = ge::render::haze_look("haze.rocket");

// A departing figure's entity in the models drawn: its own, moved out of every real one's way.
constexpr int kLeavingBase = -1000000;

void put(const ge::camera::Vec3& v, double out[3]) {
  out[0] = v.x;
  out[1] = v.y;
  out[2] = v.z;
}

// Whether a player's warp began near `at` (map units) a moment ago: the program's fog for it.
bool near_warp(const WarpWatch& w, const sh::Vec3f& at, double t) {
  for (const Warp& v : w.live) {
    const float dx = v.at.x - at.x, dy = v.at.y - at.y, dz = v.at.z - at.z;
    if (t - v.t0 < 0.25 && dx * dx + dy * dy + dz * dz < 96.0f * 96.0f) return true;
  }
  return false;
}

// `loc_draw`'s heads (place_names.hpp's `place_markers`), the nearest under a white light of
// 200 units.
void place_markers(const FxScene& l, const FxState& s, double t, FxFrame& out) {
  if (s.places == nullptr) return;
  thread_local std::vector<PlaceMarker> marks;
  if (!place_markers(*s.places, t, marks)) return;
  int n = 0;
  for (const PlaceMarker& k : marks) {
    for (const EffectModel head : {EffectModel::place_q1, EffectModel::place_q2}) {
      Placed m;
      m.entity = kPlaceEntities - n++;
      m.modelindex = l.effect_models + static_cast<int>(head);
      m.origin = k.at;
      out.models.push_back(m);
    }
    if (!k.nearest) continue;
    fx::Light light;
    put(engine_point(k.light), light.pos);
    light.radius = 200 * ge::brush::kUnitM;
    light.colour[0] = light.colour[1] = light.colour[2] = 1;
    out.lights.push_back(light);
  }
}

// A direction in the map's axes as a unit one in the engine's.
void engine_dir(const sh::Vec3f& d, double out[3]) {
  put(engine_point(d), out);
  const double n = ge::core::hypot3(out[0], out[1], out[2]);
  for (int k = 0; k < 3; ++k) out[k] = n > 0 ? out[k] / n : 0;
  if (n == 0) out[1] = 1;
}

// The surface nearest `at` (map units) within kNearSurfaceU along the six axes: its normal (out of
// it, engine axes, unit) and how far off it is (m); false in the open.
constexpr float kNearSurfaceU = 40;
// A map of brushes (the 38 family) has no clip hulls to trace: the face a line through `at` from
// `back` units behind it along `dir` crosses there (face_hit), its normal (engine axes).
bool brush_face_near(const LoadedMap& m, const sh::Vec3f& at, const sh::Vec3f& dir, float back,
                     double normal[3]) {
  const sh::Vec3f from = {at.x - dir.x * back, at.y - dir.y * back, at.z - dir.z * back};
  uint32_t texture = 0;
  sh::Vec3f n{};
  ge::detail::SurfaceClass sc = ge::detail::SurfaceClass::unknown;
  float light[3], st[2];
  if (!face_hit(m.bsp.map, from, at, &texture, &n, &sc, light, st)) return false;
  engine_dir(n, normal);
  return true;
}

bool surface_near(const LoadedMap& m, const sh::Vec3f& at, double normal[3], double* off_m) {
  if (m.bsp.world.models.empty()) {  // brushes: the face a few units off along each axis
    for (int axis = 0; axis < 3; ++axis)
      for (const float sign : {-1.0f, 1.0f}) {
        const sh::Vec3f dir = {axis == 0 ? sign : 0.0f, axis == 1 ? sign : 0.0f,
                               axis == 2 ? sign : 0.0f};
        if (brush_face_near(m, at, dir, kNearSurfaceU, normal)) {
          *off_m = 0;
          return true;
        }
      }
    return false;
  }
  const sh::Hull& hull = m.bsp.world.models[0].hulls[0];
  float best = 1;
  for (int axis = 0; axis < 3; ++axis)
    for (const float sign : {-1.0f, 1.0f}) {
      sh::Vec3f to = at;
      (axis == 0 ? to.x : axis == 1 ? to.y : to.z) += sign * kNearSurfaceU;
      const sh::HullTrace tr = sh::trace_hull(hull, at, to);
      if (tr.startsolid || tr.fraction >= best) continue;
      best = tr.fraction;
      engine_dir(tr.plane.normal, normal);
    }
  *off_m = best * kNearSurfaceU * ge::brush::kUnitM;
  return best < 1;
}

// The surface a missile flying into `at` (map units) struck: the nearest missile of a frame ago
// within kStruckU, the line on along its flight from the blast for kNearSurfaceU; its normal
// (engine axes) and how far off it the blast is (m). False for none (a grenade at rest, an air
// burst).
constexpr float kStruckU = 96;
bool struck_along(const LoadedMap& m, const std::unordered_map<int, FxState::Flight>& flights,
                  const sh::Vec3f& at, double normal[3], double* off_m) {
  const FxState::Flight* best = nullptr;
  float nearest = kStruckU * kStruckU;
  for (const auto& [e, f] : flights) {
    const float dx = f.at.x - at.x, dy = f.at.y - at.y, dz = f.at.z - at.z;
    const float d2 = dx * dx + dy * dy + dz * dz;
    if (d2 < nearest && (f.dir.x != 0 || f.dir.y != 0 || f.dir.z != 0)) {
      nearest = d2;
      best = &f;
    }
  }
  if (best == nullptr) return false;
  if (m.bsp.world.models.empty()) {  // brushes: the face its flight crossed there
    if (!brush_face_near(m, at, best->dir, kNearSurfaceU, normal)) return false;
    *off_m = 0;
    return true;
  }
  const sh::Hull& hull = m.bsp.world.models[0].hulls[0];
  const sh::Vec3f to = {at.x + best->dir.x * kNearSurfaceU, at.y + best->dir.y * kNearSurfaceU,
                        at.z + best->dir.z * kNearSurfaceU};
  const sh::HullTrace tr = sh::trace_hull(hull, at, to);
  if (tr.startsolid || tr.fraction >= 1) return false;
  engine_dir(tr.plane.normal, normal);
  *off_m = tr.fraction * kNearSurfaceU * ge::brush::kUnitM;
  return true;
}

// The floor under a hit at `at` (map units) by a round from `shooter`: the first solid a line
// straight down from two units back along the round's line meets, as world y (m); far below when
// none is within kFloorReachU.
constexpr float kFloorReachU = 1024;
constexpr float kOverBlastU = 16;    // a blast's floor is looked for from this far over it
constexpr double kBleedJumpM = 10;   // a piece that moved this far in a frame jumped: no trail
constexpr double kOwnRocketM = 1.5;  // a rocket first seen this near the eye is the eye's own
double floor_under(const LoadedMap& m, const sh::Vec3f& at, const sh::Vec3f& shooter) {
  if (m.bsp.world.models.empty()) return -1e9;
  const sh::Hull& hull = m.bsp.world.models[0].hulls[0];
  const float bx = shooter.x - at.x, by = shooter.y - at.y, bz = shooter.z - at.z;
  const float bl = std::max(std::sqrt(bx * bx + by * by + bz * bz), 1e-3f);
  const sh::Vec3f from = {at.x + bx / bl * 2, at.y + by / bl * 2, at.z + bz / bl * 2};
  const sh::HullTrace tr = sh::trace_hull(hull, from, {from.x, from.y, from.z - kFloorReachU});
  if (tr.startsolid || tr.fraction >= 1) return -1e9;
  return engine_point(tr.endpos).y;
}

// The mean colour of texture `index` (linear), off the pack's palette; kept a map. All below 0
// without the texture's pixels or a palette.
void wall_colour(FxState& s, const ge::brush::Map& m, uint32_t index, float out[3]) {
  out[0] = out[1] = out[2] = -1;
  if (const auto c = s.wall_colours.find(index); c != s.wall_colours.end()) {
    for (int k = 0; k < 3; ++k) out[k] = c->second[k];
    return;
  }
  std::array<float, 3> mean = {-1, -1, -1};
  if (index < m.textures.size() && !m.textures[index].indexed.empty() && s.palette.size() >= 768) {
    double sum[3] = {0, 0, 0};
    const std::vector<uint8_t>& px = m.textures[index].indexed;
    for (const uint8_t i : px)
      for (int k = 0; k < 3; ++k) {
        const double c = s.palette[size_t(i) * 3 + size_t(k)] / 255.0;
        sum[k] += c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
      }
    for (int k = 0; k < 3; ++k) mean[k] = static_cast<float>(sum[k] / double(px.size()));
  }
  s.wall_colours[index] = mean;
  for (int k = 0; k < 3; ++k) out[k] = mean[k];
}

// THE COLOUR WHERE A ROUND WENT IN: the texture's own texels round the point `st` (Vertex::st's
// kind) the hole is pressed at, kHolePatch across and wrapping as the texture tiles, their mean in
// linear light through the palette; the whole texture's mean (`wall_colour`) where the map holds no
// pixels for it or its coordinates are not texels. So what a hit tears off a brick is the brick's,
// off the mortar the mortar's.
constexpr int kHolePatch = 4;  // texels: about a hole's width on a classic wall
void hole_colour(FxState& s, const ge::brush::Map& m, uint32_t index, const float st[2],
                 float out[3]) {
  const ge::brush::Texture* tex = index < m.textures.size() ? &m.textures[index] : nullptr;
  if (tex == nullptr || !m.st_texels || tex->width == 0 || tex->height == 0 ||
      tex->indexed.size() < size_t(tex->width) * tex->height || s.palette.size() < 768) {
    wall_colour(s, m, index, out);
    return;
  }
  const int w = static_cast<int>(tex->width), h = static_cast<int>(tex->height);
  const int x0 = static_cast<int>(std::floor(st[0])) - kHolePatch / 2;
  const int y0 = static_cast<int>(std::floor(st[1])) - kHolePatch / 2;
  double sum[3] = {0, 0, 0};
  for (int dy = 0; dy < kHolePatch; ++dy)
    for (int dx = 0; dx < kHolePatch; ++dx) {
      const int x = ((x0 + dx) % w + w) % w, y = ((y0 + dy) % h + h) % h;
      const uint8_t i = tex->indexed[size_t(y) * size_t(w) + size_t(x)];
      for (int k = 0; k < 3; ++k) {
        const double c = s.palette[size_t(i) * 3 + size_t(k)] / 255.0;
        sum[k] += c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
      }
    }
  for (int k = 0; k < 3; ++k) out[k] = static_cast<float>(sum[k] / (kHolePatch * kHolePatch));
}

// The map's world faces (no sky, no liquid) as the marks' receivers, in engine metres, each
// triangle wound to face the way its face's normal does.
std::shared_ptr<const MarkFaces> receivers_of(const ge::brush::Map& m) {
  namespace gd = ge::detail;
  if (m.models.empty()) return nullptr;
  auto faces = std::make_shared<MarkFaces>();
  gd::TriMesh& mesh = faces->mesh;
  const ge::brush::Model& world = m.models[0];
  const uint32_t end = std::min<uint32_t>(world.first_surface + world.surface_count,
                                          static_cast<uint32_t>(m.surfaces.size()));
  for (uint32_t si = world.first_surface; si < end; ++si) {
    const ge::brush::Surface& sf = m.surfaces[si];
    if (sf.kind == ge::brush::SurfaceKind::sky || sf.kind == ge::brush::SurfaceKind::liquid)
      continue;
    if (sf.texture < m.textures.size() &&
        (m.textures[sf.texture].sky || m.textures[sf.texture].liquid != ge::brush::Liquid::none))
      continue;
    for (uint32_t t = 0; t + 2 < sf.count; t += 3) {
      gd::Vec3 c[3];
      for (int k = 0; k < 3; ++k) {
        double e[3];
        ge::brush::to_engine(m.vertices[m.indices[sf.first + t + k]].pos, e);
        c[k] = {e[0], e[1], e[2]};
      }
      double n[3];
      ge::brush::to_engine(m.vertices[m.indices[sf.first + t]].nrm, n);
      const gd::Vec3 front = gd::cross(gd::sub(c[1], c[0]), gd::sub(c[2], c[0]));
      if (front.x * n[0] + front.y * n[1] + front.z * n[2] < 0) std::swap(c[1], c[2]);
      for (const gd::Vec3& v : c) {
        mesh.idx.push_back(static_cast<uint32_t>(mesh.pos.size()));
        mesh.pos.push_back(v);
      }
      mesh.tag.push_back(
          static_cast<uint16_t>(std::min<uint32_t>(sf.texture, 0xffff)));  // its texture
    }
  }
  faces->receivers = gd::bin_receivers(mesh, 1);  // points into faces->mesh, kept beside it
  faces->probe = ge::brush::make_light_probe(m);
  return faces;
}

// A mark pressed onto the receivers (nothing without them): its slot, or -1.
int press(FxState& s, const ge::detail::DecalAt& d);

// A square mark pressed onto the receivers (nothing without them).
void mark(FxState& s, ge::detail::DecalKind kind, const double at[3], const double n[3],
          double size_m, double spin, uint32_t variant, double t) {
  ge::detail::DecalAt d;
  d.at = {at[0], at[1], at[2]};
  d.normal = {n[0], n[1], n[2]};
  d.size_m = size_m;
  d.spin_rad = spin;
  d.now_s = t;
  d.variant = variant;
  d.kind = kind;
  press(s, d);
}

// A held beam's burn: a piece of its line, or its melt pressed again in its own slot.
void beam_mark(FxState& s, HeldBeam& b, const ge::render::BeamBurn& burn) {
  ge::detail::DecalAt d;
  d.at = {burn.at[0], burn.at[1], burn.at[2]};
  d.normal = {burn.normal[0], burn.normal[1], burn.normal[2]};
  d.size_m = burn.size_m;
  d.spin_rad = burn.spin_rad;
  d.now_s = burn.born_s;
  d.variant = burn.variant;
  if (burn.melt == 0) {
    d.kind = ge::detail::DecalKind::burn_line;
    d.length_m = burn.length_m;
    d.along = {burn.along[0], burn.along[1], burn.along[2]};
    press(s, d);
    return;
  }
  d.kind = ge::detail::DecalKind::melt;
  if (burn.melt == b.melt) {
    d.slot = b.melt_slot;
    d.serial = b.melt_serial;
  }
  b.melt = burn.melt;
  b.melt_slot = press(s, d);
  b.melt_serial = b.melt_slot >= 0 ? s.marks.decals[static_cast<size_t>(b.melt_slot)].serial : 0;
}

int press(FxState& s, const ge::detail::DecalAt& placed) {
  if (!s.receivers) return -1;
  ge::detail::DecalAt d = placed;
  d.variant %= ge::render::kMarkVariants;
  const double at[3] = {d.at.x, d.at.y, d.at.z};
  const int slot = ge::detail::place_decal(s.marks, s.receivers->receivers, d);
  if (slot < 0 || s.marked == nullptr) return slot;
  // The light it lies in: the lightmap under it, a luxel of 128 a surface as painted.
  if (s.mark_light.size() < s.marks.capacity) s.mark_light.assign(s.marks.capacity, {1, 1, 1});
  const float p[3] = {static_cast<float>(at[0] / ge::brush::kUnitM),
                      static_cast<float>(at[2] / ge::brush::kUnitM),
                      static_cast<float>(at[1] / ge::brush::kUnitM)};
  float rgb[3];
  std::array<float, 3>& lit = s.mark_light[static_cast<size_t>(slot)];
  if (ge::brush::light_below(s.receivers->probe, *s.marked, p, rgb))
    lit = {2 * rgb[0], 2 * rgb[1], 2 * rgb[2]};
  else
    lit = {0.5f, 0.5f, 0.5f};
  return slot;
}

// The marks as the beams' pass's burns, each in the light it was pressed in.
void burns_of(const FxState& s, double t, std::vector<ge::render::BurnVertex>& out) {
  out.clear();
  if (s.receivers) ge::render::mark_burns(s.marks, t, s.mark_light, out);
}

}  // namespace

void glow_mark(FxState& s, const double at[3], const double n[3], const ge::render::GlowMark& m,
               double t) {
  if (!s.on.decals) return;
  ge::detail::DecalAt d;
  d.at = {at[0], at[1], at[2]};
  d.normal = {n[0], n[1], n[2]};
  d.size_m = m.size_m;
  d.ember_s = m.glow_s;
  d.now_s = t;
  const uint32_t own = s.seed++;  // its own variant and turn
  d.variant = own;
  d.spin_rad = static_cast<double>(own * 2654435761u % 360) * ge::core::kPi / 180;
  d.kind = ge::detail::DecalKind::melt;
  press(s, d);
}

void warp_dust(const Warp& w, const fx::Dust& d, double was, double age, uint32_t seed,
               fx::Particles* out) {
  if (w.arriving) {
    fx::dust_in(d, was, age, arrival_time, seed, out);
    return;
  }
  fx::dust_off(d, warp_shown(w, std::max(was, 0.0)), warp_shown(w, age), seed, out);
}

fx::Dust player_dust(const sh::Vec3f& origin, const sh::Vec3f& angles, int entity) {
  fx::Dust d;
  const ge::camera::Vec3 at = engine_point(origin);
  put(at, d.at);
  const float ang[3] = {angles.x, angles.y, angles.z};
  models::rotation_of(ang, true, d.rotation);
  const double yaw = double(angles.y) * 3.14159265358979323846 / 180;
  const ge::camera::Vec3 ahead = engine_point(
      {origin.x + float(64 * std::cos(yaw)), origin.y + float(64 * std::sin(yaw)), origin.z});
  double f[3] = {ahead.x - at.x, 0, ahead.z - at.z};
  const double fl = std::sqrt(f[0] * f[0] + f[2] * f[2]);
  for (double& c : f) c = fl > 0 ? c / fl : 0;
  if (fl == 0) f[2] = 1;
  for (int k = 0; k < 3; ++k) {
    d.facing[k] = f[k];
    d.wind[k] = -f[k];
  }
  d.low = kBoxLowUnits * ge::brush::kUnitM;
  d.high = (kBoxHighUnits - 4) * ge::brush::kUnitM;  // the head's top, under the box's
  // The sweep: ahead and up, so the face and the head lead.
  constexpr double kAhead = 0.894427191, kUp = 0.447213595;
  for (int k = 0; k < 3; ++k) d.warp.axis[k] = float(f[k] * kAhead + (k == 1 ? kUp : 0));
  d.warp.low = float(-d.half_depth * kAhead + d.low * kUp);
  d.warp.high = float(d.half_depth * kAhead + d.high * kUp);
  d.warp.seed = float(uint32_t(entity) % 16u) * 1.37f;
  return d;
}

FxSwitches fx_switches(const look::Picture& picture) {
  FxSwitches s;
  s.lerp.frames = picture.lerp_models;
  s.lerp.moves = picture.lerp_move;
  s.gibs_hidden = picture.gib_filter;  // cl_gibfilter (Quake II's cl_gibs 0)
  s.muzzle_flash = picture.muzzle_flash;
  s.q2 = picture.q2fx;  // Quake II's cl_disable_particles, cl_dlight_hacks ...
  s.fake_shaft = std::clamp(picture.fake_shaft, 0.0, 1.0);
  if (look::look_of(picture) == look::Look::classic) return s;
  s.muzzles = picture.ce.muzzles;
  const ge::classicenhance::Replacement* blast =
      ge::classicenhance::replacement_of(kEffectModels[size_t(EffectModel::explosion)]);
  s.explosions = picture.ce.explosions && blast != nullptr && blast->effect == "explosion";
  s.trails = picture.ce.trails;
  s.beams = picture.ce.beams;
  s.hits = picture.ce.hits;
  s.decals = picture.ce.decals;
  s.warps = picture.ce.warps;
  s.arrival = picture.ce.warps ? std::clamp(picture.arrival, 0.0, 1.0) : 0;
  s.flares = picture.ce.flares;
  s.bubbles = picture.ce.bubbles;
  s.monsters = picture.ce.monsters;
  s.blood = picture.ce.blood;
  s.blood_life_s = picture.blood_time > 0 ? picture.blood_time : -1;
  s.blood_amount = std::clamp(picture.blood_amount, 0.0, 3.0);
  s.respawns = picture.ce.respawns;
  s.shells = picture.ce.shells;
  return s;
}

namespace {

// THE MONSTERS' MISSILES (app/monster_fx.hpp): every drawn model the replacement table or its
// trail flags name a monster's shot lays its trail over the stretch it flew since the last frame
// (the classic specks, or under ce_monsters the modern look) and carries its light and flame; one
// gone this frame is kept for the splash where it landed, and a laser gone throws its sparks there.
// The monsters' shots are taken out of `missiles`, so the player's weapons' trails pass them by.
constexpr double kMonsterJumpM = 10;  // a shot that moved this far in a frame jumped: no trail
// A monster's gun, from its origin (map units: ahead, up), and a flash again after this long.
constexpr float kMonsterGunU = 22, kMonsterGunUpU = 10;
constexpr double kMonsterRefireS = 0.15;
constexpr float kPlayerGunU = 18, kPlayerGunUpU = 16;  // a player's gun (map units: ahead, up)
constexpr int kMuzzleFlash = 2;                        // the entity's muzzle-flash effect bit
void monster_missiles(const FxScene& l, FxState& s, double t, double dt, FxFrame& out,
                      std::vector<Missile>& missiles) {
  s.monster_next.clear();
  s.monster_gone.clear();
  const bool modern = s.on.monsters;
  if (s.models != nullptr) {
    for (const Placed& m : out.models) {
      if (m.entity <= 0 || m.modelindex <= 0 ||
          m.modelindex >= static_cast<int>(s.models->kinds.size()))
        continue;
      const ModelKind& k = s.models->kinds[size_t(m.modelindex)];
      fx::MonsterShot kind = fx::monster_shot_of(k.effect);
      if (kind == fx::MonsterShot::none) kind = fx::monster_shot_of_flags(k.flags);
      if (kind == fx::MonsterShot::none && (k.flags & fx::kTrailZombieGib) != 0)
        kind = fx::MonsterShot::flesh;
      // A zombie's flesh in the classic look is the bleeding pieces' (above).
      if (kind == fx::MonsterShot::none || (kind == fx::MonsterShot::flesh && !modern)) continue;
      FxState::MonsterFlight now;
      now.at = engine_point(m.origin);
      now.map_at = m.origin;
      now.kind = kind;
      double to[3];
      put(now.at, to);
      const auto was = s.monster_flights.find(m.entity);
      if (was != s.monster_flights.end() && was->second.kind == kind) {
        double from[3];
        put(was->second.at, from);
        const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
        const double len = ge::core::hypot3(d[0], d[1], d[2]);
        now.count = was->second.count;
        for (int c = 0; c < 3; ++c) now.dir[c] = was->second.dir[c];
        if (len > 1e-4 && len < kMonsterJumpM) {
          for (int c = 0; c < 3; ++c) now.dir[c] = d[c] / len;
          if (!modern)
            now.count =
                fx::classic_tracer(kind, from, to, now.count, s.seed++, s.palette, &out.births);
          else
            fx::monster_trail(kind, from, to, s.seed++, &out.births);
        }
      }
      if (modern) {
        double speed = 0;
        if (was != s.monster_flights.end() && dt > 0) {
          const ge::camera::Vec3 a = was->second.at;
          speed =
              std::min(ge::core::hypot3(now.at.x - a.x, now.at.y - a.y, now.at.z - a.z) / dt, 60.0);
        }
        fx::monster_head(kind, to, now.dir, speed, &out.births);
        fx::Light light;
        if (fx::monster_glow(kind, to, now.dir, uint32_t(m.entity), t, &light))
          out.lights.push_back(light);
        ge::render::MuzzleFire flame;
        if (fx::monster_flame(kind, to, now.dir, uint32_t(m.entity), t, &flame))
          out.flames.push_back(flame);
      } else if (kind == fx::MonsterShot::lava) {
        out.lights.push_back(fx::rocket_light(to));  // the classic's light on a rocket's trail
      }
      s.monster_next[m.entity] = now;
    }
  }
  // A monster's flash (the entity's muzzle-flash bit: a soldier's or an enforcer's shot, a caster's
  // throw): a light at its hands, and under ce_monsters the muzzle fire at a gun's (`monster.gun`).
  // A flash is new when the last began more than kMonsterRefireS ago.
  for (const Placed& m : out.models) {
    if (m.entity <= 0 || (m.effects & kMuzzleFlash) == 0 || s.on.muzzle_flash == 0) continue;
    const double yaw = m.angles.y * ge::core::kPi / 180;
    // Another player's gun (the own is the hand's): its light a little ahead and above its middle.
    if (m.entity <= static_cast<int>(sh::kMaxClients)) {
      if (m.entity == l.own || !l.flash_bit) continue;
      double at[3];
      const sh::Vec3f gun = {m.origin.x + float(std::cos(yaw) * kPlayerGunU),
                             m.origin.y + float(std::sin(yaw) * kPlayerGunU),
                             m.origin.z + kPlayerGunUpU};
      put(engine_point(gun), at);
      out.lights.push_back(fx::muzzle_light(at));
      continue;
    }
    const sh::Vec3f gun = {m.origin.x + float(std::cos(yaw) * kMonsterGunU),
                           m.origin.y + float(std::sin(yaw) * kMonsterGunU),
                           m.origin.z + kMonsterGunUpU};
    double at[3], dir[3];
    put(engine_point(gun), at);
    engine_dir({float(std::cos(yaw)), float(std::sin(yaw)), 0}, dir);
    out.lights.push_back(fx::muzzle_light(at));
    auto [it, fresh] = s.monster_fired.try_emplace(m.entity, t);
    if (!fresh && t - it->second > kMonsterRefireS) it->second = t;
  }
  std::erase_if(s.monster_fired, [&](const auto& kv) { return t - kv.second > 1; });
  if (modern) {
    for (const Placed& m : out.models) {
      const auto f = s.monster_fired.find(m.entity);
      if (f == s.monster_fired.end() || t - f->second > fx::kMonsterMuzzleS) continue;
      // Only a gun's: a caster's flash as it throws is a light alone.
      if (s.models == nullptr || m.modelindex <= 0 ||
          m.modelindex >= static_cast<int>(s.models->kinds.size()) ||
          s.models->kinds[size_t(m.modelindex)].effect != "monster.gun")
        continue;
      // At the barrel's tip as this frame holds it, so the fire rides the gun.
      const double yaw = m.angles.y * ge::core::kPi / 180;
      double at[3], dir[3];
      engine_dir({float(std::cos(yaw)), float(std::sin(yaw)), 0}, dir);
      if (!held_tip(*s.models, m, t, dir, at)) {
        const sh::Vec3f gun = {m.origin.x + float(std::cos(yaw) * kMonsterGunU),
                               m.origin.y + float(std::sin(yaw) * kMonsterGunU),
                               m.origin.z + kMonsterGunUpU};
        put(engine_point(gun), at);
      }
      out.flames.push_back(fx::monster_muzzle(
          at, dir, uint32_t(m.entity) * 31u + uint32_t(f->second * 77), f->second));
    }
  }
  // Those gone, in entity order (each draws on the seed in turn).
  thread_local std::vector<int> gone;
  gone.clear();
  for (const auto& kv : s.monster_flights)
    if (!s.monster_next.contains(kv.first)) gone.push_back(kv.first);
  std::sort(gone.begin(), gone.end());
  for (const int entity : gone) {
    const FxState::MonsterFlight& f = s.monster_flights[entity];
    s.monster_gone.push_back(f);
    // A laser leaves no message where it ends: its sparks where it was last seen, off the face it
    // was flying into.
    if (f.kind != fx::MonsterShot::laser || !modern || !l.map) continue;
    double normal[3], off = 0, p[3];
    const bool face = surface_near(*l.map, f.map_at, normal, &off);
    put(f.at, p);
    const double floor_y = floor_under(*l.map, f.map_at, f.map_at);
    fx::monster_splash(f.kind, p, face ? normal : nullptr, f.dir, floor_y, s.seed++, t, &s.blasts,
                       &out.births);
    FxState::MonsterSplash sp;
    put(f.at, sp.at);
    sp.t = t;
    sp.kind = f.kind;
    s.monster_splashes.push_back(sp);
  }
  s.monster_flights.swap(s.monster_next);
  std::erase_if(missiles, [&](const Missile& m) { return s.monster_flights.contains(m.entity); });
  // The splashes' light while it fades.
  std::erase_if(s.monster_splashes, [&](const FxState::MonsterSplash& sp) {
    fx::Light light;
    if (!modern || !fx::splash_light(sp.kind, sp.at, t - sp.t, &light))
      return t - sp.t > fx::kSplashLightS || t < sp.t;
    out.lights.push_back(light);
    return false;
  });
}

// Where a flier's spit or a knight's fire bolt landed (the temporary entity's codes 7 and 8):
// the classic specks of its colour, or under ce_monsters its splash off the face it flew into.
bool monster_spit_hit(const FxScene& l, FxState& s, const Effect& e, const double p[3], double t,
                      FxFrame& out) {
  if (e.code != 7 && e.code != 8) return false;
  const fx::MonsterShot kind = e.code == 7 ? fx::MonsterShot::acid : fx::MonsterShot::flame;
  if (!s.on.monsters) {
    fx::classic_spit_hit(kind, p, s.seed++, s.palette, &out.births);
    return true;
  }
  // The way it flew: the nearest shot of its kind gone this frame (else straight down).
  double dir[3] = {0, -1, 0};
  double best = 1e9;
  for (const FxState::MonsterFlight& f : s.monster_gone) {
    if (f.kind != kind) continue;
    const double d = ge::core::hypot3(f.at.x - p[0], f.at.y - p[1], f.at.z - p[2]);
    if (d >= best) continue;
    best = d;
    for (int c = 0; c < 3; ++c) dir[c] = f.dir[c];
  }
  double normal[3], off = 0;
  const bool face = l.map && surface_near(*l.map, e.at, normal, &off);
  const double floor_y = l.map ? floor_under(*l.map, e.at, e.at) : -1e9;
  fx::monster_splash(kind, p, face ? normal : nullptr, dir, floor_y, s.seed++, t, &s.blasts,
                     &out.births);
  FxState::MonsterSplash sp;
  for (int c = 0; c < 3; ++c) sp.at[c] = p[c];
  sp.t = t;
  sp.kind = kind;
  s.monster_splashes.push_back(sp);
  return true;
}

}  // namespace

void fx_scene(const Listen& l, const ViewState& v, FxScene& out) {
  out.map = l.map.get();
  listen_area_bits(l, out.areas);
  out.light_styles = {};
  if (l.server != nullptr) out.light_styles = l.server->world.lightstyles;
  out.server = l.server ? &l.server->sv : nullptr;
  out.pose = v.pose;
  out.missiles = v.missiles;
  out.explosions = l.explosions;
  out.explosion_codes = l.explosion_codes;
  out.effects = l.effects;
  out.blows = l.blows;
  out.bonuses = l.bonuses;
  for (int k = 0; k < 4; k++) out.air_tint[k] = l.cshift[k];
  out.campaign = l.campaign_host != nullptr;
  out.q2 = listen_is_q2(l);  // its own arrivals, never the warp's fog
  const auto* rules = out.q2 && l.server != nullptr ? &l.server->world.clients[0].numbers : nullptr;
  for (int k = 0; k < 4; k++) {
    out.q2_blend[k] = rules != nullptr ? rules->blend[k] : 0;
    out.q2_apart[k] = rules != nullptr ? rules->blend_apart[k] : 0;
    out.q2_damage[k] = rules != nullptr ? rules->damage_blend[k] : 0;
  }
  out.effect_models = effect_model(l, EffectModel::explosion);
  if (l.server != nullptr) out.beam_model = listed_model(l.server->models, "progs/beam.mdl");
  place_models(l, out.models);
  if (out.q2)
    scene_lasers(l, out.beams);
  else
    out.beams.clear();
  // The items coming back, each one's time left as the picture will see it: the real item shows
  // with the first state past its time, which the window reaches `owed` sooner than the state's
  // clock says.
  const double tic = l.server != nullptr ? listen_tic(l) : 0;
  place_returning(l, ge::classicenhance::kHologramLeadS + tic, out.returning);
  for (Returning& r : out.returning) r.in_s -= std::min(l.owed, tic);
  // The picture one server frame behind the newest state, as far past it as the wall clock is.
  out.clock = {};
  if (l.server != nullptr) {
    out.clock.moves = true;
    out.clock.state_s = l.server->sv.time;
    out.clock.drawn_s = l.server->sv.time + std::min(l.owed, listen_tic(l)) - listen_tic(l);
  }
  out.bodies.clear();
  out.own = 0;
  out.placed_of = nullptr;
  out.health_of = nullptr;
  if (!l.server) return;
  const int clients = std::min(l.server->sv.num_edicts - 1, int(sh::kMaxClients));
  for (int e = 1; e <= clients; e++) {
    const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
    if (ed.free || ed.modelindex <= 0) continue;
    out.bodies.push_back({e, ed.origin, field_named(l, e, "health") > 0});
  }
  out.me = player_state(l);
  out.own = out.me.entity;
  out.me_effects = int(field_named(l, out.own, "effects"));
  out.waterlevel = field_named(l, out.own, "waterlevel");
  out.watertype = field_named(l, out.own, "watertype");
  out.me_items = int(field_named(l, out.own, "items"));
  out.eye_contents = 0;
  if (l.map != nullptr && l.map->from_file && !l.map->bsp.world.models.empty()) {
    const sh::Edict& ed = l.server->sv.edicts[size_t(out.own)];
    out.eye_contents =
        sh::point_contents(l.map->bsp.world.models[0].hulls[0], ed.origin + out.me.view_ofs);
  }
  const Listen* lp = &l;
  out.placed_of = [lp](int e) {
    Placed m;
    if (lp->server == nullptr) return m;  // a warp outliving its server draws nothing new
    const sh::Edict& ed = lp->server->sv.edicts[size_t(e)];
    m.entity = e;
    m.modelindex = ed.modelindex;
    m.frame = int(field_named(*lp, e, "frame"));
    m.skin = int(field_named(*lp, e, "skin"));
    m.origin = ed.origin;
    m.angles = ed.angles;
    return m;
  };
  out.health_of = [lp](int e) { return field_named(*lp, e, "health"); };
}

void step_fx(const Listen& l, const ViewState& v, FxState& s, double t, double dt, FxFrame& out) {
  thread_local FxScene scene;
  fx_scene(l, v, scene);
  step_fx(scene, s, t, dt, out);
}

namespace {

// The player's own arc gun's beam, turned to the aim (cl_fakeshaft): not the grappling beam, not
// a rail's or a bolt's streak, and only with a map to trace and a player to be.
bool turned_own(const FxScene& l, const FxState& s, const HeldBeam& b) {
  return s.on.fake_shaft > 0 && l.own > 0 && b.entity == l.me.entity && l.map != nullptr &&
         !b.grapple && !b.rail && !b.plasma && !b.q2_trail && !b.laser && !b.across;
}

// A map point (units) from an engine one (metres), and a map direction from an engine one.
sh::Vec3f map_point(const ge::camera::Vec3& e) {
  const double u = 1 / ge::brush::kUnitM;
  return {static_cast<float>(e.x * u), static_cast<float>(e.z * u), static_cast<float>(e.y * u)};
}

// The edge `b` crossed since last frame, now pointing along `dir` from `from` (map units): kept in
// `b.edges` for its burns (own_beam.hpp's `beam_edge`).
void cross_edge(const FxScene& l, HeldBeam& b, const sh::Vec3f& from, const sh::Vec3f& dir) {
  b.edges = 0;
  sh::Vec3f at[2], n[2];
  if (b.was && l.map != nullptr &&
      beam_edge(l.map, from, b.was_dir, dir, l.bodies, b.entity, at, n) == 2) {
    for (int i = 0; i < 2; ++i) {
      put(engine_point(at[i]), b.edge_at[i]);
      engine_dir(n[i], b.edge_n[i]);
    }
    b.edges = 2;
  }
  b.was_dir = dir;
  b.was = true;
}

// `b` traced again from this frame's eye along its aim (`own_beam`), its sparks thrown at the new
// end when the server's message owed them.
void turn_own(const FxScene& l, FxState& s, HeldBeam& b, FxFrame& out) {
  const ge::camera::Vec3 f = ge::camera::look_forward(l.pose.yaw, l.pose.pitch);
  const sh::Vec3f view = {static_cast<float>(f.x), static_cast<float>(f.z),
                          static_cast<float>(f.y)};
  sh::Vec3f from, to, n;
  if (!own_beam(l.map, map_point(l.pose.eye), view, b.server_from, b.server_to, s.on.fake_shaft,
                l.bodies, l.own, from, to, n))
    return;
  cross_edge(l, b, from, {to.x - from.x, to.y - from.y, to.z - from.z});
  b.map_from = from;
  b.map_to = to;
  put(engine_point(from), b.shot.from);
  put(engine_point(to), b.shot.to);
  engine_dir(n, b.shot.normal);
  if (!b.sparks_due) return;
  b.sparks_due = false;
  if (s.on.beams) fx::bolt_hit(b.shot.to, b.shot.normal, s.seed++, &out.births);
}

}  // namespace

void step_fx(const FxScene& l, FxState& s, double t, double dt, FxFrame& out) {
  out.models = l.models;
  {
    thread_local std::vector<LerpModel> items;
    items.resize(out.models.size());
    for (size_t i = 0; i < out.models.size(); i++) {
      const Placed& p = out.models[i];
      items[i] = {.entity = p.entity,
                  .modelindex = p.modelindex,
                  .frame = p.frame,
                  .warping = p.warp.shown < 1,
                  .origin = p.origin,
                  .angles = p.angles};
    }
    lerp_models(s.lerp, items, t, l.clock, s.on.lerp);
    for (size_t i = 0; i < out.models.size(); i++) {
      Placed& p = out.models[i];
      p.origin = items[i].origin;
      p.angles = items[i].angles;
      p.frame_from = items[i].frame_from;
      p.frame_f = items[i].frame_f;
      p.lerp = items[i].lerp;
    }
    // A carried model rides its holder: the holder's drawn place and frames (it follows it).
    for (size_t i = 1; i < out.models.size(); i++) {
      Placed& c = out.models[i];
      const Placed& h = out.models[i - 1];
      if (c.entity < kCarriedEntity || h.entity != c.entity - kCarriedEntity) continue;
      c.origin = h.origin;
      c.angles = h.angles;
      c.frame_from = h.frame_from;
      c.frame_f = h.frame_f;
      c.lerp = h.lerp;
    }
  }
  // ce_respawns: each item about to come back, a blue hologram filling in where it will lie.
  if (s.on.respawns)
    for (const Returning& r : l.returning) {
      const ge::classicenhance::HologramLook h = ge::classicenhance::hologram_look(r.in_s, t);
      if (h.shown) out.models.push_back(hologram_of(r.item, h));
    }
  // The missiles where their models are drawn, so a trail and a glow ride the drawn rocket.
  thread_local std::vector<Missile> missiles;
  missiles.assign(l.missiles.begin(), l.missiles.end());
  if (l.clock.moves && s.on.lerp.moves)
    for (Missile& m : missiles)
      for (const Placed& p : out.models)
        if (p.entity == m.entity) {
          m.origin = p.origin;
          break;
        }
  out.births.clear();
  out.areas = l.areas;
  for (size_t k = 0; k < out.light_styles.size();
       k++)  // assigned in place: no allocation once warm
    out.light_styles[k] = k < l.light_styles.size() ? std::string_view(l.light_styles[k]) : "";
  out.lights.clear();
  out.puffs.clear();
  out.flames.clear();
  out.beams.clear();
  out.sounds.clear();
  // The marks' receivers, made once a map (ce_decals).
  const ge::brush::Map* faces = l.map ? &l.map->bsp.map : nullptr;
  if ((s.on.decals || s.on.blood || s.on.shells) && faces != s.marked) {
    s.marked = faces;
    s.receivers = faces != nullptr ? receivers_of(*faces) : nullptr;
    s.marks = ge::detail::make_pool(kMarks);
    s.mark_light.assign(kMarks, {1, 1, 1});
  }
  if (!s.on.decals && !s.on.blood && !s.on.shells) {
    s.marked = nullptr;
    s.receivers.reset();
  }
  ge::detail::expire(s.marks, t);  // blood past ce_blood_time gives its slot back

  monster_missiles(l, s, t, dt, out, missiles);

  // The eye's aim turning (rad/s: right and up), for a rocket's launch it fires.
  if (s.aimed && dt > 0) {
    double turn = l.pose.yaw - s.aim_yaw;
    turn -= 2 * ge::core::kPi * std::floor((turn + ge::core::kPi) / (2 * ge::core::kPi));
    s.aim_rate[0] = turn / dt;
    s.aim_rate[1] = (l.pose.pitch - s.aim_pitch) / dt;
  }
  s.aim_yaw = l.pose.yaw;
  s.aim_pitch = l.pose.pitch;
  s.aimed = true;

  // The pieces of a body (a gib, a head: their model files' trail flags) bleed over the stretch
  // each flew since the last frame; one new, changed or moved too far (a teleport, a new map)
  // starts again where it is.
  s.bleeding_next.clear();
  if (s.models != nullptr) {
    for (const Placed& m : l.models) {
      if (m.entity <= 0 || m.modelindex <= 0 ||
          m.modelindex >= static_cast<int>(s.models->kinds.size()))
        continue;
      // Quake II's pieces bleed by their gib bit; its model files name no trail.
      const uint32_t flags =
          s.models->kinds[size_t(m.modelindex)].flags | (m.bleeds ? fx::kTrailGib : 0u);
      if ((flags & (fx::kTrailGib | fx::kTrailZombieGib)) == 0) continue;
      // A zombie's flesh under ce_monsters is the monsters' missiles' (monster_missiles).
      if ((flags & fx::kTrailGib) == 0 && s.on.monsters) continue;
      if ((flags & fx::kTrailGib) != 0 && s.on.gibs_hidden) continue;  // cl_gibfilter: none flies
      FxState::Bleeding now{engine_point(m.origin), m.modelindex, 0};
      const auto was = s.bleeding.find(m.entity);
      if (was != s.bleeding.end() && was->second.modelindex == m.modelindex) {
        const ge::camera::Vec3 a = was->second.at;
        const double from[3] = {a.x, a.y, a.z}, to[3] = {now.at.x, now.at.y, now.at.z};
        const double far = ge::core::hypot3(to[0] - from[0], to[1] - from[1], to[2] - from[2]);
        now.carry = was->second.carry;
        now.count = was->second.count;
        if (far < kBleedJumpM && m.bleeds)  // Quake II's piece: its own reds, thinning as it flies
          fx::q2_trail(from, to, m.green_blood ? fx::Q2Trail::green_blood : fx::Q2Trail::blood,
                       now.count, s.seed++, s.palette, &out.births);
        else if (far < kBleedJumpM)
          now.carry = fx::blood_trail(
              from, to, (flags & fx::kTrailGib) != 0 ? fx::kBloodStep : fx::kThinBloodStep,
              now.carry, s.seed++, s.palette, &out.births);
      }
      s.bleeding_next.emplace(m.entity, now);
    }
  }
  s.bleeding.swap(s.bleeding_next);
  q2_sustains(s, t, out);  // Quake II's widow beam-outs growing
  // Quake II's flies round a corpse, a cycle from when they are first seen; thrown every frame.
  for (const Placed& m : l.models) {
    if (!m.flies) continue;
    auto [it, fresh] = s.flies_from.try_emplace(m.entity, t);
    if (t >= it->second + fx::kSqFliesCycleS) it->second = t;  // a cycle ended: another starts
    double at[3];
    put(engine_point(m.origin), at);
    fx::q2_flies(at, t, t - it->second, std::clamp(dt, 1.0 / 240, 0.1), s.palette, &out.births);
  }

  // The missiles: a trail over the stretch each flew since the last frame, a rocket's glow.
  s.next.clear();
  for (const Missile& m : missiles) {
    // Quake II's cl_disable_particles: a grenade's (2), a rocket's (8) or a blaster bolt's (32)
    // trail left out.
    const int np = s.on.q2.no_particles;
    const BirthsCut cut{out.births, (m.grenade && (np & 2) != 0) ||
                                        (m.rocket && !m.grenade && (np & 8) != 0) ||
                                        (m.streak && (np & 32) != 0)};
    ge::camera::Vec3 at = engine_point(m.origin);
    // A rocket's launch as it looks (ce_trails): off its true place while it leaves the tube, the
    // shooter's sway on it when it is the eye's own; its model, flame, trail, light and haze there.
    double launch_age = 1e9;
    if (m.rocket && !m.grenade && s.on.trails) {
      auto [it, fresh] = s.launched.try_emplace(m.entity);
      FxState::Launched& la = it->second;
      const ge::camera::Vec3 eye = l.pose.eye;
      const double aim[3] = {std::cos(l.pose.pitch) * std::sin(l.pose.yaw), std::sin(l.pose.pitch),
                             std::cos(l.pose.pitch) * std::cos(l.pose.yaw)};
      if (fresh) {
        la.born = t;
        la.launch.seed = static_cast<uint32_t>(m.entity);
        if (ge::core::hypot3(at.x - eye.x, at.y - eye.y, at.z - eye.z) < kOwnRocketM) {
          const ge::camera::Vec3 vel = engine_point(l.me.velocity);
          const double vv[3] = {vel.x, vel.y, vel.z};
          fx::launch_kick(aim, vv, s.aim_rate[0], s.aim_rate[1], la.launch.kick);
        }
      }
      launch_age = t - la.born;
      double dir[3] = {aim[0], aim[1], aim[2]};
      if (const auto f = s.flights.find(m.entity); f != s.flights.end()) {
        const ge::camera::Vec3 d = engine_point(f->second.dir);  // a direction: no origin in it
        const double dl = ge::core::hypot3(d.x, d.y, d.z);
        if (dl > 1e-6) dir[0] = d.x / dl, dir[1] = d.y / dl, dir[2] = d.z / dl;
      }
      double off[3];
      fx::launch_offset(launch_age, dir, la.launch, off);
      at = {at.x + off[0], at.y + off[1], at.z + off[2]};
      // Its model where it is seen (map units, z up).
      for (Placed& p : out.models)
        if (p.entity == m.entity) {
          p.origin.x += float(off[0] / ge::brush::kUnitM);
          p.origin.y += float(off[2] / ge::brush::kUnitM);
          p.origin.z += float(off[1] / ge::brush::kUnitM);
        }
    }
    const bool lit = launch_age >= fx::kIgniteS;  // the motor burning (a classic's always)
    s.next[m.entity] = at;
    if (const auto f = s.flights.find(m.entity); f != s.flights.end()) {
      const sh::Vec3f d = {m.origin.x - f->second.at.x, m.origin.y - f->second.at.y,
                           m.origin.z - f->second.at.z};
      const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
      s.flights_next[m.entity] = {
          m.origin, len > 0.5f ? sh::Vec3f{d.x / len, d.y / len, d.z / len} : f->second.dir,
          !m.rocket && !m.bolt && !m.bfg};
    } else {
      s.flights_next[m.entity] = {m.origin, {0, 0, 0}, !m.rocket && !m.bolt && !m.bfg};
    }
    double to[3];
    put(at, to);
    const auto was = s.last.find(m.entity);
    // The hot air behind a nail and a rocket's motor (a grenade is not hot), a stretch a frame.
    if (was != s.last.end() && s.on.trails && !m.grenade && lit && m.q2_mark == 0 && !m.q2_spin) {
      ge::render::HazeTrail h;
      put(was->second, h.a);
      put(at, h.b);
      h.born_s = t;
      h.look = m.rocket ? kRocketHaze : kNailHaze;
      h.seed = static_cast<uint32_t>(m.entity);
      s.haze.push_back(h);
    }
    // Quake II's other marks by their bits (client/world_fx_q2.cpp).
    if (m.q2_mark != 0 || m.q2_spin) {
      double from[3] = {0, 0, 0};
      if (was != s.last.end()) put(was->second, from);
      q2_mark_fx(s, m, was != s.last.end(), from, to, t, out);
    }
    // Quake II's glowing shots (a blaster's bolt, the BFG's ball): a light riding each, and in
    // the modern look a trail of shining specks behind it.
    if (m.bolt || m.bfg) {
      const float* rgb = m.bfg ? fx::kBfgRgb : fx::kBoltRgb;
      if (s.on.trails && was != s.last.end()) {
        double from[3];
        put(was->second, from);
        if (m.bolt && s.on.beams) {  // a streak of plasma riding the bolt, shedding sparks
          const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
          const double len = ge::core::hypot3(d[0], d[1], d[2]);
          if (len > 1e-3) {
            auto it = std::find_if(s.beams.begin(), s.beams.end(), [&](const HeldBeam& b) {
              return b.plasma && b.entity == m.entity && b.shot.end_s < 0;
            });
            const bool fresh = it == s.beams.end();
            if (fresh) {
              HeldBeam b;
              b.entity = m.entity;
              b.plasma = true;
              b.shot.born_s = t;
              b.shot.seed = s.seed++;
              s.beams.push_back(b);
              it = s.beams.end() - 1;
            }
            // Grown from the stretch it flew since the last frame to its full tail.
            const double grown =
                fresh ? 0
                      : ge::core::hypot3(to[0] - it->shot.from[0], to[1] - it->shot.from[1],
                                         to[2] - it->shot.from[2]);
            const double tail = std::min(fx::kPlasmaTailM, grown + len);
            for (int k = 0; k < 3; ++k) {
              it->shot.from[k] = to[k] - d[k] / len * tail;
              it->shot.to[k] = to[k];
              it->shot.normal[k] = -d[k] / len;
            }
            it->seen = t;
          }
          fx::plasma_sheds(from, to, s.seed++, &out.births);
        } else {
          fx::glow_trail(from, to, rgb, m.bfg ? 0.12 : 0.035, m.bfg ? 0.05 : 0.04, s.seed++,
                         &out.births);
        }
      } else if (l.q2 && m.streak && was != s.last.end()) {  // Quake II's blaster: orange specks
        double from[3];
        put(was->second, from);
        fx::q2_bolt_trail(from, to, s.seed++, s.palette, &out.births);
      }
      out.lights.push_back(fx::shot_glow(to, rgb, m.bfg ? fx::kBfgGlowM : fx::kBoltGlowM,
                                         m.bfg ? fx::kBfgGlowLevel : fx::kBoltGlowLevel, m.entity));
      continue;
    }
    if (!m.rocket) continue;
    if (was != s.last.end()) {
      double from[3];
      put(was->second, from);
      if (!s.on.trails && l.q2) {  // Quake II's own: thinning smoke, a rocket's fire beside it
        auto [it, fresh] = s.q2_trails.try_emplace(m.entity, fx::kSqTrailStart);
        fx::q2_trail(from, to, fx::Q2Trail::smoke, it->second, s.seed++, s.palette, &out.births);
        if (!m.grenade) fx::q2_rocket_fire(from, to, s.seed++, s.palette, &out.births);
      } else if (!s.on.trails) {
        if (trail_scripted(s, m.grenade ? "grenade_trail" : "rocket_trail", from, to, out)) {
        } else if (m.grenade) {
          fx::grenade_trail(from, to, s.seed++, s.palette, &out.births);
        } else {
          fx::rocket_trail(from, to, s.seed++, s.palette, &out.births);
        }
      } else if (m.grenade) {
        fx::smoke_trail(from, to, s.seed++, &out.births);
      } else if (!lit) {  // out of the tube, the motor not yet lit: a little smoke
        fx::smoke_trail(from, to, s.seed++, &out.births);
        continue;
      } else {
        fx::fire_trail(from, to, s.seed++, &out.births);
        const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
        const double len = ge::core::hypot3(d[0], d[1], d[2]);
        double dir[3] = {0, 0, 0};
        if (len > 1e-4) {
          for (int k = 0; k < 3; ++k) dir[k] = d[k] / len;
          out.flames.push_back(fx::exhaust(to, dir, static_cast<uint32_t>(m.entity), t));
        }
        // The motor's fire lights the world round it (the classic rocket's steady light below).
        out.lights.push_back(fx::motor_light(to, dir, static_cast<uint32_t>(m.entity), t));
        continue;
      }
    }
    if (m.grenade) {  // no motor: a classic grenade gives no light; its painted ends glow faintly
      if (s.on.trails && s.models != nullptr)
        for (const Placed& p : out.models) {
          if (p.entity != m.entity || p.modelindex <= 0 ||
              p.modelindex >= static_cast<int>(s.models->kinds.size()))
            continue;
          const ModelKind& k = s.models->kinds[size_t(p.modelindex)];
          if (k.glow_share > 0) out.lights.push_back(fx::grenade_glow(to, k.glow_rgb, m.entity));
          break;
        }
      continue;
    }
    if (s.on.trails) {  // its first frame seen: no flight yet, the light on it
      if (!lit) continue;
      const double still[3] = {0, 0, 0};
      out.lights.push_back(fx::motor_light(to, still, static_cast<uint32_t>(m.entity), t));
    } else {
      out.lights.push_back(fx::rocket_light(to));
    }
  }
  std::swap(s.last, s.next);
  std::erase_if(s.launched, [&](const auto& kv) { return !s.last.contains(kv.first); });
  std::erase_if(s.q2_trails, [&](const auto& kv) { return !s.last.contains(kv.first); });
  std::erase_if(s.haze,
                [&](const ge::render::HazeTrail& h) { return ge::render::haze_cold(h, t); });
  if (!s.on.trails) s.haze.clear();
  out.haze.assign(s.haze.begin(), s.haze.end());

  // The explosions: a burst once, then a light while it fades.
  if (l.explosions.size() < s.explosions_seen) {  // a new map
    s.explosions_seen = 0;
    s.lit.clear();
    s.sparked.clear();
    s.last.clear();
    s.blasts.clear();
    s.mains.clear();
    s.flights.clear();
    s.launched.clear();
    s.haze.clear();
    s.wall_colours.clear();
    s.wounds.clear();
    s.wound_health.clear();
    s.dropping.clear();
    s.treads.clear();
    s.marked = nullptr;
    s.beams.clear();
    s.q2_models.clear();
    s.q2_models_next = 0;
    s.q2_cables.clear();
    s.q2_lasers.clear();
  }
  for (; s.explosions_seen < l.explosions.size(); s.explosions_seen++) {
    const ge::camera::Vec3 at = engine_point(l.explosions[s.explosions_seen]);
    double p[3];
    put(at, p);
    // Quake II's cl_disable_particles and cl_disable_explosions by the kind it is.
    const uint8_t code =
        s.explosions_seen < l.explosion_codes.size() ? l.explosion_codes[s.explosions_seen] : 0;
    const uint8_t kind = q2_blast_kind(code);
    const bool no_specks = ge::classic::q2_no_blast_specks(s.on.q2, kind);
    const bool no_ball = ge::classic::q2_no_blast_ball(s.on.q2, kind);
    if (s.on.explosions) {
      // The surface it went off against, if one is within a ball's reach of it: the nearest hit
      // of six lines out of it.
      double normal[3] = {0, 1, 0}, off = 0;
      const sh::Vec3f boom = l.explosions[s.explosions_seen];
      bool against = l.map && struck_along(*l.map, s.flights, boom, normal, &off);
      if (!against) against = l.map && surface_near(*l.map, boom, normal, &off);
      const size_t first = s.blasts.size();
      const size_t thrown = out.births.size();
      fx::blast(p, against ? normal : nullptr, s.seed++, t, &s.blasts, &out.births,
                l.q2 ? q2_blast_of(code) : fx::BlastOf::common);
      if (no_specks) out.births.resize(thrown);
      // What it throws that falls meets what the frame draws (the pool collides); one flung out of
      // the view has the floor under the blast to come down on, and hops there.
      if (l.map) {
        // From a little over the blast (it goes off just inside what it struck, a floor's top),
        // down through the world and its brush entities (a lift's floor is one).
        const sh::Vec3f over = {boom.x, boom.y, boom.z + kOverBlastU};
        double floor_y = floor_under(*l.map, over, {over.x, over.y, over.z + kNearSurfaceU});
        if (l.server) {
          const sh::HullTrace tr =
              sh::trace_move(*l.server, over, {0, 0, 0}, {0, 0, 0},
                             {over.x, over.y, over.z - kFloorReachU}, sh::kTraceNoMonsters, -1);
          if (!tr.startsolid && tr.fraction < 1)
            floor_y = std::max(floor_y, engine_point(tr.endpos).y);
        }
        for (size_t i = thrown; i < out.births.size(); ++i) {
          ge::render::Particle& q = out.births[i];
          if (q.fall <= 0.5f || q.floor_y > -1e8f) continue;
          q.floor_y = static_cast<float>(floor_y);
          if (q.bounce <= 0) q.bounce = ge::render::kSurfaceBounce;
        }
      }
      // Its scorch on the surface it went off against.
      ge::render::Scorch sc;
      if (against && s.on.decals && ge::render::scorch_of(s.blasts[first], sc)) {
        const double on[3] = {p[0] - normal[0] * off, p[1] - normal[1] * off,
                              p[2] - normal[2] * off};
        mark(s, ge::detail::DecalKind::scorch, on, normal, sc.size_m, sc.spin_rad, sc.variant, t);
      }
      s.mains.push_back(s.blasts[first]);   // its light
      if (no_ball) s.blasts.resize(first);  // and no ball of fire
    } else if (q2_classic_blast(s, code, l.explosions[s.explosions_seen], p, no_specks, t, out)) {
      // Quake II's own: its specks, its ball or burst and its light (q2_model_fx.hpp)
    } else if (no_specks || !throw_scripted(s, "explosion", p, kUpWay, 1, -1e9, t, out)) {
      if (!no_specks) fx::explosion(p, s.seed++, s.palette, &out.births);
      s.lit.push_back({at, t, l.explosions[s.explosions_seen], no_ball});  // its light, its sprite
    }
    if (s.on.blood) blast_blood(l, s, l.explosions[s.explosions_seen], t, out);
  }
  // The players' warps: who arrived and who left, from where each entity is now.
  s.bodies.assign(l.bodies.begin(), l.bodies.end());
  s.began.clear();
  watch_warps(s.warps, s.bodies, t, dt, s.began);
  const int own = l.own;  // the local player's entity (0: none)
  out.has_self = false;
  // A player as the window places him (the local player's own too, never drawn).
  const auto placed_of = [&](int e) { return l.placed_of ? l.placed_of(e) : Placed{}; };
  // The local player, as the world would place him: unseen in his own eyes, his shadow still his.
  if (own > 0 && !l.me.dead) {
    out.self = placed_of(own);
    out.has_self = out.self.modelindex > 0;
  }
  // ce_warps off (or the classic look): the original. The figure is there or gone at once, and
  // each end of the warp flashes the teleport's fog, the campaign's program throwing its own.
  if (!s.on.warps) {
    if (!l.campaign && !l.q2) {  // Quake II throws its own (its events: q2_teleport_burst)
      for (const Warp& w : s.began) {
        double p[3];
        put(engine_point(w.at), p);
        fx::teleport_splash(p, s.seed++, s.palette, &out.births);
      }
    }
    s.leaving.clear();
    s.seen.clear();
    out.held_shown = 1;
    out.arrival_s = -1;
  } else {
    for (const Warp& w : s.began) {
      if (w.arriving) continue;
      // Leaving: the figure as it was drawn last frame, where it stood (the local player's own is
      // not drawn: his ash alone).
      const auto was = s.seen.find(w.entity);
      FxState::Leaving& g = s.leaving[w.entity];
      g.drawn = was != s.seen.end();
      g.figure = g.drawn ? was->second : placed_of(w.entity);
      g.figure.origin = w.at;
      if (s.models == nullptr || !body_triangles(*s.models, g.figure, t, g.skin)) g.skin.clear();
      g.dust = player_dust(w.at, g.figure.angles, w.entity);
    }
    s.seen.clear();
    for (Placed& m : out.models) {
      if (m.entity < 1 || m.entity > int(sh::kMaxClients)) continue;
      s.seen[m.entity] = m;
      const double shown = arrival_shown(s.warps, m.entity, t);
      if (shown >= 1) continue;
      m.warp = player_dust(m.origin, m.angles, m.entity).warp;
      m.warp.shown = float(shown);
    }
    out.held_shown = 1;  // his own weapon is never warped: he fights on at once
    // His own arrival's warp of the whole view, in the colour of the pack's teleport fog (its
    // specks' brightest, 7..14), made as bright as a colour goes.
    out.arrival_s = -1;
    for (const Warp& w : s.warps.live)
      if (w.arriving && own > 0 && w.entity == own) out.arrival_s = t - w.t0;
    if (out.arrival_s >= 0) {
      if (s.palette.size() >= 768) {
        const size_t i = size_t{14} * 3;
        const double hi = std::max({s.palette[i], s.palette[i + 1], s.palette[i + 2], uint8_t{1}});
        for (int k = 0; k < 3; ++k) out.arrival_tint[k] = s.palette[i + size_t(k)] / hi;
      } else {
        for (double& c : out.arrival_tint) c = 1;
      }
    }
    for (const Warp& w : s.warps.live) {
      const double age = t - w.t0, was = w.t0 >= t ? -1 : std::max(t - dt - w.t0, 0.0);
      if (w.arriving) {
        // His own arrival throws no ash: it would gather before his eyes as he must aim.
        if (w.entity == own) continue;
        // Onto his body as it stands now.
        const Placed m = placed_of(w.entity);
        fx::Dust d = player_dust(m.origin, m.angles, w.entity);
        if (s.models != nullptr && body_triangles(*s.models, m, t, s.skin)) d.skin = s.skin;
        warp_dust(w, d, was, age, s.seed++, &out.births);
        continue;
      }
      const auto leaving = s.leaving.find(w.entity);
      if (leaving == s.leaving.end()) continue;
      const FxState::Leaving& g = leaving->second;
      fx::Dust d = g.dust;
      d.skin = g.skin;
      warp_dust(w, d, was, age, s.seed++, &out.births);
      if (!g.drawn) continue;
      Placed m = g.figure;
      m.entity = kLeavingBase - w.entity;
      m.warp = g.dust.warp;
      m.warp.shown = float(warp_shown(w, t));
      out.models.push_back(m);
    }
    std::erase_if(s.leaving, [&](const auto& kv) {
      return std::none_of(s.warps.live.begin(), s.warps.live.end(),
                          [&](const Warp& w) { return w.entity == kv.first && !w.arriving; });
    });
  }
  s.flights_was.swap(s.flights);         // the frame before's, for the blood a nail drew
  std::swap(s.flights, s.flights_next);  // the missiles flying now, for the next frame's blasts
  s.flights_next.clear();
  // A campaign's other effects (its temporary entities and particle bursts; its explosions are
  // drawn above): a shot's or a spike's hit throws sparks back toward the eye, blood falls, a
  // teleport flares, lava splashes, a beam lights and sparks where it ends.
  if (l.effects.size() < s.effects_seen) s.effects_seen = 0;  // a new map
  const ge::camera::Vec3 eye = l.pose.eye;
  const PlayerState& me = l.me;
  const sh::Vec3f eye_map = {me.origin.x + me.view_ofs.x, me.origin.y + me.view_ofs.y,
                             me.origin.z + me.view_ofs.z};
  const ge::brush::Map* map = l.map ? &l.map->bsp.map : nullptr;
  for (; s.effects_seen < l.effects.size(); s.effects_seen++) {
    const Effect& e = l.effects[s.effects_seen];
    const ge::camera::Vec3 at = engine_point(e.kind == EffectKind::beam ? e.end : e.at);
    double p[3], to_eye[3] = {eye.x - at.x, eye.y - at.y, eye.z - at.z};
    put(at, p);
    const double n = ge::core::hypot3(to_eye[0], to_eye[1], to_eye[2]);
    for (double& c : to_eye) c = n > 0 ? c / n : 0;
    if (n == 0) to_eye[1] = 1;
    // The face it landed on, seen along the line from the eye (a beam's from its root).
    bool on_face = false;                // `face` found the map's triangle
    uint32_t face_texture = 0;           // and its texture
    float face_light[3] = {-1, -1, -1};  // and its baked light there
    float face_st[2] = {0, 0};           // and where on its texture
    const auto face = [&](const sh::Vec3f& from, double normal[3], double dir[3]) {
      sh::Vec3f fn{};
      ge::detail::SurfaceClass sc = ge::detail::SurfaceClass::unknown;
      const sh::Vec3f hit = e.kind == EffectKind::beam ? e.end : e.at;
      engine_dir({hit.x - from.x, hit.y - from.y, hit.z - from.z}, dir);
      on_face =
          map != nullptr && face_hit(*map, from, hit, &face_texture, &fn, &sc, face_light, face_st);
      if (on_face) {
        engine_dir(fn, normal);
      } else {
        for (int k = 0; k < 3; ++k) normal[k] = -dir[k];
      }
      return sc;
    };
    switch (e.kind) {
      case EffectKind::impact:
      case EffectKind::spike: {
        if (monster_spit_hit(l, s, e, p, t, out)) break;
        double normal[3], dir[3];
        ge::detail::SurfaceClass sc = ge::detail::SurfaceClass::unknown;
        if (s.on.hits || s.on.decals) sc = face(eye_map, normal, dir);
        const bool nail = e.kind == EffectKind::spike;
        // The hole it leaves in a solid face (none in the sky or a liquid).
        if (s.on.decals && on_face && sc != ge::detail::SurfaceClass::sky &&
            sc != ge::detail::SurfaceClass::liquid) {
          // One hole a pellet where it hit; a message that stands for several pellets (a classic
          // program's one gunshot a shot) has them spread round its point as the shot's cone
          // spreads them at that range.
          const int holes = nail ? 1 : std::clamp(e.count, 1, 16);
          const double range =
              ge::core::hypot3(eye.x - p[0], eye.y - p[1], eye.z - p[2]) * kPelletSpread;
          double ta[3], tb[3];
          tangents(normal, ta, tb);
          for (int i = 0; i < holes; ++i) {
            const uint32_t h = s.seed++ * 2654435761u;
            const double u = holes > 1 ? ((h & 0xff) / 127.5 - 1) * range : 0;
            const double w = holes > 1 ? (((h >> 16) & 0xff) / 127.5 - 1) * range : 0;
            const double q[3] = {p[0] + ta[0] * u + tb[0] * w, p[1] + ta[1] * u + tb[1] * w,
                                 p[2] + ta[2] * u + tb[2] * w};
            mark(s, ge::detail::DecalKind::bullet_hole, q, normal, nail ? kNailHoleM : kHoleM,
                 (h >> 8) % 628 / 100.0, h >> 24, t);
          }
        }
        if (s.on.hits && l.q2 && q2_enhance_pack_hit(s, e.code, p, normal, t, out)) {
          // a mission pack's hit in its own colour (world_fx_q2.cpp)
        } else if (s.on.hits) {
          // A gunshot's count is its pellets (a match's); each its own hit.
          fx::WallHit w;
          for (int k = 0; k < 3; ++k) {
            w.at[k] = p[k];
            w.normal[k] = normal[k];
            w.dir[k] = dir[k];
          }
          w.surface = ge::detail::surface_name(sc);
          w.nail = nail;
          if (on_face && map != nullptr) hole_colour(s, *map, face_texture, face_st, w.colour);
          // The light on the face where it hit (its own lightmap there).
          for (int k = 0; k < 3; ++k) w.light[k] = face_light[k] >= 0 ? 2 * face_light[k] : -1;
          if (l.map) w.floor_y = floor_under(*l.map, e.at, eye_map);
          const int hits = nail ? 1 : std::clamp(e.count, 1, 4);
          for (int i = 0; i < hits; ++i) fx::hit(w, s.seed++, &out.births);
          // A nail off metal strikes sparks, and their flick lights the face round it.
          if (nail && sc == ge::detail::SurfaceClass::metal) {
            const double off[3] = {p[0] + normal[0] * 0.1, p[1] + normal[1] * 0.1,
                                   p[2] + normal[2] * 0.1};
            s.sparked.push_back({{off[0], off[1], off[2]}, t, e.at});
          }
        } else if (e.q2_specks > 0) {  // Quake II's puff off the face (q2_temp_fx.hpp)
          const double off_face[3] = {e.way.x, e.way.y, e.way.z};
          fx::q2_specks(p, off_face, e.q2_specks, e.colour, 32, true, s.seed++, s.palette,
                        &out.births);
          if (l.q2) q2_model_fx(s, e.code, e.at, e.way, t);  // its smoke and flash
        } else {
          // 20 specks a pellet, 10 a nail, 20 a big nail.
          const int specks = e.kind == EffectKind::spike ? 10 * e.count : 20 * e.count;
          if (!throw_scripted(s, e.kind == EffectKind::spike ? "spike" : "gunshot", p,
                              toward(p, eye).data(), e.count, -1e9, t, out))
            fx::impact(p, specks, s.seed++, s.palette, &out.births);
        }
        break;
      }
      case EffectKind::blood: {
        const bool arc = e.code == 13 && !l.campaign;  // a match's beam's blood
        // A program's own burst (a campaign's `particle`): its count of drops thrown along its
        // way at 15 times it, units a second; a match's blood: 20 hanging drops a count.
        const bool burst = e.code == -1;
        const ge::camera::Vec3 way = engine_point(e.way * 15);
        const double v[3] = {way.x, way.y, way.z};
        if ((s.on.q2.no_particles & 16) != 0) break;  // Quake II's cl_disable_particles 16
        if (s.on.blood) {
          bleed_fx(l, s, e, p, eye_map, l.map ? floor_under(*l.map, e.at, eye_map) : -1e9, t, out);
        } else if (e.q2_specks > 0) {  // Quake II's puff of blood off the hit (q2_temp_fx.hpp)
          const double off_face[3] = {e.way.x, e.way.y, e.way.z};
          fx::q2_specks(p, off_face, e.q2_specks, e.colour, 32, true, s.seed++, s.palette,
                        &out.births);
        } else if (!throw_scripted(s, "blood", p, toward(p, eye).data(), arc ? 1 : e.count, -1e9, t,
                                   out)) {
          fx::blood(p,
                    arc     ? 50
                    : burst ? e.count
                            : 20 * e.count,
                    arc, s.seed++, s.palette, &out.births, burst ? v : nullptr);
        }
        // Under ce_blood the blow's own wound (bleed_fx, along the striker's line).
        if (s.on.decals && !arc && !s.on.blood)
          wound(s, out.models, e.at, p, eye, std::clamp(e.count, 1, 8), t);
        break;
      }
      case EffectKind::teleport:
        // A player's own warp is the dust's when it is on: its fog is not thrown over it.
        if ((!s.on.warps || !near_warp(s.warps, e.at, t)) &&
            !throw_scripted(s, "teleport", p, kUpWay, 1, -1e9, t, out))
          fx::teleport_splash(p, s.seed++, s.palette, &out.births);
        break;
      case EffectKind::lava:
        if (!throw_scripted(s, "lava", p, kUpWay, 1, -1e9, t, out))
          fx::lava_splash(p, s.seed++, s.palette, &out.births);
        break;
      case EffectKind::beam: {
        if (l.q2 && e.code == 23) {  // the BFG's laser: a strip for a tenth of a second
          q2_laser(s, e, t);
          break;
        }
        if (l.q2 && s.on.beams && (e.code == 38 || e.code == 39)) {  // a heat beam: a hot strip
          q2_enhance_heat_beam(s, e, t);
          break;
        }
        double normal[3], dir[3];
        face(e.at, normal, dir);
        const ge::camera::Vec3 root = engine_point(e.at);
        auto held = std::find_if(s.beams.begin(), s.beams.end(), [&](const HeldBeam& b) {
          return b.entity == e.entity && b.shot.end_s < 0;
        });
        if (held == s.beams.end()) {
          HeldBeam b;
          b.entity = e.entity;
          // The classic bolt it is drawn as: the arc gun's, the big monster's, the boss's.
          b.model = e.code == 5   ? EffectModel::bolt
                    : e.code == 9 ? EffectModel::bolt3
                                  : EffectModel::bolt2;
          // The grappling beam (13) is its game's own model, in the model list when it has one.
          if (e.code == 13) b.listed = l.beam_model;
          b.shot.born_s = t;
          b.shot.seed = s.seed++;
          s.beams.push_back(b);
          held = s.beams.end() - 1;
        }
        put(root, held->shot.from);
        put(at, held->shot.to);
        for (int k = 0; k < 3; ++k) held->shot.normal[k] = normal[k];
        held->seen = t;
        // The classic roots the player's own bolt at his origin, under the gun.
        held->map_from = e.entity == me.entity ? me.origin : e.at;
        held->map_to = e.end;
        held->server_from = e.at;
        held->server_to = e.end;
        held->grapple = e.code == 13;
        // The own beam turned to the aim (cl_fakeshaft) sparks where it is drawn, below.
        if (turned_own(l, s, *held)) {
          held->sparks_due = true;
        } else {
          cross_edge(l, *held, e.at, {e.end.x - e.at.x, e.end.y - e.at.y, e.end.z - e.at.z});
          if (s.on.beams) fx::bolt_hit(p, normal, s.seed++, &out.births);
        }
        break;
      }
      case EffectKind::explosion:  // the mission packs' blast in its colours (12); the rest above
        if (e.code == 12) {
          if (!throw_scripted(s, "explosion2", p, kUpWay, 1, -1e9, t, out))
            fx::explosion2(p, e.colour, e.count, s.seed++, s.palette, &out.births);
          s.lit.push_back({at, t, e.at, true});  // its light, no sprite
        }
        break;
      case EffectKind::burst: {  // a program's burst of a colour's shades
        const ge::camera::Vec3 way = engine_point(e.way * 15);
        const double v[3] = {way.x, way.y, way.z};
        if (!throw_scripted(s, "particle", p, toward(p, eye).data(), e.count, -1e9, t, out))
          fx::burst(p, e.count, e.colour, v, s.seed++, s.palette, &out.births);
        break;
      }
      case EffectKind::named: {  // a mod's own effect: the files' of its name, or nothing
        // Quake II's arrivals (effects.hpp): a player's own warp is the dust's when it is on,
        // so his teleport's or entry's burst is not thrown over it.
        if (e.name.starts_with("sq_")) {
          q2_named_fx(s, e, p, s.on.warps && near_warp(s.warps, e.at, t), t, out);
          break;
        }
        if (e.name == "te_blaster" && l.q2 && !s.on.hits) {  // the classic: its specks, its flare
          const double off_face[3] = {e.way.x, e.way.y, e.way.z};
          fx::q2_blaster_specks(p, off_face, s.seed++, s.palette, &out.births);
          q2_model_fx(s, 2, e.at, e.way, t);
          break;
        }
        if (e.name == "te_blaster" || e.name == "te_blaster2") {
          // Quake II's bolt landing: yellow sparks; Ground Zero's green bolt's are green.
          fx::blaster_hit(p, s.seed++, &out.births, e.name == "te_blaster2");
          // On a wall: a small glowing mark, the bolt's heat left in it.
          double normal[3], dir[3];
          const ge::detail::SurfaceClass sc = face(eye_map, normal, dir);
          if (on_face && sc != ge::detail::SurfaceClass::sky &&
              sc != ge::detail::SurfaceClass::liquid)
            glow_mark(s, p, normal, fx::kBoltMark, t);
          break;
        }
        if (e.name == "te_bfg_bigexplosion") {  // the BFG's burst (its blast is an explosion's)
          if (s.on.explosions) fx::bfg_burst(p, s.seed++, &out.births);
          // On a wall (the face seen from the eye at the burst): a large glowing mark, slow to
          // cool.
          double normal[3], dir[3];
          face(eye_map, normal, dir);
          if (on_face) glow_mark(s, p, normal, fx::kEnergyBallMark, t);
          break;
        }
        double way[3] = {kUpWay[0], kUpWay[1], kUpWay[2]};
        if (e.way.x != 0 || e.way.y != 0 || e.way.z != 0) engine_dir(e.way, way);
        const double floor_y = l.map ? floor_under(*l.map, e.at, eye_map) : -1e9;
        throw_scripted(s, e.name, p, way, e.count, floor_y, t, out);
        break;
      }
      case EffectKind::named_trail: {  // a mod's own trail over its stretch
        double from[3], to[3];
        put(engine_point(e.at), from);
        put(engine_point(e.end), to);
        // Quake II's bubbles and force walls along their lines (client/world_fx_q2.cpp).
        if (q2_named_trail(s, e, from, to, t, out)) break;
        // Quake II's cables in the classic look: a segment model every 30 units (q2_model_fx.hpp).
        if (l.q2 && !s.on.monsters && q2_cable(s, e, t)) break;
        // Quake II's monsters' cables (a parasite's tongue, a medic's line) and shots leaving.
        if (const fx::Cable c = fx::cable_of(e.name); c != fx::Cable::none) {
          if (s.on.monsters) {
            fx::monster_cable(c, from, to, s.seed++, t, &out.births);
            fx::Light light;
            if (fx::cable_light(c, to, t, &light)) out.lights.push_back(light);
          } else {
            fx::classic_cable(c, from, to, s.seed++, &out.births);
          }
          break;
        }
        if (const fx::Flash f = fx::flash_of(e.name); f != fx::Flash::none) {
          FxState::MonsterFlash m;
          const double d[3] = {to[0] - from[0], to[1] - from[1], to[2] - from[2]};
          const double len = ge::core::hypot3(d[0], d[1], d[2]);
          for (int k = 0; k < 3; ++k) {
            m.at[k] = from[k];
            m.dir[k] = len > 1e-6 ? d[k] / len : 0;
          }
          m.t = t;
          m.entity = e.entity;
          m.seed = s.seed++;
          m.kind = f;
          m.sparked = false;
          s.monster_flashes.push_back(m);
          break;
        }
        if (e.name == "te_railtrail") {  // Quake II's rail: a beam, a line circling it (ce_beams)
          if (s.on.q2.rail_type > 0) {   // cl_railtrail_type: its own core, and spiral at 2
            q2_rail_trail(s, e.entity, from, to, t);
          } else if (s.on.beams) {
            HeldBeam b;
            b.entity = e.entity;
            b.rail = true;
            b.shot.born_s = t;
            b.shot.end_s = t + 0.05;
            b.shot.seed = s.seed++;
            for (int k = 0; k < 3; ++k) {
              b.shot.from[k] = from[k];
              b.shot.to[k] = to[k];
            }
            double normal[3], dir[3];
            face(e.end, normal, dir);
            for (int k = 0; k < 3; ++k) b.shot.normal[k] = normal[k];
            b.seen = t;
            s.beams.push_back(b);
            b.coil = true;  // and the line circling it
            s.beams.push_back(b);
            fx::ion_trail(from, to, s.seed++, &out.births);  // its vapour
          } else {
            fx::rail_spiral(from, to, s.seed++, &out.births);  // the classic coil of specks
          }
          // Where it struck a wall (the line run on a little past its end): a small glowing mark.
          const float dx = e.end.x - e.at.x, dy = e.end.y - e.at.y, dz = e.end.z - e.at.z;
          const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
          sh::Vec3f fn{};
          ge::detail::SurfaceClass sc = ge::detail::SurfaceClass::unknown;
          if (map != nullptr && len > 1 &&
              face_hit(*map, e.at,
                       {e.end.x + dx / len * 8, e.end.y + dy / len * 8, e.end.z + dz / len * 8},
                       &face_texture, &fn, &sc, face_light, face_st)) {
            double normal[3];
            engine_dir(fn, normal);
            glow_mark(s, to, normal, fx::kSlugMark, t);
          }
          break;
        }
        trail_scripted(s, e.name, from, to, out);
        break;
      }
    }
  }
  // Quake II's monsters' flashes: a light each look draws, the fire the modern look's.
  // The shot leaves from the shooter's eye; the flash sits at its gun's tip as this frame holds
  // the model (the vertex furthest along the aim), or at the eye when it has no model drawn.
  std::erase_if(s.monster_flashes, [&](FxState::MonsterFlash& f) {
    double at[3] = {f.at[0], f.at[1], f.at[2]}, dir[3] = {f.dir[0], f.dir[1], f.dir[2]};
    if (s.models != nullptr)
      for (const Placed& m : out.models)
        if (m.entity == f.entity) {  // along the way the model faces, as a gun is held
          const double yaw = m.angles.y * ge::core::kPi / 180;
          double ahead[3], tip[3];
          engine_dir({float(std::cos(yaw)), float(std::sin(yaw)), 0}, ahead);
          // A little up as well, so a foot striding out is never taken for the gun.
          const double reach[3] = {ahead[0], ahead[1] + 0.4, ahead[2]};
          if (held_tip(*s.models, m, t, reach, tip))
            for (int k = 0; k < 3; ++k) {
              at[k] = tip[k];
              dir[k] = ahead[k];
            }
          break;
        }
    if (s.on.monsters && !f.sparked) fx::flash_sparks(f.kind, at, dir, f.seed, &out.births);
    f.sparked = true;
    fx::Light light;
    // Quake II's cl_muzzlelight_time stretches the light's fade; cl_dlight_hacks 4 drops a
    // machine gun's or a chain gun's.
    const bool lit =
        fx::flash_light(f.kind, at, ge::classic::q2_muzzle_age(s.on.q2, t - f.t), &light) &&
        !(f.kind == fx::Flash::gun && (s.on.q2.light_hacks & 4) != 0);
    if (lit) out.lights.push_back(light);
    ge::render::MuzzleFire fire;
    if (s.on.monsters && s.on.q2.muzzle_models && t - f.t < fx::kMonsterMuzzleS &&
        fx::flash_fire(f.kind, at, dir, f.seed, f.t, &fire))
      out.flames.push_back(fire);
    return !lit && t - f.t >= fx::kMonsterMuzzleS * 1.5;  // its fire's time over too
  });
  // The enhanced blasts and puffs while they are seen, their main balls' light while it lasts.
  std::erase_if(s.blasts, [&](const ge::render::Blast& b) {
    return t - b.born_s > ge::render::blast_life(b) || t < b.born_s - 1;
  });
  const size_t puffs_from = out.puffs.size();
  out.puffs.insert(out.puffs.end(), s.blasts.begin(), s.blasts.end());
  // cl_smooth_explosions 0: each drawn at its age in whole tenths, so it moves on by steps.
  const bool stepped = !s.on.q2.smooth_explosions;
  for (size_t i = puffs_from; stepped && i < out.puffs.size(); ++i)
    out.puffs[i].born_s = t - ge::classic::q2_blast_age(s.on.q2, t - out.puffs[i].born_s);
  std::erase_if(s.mains, [&](const ge::render::Blast& b) {
    fx::Light light;
    const double at = stepped ? b.born_s + ge::classic::q2_blast_age(s.on.q2, t - b.born_s) : t;
    if (!fx::blast_light(b, at, &light)) return t - b.born_s > 0.1;
    out.lights.push_back(q2_blast_light(s, light));
    return false;
  });
  // The beams: let go once the program stops sending one, gone once faded; their lights.
  for (HeldBeam& b : s.beams)
    if (b.shot.end_s < 0 && t - b.seen > kBeamHoldS) b.shot.end_s = b.seen + kBeamHoldS;
  std::erase_if(s.beams, [&](const HeldBeam& b) {
    const double release = b.q2_trail    ? b.trail_fade_s
                           : !s.on.beams ? 0
                           : b.coil      ? fx::rail_coil_look().release_s
                           : b.rail      ? fx::rail_look().release_s
                           : b.plasma    ? fx::plasma_look().release_s
                                         : fx::bolt_look().release_s;
    return b.shot.end_s >= 0 && t > b.shot.end_s + release;
  });
  // The own beam turned to the aim, traced again from this frame's eye (cl_fakeshaft).
  for (HeldBeam& b : s.beams)
    if (b.shot.end_s < 0 && turned_own(l, s, b)) turn_own(l, s, b, out);
  if (!l.beams.empty())
    laser_beams(l.beams, s.seed++, s.palette, t, out.beams);  // seed: lasers only
  ge::core::Rng roll = ge::core::make_rng(s.seed++);          // the classic bolt's segments' roll
  int segment = kBoltEntities;
  for (HeldBeam& b : s.beams) {
    if (b.q2_trail) {  // Quake II's cl_railtrail_type beam: drawn in either look
      out.beams.push_back(b);
      continue;
    }
    if (b.rail || b.plasma) {  // a rail's or a bolt's streak: drawn, no burns
      if (!s.on.beams) continue;
      out.beams.push_back(b);
      if (b.rail && !b.coil) {  // the rail's soft light along its line
        fx::Light ll[fx::kRailLights];
        const int n = fx::rail_lights(b.shot, t, ll);
        out.lights.insert(out.lights.end(), ll, ll + n);
      }
      continue;
    }
    if (s.on.beams) {
      // Its burns where it lands: the line it traces, a few pieces to fill a fast sweep, and the
      // melt where it stays.
      // An edge crossed first: the line laid up to it on the face it left, then started just past
      // it on the next.
      ge::render::BeamBurn burn;
      for (int k = 0; s.on.decals && k < b.edges; ++k) {
        ge::render::BeamShot edge = b.shot;
        for (int c = 0; c < 3; ++c) edge.to[c] = b.edge_at[k][c], edge.normal[c] = b.edge_n[k][c];
        for (int i = 0;
             i < 6 && ge::render::beam_burns(fx::bolt_look(), edge, t, b.trail, burn, k == 0); ++i)
          beam_mark(s, b, burn);
      }
      b.edges = 0;
      for (int i = 0; s.on.decals && i < 6 &&
                      ge::render::beam_burns(fx::bolt_look(), b.shot, t, b.trail, burn);
           ++i)
        beam_mark(s, b, burn);
      out.beams.push_back(b);
      fx::Light ll[ge::render::kBeamLights];
      const int n = fx::bolt_lights(b.shot, t, ll);
      out.lights.insert(out.lights.end(), ll, ll + n);
      continue;
    }
    // The classic bolt: its model every 30 units from the root while short of the end, each
    // turned to a fresh roll every frame (the bolt's crackle).
    Chain c;
    c.modelindex = b.listed >= 0 ? b.listed : l.effect_models + static_cast<int>(b.model);
    place_chain(b.map_from, b.map_to, c, roll, segment, out.models);
  }
  burns_of(s, t, out.burns);
  // The marks still glowing light the room round them (render/marks.hpp's mark_glows).
  if (s.on.decals && s.receivers) {
    ge::render::MarkGlow glows[kMarkGlowLights];
    const int n = ge::render::mark_glows(s.marks, t, glows, kMarkGlowLights);
    for (int i = 0; i < n; ++i) {
      fx::Light g;
      for (int k = 0; k < 3; ++k) {
        g.pos[k] = glows[i].at[k];
        g.colour[k] = kMarkGlowGain * glows[i].rgb[k];
      }
      g.radius = glows[i].reach_m;
      out.lights.push_back(g);
    }
  }
  // The hot marks burn a little and then smoke as they cool (render/mark_fire.hpp).
  out.fires.clear();
  if (s.on.decals && s.receivers) {
    ge::render::mark_flames(ge::render::mark_fire_look(), s.marks, t, out.fires);
    ge::render::mark_fire_births(s.mark_fire, ge::render::mark_fire_look(), s.marks, t, dt,
                                 out.births);
  }
  heal_wounds(l, s);
  if (s.on.decals) draw_wounds(s, out.models, l.pose.eye, t, dt, out.births);
  body_blood(l, s, t, dt, out);
  if (!s.on.decals && !s.on.blood) s.wounds.clear();
  std::erase_if(s.sparked, [&](const Blast& b) { return t - b.t >= fx::kSparkLightS; });
  for (const Blast& b : s.sparked) {
    double p[3];
    put(b.at, p);
    fx::Light light;
    if (fx::spark_light(p, t - b.t, &light)) out.lights.push_back(light);
  }
  std::erase_if(s.lit, [&](const Blast& b) {
    return t - b.t > std::max(fx::kExplosionLight, kSpriteFrames / kSpriteFps);
  });
  scripted_lights(s, t, out);
  int sprite = 0;
  for (const Blast& b : s.lit) {
    double p[3];
    put(b.at, p);
    fx::Light light;
    if (fx::explosion_light(p, ge::classic::q2_blast_age(s.on.q2, t - b.t), &light))
      out.lights.push_back(q2_blast_light(s, light));
    // The pack's explosion sprite, its frames at ten a second.
    const int frame = static_cast<int>((t - b.t) * kSpriteFps);
    if (frame >= kSpriteFrames || b.no_sprite) continue;
    Placed spr;
    spr.entity = kSpriteEntities - sprite++;
    spr.modelindex = l.effect_models + static_cast<int>(EffectModel::explosion);
    spr.frame = frame;
    spr.origin = b.map_at;
    out.models.push_back(spr);
  }
  q2_draw_model_fx(s, l.effect_models, l.me, l.own > 0 ? l.me.entity : 0, t, out);
  step_cases(l, s, t, dt, out);  // the guns' spent cases (ce_shells)
  q2_place_bubbles(s, l.effect_models, t, out);

  // The powers held: a glow round each holder (blue the quad's, red the shield's), the local
  // player's own among them.
  const PlayerState& p = l.me;
  const auto glow = [&](int effects, const sh::Vec3f& at) {
    if (!(effects & kGlow)) return;
    fx::Light light;
    put(engine_point(at), light.pos);
    light.radius = 6;
    const bool blue = (effects & kGlowBlue) != 0, red = (effects & kGlowRed) != 0;
    light.colour[0] = red ? 1.6 : blue ? 0.35 : 1.0;
    light.colour[1] = red && blue ? 0.5 : blue || red ? 0.35 : 0.9;
    light.colour[2] = blue ? 1.6 : red ? 0.35 : 0.7;
    light.group = fx::kLightPower;
    out.lights.push_back(light);
  };
  for (const Placed& m : out.models) glow(m.effects, m.origin);
  place_markers(l, s, t, out);
  const int mine = l.own > 0 ? l.me_effects : 0;
  if (l.own > 0) glow(mine, p.origin);

  step_screen(l, s, dt, out);
}

bool load_held_fire(HeldFire& f, std::string* error, std::span<const std::string_view> at_eye) {
  std::vector<fx::FireRow> rows;
  f.ejects.clear();
  if (!at_eye.empty()) {
    for (const std::string_view name : at_eye) f.ejects.push_back(held_ejection(name));
  } else {
    for (int w = 0; w < view::kWeapons; ++w) f.ejects.push_back(own_set_ejection(w));
  }
  if (!at_eye.empty()) {
    for (const std::string_view name : at_eye) {
      const ge::classicenhance::ClassicFire* c = ge::classicenhance::classic_fire_of(name);
      rows.push_back(c != nullptr ? fx::FireRow{c->muzzle, c->knobs} : fx::FireRow{});
    }
    return fx::fire_looks(rows, 1.0, f.looks, error);
  }
  for (const view::WeaponLook& look : view::weapon_looks())
    rows.push_back({look.muzzle, look.muzzle_knobs});
  return fx::fire_looks(rows, view::kBuiltScale, f.looks, error);
}

void fire_held(HeldFire& f, const view::HandFrame& hand, bool on, double now_s, FxFrame& out) {
  const auto w = static_cast<size_t>(hand.weapon);
  const ge::render::MuzzleLook* look = on && w < f.looks.size() ? &f.looks[w] : nullptr;
  ge::render::MuzzlePose pose;
  for (int k = 0; k < 3; ++k) {
    pose.at[k] = hand.muzzle_at[k];
    pose.dir[k] = hand.muzzle_dir[k];
    pose.up[k] = hand.muzzle_up[k];
  }
  fx::step_muzzle(f.shots, look, hand.fired && hand.drawn, pose, now_s, &out.births, &out.puffs,
                  &out.flames);
  if (!hand.drawn) return;
  for (HeldBeam& b : out.beams)
    if (b.entity == 1)
      for (int k = 0; k < 3; ++k) b.shot.from[k] = hand.muzzle_at[k];
  // While the player's own bolt is held, an arc crackles across the gun's prongs too, as the
  // classic gun's own flash does: the same bolt from one prong to the other, struck on its own
  // seed; drawn only (the bolt's lights already light the gun).
  if (!hand.arc) return;
  for (size_t i = 0, n = out.beams.size(); i < n; ++i) {
    if (out.beams[i].entity != 1) continue;
    HeldBeam across = out.beams[i];
    across.entity = kArcEntity;
    across.across = true;
    across.shot.seed ^= 0x5bd1e995u;
    for (int k = 0; k < 3; ++k) {
      across.shot.from[k] = hand.arc_from[k];
      across.shot.to[k] = hand.arc_to[k];
      across.shot.normal[k] = -hand.muzzle_dir[k];
    }
    out.beams.push_back(across);
    return;
  }
}

void place_chain(const sh::Vec3f& from, const sh::Vec3f& to, const Chain& c, ge::core::Rng& roll,
                 int& entity, std::vector<Placed>& out) {
  const sh::Vec3f d = {to.x - from.x, to.y - from.y, to.z - from.z};
  const double len = ge::core::hypot3(d.x, d.y, d.z);
  if (len < (c.fit ? 1e-6 : 1)) return;
  const float yaw = static_cast<float>(std::atan2(d.y, d.x) * 180 / ge::core::kPi);
  const float pitch =
      static_cast<float>(std::atan2(d.z, std::sqrt(d.x * d.x + d.y * d.y)) * 180 / ge::core::kPi);
  const int n = chain_links(len);
  for (int i = 0; i < n; ++i) {
    Placed seg;
    seg.entity = entity--;
    seg.modelindex = c.modelindex;
    const auto k = static_cast<float>(chain_share(i, n, len, c.fit));
    seg.origin = {from.x + d.x * k, from.y + d.y * k, from.z + d.z * k};
    seg.angles = {pitch, yaw, static_cast<float>(ge::core::range(roll, 0, 360))};
    seg.no_shadow = c.no_shadow;
    out.push_back(seg);
  }
}

int press_mark(FxState& s, const ge::detail::DecalAt& d) { return press(s, d); }

double map_floor_under(const LoadedMap& m, const sh::Vec3f& at, const sh::Vec3f& from) {
  return floor_under(m, at, from);
}

}  // namespace shooters_edge::client
