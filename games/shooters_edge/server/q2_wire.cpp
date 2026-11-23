// Quake II's protocol 34 server messages from a runner's rows (q2_wire.hpp).
//
// ge:summary Quake II's wire, server side: the signon, frames, deltas and the recorder
// ge:keywords q2 wire signon configstring baseline frame playerinfo packetentities delta
// ge:keywords remove old-origin solid stats sound print recorder dm2
// ge:skill shooter replay
// ge:test tests/shooters_edge_q2_wire_test.cpp
#include "shooters_edge/server/q2_wire.hpp"

#include <algorithm>
#include <cstring>

#include "brush/areas.hpp"
#include "classic/protocol/directions.hpp"
#include "classic/protocol/info.hpp"
#include "shooters_edge/server/md4.hpp"
#include "shooters_edge/server/q2_screens.hpp"

namespace shooters_edge::server {

namespace {

namespace sh = ge::shooter;

// Server message ids.
constexpr uint8_t kPrint = 10, kStuffText = 11, kServerData = 12, kConfigString = 13,
                  kBaseline = 14, kCenterPrint = 15, kPlayerInfo = 17, kPacketEntities = 18,
                  kFrame = 20, kSound = 9, kLayout = 4, kInventory = 5, kTempEntity = 3,
                  kMuzzleFlash = 1, kMuzzleFlash2 = 2;

// Configstring slots.
constexpr int kCsName = 0, kCsSky = 2, kCsSkyAxis = 3, kCsSkyRotate = 4, kCsStatusBar = 5,
              kCsMaxClients = 30, kCsChecksum = 31, kCsModels = 32, kCsSounds = 288,
              kCsImages = 544, kCsLights = 800, kCsItems = 1056, kCsSkins = 1312;

// An entity delta's flag bits.
constexpr uint32_t kOriginX = 1u << 0, kOriginY = 1u << 1, kAngleYaw = 1u << 2,
                   kAngleRoll = 1u << 3, kFrameByte = 1u << 4, kEvent = 1u << 5, kRemove = 1u << 6,
                   kMore1 = 1u << 7, kNumberShort = 1u << 8, kOriginZ = 1u << 9,
                   kAnglePitch = 1u << 10, kModel = 1u << 11, kRenderLo = 1u << 12,
                   kEffectsLo = 1u << 14, kMore2 = 1u << 15, kSkinLo = 1u << 16,
                   kFrameShort = 1u << 17, kRenderHi = 1u << 18, kEffectsHi = 1u << 19,
                   kModel2 = 1u << 20, kModel3 = 1u << 21, kModel4 = 1u << 22, kMore3 = 1u << 23,
                   kOldOrigin = 1u << 24, kSkinHi = 1u << 25, kSoundBit = 1u << 26,
                   kSolid = 1u << 27;
constexpr uint32_t kBeam = 128;  // the render effect that always carries its old origin

// The player state's flag bits.
constexpr uint16_t kPmType = 1 << 0, kPmOrigin = 1 << 1, kPmVelocity = 1 << 2, kPmTime = 1 << 3,
                   kPmFlags = 1 << 4, kPmGravity = 1 << 5, kPmDelta = 1 << 6, kViewOffset = 1 << 7,
                   kViewAngles = 1 << 8, kKick = 1 << 9, kBlend = 1 << 10, kFov = 1 << 11,
                   kGunIndex = 1 << 12, kGunFrame = 1 << 13, kRdFlags = 1 << 14;

struct Out {
  std::vector<uint8_t>& b;
  void u8(uint32_t v) { b.push_back(static_cast<uint8_t>(v)); }
  void i16(int32_t v) {
    const auto u = static_cast<uint16_t>(v);
    u8(u & 0xFFu);
    u8(u >> 8);
  }
  void i32(int64_t v) {
    const auto u = static_cast<uint32_t>(v);
    for (int k = 0; k < 4; k++) u8((u >> (8 * k)) & 0xFFu);
  }
  void str(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    u8(0);
  }
};

int16_t coord(float v) { return static_cast<int16_t>(static_cast<int32_t>(v * 8.0f)); }
uint8_t angle8(float deg) {
  return static_cast<uint8_t>(static_cast<int32_t>(deg * 256.0f / 360.0f) & 0xFF);
}
int16_t angle16(float deg) {
  return static_cast<int16_t>(static_cast<int32_t>(deg * 65536.0f / 360.0f) & 0xFFFF);
}
int8_t quarter(float v) { return static_cast<int8_t>(std::clamp(v * 4.0f, -128.0f, 127.0f)); }

// A named effect (a point or a line the rules threw) as the protocol's temporary entity: its kind
// byte and payload. How each kind's bytes run: a point; a point and a direction byte; a count, a
// point, a direction and a colour; two points (a line); an entity and two points; two entities
// (the struck, then the striker in the event's level) and two points (the end, then the start).
enum class TeBody : uint8_t { point, point_dir, splash, line, entity_line, count, lightning };
struct TeName {
  const char* name;
  uint8_t kind;
  TeBody body;
  uint8_t colour;  // a splash's
};
constexpr TeName kTeNames[] = {
    {"te_gunshot", 0, TeBody::point_dir, 0},
    {"te_damage_dealt", 128, TeBody::count, 0},  // the hit report: one short, the points dealt
    {"te_blood", 1, TeBody::point_dir, 0},
    {"te_blaster", 2, TeBody::point_dir, 0},
    {"te_railtrail", 3, TeBody::line, 0},
    {"te_shotgun", 4, TeBody::point_dir, 0},
    {"te_explosion1", 5, TeBody::point, 0},
    {"te_explosion2", 6, TeBody::point, 0},
    {"te_explosion", 7, TeBody::point, 0},  // our rules' blast: a rocket's
    {"te_rocket_explosion", 7, TeBody::point, 0},
    {"te_grenade_explosion", 8, TeBody::point, 0},
    {"te_sparks", 9, TeBody::point_dir, 0},
    {"te_splash", 10, TeBody::splash, 0},
    {"te_splash_sparks", 10, TeBody::splash, 1},
    {"te_splash_water", 10, TeBody::splash, 2},
    {"te_splash_mud", 10, TeBody::splash, 3},
    {"te_splash_slime", 10, TeBody::splash, 4},
    {"te_splash_lava", 10, TeBody::splash, 5},
    {"te_splash_blood", 10, TeBody::splash, 6},
    {"te_screen_sparks", 12, TeBody::point_dir, 0},
    {"te_shield_sparks", 13, TeBody::point_dir, 0},
    {"te_bullet_sparks", 14, TeBody::point_dir, 0},
    {"te_laser_sparks", 15, TeBody::splash, 0},  // the colour the laser's event carries
    {"te_parasite_attack", 16, TeBody::entity_line, 0},
    {"te_medic_cable_attack", 19, TeBody::entity_line, 0},
    {"te_bfg_explosion", 20, TeBody::point, 0},
    {"te_bfg_bigexplosion", 21, TeBody::point, 0},
    {"te_boss_teleport", 22, TeBody::point, 0},
    // A pack's (The Reckoning's): the welder's and a cycling push's sparks in the event's colour,
    // a gekk's blood, the phalanx's blast.
    {"te_welding_sparks", 25, TeBody::splash, 0},
    {"te_greenblood", 26, TeBody::point_dir, 0},
    {"te_plasma_explosion", 28, TeBody::point, 0},
    {"te_tunnel_sparks", 29, TeBody::splash, 0},
    {"te_teleport", 48, TeBody::point, 0},
    // Ground Zero's: the rocket and grenade bursts under water, the tesla's bolt, the plasma beam
    // (a player's and a monster's), its bubbles, sparks and steam, more blood, the chainfist's
    // smoke, electric sparks, the disruptor's burst, the A-M bomb's blast and its big explosion,
    // the flechette's mark.
    {"te_rocket_explosion_water", 17, TeBody::point, 0},
    {"te_grenade_explosion_water", 18, TeBody::point, 0},
    {"te_lightning", 33, TeBody::lightning, 0},
    {"te_heatbeam", 38, TeBody::entity_line, 0},
    {"te_monster_heatbeam", 39, TeBody::entity_line, 0},
    {"te_bubbletrail2", 41, TeBody::line, 0},
    {"te_moreblood", 42, TeBody::point_dir, 0},
    {"te_heatbeam_sparks", 43, TeBody::point_dir, 0},
    {"te_heatbeam_steam", 44, TeBody::point_dir, 0},
    {"te_chainfist_smoke", 45, TeBody::point, 0},
    {"te_electric_sparks", 46, TeBody::point_dir, 0},
    {"te_tracker_explosion", 47, TeBody::point, 0},
    {"te_nukeblast", 51, TeBody::point, 0},
    {"te_explosion1_big", 53, TeBody::point, 0},
    {"te_flechette", 55, TeBody::point_dir, 0},
};

}  // namespace

std::string q2_temp_entity_name(int code) {
  for (const TeName& t : kTeNames)
    if (t.kind == code) return std::string(t.name);
  return {};
}

namespace {

// A player's entry or exit flash for `e` ("sq_login", "sq_logout" on its entity) as the
// protocol's muzzle flash 9 or 10; false for any other effect.
// A gun's flash: a player's shot ("q2_flash", its value in values[0]) as message 1, a monster's
// ("te_monster_flash.*" with a value in values[3]) as message 2 (server/q2_flash.hpp).
bool write_flash(Out& o, const NetEvent& e) {
  if (e.text == "q2_flash" && e.ent > 0 && e.ent <= sh::kMaxClients) {
    o.u8(kMuzzleFlash);
    o.i16(e.ent);
    o.u8(static_cast<uint32_t>(e.values[0]) & 255u);
    return true;
  }
  if (e.kind == EventKind::trail_effect && e.text.starts_with("te_monster_flash.")) {
    const int value = static_cast<int>(e.values[3]);
    if (value <= 0 || value > 255 || e.ent <= 0) return true;  // none the protocol has: dropped
    o.u8(kMuzzleFlash2);
    o.i16(e.ent);
    o.u8(static_cast<uint32_t>(value));
    return true;
  }
  const int flash = e.text == "sq_login" ? 9 : e.text == "sq_logout" ? 10 : 0;
  if (flash == 0 || e.ent <= 0 || e.ent > sh::kMaxClients) return false;
  o.u8(kMuzzleFlash);
  o.i16(e.ent);
  o.u8(static_cast<uint32_t>(flash));
  return true;
}

// A temporary entity for `e` onto `o`; false for a name the protocol has none for.
bool write_temp_entity(Out& o, const NetEvent& e) {
  const TeName* row = nullptr;
  for (const TeName& t : kTeNames)
    if (e.text == t.name) row = &t;
  if (row == nullptr) return false;
  const auto point = [&o](sh::Vec3f p) {
    o.i16(coord(p.x));
    o.i16(coord(p.y));
    o.i16(coord(p.z));
  };
  const auto way = [&o, &e] {
    float d[3] = {e.values[0], e.values[1], e.values[2]};
    if (d[0] == 0 && d[1] == 0 && d[2] == 0) d[2] = 1;  // none given: up
    o.u8(static_cast<uint32_t>(ge::classic::protocol::direction_index(d)));
  };
  const sh::Vec3f end = {e.values[0], e.values[1], e.values[2]};
  o.u8(kTempEntity);
  o.u8(row->kind);
  switch (row->body) {
    case TeBody::point:
      point(e.origin);
      break;
    case TeBody::point_dir:
      point(e.origin);
      way();
      break;
    case TeBody::splash: {
      const float n = e.values[3];
      o.u8(n >= 1 ? static_cast<uint32_t>(std::min(n, 255.0f)) : 8u);
      point(e.origin);
      way();
      o.u8(row->kind == 15 || row->kind == 25 || row->kind == 29
               ? static_cast<uint32_t>(e.level & 0xff)
               : row->colour);  // its event's own
      break;
    }
    case TeBody::line:
      point(e.origin);
      point(end);
      break;
    case TeBody::count:
      o.i16(static_cast<int>(e.values[0]));
      break;
    case TeBody::entity_line:
      o.i16(std::clamp(e.ent, 0, kQ2Entities - 1));
      point(e.origin);
      point(end);
      break;
    case TeBody::lightning:  // the source (the tesla), the target, then from and to
      o.i16(std::clamp(e.level, 0, kQ2Entities - 1));
      o.i16(std::clamp(e.ent, 0, kQ2Entities - 1));
      point(e.origin);
      point(end);
      break;
  }
  return true;
}

// The wire's model number for the runner's: the map itself is 1, so every other is one up.
int wire_model(int ours) { return ours <= 0 ? 0 : ours + 1; }

// The status bar of rules that send none, in the layout language a client draws (y from the bottom,
// x from the centre, the big numbers of health, ammunition and armour by their stats 1, 3 and 5).
// A longer one (Quake II's own, q2_bar.hpp) runs on into the slots after 5, as a client
// reads it.
constexpr const char* kStatusBar = "yb -24 xv 16 hnum xv 116 anum xv 216 rnum";

// The configstrings the runner's state gives.
void config_strings(const RulesCommon& c, const Q2WireOptions& opt, std::vector<std::string>& cs) {
  cs.assign(kQ2ConfigStrings, std::string());
  cs[kCsName] = c.world.map_title.empty() ? c.map_name : c.world.map_title;
  cs[1] = c.world.cd_track;  // the music's track
  cs[kCsMaxClients] = std::to_string(c.world.max_clients);
  cs[kCsChecksum] = std::to_string(opt.map_checksum);
  cs[kCsSky] = opt.sky;
  cs[kCsSkyAxis] = opt.sky_axis;
  cs[kCsSkyRotate] = opt.sky_rotate;
  cs[kCsStatusBar] = c.world.status_bar.empty() ? std::string(kStatusBar) : c.world.status_bar;
  cs[kCsModels + 1] = "maps/" + c.map_name + ".bsp";
  for (size_t i = 1; i < c.models.size(); i++) {
    const int at = wire_model(static_cast<int>(i));
    if (at < 256) cs[size_t(kCsModels) + size_t(at)] = c.models[i];
  }
  for (size_t i = 1; i < c.sounds.size() && i < 256; i++) cs[kCsSounds + i] = c.sounds[i];
  for (size_t i = 1; i < c.world.images.size() && i < 256; i++)
    cs[kCsImages + i] = c.world.images[i];
  for (size_t i = 1; i < c.world.item_names.size() && i < 256; i++)
    cs[kCsItems + i] = c.world.item_names[i];
  for (int i = 0; i < kLightStyles && i < kWorldLightStyles; i++)
    cs[size_t(kCsLights) + size_t(i)] = c.world.lightstyles[i];
  for (int s = 0; s < sh::kMaxClients; s++)
    if (c.world.clients[s].active)
      cs[size_t(kCsSkins) + size_t(s)] = c.world.clients[s].name + "\\male/grunt";
}

// The solid's packed box (a brush model's 31; a box by its half width, its depth and height).
int packed_solid(const WorldEntity& e, bool brush) {
  if (brush) return 31;
  const int x = std::clamp(static_cast<int>(e.maxs.x / 8), 1, 31);
  const int down = std::clamp(static_cast<int>(-e.mins.z / 8), 1, 31);
  const int up = std::clamp(static_cast<int>((e.maxs.z + 32) / 8), 1, 63);
  return (up << 10) | (down << 5) | x;
}

// Entity `n` as the wire shows it, or not present.
Q2EntityState entity_state(const RulesCommon& c, int n) {
  Q2EntityState s;
  const RulesWorld& w = c.world;
  if (n <= 0 || n >= w.entity_count || n >= kQ2Entities) return s;
  const WorldEntity& e = w.entities[size_t(n)];
  // Drawn, or heard: a speaker has no model but its loop (the sight's 400 units keep it near).
  // A beam has no model of its own (it is sent as the world's, which only has to be non-zero).
  const bool beam = (e.render_fx & kBeam) != 0;
  if (e.free || (e.modelindex <= 0 && e.loop <= 0 && !beam)) return s;
  const bool player = n <= sh::kMaxClients;
  if (player && !w.clients[n - 1].active) return s;
  s.present = true;
  const float o[3] = {e.origin.x, e.origin.y, e.origin.z};
  const float a[3] = {e.angles.x, e.angles.y, e.angles.z};
  for (int k = 0; k < 3; k++) {
    s.origin[k] = coord(o[k]);
    s.angles[k] = angle8(a[k]);
  }
  std::memcpy(s.old_origin, s.origin, sizeof s.origin);
  s.model[0] = beam ? 1 : wire_model(e.modelindex);
  if (beam) {  // drawn to its far end: the old origin
    const float to[3] = {e.beam_end.x, e.beam_end.y, e.beam_end.z};
    for (int k = 0; k < 3; k++) s.old_origin[k] = coord(to[k]);
  }
  s.frame = e.frame;
  s.skin = static_cast<uint32_t>(e.skin);
  if (player) {
    // A player is drawn from his client info (model 255, the low byte of the skin his slot), his
    // carried weapon a second model (255 too), chosen by the skin's high byte: its place among
    // the model list's carried names ("#..."), from 1.
    s.model[0] = 255;
    s.skin = static_cast<uint32_t>(n - 1);
    if (e.carried_model > 0) {
      uint32_t place = 0;
      for (int i = 1; i <= e.carried_model && size_t(i) < c.models.size(); i++)
        if (!c.models[size_t(i)].empty() && c.models[size_t(i)][0] == '#') place++;
      s.model[1] = 255;
      s.skin |= place << 8;
    }
  }
  s.effects = static_cast<uint32_t>(e.effects);
  s.renderfx = static_cast<uint32_t>(e.render_fx);
  s.event = e.event;
  s.sound = e.loop > 0 && e.loop < 256 ? e.loop : 0;  // the loop it carries
  const std::string& model =
      size_t(e.modelindex) < c.models.size() ? c.models[size_t(e.modelindex)] : std::string();
  const bool brush = !model.empty() && model[0] == '*';
  if (!beam && (player || brush || e.solid_box)) s.solid = packed_solid(e, brush);
  return s;
}

// The flags a delta from `from` to `to` sets (`old` forces the old origin).
uint32_t delta_flags(const Q2EntityState& from, const Q2EntityState& to, int number, bool old) {
  uint32_t f = 0;
  if (to.origin[0] != from.origin[0]) f |= kOriginX;
  if (to.origin[1] != from.origin[1]) f |= kOriginY;
  if (to.origin[2] != from.origin[2]) f |= kOriginZ;
  if (to.angles[0] != from.angles[0]) f |= kAnglePitch;
  if (to.angles[1] != from.angles[1]) f |= kAngleYaw;
  if (to.angles[2] != from.angles[2]) f |= kAngleRoll;
  if (to.model[0] != from.model[0]) f |= kModel;
  if (to.model[1] != from.model[1]) f |= kModel2;
  if (to.model[2] != from.model[2]) f |= kModel3;
  if (to.model[3] != from.model[3]) f |= kModel4;
  if (to.frame != from.frame) f |= to.frame < 256 ? kFrameByte : kFrameShort;
  if (to.skin != from.skin)
    f |= to.skin < 256 ? kSkinLo : to.skin < 65536 ? kSkinHi : (kSkinLo | kSkinHi);
  if (to.effects != from.effects)
    f |= to.effects < 256     ? kEffectsLo
         : to.effects < 32768 ? kEffectsHi
                              : (kEffectsLo | kEffectsHi);
  if (to.renderfx != from.renderfx)
    f |= to.renderfx < 256 ? kRenderLo : to.renderfx < 32768 ? kRenderHi : (kRenderLo | kRenderHi);
  if (to.sound != from.sound) f |= kSoundBit;
  if (to.event) f |= kEvent;
  if (to.solid != from.solid) f |= kSolid;
  if (old || (to.renderfx & kBeam)) f |= kOldOrigin;
  if (number >= 256) f |= kNumberShort;
  return f;
}

void sized(Out& o, uint32_t v, bool lo, bool hi) {
  if (lo && hi)
    o.i32(static_cast<int32_t>(v));
  else if (hi)
    o.i16(static_cast<int32_t>(v));
  else
    o.u8(v);
}

// One entity entry: the header bytes, the number, the fields in the wire's order.
void write_entity(Out& o, uint32_t f, int number, const Q2EntityState& to,
                  bool long_solid = false) {
  if (f & 0xFF000000u) f |= kMore3;
  if (f & 0x00FF0000u) f |= kMore2;
  if (f & 0x0000FF00u) f |= kMore1;
  o.u8(f & 0xFFu);
  if (f & kMore1) o.u8((f >> 8) & 0xFFu);
  if (f & kMore2) o.u8((f >> 16) & 0xFFu);
  if (f & kMore3) o.u8((f >> 24) & 0xFFu);
  if (f & kNumberShort)
    o.i16(number);
  else
    o.u8(static_cast<uint32_t>(number));
  if (f & kRemove) return;
  if (f & kModel) o.u8(static_cast<uint32_t>(to.model[0]));
  if (f & kModel2) o.u8(static_cast<uint32_t>(to.model[1]));
  if (f & kModel3) o.u8(static_cast<uint32_t>(to.model[2]));
  if (f & kModel4) o.u8(static_cast<uint32_t>(to.model[3]));
  if (f & kFrameByte) o.u8(static_cast<uint32_t>(to.frame));
  if (f & kFrameShort) o.i16(to.frame);
  if (f & (kSkinLo | kSkinHi)) sized(o, to.skin, (f & kSkinLo) != 0, (f & kSkinHi) != 0);
  if (f & (kEffectsLo | kEffectsHi))
    sized(o, to.effects, (f & kEffectsLo) != 0, (f & kEffectsHi) != 0);
  if (f & (kRenderLo | kRenderHi))
    sized(o, to.renderfx, (f & kRenderLo) != 0, (f & kRenderHi) != 0);
  if (f & kOriginX) o.i16(to.origin[0]);
  if (f & kOriginY) o.i16(to.origin[1]);
  if (f & kOriginZ) o.i16(to.origin[2]);
  if (f & kAnglePitch) o.u8(to.angles[0]);
  if (f & kAngleYaw) o.u8(to.angles[1]);
  if (f & kAngleRoll) o.u8(to.angles[2]);
  if (f & kOldOrigin)
    for (const int32_t v : to.old_origin) o.i16(v);
  if (f & kSoundBit) o.u8(static_cast<uint32_t>(to.sound));
  if (f & kEvent) o.u8(static_cast<uint32_t>(to.event));
  if (f & kSolid) {
    if (long_solid)
      o.i32(q2_long_solid(to.solid));
    else
      o.i16(to.solid);
  }
}

Q2PlayerState player_state(const RulesCommon& c, int slot, const ge::classic::Q2State* move) {
  Q2PlayerState p;
  const RulesWorld& w = c.world;
  const WorldClient& wc = w.clients[slot];
  const size_t n = size_t(slot) + 1;
  const WorldEntity e = n < w.entities.size() ? w.entities[n] : WorldEntity{};
  if (move) {
    p.type = static_cast<uint8_t>(move->type);
    for (int k = 0; k < 3; k++) {
      p.origin[k] = move->origin[k];
      p.velocity[k] = move->velocity[k];
      p.delta_angles[k] = move->delta_angles[k];
    }
    p.time = move->time;
    p.flags = move->flags;
    p.gravity = move->gravity;
  } else {
    const float o[3] = {e.origin.x, e.origin.y, e.origin.z};
    const float v[3] = {e.velocity.x, e.velocity.y, e.velocity.z};
    for (int k = 0; k < 3; k++) {
      p.origin[k] = coord(o[k]);
      p.velocity[k] = coord(v[k]);
    }
    p.gravity = 800;
  }
  p.view_offset[2] = quarter(wc.numbers.view_z);
  if (wc.numbers.view_set)  // the rules' own view: the bob, the tilts and the kicks
    for (int k = 0; k < 3; k++) {
      p.view_offset[k] = quarter(wc.numbers.view_offset[k]);
      p.kick[k] = quarter(wc.numbers.kick[k]);
      p.gun_offset[k] = quarter(wc.numbers.gun_offset[k]);
      p.gun_angles[k] = quarter(wc.numbers.gun_angles[k]);
    }
  const float va[3] = {e.view_angles.x, e.view_angles.y, e.view_angles.z};
  for (int k = 0; k < 3; k++) p.view_angles[k] = angle16(va[k]);
  p.gun_index = static_cast<uint8_t>(wire_model(wc.numbers.weapon_model));
  p.gun_frame = static_cast<uint8_t>(static_cast<int>(wc.numbers.weapon_frame));
  p.fov = 90;
  p.rdflags = static_cast<uint8_t>(wc.numbers.rdflags);
  for (int k = 0; k < 4; k++)  // each part as a byte, cut down
    p.blend[k] = static_cast<uint8_t>(std::clamp(wc.numbers.blend[k], 0.0f, 1.0f) * 255.0f);
  if (!c.world.status_bar.empty()) {  // the rules' own stats for their own bar
    for (int k = 0; k < 32; k++) p.stats[k] = wc.numbers.stats[k];
  } else {
    p.stats[1] = static_cast<int16_t>(static_cast<int>(wc.numbers.health));
    p.stats[3] = static_cast<int16_t>(static_cast<int>(wc.numbers.current_ammo));
    p.stats[5] = static_cast<int16_t>(static_cast<int>(wc.numbers.armour));
  }
  return p;
}

void write_player(Out& o, const Q2PlayerState& a, const Q2PlayerState& b) {
  const auto same3 = [](const auto& x, const auto& y) {
    return x[0] == y[0] && x[1] == y[1] && x[2] == y[2];
  };
  uint16_t f = kGunIndex;  // always, as the game's own server sends it
  if (b.type != a.type) f |= kPmType;
  if (!same3(b.origin, a.origin)) f |= kPmOrigin;
  if (!same3(b.velocity, a.velocity)) f |= kPmVelocity;
  if (b.time != a.time) f |= kPmTime;
  if (b.flags != a.flags) f |= kPmFlags;
  if (b.gravity != a.gravity) f |= kPmGravity;
  if (!same3(b.delta_angles, a.delta_angles)) f |= kPmDelta;
  if (!same3(b.view_offset, a.view_offset)) f |= kViewOffset;
  if (!same3(b.view_angles, a.view_angles)) f |= kViewAngles;
  if (!same3(b.kick, a.kick)) f |= kKick;
  if (b.gun_frame != a.gun_frame) f |= kGunFrame;
  if (std::memcmp(b.blend, a.blend, sizeof b.blend) != 0) f |= kBlend;
  if (b.fov != a.fov) f |= kFov;
  if (b.rdflags != a.rdflags) f |= kRdFlags;
  o.u8(kPlayerInfo);
  o.i16(f);
  if (f & kPmType) o.u8(b.type);
  if (f & kPmOrigin)
    for (const int32_t v : b.origin) o.i16(v);
  if (f & kPmVelocity)
    for (const int32_t v : b.velocity) o.i16(v);
  if (f & kPmTime) o.u8(b.time);
  if (f & kPmFlags) o.u8(b.flags);
  if (f & kPmGravity) o.i16(b.gravity);
  if (f & kPmDelta)
    for (const int16_t v : b.delta_angles) o.i16(v);
  if (f & kViewOffset)
    for (const int8_t v : b.view_offset) o.u8(static_cast<uint8_t>(v));
  if (f & kViewAngles)
    for (const int16_t v : b.view_angles) o.i16(v);
  if (f & kKick)
    for (const int8_t v : b.kick) o.u8(static_cast<uint8_t>(v));
  if (f & kGunIndex) o.u8(b.gun_index);
  if (f & kGunFrame) {
    o.u8(b.gun_frame);
    for (const int8_t v : b.gun_offset) o.u8(static_cast<uint8_t>(v));
    for (const int8_t v : b.gun_angles) o.u8(static_cast<uint8_t>(v));
  }
  if (f & kBlend)
    for (const uint8_t v : b.blend) o.u8(v);
  if (f & kFov) o.u8(b.fov);
  if (f & kRdFlags) o.u8(b.rdflags);
  uint32_t mask = 0;
  for (int i = 0; i < 32; i++)
    if (b.stats[i] != a.stats[i]) mask |= 1u << i;
  o.i32(static_cast<int32_t>(mask));
  for (int i = 0; i < 32; i++)
    if (mask & (1u << i)) o.i16(b.stats[i]);
}

// A later protocol's viewer reads the solid as a long (36, and 35 from minor 1905).
bool long_solid(const Q2WireOptions& opt) {
  return opt.protocol == 36 || (opt.protocol == 35 && opt.minor >= 1905);
}

bool to_slot(const NetEvent& e, int slot) { return (e.to >> slot) & 1u; }

// The most bytes this frame's block may take: the block's own 1400, or less when the packet's
// room (the reliable data it carries, and `written` more when this frame's reliable data joins
// it, with 64 for the messages the caller adds after) leaves less; never under 64.
size_t frame_cap(const Q2Wire& w, size_t written) {
  if (w.room == 0) return size_t(kQ2MaxBlock);
  const size_t taken = w.room_grows ? written + 64 : 0;
  const size_t left = taken < w.room ? w.room - taken : 0;
  return std::clamp(left, size_t{64}, size_t(kQ2MaxBlock));
}

// The bytes the entities may fill of a frame of `cap` bytes: the list's end and the effects' and
// sounds' share (200 bytes, or a quarter of a short frame) kept.
size_t entity_room(size_t cap) { return cap - 2 - std::min(size_t{200}, cap / 4); }

// The viewer's eye for the cut (map units): the player state's origin and view offset.
void sight_eye(const Q2PlayerState& p, float eye[3]) {
  for (int k = 0; k < 3; k++)
    eye[k] = static_cast<float>(p.origin[k]) / 8.0f + static_cast<float>(p.view_offset[k]) / 4.0f;
}

}  // namespace

bool q2_sight_view(Q2Wire& w, const float eye[3]) {
  w.area = 0;
  w.area_bits.clear();
  if (w.sight == nullptr || w.sight->map == nullptr) return false;
  const ge::brush::Map& m = *w.sight->map;
  ge::brush::clusters_seen_from(m, eye, 8.0f, w.seen);
  ge::brush::clusters_heard_from(m, eye, w.heard);
  if (!m.areas.empty() && !w.sight->groups.empty()) {
    w.area = ge::brush::area_at(m, eye);
    const size_t n = m.areas.size();
    w.area_bits.assign((n + 7) / 8, w.area == 0 ? 0xff : 0);
    if (w.area != 0)
      for (size_t a = 1; a < n; a++)
        if (ge::brush::areas_connected(w.sight->groups, w.area, int32_t(a)))
          w.area_bits[a / 8] |= uint8_t(1u << (a % 8));
  }
  return true;
}

void q2_write_area_bits(std::vector<uint8_t>& out, const Q2Wire& w) {
  if (w.area_bits.empty()) {
    out.push_back(32);
    out.insert(out.end(), 32, 0xFF);
    return;
  }
  out.push_back(static_cast<uint8_t>(w.area_bits.size()));
  out.insert(out.end(), w.area_bits.begin(), w.area_bits.end());
}

namespace {
bool bit(const std::vector<uint8_t>& bits, int32_t c) {
  return c >= 0 && size_t(c) / 8 < bits.size() && ((bits[size_t(c) / 8] >> (c % 8)) & 1) != 0;
}

// Whether sound `e` may leave its position out for this viewer, so his client plays it on its
// entity as it moves: the entity is in the frame just written, is not a brush model (a door's
// sound is placed at its middle, not its origin) and stands where the sound was made.
bool follows(const Q2Wire& w, const RulesCommon& c, const NetEvent& e) {
  if (e.ent <= 0 || size_t(e.ent) >= w.last.size()) return false;
  const Q2EntityState& s = w.last[size_t(e.ent)];
  if (!s.present || s.model[0] == 0) return false;
  if (size_t(s.model[0]) < c.models.size() && !c.models[size_t(s.model[0])].empty() &&
      c.models[size_t(s.model[0])][0] == '*')
    return false;
  const float at[3] = {e.origin.x, e.origin.y, e.origin.z};
  for (int k = 0; k < 3; k++)
    if (std::abs(static_cast<float>(s.origin[k]) / 8.0f - at[k]) > 1.0f) return false;
  return true;
}

}  // namespace

bool q2_sight_hears(const Q2Wire& w, const float ear[3], sh::Vec3f at) {
  const ge::brush::Map& m = *w.sight->map;
  if (m.leaves.empty()) return true;
  const float p[3] = {at.x, at.y, at.z};
  const int32_t from = m.leaves[ge::brush::leaf_at(m, p)].cluster;
  const int32_t to = m.leaves[ge::brush::leaf_at(m, ear)].cluster;
  if (!m.phs.empty() && !ge::brush::cluster_hears(m, from, to)) return false;
  if (w.sight->groups.empty() || m.areas.empty()) return true;
  const int32_t a = ge::brush::area_at(m, p), b = ge::brush::area_at(m, ear);
  return a != 0 && b != 0 && ge::brush::areas_connected(w.sight->groups, a, b);
}

bool q2_sight_keeps(const Q2Wire& w, int n, const Q2EntityState& s, const float eye[3]) {
  if (n == w.slot + 1) return true;
  if (size_t(n) >= w.sight->links.size()) return true;
  const Q2SightLinks& l = w.sight->links[size_t(n)];
  if (l.everywhere) return true;
  if ((s.renderfx & kBeam) != 0) {
    if (l.count == 0 || !bit(w.heard, l.clusters[0])) return false;
  } else {
    bool seen = false;
    for (int i = 0; i < l.count && !seen; i++) seen = bit(w.seen, l.clusters[i]);
    if (!seen) return false;
  }
  if (!w.sight->groups.empty() && !w.sight->map->areas.empty()) {  // a shut door between them
    const auto joined = [&](int32_t a) {
      return a != 0 && w.area != 0 && ge::brush::areas_connected(w.sight->groups, w.area, a);
    };
    if (!joined(l.area1) && (l.area2 == 0 || !joined(l.area2))) return false;
  }
  if (s.model[0] == 0) {
    float d2 = 0;
    for (int k = 0; k < 3; k++) {
      const float d = static_cast<float>(s.origin[k]) / 8.0f - eye[k];
      d2 += d * d;
    }
    if (d2 > 400.0f * 400.0f) return false;
  }
  return true;
}

namespace {

// The entities past the frame's cap (sv_maxents), by number: 1 kept out this frame.
std::vector<uint8_t> over_cap(const Q2Wire& w, const RulesCommon& c, const float eye[3], bool cut) {
  struct Want {
    int n, rank;
    float d2;
  };
  std::vector<Want> in;
  for (int n = 1; n < kQ2Entities; n++) {
    const Q2EntityState s = entity_state(c, n);
    if (!s.present || (cut && !q2_sight_keeps(w, n, s, eye))) continue;
    const std::string& model =
        s.model[0] > 0 && size_t(s.model[0]) < c.models.size() ? c.models[size_t(s.model[0])] : "";
    const bool alive =
        size_t(n) < c.world.entities.size() && c.world.entities[size_t(n)].health > 0;
    const int rank = n <= sh::kMaxClients || (!model.empty() && model[0] == '*') || alive ? 0
                     : model.empty() || model.find("/gibs/") != std::string::npos         ? 2
                                                                                          : 1;
    float d2 = 0;
    for (int k = 0; k < 3; k++) {
      const float d = static_cast<float>(s.origin[k]) / 8.0f - eye[k];
      d2 += d * d;
    }
    in.push_back({n, rank, d2});
  }
  std::vector<uint8_t> out;
  const auto most = static_cast<size_t>(w.opt.max_entities);
  if (in.size() <= most) return out;
  if (w.opt.prioritize)
    std::stable_sort(in.begin(), in.end(), [](const Want& a, const Want& b) {
      return a.rank != b.rank ? a.rank < b.rank : a.d2 < b.d2;
    });
  out.assign(kQ2Entities, 0);
  for (size_t i = most; i < in.size(); i++) out[size_t(in[i].n)] = 1;
  return out;
}

}  // namespace

void q2_sight_reset(Q2Sight& s, const ge::brush::Map* map) {
  s.map = map;
  s.links.assign(kQ2Entities, Q2SightLinks{});
}

void q2_sight_link(Q2Sight& s, int number, const float lo[3], const float hi[3]) {
  if (s.map == nullptr || number <= 0 || size_t(number) >= s.links.size()) return;
  Q2SightLinks& l = s.links[size_t(number)];
  l = Q2SightLinks{};
  uint32_t leaves[128];
  const uint32_t found = ge::brush::box_leaves(*s.map, lo, hi, leaves, 128);
  if (found > 128) {
    l.everywhere = true;
    return;
  }
  for (uint32_t i = 0; i < found; i++) {  // its areas: the first two its leaves lie in
    const int32_t a = leaves[i] < s.map->leaves.size() ? s.map->leaves[leaves[i]].area : 0;
    if (a == 0 || a == l.area1 || a == l.area2) continue;
    if (l.area1 == 0)
      l.area1 = a;
    else if (l.area2 == 0)
      l.area2 = a;
  }
  for (uint32_t i = 0; i < found; i++) {
    const int32_t c = leaves[i] < s.map->leaves.size() ? s.map->leaves[leaves[i]].cluster : -1;
    if (c < 0) continue;
    if (std::find(l.clusters, l.clusters + l.count, c) != l.clusters + l.count) continue;
    if (l.count == kSightClusters) {
      l.everywhere = true;
      return;
    }
    l.clusters[l.count++] = c;
  }
}

void q2_sight_link_rules(Q2Sight& s, const RulesCommon& c) {
  const RulesWorld& w = c.world;
  const int count = std::min(w.entity_count, kQ2Entities);
  for (int n = 1; n < count; n++) {
    const WorldEntity& e = w.entities[size_t(n)];
    if (e.free) continue;
    const bool linked =
        e.absmin.x != e.absmax.x || e.absmin.y != e.absmax.y || e.absmin.z != e.absmax.z;
    const float lo[3] = {linked ? e.absmin.x : e.origin.x + e.mins.x,
                         linked ? e.absmin.y : e.origin.y + e.mins.y,
                         linked ? e.absmin.z : e.origin.z + e.mins.z};
    const float hi[3] = {linked ? e.absmax.x : e.origin.x + e.maxs.x,
                         linked ? e.absmax.y : e.origin.y + e.maxs.y,
                         linked ? e.absmax.z : e.origin.z + e.maxs.z};
    q2_sight_link(s, n, lo, hi);
  }
}

void q2_sky(const ge::brush::Map& map, Q2WireOptions& opt) {
  // A world that names none gets the sky, axis and turn an unmodified Quake II server sends for it
  // (measured on the full game's q2dm1): an empty sky has a client ask for "env/rt.tga" and stall.
  opt.sky = "unit1_";
  opt.sky_axis = "0.000000 0.000000 0.000000";
  opt.sky_rotate = "0.000000";
  if (map.entities.empty()) return;
  const ge::brush::Entity& world = map.entities[0];
  if (!world.get_any_case("sky").empty()) opt.sky = world.get_any_case("sky");
  if (!world.get_any_case("skyaxis").empty()) opt.sky_axis = world.get_any_case("skyaxis");
  if (!world.get_any_case("skyrotate").empty()) opt.sky_rotate = world.get_any_case("skyrotate");
}

int32_t q2_map_checksum(std::span<const uint8_t> bsp) {
  return static_cast<int32_t>(md4_word(bsp));
}

void q2_signon_parts(Q2Wire& w, const RulesCommon& c, int slot, const Q2WireOptions& opt,
                     Q2Signon& out) {
  w = Q2Wire{};
  w.opt = opt;
  w.slot = slot;
  w.baselines.assign(kQ2Entities, Q2EntityState{});
  w.last.assign(kQ2Entities, Q2EntityState{});
  w.events_done = c.out.events_total;
  config_strings(c, opt, w.sent);
  out = Q2Signon{};
  {
    Out o{out.server_data};
    o.u8(kServerData);
    o.i32(opt.protocol);  // the viewer's: 34, or a later one with its tail below
    o.i32(0x10000 + opt.server_count);
    o.u8(opt.attract_loop);
    o.str("");
    o.i16(opt.show.empty() ? slot : -1);
    o.str(opt.show.empty() ? w.sent[kCsName] : opt.show);
  }
  q2_server_data_tail(out.server_data, opt);
  if (!opt.show.empty()) return;  // a cinematic or a picture: nothing more
  for (int i = 0; i < kQ2ConfigStrings; i++) {
    const std::string& s = w.sent[size_t(i)];
    if (s.empty()) continue;
    std::vector<uint8_t> one;
    Out o{one};
    o.u8(kConfigString);
    o.i16(i);
    o.str(s);
    out.config_strings.emplace_back(i, std::move(one));
  }
  const Q2EntityState zero;
  for (int n = 1; n < kQ2Entities; n++) {
    const Q2EntityState s = entity_state(c, n);
    if (!s.present) continue;
    w.baselines[size_t(n)] = s;
    std::vector<uint8_t> one;
    Out e{one};
    e.u8(kBaseline);
    write_entity(e, delta_flags(zero, s, n, true), n, s, long_solid(opt));
    out.baselines.emplace_back(n, std::move(one));
  }
}

void q2_signon(Q2Wire& w, const RulesCommon& c, int slot, const Q2WireOptions& opt,
               std::vector<std::vector<uint8_t>>& blocks) {
  Q2Signon parts;
  q2_signon_parts(w, c, slot, opt, parts);
  blocks.assign(1, parts.server_data);
  for (const auto& [i, one] : parts.config_strings) {
    // The message less its id, index and zero: the string's length.
    if (blocks.back().size() + (one.size() - 4) + 32 > size_t(kQ2MaxBlock)) blocks.emplace_back();
    blocks.back().insert(blocks.back().end(), one.begin(), one.end());
  }
  for (const auto& [n, one] : parts.baselines) {
    if (blocks.back().size() + one.size() + 64 > size_t(kQ2MaxBlock)) blocks.emplace_back();
    blocks.back().insert(blocks.back().end(), one.begin(), one.end());
  }
  for (const std::string& line : opt.stuffed) {
    if (blocks.back().size() + line.size() + 16 > size_t(kQ2MaxBlock)) blocks.emplace_back();
    Out l{blocks.back()};
    l.u8(kStuffText);
    l.str(line);
  }
  if (blocks.back().size() + 16 > size_t(kQ2MaxBlock)) blocks.emplace_back();
  Out o{blocks.back()};
  o.u8(kStuffText);
  o.str("precache\n");
}

void q2_resync(Q2Wire& w) {
  w.last_frame = -1;
  w.last.assign(kQ2Entities, Q2EntityState{});
}

void q2_frame(Q2Wire& w, const RulesCommon& c, int32_t frame_number,
              const ge::classic::Q2State* move, std::vector<uint8_t>& block,
              std::vector<uint8_t>* reliable) {
  block.clear();
  Out r{reliable != nullptr ? *reliable : block};
  // The reliable part: what changed among the configstrings, then the prints and the lines.
  std::vector<std::string> now;
  config_strings(c, w.opt, now);
  for (int i = 0; i < kQ2ConfigStrings; i++)
    if (now[size_t(i)] != w.sent[size_t(i)]) {
      r.u8(kConfigString);
      r.i16(i);
      r.str(now[size_t(i)]);
      w.sent[size_t(i)] = now[size_t(i)];
    }
  // The viewer's layout program (a scoreboard, the help computer) when it changed: his client
  // keeps the last one.
  if (w.slot >= 0 && w.slot < sh::kMaxClients && c.world.clients[w.slot].layout != w.layout) {
    w.layout = c.world.clients[w.slot].layout;
    r.u8(kLayout);
    r.str(w.layout.substr(0, kLayoutMost));
  }
  // His inventory's counts when he opened it anew.
  if (w.slot >= 0 && w.slot < sh::kMaxClients && c.world.clients[w.slot].inventory != w.inventory) {
    w.inventory = c.world.clients[w.slot].inventory;
    r.u8(kInventory);
    for (int n = 0; n < kSqInventorySlots; n++)
      r.i16(size_t(n) < w.inventory.size() ? w.inventory[size_t(n)] : 0);
  }
  const std::vector<NetEvent>& events = c.out.events;
  // The viewer's message level: prints below it are not sent (the recorder, no slot, takes all).
  int msg = 0;
  if (w.slot >= 0 && w.slot < sh::kMaxClients)
    for (const auto& [k, v] : c.world.clients[w.slot].userinfo)
      if (k == "msg") msg = ge::classic::protocol::msg_level_of(v);
  const int64_t first = c.out.events_total - static_cast<int64_t>(events.size());
  const size_t from = static_cast<size_t>(std::max<int64_t>(0, w.events_done - first));
  for (size_t i = from; i < events.size(); i++) {
    const NetEvent& e = events[i];
    if (!to_slot(e, w.slot)) continue;
    if (e.kind == EventKind::print) {
      if (!ge::classic::protocol::print_reaches(std::clamp(e.level, 0, 3), msg, false)) continue;
      r.u8(kPrint);
      r.u8(static_cast<uint32_t>(std::clamp(e.level, 0, 3)));
      r.str(e.text);
    } else if (e.kind == EventKind::centerprint) {
      r.u8(kCenterPrint);
      r.str(e.text);
    } else if (e.kind == EventKind::stuffcmd) {
      r.u8(kStuffText);
      r.str(e.text);
    }
  }
  Out o{block};
  // The frame, numbered from 1: a stock client draws nothing from a frame numbered 0 (measured: it
  // stops on a field of view of 0 before the first frame it keeps).
  const int32_t number = frame_number + 1;
  const Q2PlayerState p = player_state(c, w.slot, move);
  float eye[3];
  sight_eye(p, eye);
  const bool cut = q2_sight_view(w, eye);
  const bool ext = w.opt.protocol != kQ2Protocol;
  if (ext) {
    q2_write_frame_top_ext(block, w, number, p);
  } else {
    o.u8(kFrame);
    o.i32(number);
    o.i32(w.last_frame);
    o.u8(w.suppress);
    w.suppress = 0;
    q2_write_area_bits(block, w);
    write_player(o, w.last_frame < 0 ? Q2PlayerState{} : w.last_player, p);
    w.last_player = p;
  }
  const std::vector<uint8_t> over =
      w.opt.max_entities > 0 ? over_cap(w, c, eye, cut) : std::vector<uint8_t>{};
  if (!ext) o.u8(kPacketEntities);
  const Q2EntityState zero;
  // An entry is at most 44 bytes; past the room left (the list's end and the sounds' share kept)
  // an entity waits for the next frame, as the one it holds is what the viewer holds.
  const size_t cap = frame_cap(w, reliable != nullptr ? reliable->size() : 0);
  const size_t room = entity_room(cap);
  for (int n = 1; n < kQ2Entities; n++) {
    Q2EntityState s = entity_state(c, n);
    const Q2EntityState& was = w.last[size_t(n)];
    if (block.size() + 44 > room) break;
    if (cut && s.present && !q2_sight_keeps(w, n, s, eye)) {
      s.present = false;
      w.cut++;
    }
    if (!over.empty() && over[size_t(n)] != 0 && s.present) {
      s.present = false;
      w.capped++;
    }
    if (!s.present) {
      if (was.present) write_entity(o, kRemove | (n >= 256 ? kNumberShort : 0u), n, s);
      w.last[size_t(n)] = Q2EntityState{};
      continue;
    }
    const bool fresh = !was.present || w.last_frame < 0;
    const Q2EntityState& base =
        fresh ? (w.baselines[size_t(n)].present ? w.baselines[size_t(n)] : zero) : was;
    std::memcpy(s.old_origin, base.origin, sizeof s.old_origin);
    if (ext) {
      q2_entity_for_ext(w, n, base, s);
      if (!s.present) {
        if (was.present) write_entity(o, kRemove | (n >= 256 ? kNumberShort : 0u), n, s);
        w.last[size_t(n)] = Q2EntityState{};
        continue;
      }
    }
    const bool player = n <= sh::kMaxClients;
    const uint32_t f = delta_flags(base, s, n, fresh || player);
    if ((f & ~kNumberShort) != 0) write_entity(o, f, n, s, long_solid(w.opt));
    w.last[size_t(n)] = s;
  }
  o.u8(0);
  o.u8(0);
  w.last_frame = number;
  // The unreliable part: the frame's temporary entities (the rules' named effects the protocol
  // has: blasts, sparks, blood, the rail's line, splashes, cables, teleports), then its sounds,
  // each to a viewer in its point's hearing set with the areas joined when the frame is cut.
  const float ear[3] = {static_cast<float>(p.origin[0]) / 8.0f,
                        static_cast<float>(p.origin[1]) / 8.0f,
                        static_cast<float>(p.origin[2]) / 8.0f};
  for (size_t i = from; i < events.size(); i++) {
    const NetEvent& e = events[i];
    if ((e.kind != EventKind::point_effect && e.kind != EventKind::trail_effect) ||
        !to_slot(e, w.slot))
      continue;
    if (cut && !q2_sight_hears(w, ear, e.origin)) continue;
    if (block.size() + 18 + 13 > cap) break;                          // a sound's room kept too
    if (e.text == "te_damage_dealt" && !w.opt.hit_reports) continue;  // a stock client has none
    if (!write_flash(o, e)) write_temp_entity(o, e);
  }
  for (size_t i = from; i < events.size(); i++) {
    const NetEvent& e = events[i];
    if (e.kind != EventKind::sound || !to_slot(e, w.slot)) continue;
    if (block.size() + 13 > cap) break;  // a sound is at most 13 bytes
    const auto at = std::find(c.sounds.begin(), c.sounds.end(), e.text);
    if (at == c.sounds.end() || at == c.sounds.begin() || at - c.sounds.begin() > 255) continue;
    const float volume = e.values[0] > 0 ? e.values[0] / 255.0f : 1.0f;  // sent as 0..255
    const float att = e.values[1];
    if (cut && att != 0 && !q2_sight_hears(w, ear, e.origin)) continue;  // 0: heard everywhere
    uint32_t flags = 8;                                                  // the entity and channel
    if (!follows(w, c, e)) flags |= 4;  // the position, unless it is the entity's own
    if (volume != 1.0f) flags |= 1;
    if (att != 1.0f) flags |= 2;
    o.u8(kSound);
    o.u8(flags);
    o.u8(static_cast<uint32_t>(at - c.sounds.begin()));
    if (flags & 1) o.u8(static_cast<uint32_t>(std::clamp(volume * 255.0f, 0.0f, 255.0f)));
    if (flags & 2) o.u8(static_cast<uint32_t>(std::clamp(att * 64.0f, 0.0f, 255.0f)));
    o.i16(std::clamp(e.ent, 0, kQ2Entities - 1) * 8 + (e.level & 7));
    if ((flags & 4) == 0) continue;
    o.i16(coord(e.origin.x));
    o.i16(coord(e.origin.y));
    o.i16(coord(e.origin.z));
  }
  w.events_done = c.out.events_total;
}

void q2_signon_raw(Q2Wire& w, const Q2Raw& raw, int slot, const Q2WireOptions& opt, Q2Signon& out) {
  w = Q2Wire{};
  w.opt = opt;
  w.slot = slot;
  w.baselines.assign(kQ2Entities, Q2EntityState{});
  w.last.assign(kQ2Entities, Q2EntityState{});
  w.sent = raw.config;
  w.sent.resize(kQ2ConfigStrings);
  out = Q2Signon{};
  {
    Out o{out.server_data};
    o.u8(kServerData);
    o.i32(opt.protocol);  // the viewer's: 34, or a later one with its tail below
    o.i32(0x10000 + opt.server_count);
    o.u8(opt.attract_loop);
    o.str("");
    o.i16(opt.show.empty() ? slot : -1);
    o.str(opt.show.empty() ? w.sent[kCsName] : opt.show);
  }
  q2_server_data_tail(out.server_data, opt);
  if (!opt.show.empty()) return;  // a cinematic or a picture: nothing more
  for (int i = 0; i < kQ2ConfigStrings; i++) {
    const std::string& s = w.sent[size_t(i)];
    if (s.empty()) continue;
    std::vector<uint8_t> one;
    Out o{one};
    o.u8(kConfigString);
    o.i16(i);
    o.str(s);
    out.config_strings.emplace_back(i, std::move(one));
  }
  const Q2EntityState zero;
  for (int n = 1; n < kQ2Entities && size_t(n) < raw.baselines.size(); n++) {
    const Q2EntityState& s = raw.baselines[size_t(n)];
    if (!s.present) continue;
    w.baselines[size_t(n)] = s;
    std::vector<uint8_t> one;
    Out e{one};
    e.u8(kBaseline);
    write_entity(e, delta_flags(zero, s, n, true), n, s, long_solid(opt));
    out.baselines.emplace_back(n, std::move(one));
  }
}

void q2_frame_raw(Q2Wire& w, Q2Raw& raw, int32_t frame_number, std::vector<uint8_t>& block,
                  std::vector<uint8_t>& reliable) {
  block.clear();
  Out r{reliable};
  for (int i = 0; i < kQ2ConfigStrings && size_t(i) < raw.config.size(); i++)
    if (raw.config[size_t(i)] != w.sent[size_t(i)]) {
      r.u8(kConfigString);
      r.i16(i);
      r.str(raw.config[size_t(i)]);
      w.sent[size_t(i)] = raw.config[size_t(i)];
    }
  const int slot = std::clamp(w.slot, 0, sh::kMaxClients - 1);
  reliable.insert(reliable.end(), raw.reliable[slot].begin(), raw.reliable[slot].end());
  raw.reliable[slot].clear();
  Out o{block};
  const int32_t number = frame_number + 1;  // as q2_frame numbers them
  const Q2PlayerState& p = raw.players[slot];
  float eye[3];
  sight_eye(p, eye);
  const bool cut = q2_sight_view(w, eye);
  const bool ext = w.opt.protocol != kQ2Protocol;
  if (ext) {
    q2_write_frame_top_ext(block, w, number, p);
  } else {
    o.u8(kFrame);
    o.i32(number);
    o.i32(w.last_frame);
    o.u8(w.suppress);
    w.suppress = 0;
    q2_write_area_bits(block, w);
    write_player(o, w.last_frame < 0 ? Q2PlayerState{} : w.last_player, p);
    w.last_player = p;
  }
  if (!ext) o.u8(kPacketEntities);
  const Q2EntityState zero;
  const size_t cap = frame_cap(w, reliable.size());
  const size_t room = entity_room(cap);
  for (int n = 1; n < kQ2Entities; n++) {
    Q2EntityState s = size_t(n) < raw.entities.size() ? raw.entities[size_t(n)] : zero;
    const Q2EntityState& was = w.last[size_t(n)];
    if (block.size() + 44 > room) break;
    if (cut && s.present && !q2_sight_keeps(w, n, s, eye)) {
      s.present = false;
      w.cut++;
    }
    if (s.present && raw.touch != nullptr && !raw.touch(raw.touch_ctx, slot, n, s))
      s.present = false;
    if (!s.present) {
      if (was.present) write_entity(o, kRemove | (n >= 256 ? kNumberShort : 0u), n, s);
      w.last[size_t(n)] = Q2EntityState{};
      continue;
    }
    // A player's own shots are not solid to him, so his client does not predict against them.
    if (size_t(n) < raw.owners.size() && raw.owners[size_t(n)] == slot + 1) s.solid = 0;
    const bool fresh = !was.present || w.last_frame < 0;
    const Q2EntityState& base =
        fresh ? (w.baselines[size_t(n)].present ? w.baselines[size_t(n)] : zero) : was;
    if (ext) {
      q2_entity_for_ext(w, n, base, s);
      if (!s.present) {
        if (was.present) write_entity(o, kRemove | (n >= 256 ? kNumberShort : 0u), n, s);
        w.last[size_t(n)] = Q2EntityState{};
        continue;
      }
    }
    const uint32_t f = delta_flags(base, s, n, fresh || n <= sh::kMaxClients);
    if ((f & ~kNumberShort) != 0) write_entity(o, f, n, s, long_solid(w.opt));
    w.last[size_t(n)] = s;
  }
  o.u8(0);
  o.u8(0);
  w.last_frame = number;
  std::vector<uint8_t>& d = raw.datagram[slot];
  if (block.size() + d.size() <= cap) block.insert(block.end(), d.begin(), d.end());
  d.clear();
}

void q2_record_start(Q2Recorder& r, const RulesCommon& c, int slot, const Q2WireOptions& opt) {
  r = Q2Recorder{};
  std::vector<std::vector<uint8_t>> blocks;
  q2_signon(r.wire, c, slot, opt, blocks);
  for (const std::vector<uint8_t>& b : blocks) {
    Out o{r.bytes};
    o.i32(static_cast<int64_t>(b.size()));
    r.bytes.insert(r.bytes.end(), b.begin(), b.end());
  }
  r.on = true;
}

void q2_record_frame(Q2Recorder& r, const RulesCommon& c, int32_t frame_number,
                     const ge::classic::Q2State* move) {
  if (!r.on || frame_number == r.at) return;
  std::vector<uint8_t> block;
  q2_frame(r.wire, c, frame_number, move, block);
  Out o{r.bytes};
  o.i32(static_cast<int64_t>(block.size()));
  r.bytes.insert(r.bytes.end(), block.begin(), block.end());
  r.at = frame_number;
  r.frames++;
}

void q2_record_stop(Q2Recorder& r) {
  if (!r.on) return;
  Out o{r.bytes};
  o.i32(-1);
  r.on = false;
}

void q2_config_strings(const RulesCommon& c, const Q2WireOptions& opt,
                       std::vector<std::string>& out) {
  config_strings(c, opt, out);
}

Q2EntityState q2_entity_state(const RulesCommon& c, int number) { return entity_state(c, number); }

Q2PlayerState q2_player_state(const RulesCommon& c, int slot, const ge::classic::Q2State* move) {
  return player_state(c, slot, move);
}

uint32_t q2_entity_flags(const Q2EntityState& from, const Q2EntityState& to, int number, bool old) {
  return delta_flags(from, to, number, old);
}

void q2_write_entity(std::vector<uint8_t>& out, uint32_t flags, int number,
                     const Q2EntityState& to) {
  Out o{out};
  write_entity(o, flags, number, to);
}

void q2_write_player(std::vector<uint8_t>& out, const Q2PlayerState& from,
                     const Q2PlayerState& to) {
  Out o{out};
  write_player(o, from, to);
}

bool q2_write_effect(std::vector<uint8_t>& out, const NetEvent& e) {
  if (e.kind != EventKind::point_effect && e.kind != EventKind::trail_effect) return false;
  Out o{out};
  const size_t before = out.size();
  if (!write_flash(o, e)) write_temp_entity(o, e);
  return out.size() > before;
}

}  // namespace shooters_edge::server
