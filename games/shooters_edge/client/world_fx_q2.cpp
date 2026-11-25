// QUAKE II'S OWN EFFECTS IN THE WORLD (client/world_fx.hpp): the temp entities that come as named
// effects (`sq_*`: arrivals, an item's return, sparks off a weld or a tunnel, steam and smoke, the
// blue hyperblaster's hit, the widow's splash, the flashlight), the trails along a line (bubbles,
// a force wall) and the marks a flying entity's effect bits leave (a flag's or the tag's column,
// the ion ripper's specks, the trap's glow, the tracker's strands and dark light, the spinning
// lights, each one's light), thrown by world_fx.cpp's effect loop.
//
// ge:summary Quake II's named effects, line trails and effect-bit marks thrown into the world
// ge:keywords q2-effects temp-entity te_bubbletrail te_forcewall te_welding_sparks te_steam
// ge:keywords te_flashlight te_bosstport boss-teleport ef_trap ef_tracker ef_trackertrail
// ge:keywords ef_spinninglights
// ge:keywords ef_flag1 ef_flag2 ef_ionripper ef_tagtrail ef_plasma q2_mark
// ge:keywords te_heatbeam te_monster_heatbeam te_lightning tesla nuke-blast tracker-explosion
// ge:keywords negative-light q2_pack_hit heat-beam-rings
// ge:skill render shooter
// ge:test tests/shooters_edge_q2_world_fx_test.cpp
#include <algorithm>
#include <cmath>
#include <string_view>
#include <vector>

#include "brush/map.hpp"
#include "core/num.hpp"
#include "core/prng.hpp"

#include "shooters_edge/app/effects.hpp"
#include "shooters_edge/client/q2_enhance_fx.hpp"
#include "shooters_edge/client/q2_look.hpp"
#include "shooters_edge/client/q2_model_fx.hpp"
#include "shooters_edge/client/q2_temp_fx.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_fx.hpp"

