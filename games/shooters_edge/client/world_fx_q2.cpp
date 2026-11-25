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

// Ground Zero's held lines each frame: the tesla's bolts and the heat beams as chains of their
// segments, a monster's beam's shell, the player's own beam's rings (defined below).
void q2_draw_held(FxState& s, int first, const PlayerState& me, int me_entity, double t,
                  int& entity, FxFrame& out);

// A light of `reach` units in `rgb` at `at` (map units).
void q2_light(const float at[3], float reach, const float rgb[3], FxFrame& out) {
  if (reach <= 0) return;
  double p[3];
  put_point({at[0], at[1], at[2]}, p);
  out.lights.push_back(fx::q2_point_light(p, reach, rgb[0], rgb[1], rgb[2]));
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
    case 28:  // The Reckoning's plasma explosion: explosion1's
      if (!no_specks) fx::q2_blast_specks(p, 0xe0, s.seed++, s.palette, &out.births);
      q2_model_fx(s, code, at, {0, 0, 1}, t);
      return true;
    case 20:  // the BFG ball's burst: its sprite and green light, no specks
      q2_model_fx(s, code, at, {0, 0, 1}, t);
      return true;
    case 21:  // its big burst: specks alone
      fx::q2_blast_specks(p, 0xd0, s.seed++, s.palette, &out.births);
      return true;
    case 35:  // Ground Zero's plain, big and speckless explosions: the ball and its light alone
    case 53:
    case 54:
      q2_model_fx(s, code, at, {0, 0, 1}, t);
      return true;
    case 47:  // the disruptor's burst: black specks and a darkening light
      if (!no_specks) fx::q2_tracker_burst(p, s.seed++, s.palette, &out.births);
      q2_model_fx(s, code, at, {0, 0, 1}, t);
      return true;
    case 51:  // the A-M bomb's blast: a sphere of specks growing for a second (q2_sustains)
      if (s.q2_nukes.size() < kQ2Nukes) {
        FxState::Q2Nuke n;
        for (int k = 0; k < 3; ++k) n.at[k] = p[k];
        n.born = t;
        s.q2_nukes.push_back(n);
      }
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
  const bool ctf = e.name == "te_grapple_cable";
  if (!ctf && e.name != "te_parasite_attack" && e.name != "te_medic_cable_attack") return false;
  auto c = std::find_if(s.q2_cables.begin(), s.q2_cables.end(),
                        [&](const FxState::Q2Cable& k) { return k.entity == e.entity; });
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

bool q2_held_line(FxState& s, const Effect& e, double t) {
  const bool bolt = e.name == "te_lightning";
  const bool monster = e.name == "te_monster_heatbeam";
  if (!bolt && !monster && e.name != "te_heatbeam") return false;
  // A bolt is one a pair (its source and what it strikes), a heat beam one a source.
  auto h = std::find_if(s.q2_held.begin(), s.q2_held.end(), [&](const FxState::Q2HeldLine& k) {
    return k.bolt == bolt && k.entity == e.entity && (!bolt || k.struck == e.colour);
  });
  double life = kQ2HeldLineS;
  if (h == s.q2_held.end()) {
    // A new one takes a free or a spent place; none: dropped.
    h = std::find_if(s.q2_held.begin(), s.q2_held.end(),
                     [&](const FxState::Q2HeldLine& k) { return k.bolt == bolt && t > k.until; });
    if (h == s.q2_held.end()) {
      const auto n = std::count_if(s.q2_held.begin(), s.q2_held.end(),
                                   [&](const FxState::Q2HeldLine& k) { return k.bolt == bolt; });
      if (static_cast<size_t>(n) >= kQ2HeldLines) return true;
      s.q2_held.emplace_back();
      h = s.q2_held.end() - 1;
    }
    if (!bolt) life = kQ2HeldLineFirstS;  // a beam's first message: it is not stacked
  }
  h->entity = e.entity;
  h->struck = bolt ? e.colour : 0;
  h->from = e.at;
  h->to = e.end;
  h->bolt = bolt;
  h->monster = monster;
  h->until = t + life;
  return true;
}

bool q2_pack_point(FxState& s, const Effect& e, const double p[3], double t, FxFrame& out) {
  const int code = q2_pack_te_code(e.name);
  if (code == 0) return false;
  const double way[3] = {e.way.x, e.way.y, e.way.z};
  switch (code) {
    case 30:  // a green blaster's hit and a flechette's: specks, the flare and its light
    case 55:
      fx::q2_blaster_specks(p, way, s.seed++, s.palette, &out.births, code == 30 ? 0xd0 : 0x6f);
      q2_model_fx(s, code, e.at, e.way, t);
      break;
    case 42:  // more blood: 250 red specks off the face
      fx::q2_specks(p, way, 250, 0xe8, 32, true, s.seed++, s.palette, &out.births);
      break;
    case 46:  // electric sparks: 40 blue specks off the face
      fx::q2_specks(p, way, 40, 0x75, 32, true, s.seed++, s.palette, &out.births);
      break;
    case 43:    // the heat beam's sparks and steam: thrown along the face at 60 units/s, half the
    case 44: {  // fall
      const double jet[3] = {way[0] * 60, way[1] * 60, way[2] * 60};
      fx::q2_steam(p, jet, code == 43 ? 50 : 20, code == 43 ? 0x08 : 0xe0, 40, s.seed++, s.palette,
                   &out.births);
      break;
    }
    default:
      break;
  }
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

void q2_draw_model_fx(FxState& s, int effect_models, const PlayerState& me, int me_entity, double t,
                      FxFrame& out) {
  const int first = effect_models + static_cast<int>(EffectModel::q2_fx);
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
    Chain chain;
    chain.modelindex =
        first + static_cast<int>(c.ctf ? Q2FxModel::ctf_segment : Q2FxModel::segment);
    chain.fit = chain.no_shadow = true;
    place_chain(from, c.to, chain, roll, entity, out.models);
  }
  q2_draw_held(s, first, me, me_entity, t, entity, out);
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

namespace {
// One link of a held line at `at` (map units) turned to (`pitch`, `yaw`, `roll`), degrees.
void q2_link(int modelindex, int frame, const sh::Vec3f& at, float pitch, float yaw, float roll,
             int& entity, FxFrame& out) {
  Placed seg;
  seg.entity = entity--;
  seg.modelindex = modelindex;
  seg.frame = seg.frame_from = frame;
  seg.full_bright = true;
  seg.no_shadow = true;
  seg.origin = at;
  seg.angles = {pitch, yaw, roll};
  out.models.push_back(seg);
}
}  // namespace

namespace {
void q2_draw_held(FxState& s, int first, const PlayerState& me, int me_entity, double t,
                  int& entity, FxFrame& out) {
  std::erase_if(s.q2_held, [&](const FxState::Q2HeldLine& h) { return t > h.until; });
  constexpr double kDeg = 180 / 3.14159265358979323846;
  // The bolts' rolls: a new draw every 16 ms of the clock, the same within one.
  ge::core::Rng roll = ge::core::make_rng(static_cast<uint32_t>(t * 1000 / 16));
  const sh::Vec3f eye = {me.origin.x + me.view_ofs.x, me.origin.y + me.view_ofs.y,
                         me.origin.z + me.view_ofs.z};
  for (const FxState::Q2HeldLine& h : s.q2_held) {
    const bool own = me_entity > 0 && h.entity == me_entity;
    sh::Vec3f from = h.from;
    sh::Vec3f d;
    if (h.bolt) {
      if (own) from = eye;  // the player's own bolt from his eye
      d = h.to - from;
    } else if (own && !h.monster) {
      // The player's own heat beam: from his gun (2 right, 7 ahead, 3 down from the eye), along
      // where he looks, as far as the server's end is from there.
      const sh::ViewAxes ax = sh::angle_vectors(me.v_angle);
      from = eye + ax.right * 2.0f + ax.forward * 7.0f + ax.up * -3.0f;
      const sh::Vec3f reach = h.to - from;
      d = ax.forward * std::sqrt(reach.x * reach.x + reach.y * reach.y + reach.z * reach.z);
    } else {
      d = h.to - from;
      if (!h.monster) {  // another player's: the gun's offset in the beam's own axes, which
                         // lands 7 units on toward the end, 1 to its right and 7 below
        const double yaw = std::atan2(d.y, d.x) * kDeg;
        const double down = -std::atan2(d.z, std::sqrt(d.x * d.x + d.y * d.y)) * kDeg;
        const sh::ViewAxes ax =
            sh::angle_vectors({static_cast<float>(down), static_cast<float>(yaw), 0});
        from = from + ax.forward * 7.0f + ax.right * 1.0f + ax.up * -7.0f;
        d = h.to - from;
      }
    }
    const double len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (len < 1e-6) continue;
    const auto yaw = static_cast<float>(std::atan2(d.y, d.x) * kDeg);
    const auto pitch = static_cast<float>(std::atan2(d.z, std::sqrt(d.x * d.x + d.y * d.y)) * kDeg);
    const sh::Vec3f u = d * static_cast<float>(1 / len);
    const Q2Links k = q2_links(len, h.bolt);
    const int model =
        first + static_cast<int>(h.bolt ? Q2FxModel::lightning : Q2FxModel::heat_beam);
    const int frame = h.bolt ? 0 : own && !h.monster ? 1 : 2;
    // The heat beam's links share one roll, turning a degree a millisecond.
    const auto spin = static_cast<float>(std::fmod(t * 1000, 360.0));
    if (k.alone) {
      q2_link(model, frame, h.to, pitch, yaw, static_cast<float>(ge::core::range(roll, 0, 360)),
              entity, out);
      continue;
    }
    for (int i = 0; i < k.n; ++i) {
      const sh::Vec3f at = from + u * static_cast<float>(k.step * i);
      const float r = h.bolt ? static_cast<float>(ge::core::range(roll, 0, 360)) : spin;
      q2_link(model, frame, at, -pitch, yaw + 180, r, entity, out);
    }
    if (h.bolt) continue;
    double p[3];
    put_point(from, p);
    if (h.monster) {  // a monster's: a shell of specks at its gun
      fx::q2_monster_beam_shell(p, s.seed++, s.palette, &out.births);
    } else if (own) {  // the player's own: rings sliding along it at 96 units a second
      const sh::ViewAxes ax = sh::angle_vectors(me.v_angle);
      const double way[3] = {u.x, u.y, u.z}, right[3] = {ax.right.x, ax.right.y, ax.right.z},
                   up[3] = {ax.up.x, ax.up.y, ax.up.z};
      fx::q2_heat_rings(p, way, right, up, len, std::fmod(t * 96, 32.0), s.seed++, s.palette,
                        &out.births);
    }
  }
}
}  // namespace

void q2_named_fx(FxState& s, const Effect& e, const double p[3], bool dust, double t,
                 FxFrame& out) {
  const std::string_view n = e.name;
  if (n == "sq_widow_beam") {  // kept for 2.1 s, one per id (a later one takes its place)
    std::erase_if(s.beam_outs, [&](const FxState::BeamOut& b) { return b.id == e.entity; });
    if (e.entity == 0 || s.beam_outs.size() >= 32) return;
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

void q2_sustains(FxState& s, double t, FxFrame& out) {
  std::erase_if(s.q2_nukes, [&](const FxState::Q2Nuke& n) { return t - n.born > kQ2NukeS; });
  for (const FxState::Q2Nuke& n : s.q2_nukes)
    fx::q2_nuke_blast(n.at, std::clamp((t - n.born) / kQ2NukeS, 0.0, 1.0), s.seed++, s.palette,
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
