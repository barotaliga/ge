// The sequel's protocol 34 server messages from a runner's rows (sequel_wire.hpp).
//
// ge:summary The sequel's wire, server side: the signon, frames, deltas and the recorder
// ge:keywords sequel wire signon configstring baseline frame playerinfo packetentities delta
// ge:keywords remove old-origin solid stats sound print recorder dm2
// ge:skill shooter replay
// ge:test tests/shooters_edge_sequel_wire_test.cpp
#include "shooters_edge/server/sequel_wire.hpp"

#include <algorithm>
#include <cstring>

#include "shooters_edge/server/md4.hpp"
#include "shooters_edge/server/sequel_screens.hpp"

namespace shooters_edge::server {

namespace {

namespace sh = ge::shooter;

// Server message ids.
constexpr uint8_t kPrint = 10, kStuffText = 11, kServerData = 12, kConfigString = 13,
                  kBaseline = 14, kCenterPrint = 15, kPlayerInfo = 17, kPacketEntities = 18,
                  kFrame = 20, kSound = 9, kLayout = 4;

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

// The wire's model number for the runner's: the map itself is 1, so every other is one up.
int wire_model(int ours) { return ours <= 0 ? 0 : ours + 1; }

// The status bar of rules that send none, in the layout language a client draws (y from the bottom,
// x from the centre, the big numbers of health, ammunition and armour by their stats 1, 3 and 5).
// A longer one (the sequel's own, sequel_bar.hpp) runs on into the slots after 5, as a client
// reads it.
constexpr const char* kStatusBar = "yb -24 xv 16 hnum xv 116 anum xv 216 rnum";

// The configstrings the runner's state gives.
void config_strings(const RulesCommon& c, const SequelWireOptions& opt,
                    std::vector<std::string>& cs) {
  cs.assign(kSequelConfigStrings, std::string());
  cs[kCsName] = c.world.map_title.empty() ? c.map_name : c.world.map_title;
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
SequelEntityState entity_state(const RulesCommon& c, int n) {
  SequelEntityState s;
  const RulesWorld& w = c.world;
  if (n <= 0 || n >= w.entity_count || n >= kSequelEntities) return s;
  const WorldEntity& e = w.entities[size_t(n)];
  if (e.free || e.modelindex <= 0) return s;
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
  s.model[0] = wire_model(e.modelindex);
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
  const std::string& model =
      size_t(e.modelindex) < c.models.size() ? c.models[size_t(e.modelindex)] : std::string();
  const bool brush = !model.empty() && model[0] == '*';
  if (player || brush) s.solid = packed_solid(e, brush);
  return s;
}

// The flags a delta from `from` to `to` sets (`old` forces the old origin).
uint32_t delta_flags(const SequelEntityState& from, const SequelEntityState& to, int number,
                     bool old) {
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
void write_entity(Out& o, uint32_t f, int number, const SequelEntityState& to) {
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
    for (const int16_t v : to.old_origin) o.i16(v);
  if (f & kSoundBit) o.u8(static_cast<uint32_t>(to.sound));
  if (f & kEvent) o.u8(static_cast<uint32_t>(to.event));
  if (f & kSolid) o.i16(to.solid);
}

SequelPlayerState player_state(const RulesCommon& c, int slot, const sh::SequelState* move) {
  SequelPlayerState p;
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
    }
  const float va[3] = {e.view_angles.x, e.view_angles.y, e.view_angles.z};
  for (int k = 0; k < 3; k++) p.view_angles[k] = angle16(va[k]);
  p.gun_index = static_cast<uint8_t>(wire_model(wc.numbers.weapon_model));
  p.gun_frame = static_cast<uint8_t>(static_cast<int>(wc.numbers.weapon_frame));
  p.fov = 90;
  if (!c.world.status_bar.empty()) {  // the rules' own stats for their own bar
    for (int k = 0; k < 32; k++) p.stats[k] = wc.numbers.stats[k];
  } else {
    p.stats[1] = static_cast<int16_t>(static_cast<int>(wc.numbers.health));
    p.stats[3] = static_cast<int16_t>(static_cast<int>(wc.numbers.current_ammo));
    p.stats[5] = static_cast<int16_t>(static_cast<int>(wc.numbers.armour));
  }
  return p;
}

void write_player(Out& o, const SequelPlayerState& a, const SequelPlayerState& b) {
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
    for (const int16_t v : b.origin) o.i16(v);
  if (f & kPmVelocity)
    for (const int16_t v : b.velocity) o.i16(v);
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

bool to_slot(const NetEvent& e, int slot) { return (e.to >> slot) & 1u; }

}  // namespace

void sequel_sky(const ge::brush::Map& map, SequelWireOptions& opt) {
  // A world that names none gets the sky, axis and turn an unmodified sequel server sends for it
  // (measured on the full game's q2dm1): an empty sky has a client ask for "env/rt.tga" and stall.
  opt.sky = "unit1_";
  opt.sky_axis = "0.000000 0.000000 0.000000";
  opt.sky_rotate = "0.000000";
  if (map.entities.empty()) return;
  const ge::brush::Entity& world = map.entities[0];
  if (!world.get("sky").empty()) opt.sky = world.get("sky");
  if (!world.get("skyaxis").empty()) opt.sky_axis = world.get("skyaxis");
  if (!world.get("skyrotate").empty()) opt.sky_rotate = world.get("skyrotate");
}

int32_t sequel_map_checksum(std::span<const uint8_t> bsp) {
  return static_cast<int32_t>(md4_word(bsp));
}

void sequel_signon_parts(SequelWire& w, const RulesCommon& c, int slot,
                         const SequelWireOptions& opt, SequelSignon& out) {
  w = SequelWire{};
  w.opt = opt;
  w.slot = slot;
  w.baselines.assign(kSequelEntities, SequelEntityState{});
  w.last.assign(kSequelEntities, SequelEntityState{});
  w.events_done = c.out.events_total;
  config_strings(c, opt, w.sent);
  out = SequelSignon{};
  {
    Out o{out.server_data};
    o.u8(kServerData);
    o.i32(kSequelProtocol);
    o.i32(0x10000 + opt.server_count);
    o.u8(opt.attract_loop);
    o.str("");
    o.i16(slot);
    o.str(w.sent[kCsName]);
  }
  for (int i = 0; i < kSequelConfigStrings; i++) {
    const std::string& s = w.sent[size_t(i)];
    if (s.empty()) continue;
    std::vector<uint8_t> one;
    Out o{one};
    o.u8(kConfigString);
    o.i16(i);
    o.str(s);
    out.config_strings.emplace_back(i, std::move(one));
  }
  const SequelEntityState zero;
  for (int n = 1; n < kSequelEntities; n++) {
    const SequelEntityState s = entity_state(c, n);
    if (!s.present) continue;
    w.baselines[size_t(n)] = s;
    std::vector<uint8_t> one;
    Out e{one};
    e.u8(kBaseline);
    write_entity(e, delta_flags(zero, s, n, true), n, s);
    out.baselines.emplace_back(n, std::move(one));
  }
}

void sequel_signon(SequelWire& w, const RulesCommon& c, int slot, const SequelWireOptions& opt,
                   std::vector<std::vector<uint8_t>>& blocks) {
  SequelSignon parts;
  sequel_signon_parts(w, c, slot, opt, parts);
  blocks.assign(1, parts.server_data);
  for (const auto& [i, one] : parts.config_strings) {
    // The message less its id, index and zero: the string's length.
    if (blocks.back().size() + (one.size() - 4) + 32 > size_t(kSequelMaxBlock))
      blocks.emplace_back();
    blocks.back().insert(blocks.back().end(), one.begin(), one.end());
  }
  for (const auto& [n, one] : parts.baselines) {
    if (blocks.back().size() + one.size() + 64 > size_t(kSequelMaxBlock)) blocks.emplace_back();
    blocks.back().insert(blocks.back().end(), one.begin(), one.end());
  }
  for (const std::string& line : opt.stuffed) {
    if (blocks.back().size() + line.size() + 16 > size_t(kSequelMaxBlock)) blocks.emplace_back();
    Out l{blocks.back()};
    l.u8(kStuffText);
    l.str(line);
  }
  if (blocks.back().size() + 16 > size_t(kSequelMaxBlock)) blocks.emplace_back();
  Out o{blocks.back()};
  o.u8(kStuffText);
  o.str("precache\n");
}

void sequel_resync(SequelWire& w) {
  w.last_frame = -1;
  w.last.assign(kSequelEntities, SequelEntityState{});
}

void sequel_frame(SequelWire& w, const RulesCommon& c, int32_t frame_number,
                  const sh::SequelState* move, std::vector<uint8_t>& block,
                  std::vector<uint8_t>* reliable) {
  block.clear();
  Out r{reliable != nullptr ? *reliable : block};
  // The reliable part: what changed among the configstrings, then the prints and the lines.
  std::vector<std::string> now;
  config_strings(c, w.opt, now);
  for (int i = 0; i < kSequelConfigStrings; i++)
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
  const std::vector<NetEvent>& events = c.out.events;
  const int64_t first = c.out.events_total - static_cast<int64_t>(events.size());
  const size_t from = static_cast<size_t>(std::max<int64_t>(0, w.events_done - first));
  for (size_t i = from; i < events.size(); i++) {
    const NetEvent& e = events[i];
    if (!to_slot(e, w.slot)) continue;
    if (e.kind == EventKind::print) {
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
  o.u8(kFrame);
  o.i32(number);
  o.i32(w.last_frame);
  o.u8(0);
  o.u8(32);
  for (int k = 0; k < 32; k++) o.u8(0xFF);
  const SequelPlayerState p = player_state(c, w.slot, move);
  write_player(o, w.last_frame < 0 ? SequelPlayerState{} : w.last_player, p);
  w.last_player = p;
  o.u8(kPacketEntities);
  const SequelEntityState zero;
  // An entry is at most 44 bytes; past the room left (the list's end and the sounds' share kept)
  // an entity waits for the next frame, as the one it holds is what the viewer holds.
  const size_t room = size_t(kSequelMaxBlock) - 2 - 200;
  for (int n = 1; n < kSequelEntities; n++) {
    SequelEntityState s = entity_state(c, n);
    const SequelEntityState& was = w.last[size_t(n)];
    if (block.size() + 44 > room) break;
    if (!s.present) {
      if (was.present) write_entity(o, kRemove | (n >= 256 ? kNumberShort : 0u), n, s);
      w.last[size_t(n)] = SequelEntityState{};
      continue;
    }
    const bool fresh = !was.present || w.last_frame < 0;
    const SequelEntityState& base =
        fresh ? (w.baselines[size_t(n)].present ? w.baselines[size_t(n)] : zero) : was;
    std::memcpy(s.old_origin, base.origin, sizeof s.old_origin);
    const bool player = n <= sh::kMaxClients;
    const uint32_t f = delta_flags(base, s, n, fresh || player);
    if ((f & ~kNumberShort) != 0) write_entity(o, f, n, s);
    w.last[size_t(n)] = s;
  }
  o.u8(0);
  o.u8(0);
  w.last_frame = number;
  // The unreliable part: the frame's sounds.
  for (size_t i = from; i < events.size(); i++) {
    const NetEvent& e = events[i];
    if (e.kind != EventKind::sound || !to_slot(e, w.slot)) continue;
    if (block.size() + 13 > size_t(kSequelMaxBlock)) break;  // a sound is at most 13 bytes
    const auto at = std::find(c.sounds.begin(), c.sounds.end(), e.text);
    if (at == c.sounds.end() || at == c.sounds.begin() || at - c.sounds.begin() > 255) continue;
    const float volume = e.values[0] > 0 ? e.values[0] : 1.0f;
    const float att = e.values[1];
    uint32_t flags = 8 | 4;  // the entity and channel, the position
    if (volume != 1.0f) flags |= 1;
    if (att != 1.0f) flags |= 2;
    o.u8(kSound);
    o.u8(flags);
    o.u8(static_cast<uint32_t>(at - c.sounds.begin()));
    if (flags & 1) o.u8(static_cast<uint32_t>(std::clamp(volume * 255.0f, 0.0f, 255.0f)));
    if (flags & 2) o.u8(static_cast<uint32_t>(std::clamp(att * 64.0f, 0.0f, 255.0f)));
    o.i16(std::clamp(e.ent, 0, kSequelEntities - 1) * 8 + (e.level & 7));
    o.i16(coord(e.origin.x));
    o.i16(coord(e.origin.y));
    o.i16(coord(e.origin.z));
  }
  w.events_done = c.out.events_total;
}

void sequel_record_start(SequelRecorder& r, const RulesCommon& c, int slot,
                         const SequelWireOptions& opt) {
  r = SequelRecorder{};
  std::vector<std::vector<uint8_t>> blocks;
  sequel_signon(r.wire, c, slot, opt, blocks);
  for (const std::vector<uint8_t>& b : blocks) {
    Out o{r.bytes};
    o.i32(static_cast<int64_t>(b.size()));
    r.bytes.insert(r.bytes.end(), b.begin(), b.end());
  }
  r.on = true;
}

void sequel_record_frame(SequelRecorder& r, const RulesCommon& c, int32_t frame_number,
                         const sh::SequelState* move) {
  if (!r.on || frame_number == r.at) return;
  std::vector<uint8_t> block;
  sequel_frame(r.wire, c, frame_number, move, block);
  Out o{r.bytes};
  o.i32(static_cast<int64_t>(block.size()));
  r.bytes.insert(r.bytes.end(), block.begin(), block.end());
  r.at = frame_number;
  r.frames++;
}

void sequel_record_stop(SequelRecorder& r) {
  if (!r.on) return;
  Out o{r.bytes};
  o.i32(-1);
  r.on = false;
}

}  // namespace shooters_edge::server