namespace shooters_edge::client {

namespace fx = shooters_edge::effects;

namespace {
constexpr double kSqBeamOutS = 2.1;  // a widow beam-out's life
constexpr double kSqNukeS = 1;       // an A-M bomb's blast's
// The placed model effects' entity numbers (below every other effect's).
constexpr int kQ2FxEntities = -(1 << 23);
// The engine's bubbles for Quake II's (ce_bubbles): their own entity numbers, and as many alive at
// once as the classic's particles could hold of them.
constexpr int kBubbleEntities = -(1 << 24);
constexpr size_t kOwnBubbles = 1024;

void put_point(const sh::Vec3f& at, double p[3]) {
  const ge::camera::Vec3 e = engine_point(at);
  p[0] = e.x;
  p[1] = e.y;
  p[2] = e.z;
}

// A light of `reach` units in `rgb` at `at` (map units).
void q2_light(const float at[3], float reach, const float rgb[3], FxFrame& out) {
  if (reach <= 0) return;
  double p[3];
  put_point({at[0], at[1], at[2]}, p);
  out.lights.push_back(fx::q2_point_light(p, reach, rgb[0], rgb[1], rgb[2]));
}
// The tesla's lightning from `from` to `to` (map units): 35-unit links over the line less 20
// units, each facing back toward the start; one link alone at the end when that is 35 or less.
void q2_lightning(const sh::Vec3f& from, const sh::Vec3f& to, int modelindex, ge::core::Rng& roll,
                  int& entity, std::vector<Placed>& out) {
  const sh::Vec3f d = {to.x - from.x, to.y - from.y, to.z - from.z};
  const double len = ge::core::hypot3(d.x, d.y, d.z);
  const double reach = len - kQ2LightningShort;
  Chain chain;
  chain.modelindex = modelindex;
  chain.no_shadow = chain.full_bright = true;
  chain.step = kQ2LightningStep;
  if (reach <= kQ2LightningStep || len <= 0) {  // one link, at the end, along the line
    const size_t first = out.size();
    place_chain(to, {to.x + d.x, to.y + d.y, to.z + d.z}, chain, roll, entity, out);
    if (out.size() > first + 1) out.resize(first + 1);
    return;
  }
  const auto k = static_cast<float>(reach / len);
  chain.fit = chain.back = true;
  place_chain(from, {from.x + d.x * k, from.y + d.y * k, from.z + d.z * k}, chain, roll, entity,
              out);
}

// The view's forward, right and up axes (map units' axes) from the eye's yaw and pitch.
void view_axes(const FxScene& l, sh::Vec3f& fwd, sh::Vec3f& right, sh::Vec3f& up) {
  const ge::camera::Vec3 f = ge::camera::look_forward(l.pose.yaw, l.pose.pitch);
  fwd = {static_cast<float>(f.x), static_cast<float>(f.z), static_cast<float>(f.y)};
  // Right: level, across the forward; up: the third axis.
  const double h = std::sqrt(double(fwd.x) * fwd.x + double(fwd.y) * fwd.y);
  right = h > 1e-6 ? sh::Vec3f{static_cast<float>(fwd.y / h), static_cast<float>(-fwd.x / h), 0}
                   : sh::Vec3f{0, -1, 0};
  up = {right.y * fwd.z - right.z * fwd.y, right.z * fwd.x - right.x * fwd.z,
        right.x * fwd.y - right.y * fwd.x};
}

// THE PLASMA BEAMS: each a chain of the beam model in 32-unit links all at one roll that turns
// 1000 degrees a second. The player's own (or a monster's sent from him) leaves his gun, 2 units
// to the hand's side, 7 forward and 3 down (`kQ2HeatBeamGun`; the centre hand's 1 lower still, his
// own sent as a monster's from the eye itself), runs along his view as far as the server's end, or
// to what his aim meets with cl_fakeshaft on (`beam_trace`), wears frame 1 and is ringed by specks
// sliding forward at 96 units a second, a ring every 32 units, shrunk within 10 units of the gun.
// Another player's leaves his start 1 unit right, 7 along and 7 down; a monster's leaves its start
// with a glow of specks round it; both frame 2, to the server's end.
void q2_heat_beams(FxState& s, const FxScene& l, int modelindex, double t, int& entity,
                   FxFrame& out) {
  std::erase_if(s.q2_heat_beams, [&](const FxState::Q2HeatBeam& h) { return t > h.until; });
  if (s.q2_heat_beams.empty()) return;
  const int own = l.own > 0 ? l.me.entity : 0;
  ge::core::Rng unused = ge::core::make_rng(0);
  Chain chain;
  chain.modelindex = modelindex;
  chain.fit = chain.back = chain.no_shadow = chain.full_bright = true;
  chain.step = kQ2HeatBeamStep;
  chain.roll = std::fmod(t * 1000, 360);
  for (const FxState::Q2HeatBeam& h : s.q2_heat_beams) {
    if (own > 0 && h.entity == own) {
      sh::Vec3f fwd, right, up;
      view_axes(l, fwd, right, up);
      const ge::camera::Vec3 eye = l.pose.eye;
      const double u = 1 / ge::brush::kUnitM;
      sh::Vec3f from = {static_cast<float>(eye.x * u), static_cast<float>(eye.z * u),
                        static_cast<float>(eye.y * u)};
      const float side = s.on.q2.hand == 2 ? 0.0f : s.on.q2.hand == 1 ? -1.0f : 1.0f;
      const float off[3] = {h.monster ? 0.0f : 2 * side, h.monster ? 0.0f : 7.0f,
                            (h.monster ? 0.0f : -3.0f) - (s.on.q2.hand == 2 ? 1.0f : 0.0f)};
      for (int k = 0; k < 3; ++k) {
        const float r = k == 0 ? right.x : k == 1 ? right.y : right.z;
        const float f = k == 0 ? fwd.x : k == 1 ? fwd.y : fwd.z;
        const float v = k == 0 ? up.x : k == 1 ? up.y : up.z;
        (k == 0 ? from.x : k == 1 ? from.y : from.z) += off[0] * r + off[1] * f + off[2] * v;
      }
      float len =
          static_cast<float>(ge::core::hypot3(h.to.x - from.x, h.to.y - from.y, h.to.z - from.z));
      sh::Vec3f hit, normal;
      if (s.on.fake_shaft > 0 && l.map != nullptr &&
          beam_trace(l.map, from, fwd, l.bodies, own, hit, normal))
        len = static_cast<float>(ge::core::hypot3(hit.x - from.x, hit.y - from.y, hit.z - from.z));
      const sh::Vec3f to = {from.x + fwd.x * len, from.y + fwd.y * len, from.z + fwd.z * len};
      chain.frame = 1;
      place_chain(from, to, chain, unused, entity, out.models);
      // The rings, sliding forward at 96 units a second.
      const double r[3] = {right.x, right.y, right.z}, v[3] = {up.x, up.y, up.z};
      const double first = std::fmod(t * 96, 32);
      for (int i = 0; first + 32.0 * i < len; i++) {
        const double d = first + 32.0 * i;
        const double whole = std::floor(d);
        const double scale = whole < 10 ? whole / 10 : 1;
        double p[3];
        put_point({from.x + fwd.x * float(d), from.y + fwd.y * float(d), from.z + fwd.z * float(d)},
                  p);
        fx::q2_heat_ring(p, r, v, scale, s.seed++, s.palette, &out.births);
      }
      continue;
    }
    sh::Vec3f from = h.from;
    if (!h.monster) {  // another player's: off his start, 1 right, 7 along, 7 down the line
      const sh::Vec3f d = {h.to.x - h.from.x, h.to.y - h.from.y, h.to.z - h.from.z};
      const float n = static_cast<float>(ge::core::hypot3(d.x, d.y, d.z));
      if (n > 1e-6f) {
        const sh::Vec3f f = {d.x / n, d.y / n, d.z / n};
        const float flat = std::sqrt(f.x * f.x + f.y * f.y);
        const sh::Vec3f rt =
            flat > 1e-6f ? sh::Vec3f{f.y / flat, -f.x / flat, 0} : sh::Vec3f{0, -1, 0};
        const sh::Vec3f upv = {rt.y * f.z - rt.z * f.y, rt.z * f.x - rt.x * f.z,
                               rt.x * f.y - rt.y * f.x};
        from = {from.x + rt.x + 7 * f.x - 7 * upv.x, from.y + rt.y + 7 * f.y - 7 * upv.y,
                from.z + rt.z + 7 * f.z - 7 * upv.z};
      }
    } else {
      double p[3];
      put_point(h.from, p);
      fx::q2_heat_glow(p, s.seed++, s.palette, &out.births);
    }
    chain.frame = 2;
    place_chain(from, h.to, chain, unused, entity, out.models);
  }
}
}  // namespace

void q2_model_fx(FxState& s, int code, const sh::Vec3f& at, const sh::Vec3f& way, double t) {
  Q2ModelFx m[2];
  const float a[3] = {at.x, at.y, at.z}, w[3] = {way.x, way.y, way.z};
  const int n = q2_model_fx_of(code, s.seed++ * 2654435761u, a, w, t, s.on.q2, m);
  for (int i = 0; i < n; ++i) {
    if (s.q2_models.size() < FxState::kQ2ModelFx) {
      s.q2_models.push_back(m[i]);
      continue;
    }
    s.q2_models[s.q2_models_next] = m[i];  // full: the oldest taken
    s.q2_models_next = (s.q2_models_next + 1) % FxState::kQ2ModelFx;
  }
}

bool q2_classic_blast(FxState& s, int code, const sh::Vec3f& at, const double p[3], bool no_specks,
                      double t, FxFrame& out) {
  switch (code) {
    case 5:
    case 6:
    case 7:
    case 8:
    case 17:
    case 18:
    case 62:  // explosion1 and 2 with no light (q2_model_fx_of leaves them dark)
    case 63:
      if (!no_specks) fx::q2_blast_specks(p, 0xe0, s.seed++, s.palette, &out.births);
      q2_model_fx(s, code, at, {0, 0, 1}, t);
      return true;
    case 20:  // the BFG ball's burst: its sprite and green light, no specks
    case 35:  // a plain explosion, explosion1 without specks, a boss's big one: the ball alone
    case 53:
    case 54:
      q2_model_fx(s, code, at, {0, 0, 1}, t);
      return true;
    case 47: {  // the disruptor's dark burst: black specks, a darkening light for a tenth
      fx::q2_tracker_burst(p, s.seed++, s.palette, &out.births);
      Q2ModelFx dark;
      dark.cls = Q2FxClass::flash;
      dark.drawn = false;
      dark.born_s = t;
      dark.at[0] = at.x, dark.at[1] = at.y, dark.at[2] = at.z;
      dark.light = 150;
      dark.rgb[0] = dark.rgb[1] = dark.rgb[2] = -1;
      if (s.q2_models.size() < FxState::kQ2ModelFx) s.q2_models.push_back(dark);
      return true;
    }
    case 51: {  // the A-M bomb's blast: a shell growing for a second (32 such at once at most)
      if (s.nukes.size() + s.beam_outs.size() >= 32) return true;
      FxState::Nuke n;
      for (int k = 0; k < 3; ++k) n.at[k] = p[k];
      n.born = t;
      s.nukes.push_back(n);
      return true;
    }
    case 21:  // its big burst: specks alone
      fx::q2_blast_specks(p, 0xd0, s.seed++, s.palette, &out.births);
      return true;
    default:
      return false;
  }
}

bool q2_enhance_pack_hit(FxState& s, int code, const double p[3], const double normal[3], double t,
                         FxFrame& out) {
  Q2PackHit h;
  if (!q2_pack_hit_of(code, &h)) return false;
  fx::tinted_hit(p, normal, h.hit, s.seed++, &out.births);
  if (h.flick)
    s.sparked.push_back(
        {{p[0] + normal[0] * 0.1, p[1] + normal[1] * 0.1, p[2] + normal[2] * 0.1}, t, {}});
  return true;
}

bool q2_cable(FxState& s, const Effect& e, double t) {
  if (e.name == "te_heatbeam" || e.name == "te_monster_heatbeam") {  // kept by its source
    auto h = std::find_if(s.q2_heat_beams.begin(), s.q2_heat_beams.end(),
                          [&](const FxState::Q2HeatBeam& k) { return k.entity == e.entity; });
    double life = kQ2HeatBeamS;
    if (h == s.q2_heat_beams.end()) {
      if (s.q2_heat_beams.size() >= 128) return true;  // the classic's 128: a new one dropped
      s.q2_heat_beams.emplace_back();
      h = s.q2_heat_beams.end() - 1;
      life = kQ2HeatBeamNewS;
    }
    h->entity = e.entity;
    h->from = e.at;
    h->to = e.end;
    h->monster = e.name == "te_monster_heatbeam";
    h->until = t + life;
    return true;
  }
  if (e.name == "te_lightning") {  // kept by the two it joins
    auto c = std::find_if(s.q2_cables.begin(), s.q2_cables.end(), [&](const FxState::Q2Cable& k) {
      return k.lightning && k.entity == e.entity && k.other == e.count;
    });
    if (c == s.q2_cables.end()) {
      if (s.q2_cables.size() >= 128) return true;
      s.q2_cables.emplace_back();
      c = s.q2_cables.end() - 1;
    }
    c->entity = e.entity;
    c->other = e.count;
    c->from = e.at;
    c->to = e.end;
    c->lightning = true;
    c->until = t + kQ2CableS;
    return true;
  }
  const bool ctf = e.name == "te_grapple_cable";
  if (!ctf && e.name != "te_parasite_attack" && e.name != "te_medic_cable_attack") return false;
  auto c = std::find_if(s.q2_cables.begin(), s.q2_cables.end(), [&](const FxState::Q2Cable& k) {
    return !k.lightning && k.entity == e.entity;
  });
  if (c == s.q2_cables.end()) {
    if (s.q2_cables.size() >= 128) return true;  // the classic's 128: a new one dropped
    s.q2_cables.emplace_back();
    c = s.q2_cables.end() - 1;
  }
  c->entity = e.entity;
  c->from = e.at;
  c->to = e.end;
  c->ctf = ctf;
  c->offset = ctf ? e.way : sh::Vec3f{0, 0, 0};
  c->until = t + kQ2CableS;
  return true;
}

void q2_laser(FxState& s, const Effect& e, double t) {
  FxState::Q2Laser z;
  z.from = e.at;
  z.to = e.end;
  z.born = t;
  const size_t index = 0xd0 + (s.seed++ % 4);
  for (int k = 0; k < 3; ++k)
    z.rgb[k] = 3 * index + 2 < s.palette.size()
                   ? static_cast<float>(s.palette[3 * index + size_t(k)]) / 255.0f
               : k == 1 ? 1.0f
                        : 0.0f;
  if (s.q2_lasers.size() < 256) s.q2_lasers.push_back(z);  // the classic's 256: one more dropped
}

void q2_enhance_heat_beam(FxState& s, const Effect& e, double t) {
  FxState::Q2Laser z;
  z.from = e.at;
  z.to = e.end;
  z.born = t;
  for (int k = 0; k < 3; ++k) z.rgb[k] = kQ2HeatBeamRgb[k];
  z.width_u = kQ2HeatBeamU;
  if (s.q2_lasers.size() < 256) s.q2_lasers.push_back(z);
}

void q2_place_bubbles(FxState& s, int effect_models, double t, FxFrame& out) {
  std::erase_if(s.own_bubbles,
                [&](const FxState::OwnBubble& b) { return t - b.born >= b.b.life || t < b.born; });
  for (const FxState::OwnBubble& b : s.own_bubbles) {
    double at[3];
    fx::rising_bubble_at(b.b, t - b.born, at);
    Placed d;
    d.entity = kBubbleEntities - static_cast<int>(b.id % (1u << 20));
    d.modelindex = effect_models + static_cast<int>(EffectModel::bubble);
    d.origin = {static_cast<float>(at[0]), static_cast<float>(at[1]), static_cast<float>(at[2])};
    out.models.push_back(d);
  }
}

void q2_draw_model_fx(FxState& s, const FxScene& l, double t, FxFrame& out) {
  const PlayerState& me = l.me;
  const int me_entity = l.own > 0 ? l.me.entity : 0;
  const int first = l.effect_models + static_cast<int>(EffectModel::q2_fx);
  int entity = kQ2FxEntities;
  std::erase_if(s.q2_models, [&](const Q2ModelFx& m) {
    return !q2_model_fx_pose(m, t, s.on.q2.smooth_explosions).alive;
  });
  if (s.q2_models_next >= s.q2_models.size()) s.q2_models_next = 0;
  for (const Q2ModelFx& m : s.q2_models) {
    const Q2FxPose pose = q2_model_fx_pose(m, t, s.on.q2.smooth_explosions);
    q2_light(m.at, pose.light, m.rgb, out);
    if (!m.drawn || pose.alpha <= 0) continue;
    Placed d;
    d.entity = entity--;
    d.modelindex = first + static_cast<int>(m.model);
    d.frame = pose.frame;
    d.frame_from = pose.frame_from;
    d.frame_f = pose.f;
    d.skin = pose.skin;
    d.alpha = pose.see_through || pose.alpha < 1 ? pose.alpha : 1;
    d.full_bright = m.full_bright;
    d.no_shadow = true;
    d.origin = {m.at[0], m.at[1], m.at[2]};
    d.angles = {m.angles[0], m.angles[1], m.angles[2]};
    d.scale = m.scale;
    out.models.push_back(d);
  }
  // The cables: a segment every 30 units, each at a new roll this frame.
  std::erase_if(s.q2_cables, [&](const FxState::Q2Cable& c) { return t > c.until; });
  // Their roll's seed taken only while one is up: another game's effects keep their own seeds.
  ge::core::Rng roll = ge::core::make_rng(s.q2_cables.empty() ? 0 : s.seed++);
  for (const FxState::Q2Cable& c : s.q2_cables) {
    sh::Vec3f from = c.from;
    if (me_entity > 0 && c.entity == me_entity)  // the player's own: from him as drawn
      from = {me.origin.x + c.offset.x, me.origin.y + c.offset.y, me.origin.z + c.offset.z};
    if (c.lightning) {
      q2_lightning(from, c.to, first + static_cast<int>(Q2FxModel::lightning), roll, entity,
                   out.models);
      continue;
    }
    Chain chain;
    chain.modelindex =
        first + static_cast<int>(c.ctf ? Q2FxModel::ctf_segment : Q2FxModel::segment);
    chain.fit = chain.no_shadow = true;
    place_chain(from, c.to, chain, roll, entity, out.models);
  }
  q2_heat_beams(s, l, first + static_cast<int>(Q2FxModel::heat_beam), t, entity, out);
  // The BFG's lasers: a strip 4 units wide in their shade, a tenth of a second each.
  std::erase_if(s.q2_lasers, [&](const FxState::Q2Laser& z) { return t - z.born >= kQ2BfgLaserS; });
  for (const FxState::Q2Laser& z : s.q2_lasers) {
    HeldBeam b;
    b.entity = entity--;
    b.laser = true;
    b.laser_m = static_cast<float>(z.width_u * ge::brush::kUnitM);
    for (int k = 0; k < 3; ++k) b.laser_rgb[k] = z.rgb[k];
    b.shot.born_s = t - 1;  // long on: no swell
    b.shot.seed = static_cast<uint32_t>(-b.entity);
    put_point(z.from, b.shot.from);
    put_point(z.to, b.shot.to);
    out.beams.push_back(b);
  }
}

void q2_named_fx(FxState& s, const Effect& e, const double p[3], bool dust, double t,
                 FxFrame& out) {
  const std::string_view n = e.name;
  if (n == "sq_widow_beam") {  // kept for 2.1 s, one per id (a later one takes its place)
    std::erase_if(s.beam_outs, [&](const FxState::BeamOut& b) { return b.id == e.entity; });
    if (e.entity == 0 || s.beam_outs.size() + s.nukes.size() >= 32) return;
    FxState::BeamOut b;
    b.id = e.entity;
    for (int k = 0; k < 3; ++k) b.at[k] = p[k];
    b.born = t;
    s.beam_outs.push_back(b);
    return;
  }
  if (n == "sq_power_splash") {
    fx::q2_power_splash(p, e.way.x, s.seed++, s.palette, &out.births);
    return;
  }
  if (n == "sq_boss_teleport")
    fx::q2_boss_teleport(p, s.seed++, s.palette, &out.births);
  else if (n == "sq_teleport" && !dust)
    fx::q2_teleport_burst(p, s.seed++, s.palette, &out.births);
  else if (n == "sq_item_back")
    fx::q2_item_back(p, s.seed++, s.palette, &out.births);
  else if (n == "sq_fountain")
    fx::q2_fountain(p, s.seed++, s.palette, &out.births);
  else if (n == "sq_laser_sparks" || n == "sq_splash") {
    const double off_face[3] = {e.way.x, e.way.y, e.way.z};
    const bool splash = n == "sq_splash";
    fx::q2_specks(p, off_face, e.count, e.colour, splash ? 32 : 8, splash, s.seed++, s.palette,
                  &out.births);
  } else if (n == "sq_welding" || n == "sq_tunnel_sparks") {
    const double off_face[3] = {e.way.x, e.way.y, e.way.z};
    const bool rising = n == "sq_tunnel_sparks";
    fx::q2_specks(p, off_face, e.count, e.colour, 8, false, s.seed++, s.palette, &out.births,
                  rising ? -80 : 80);
    if (!rising)  // the welder's light, yellow-white, for the frame it is thrown in
      out.lights.push_back(fx::q2_point_light(p, 100 + double(s.seed++ % 75), 1, 1, 0.3));
  } else if (n == "sq_steam" || n == "sq_smoke") {
    const bool smoke = n == "sq_smoke";
    const double jet[3] = {smoke ? 0.0 : e.way.x, smoke ? 0.0 : e.way.y, smoke ? 20.0 : e.way.z};
    fx::q2_steam(p, jet, smoke ? 20 : e.count, smoke ? 0 : e.colour, smoke ? 0 : 40, s.seed++,
                 s.palette, &out.births);
  } else if (n == "sq_blue_hit") {
    const double way[3] = {e.way.x, e.way.y, e.way.z};
    fx::q2_blue_hit(p, way, s.seed++, s.palette, &out.births);
  } else if (n == "sq_widow_splash") {
    fx::q2_widow_splash(p, s.seed++, s.palette, &out.births);
  } else if (n == "sq_flashlight") {
    out.lights.push_back(fx::q2_point_light(p, 400, 1, 1, 1));
  } else if (n == "sq_logout")
    fx::q2_player_flash(p, fx::kSqLogout, s.seed++, s.palette, &out.births);
  else if ((n == "sq_login" || n == "sq_respawn") && !dust)
    fx::q2_player_flash(p, n == "sq_login" ? fx::kSqLogin : fx::kSqRespawn, s.seed++, s.palette,
                        &out.births);
  // The flash's light, for the one frame it is thrown in.
  if (n == "sq_login" || n == "sq_logout" || n == "sq_respawn") {
    const fx::Q2Arrival kind = n == "sq_login"    ? fx::kSqLogin
                               : n == "sq_logout" ? fx::kSqLogout
                                                  : fx::kSqRespawn;
    out.lights.push_back(fx::q2_arrival_light(p, kind, s.seed++));
  }
}

bool q2_pack_hit(FxState& s, const Effect& e, const double p[3], double t, FxFrame& out) {
  const std::string_view n = e.name;
  const double way[3] = {e.way.x, e.way.y, e.way.z};
  if (n == "te_blaster2" || n == "te_flechette") {  // the blaster's hit in its own colours
    const bool green = n == "te_blaster2";
    fx::q2_blaster_specks(p, way, s.seed++, s.palette, &out.births, green ? 0xd0 : 0x6f);
    q2_model_fx(s, green ? 30 : 55, e.at, e.way, t);
    return true;
  }
  if (n == "te_heatbeam_sparks" || n == "te_heatbeam_steam") {  // steam off the face at 60
    const bool sparks = n == "te_heatbeam_sparks";
    const double jet[3] = {way[0] * 60, way[1] * 60, way[2] * 60};
    fx::q2_steam(p, jet, sparks ? 50 : 20, sparks ? 8 : 0xe0, 40, s.seed++, s.palette, &out.births);
    return true;
  }
  if (const Q2Puff* puff = q2_puff(n); puff != nullptr && (puff->code == 42 || puff->code == 46)) {
    fx::q2_specks(p, way, puff->count, puff->colour, 32, true, s.seed++, s.palette, &out.births);
    return true;
  }
  return false;
}

void q2_sustains(FxState& s, double t, FxFrame& out) {
  std::erase_if(s.nukes, [&](const FxState::Nuke& n) { return t - n.born > kSqNukeS; });
  for (const FxState::Nuke& n : s.nukes)
    fx::q2_nuke_shell(n.at, std::clamp((t - n.born) / kSqNukeS, 0.0, 1.0), s.seed++, s.palette,
                      &out.births);
  std::erase_if(s.beam_outs, [&](const FxState::BeamOut& b) { return t - b.born > kSqBeamOutS; });
  for (const FxState::BeamOut& b : s.beam_outs)
    fx::q2_beam_out(b.at, std::clamp((t - b.born) / kSqBeamOutS, 0.0, 1.0), s.seed++, s.palette,
                    &out.births);
}

bool q2_named_trail(FxState& s, const Effect& e, const double from[3], const double to[3], double t,
                    FxFrame& out) {
  if (e.name == "sq_bubbles" || e.name == "sq_bubbles2") {
    const bool second = e.name == "sq_bubbles2";
    if (!s.on.bubbles) {
      fx::q2_bubbles(from, to, second, s.seed++, s.palette, &out.births);
      return true;
    }
    // The engine's bubbles, one sprite each where the classic's specks would rise.
    const double a[3] = {e.at.x, e.at.y, e.at.z}, b[3] = {e.end.x, e.end.y, e.end.z};
    std::vector<fx::RisingBubble> made;
    fx::q2_rising_bubbles(a, b, second, s.seed++, &made);
    for (const fx::RisingBubble& r : made) {
      if (s.own_bubbles.size() >= kOwnBubbles) break;
      s.own_bubbles.push_back({r, t, s.bubble_ids++});
    }
    return true;
  }
  if (e.name == "sq_force_wall") {
    fx::q2_force_wall(from, to, e.colour, s.seed++, s.palette, &out.births);
    return true;
  }
  return false;
}

void q2_mark_fx(FxState& s, const Missile& m, bool moved, const double from[3], const double to[3],
                double t, FxFrame& out) {
  using K = Q2Look::Missile;
  const auto k = static_cast<K>(m.q2_mark);
  // Spinning lights: a red light 64 units ahead along the spin, whatever else it carries.
  if (m.q2_spin) {
    const double yaw = (t * kSqSpinDegPerS + m.yaw) * 3.14159265358979323846 / 180;
    const double ahead[3] = {to[0] + 64 * ge::brush::kUnitM * std::cos(yaw), to[1],
                             to[2] + 64 * ge::brush::kUnitM * std::sin(yaw)};
    out.lights.push_back(fx::q2_point_light(ahead, 100, 1, 0, 0));
  }
  if (k == K::none) return;
  if (k == K::trap) {  // its glow 32 units over it, a light flickering each frame
    const double up[3] = {to[0], to[1] + 32 * ge::brush::kUnitM, to[2]};
    fx::q2_trap(up, s.seed++, s.palette, &out.births);
    out.lights.push_back(fx::q2_point_light(up, 100 + double(s.seed++ % 100), 1, 0.8, 0.1));
    return;
  }
  if (k == K::tracker_dark) {  // no specks; a darkening light whose reach pulses
    const double reach = 50 + 500 * (std::sin(t * 1000 / 500) + 1);
    out.lights.push_back(fx::q2_point_light(to, reach, -1, -1, -1));
    return;
  }
  if (k == K::tracker_shell) {  // a black shell round it, a darkening light
    fx::q2_tracker_shell(to, 40, s.seed++, s.palette, &out.births);
    out.lights.push_back(fx::q2_point_light(to, 155, -1, -1, -1));
    return;
  }
  if (k == K::tracker) {  // two black strands and a darkening light
    if (moved) fx::q2_tracker_trail(from, to, s.seed++, s.palette, &out.births);
    out.lights.push_back(fx::q2_point_light(to, 200, -1, -1, -1));
    return;
  }
  if (moved) {
    if (k == K::flag1 || k == K::flag2 || k == K::tag)
      fx::q2_column_trail(from, to,
                          k == K::flag1   ? 242
                          : k == K::flag2 ? 115
                                          : 220,
                          s.seed++, s.palette, &out.births);
    else if (k == K::ion) {
      int side = 1;
      fx::q2_ion_trail(from, to, side, s.seed++, s.palette, &out.births);
    }
  }
  const double r = k == K::ion ? 100 : k == K::blue ? 200 : k == K::plasma ? 130 : 225;
  const double red = k == K::flag2 ? 0.1 : k == K::blue ? 0 : 1;
  const double green = k == K::flag1 || k == K::flag2 ? 0.1
                       : k == K::tag                  ? 1
                       : k == K::blue                 ? 0
                                                      : 0.5;
  const double blue = k == K::flag2 || k == K::blue ? 1
                      : k == K::flag1               ? 0.1
                      : k == K::tag                 ? 0
                                                    : 0.5;
  out.lights.push_back(fx::q2_point_light(to, r, red, green, blue));
}

}  // namespace shooters_edge::client
