// The single-player demo written from a multiplayer match (demo_convert_single_out.hpp).
//
// ge:summary The single-player demo written from a multiplayer match: blocks, signon, rewrites
// ge:keywords demo-convert single-out dem protocol-15 fast-update clientdata
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#include "shooters_edge/client/demo_convert_single_out.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace shooters_edge::client {

namespace {

constexpr size_t kMostBlock = 8000;  // a block's messages, bytes
constexpr int kSlots = 16;           // the player slots the protocol takes
constexpr int kOverflow = 496;       // players past the 16: entities from here
constexpr size_t kMostDrawn = 240;   // entities a frame (the client draws 256)
constexpr double kMostGap = 0.1;     // frames further apart play too fast
constexpr size_t kMostNames = 255;   // models, sounds
constexpr int kStatWeaponFrame = 5;  // the held weapon's frame (demo.hpp names the rest)

// The update's flags (the first byte's low 7 bits, the second byte's 8).
constexpr int kMoreBits = 1 << 0, kOrigin1 = 1 << 1, kOrigin2 = 1 << 2, kOrigin3 = 1 << 3;
constexpr int kAngle2 = 1 << 4, kFrame = 1 << 6, kSignal = 1 << 7, kAngle1 = 1 << 8;
constexpr int kAngle3 = 1 << 9, kModel = 1 << 10, kColormap = 1 << 11, kSkin = 1 << 12;
constexpr int kEffects = 1 << 13, kLongEntity = 1 << 14;

// clientdata's flags.
constexpr int kCdVelocity1 = 1 << 5, kCdItems = 1 << 9, kCdOnGround = 1 << 10;
constexpr int kCdWeaponFrame = 1 << 12, kCdArmour = 1 << 13, kCdWeapon = 1 << 14;

std::string str_at(std::span<const uint8_t> b, size_t& at) {
  std::string s;
  while (at < b.size() && b[at] != 0) s.push_back(static_cast<char>(b[at++]));
  at++;
  return s;
}

// An info string's value for `key` ("" when absent).
std::string info_value(const std::string& info, const std::string& key) {
  size_t at = 0;
  while (at < info.size()) {
    if (info[at] == '\\') at++;
    const size_t k = info.find('\\', at);
    if (k == std::string::npos) return "";
    const size_t v = info.find('\\', k + 1);
    if (info.compare(at, k - at, key) == 0)
      return info.substr(k + 1, v == std::string::npos ? std::string::npos : v - k - 1);
    if (v == std::string::npos) return "";
    at = v;
  }
  return "";
}

std::string set_info_value(const std::string& info, const std::string& key,
                           const std::string& value) {
  std::string out;
  size_t at = 0;
  while (at < info.size()) {
    if (info[at] == '\\') at++;
    const size_t k = info.find('\\', at);
    if (k == std::string::npos) break;
    const size_t v = info.find('\\', k + 1);
    const std::string kk = info.substr(at, k - at);
    const std::string vv =
        info.substr(k + 1, v == std::string::npos ? std::string::npos : v - k - 1);
    if (kk != key) {
      out += '\\';
      out += kk;
      out += '\\';
      out += vv;
    }
    if (v == std::string::npos) break;
    at = v;
  }
  return out + "\\" + key + "\\" + value;
}

int angle8(float deg) { return static_cast<int>(std::lround(deg * 256.0f / 360.0f)) & 255; }

}  // namespace

void SingleRecording::block(std::vector<uint8_t>& out, const std::array<float, 3>& view,
                            const ConvOut& body) {
  ConvOut o;
  const size_t len = body.empty() ? 1 : body.size();
  o.i32(static_cast<int32_t>(len));
  for (float a : view) o.f32(a);
  if (body.empty())
    o.u8(1);  // a nop: an empty block stops a playing client
  else
    o.bytes(body.b);
  out.insert(out.end(), o.b.begin(), o.b.end());
}

void SingleRecording::level(std::vector<uint8_t>&, double, const ClientLevel& l, ConvertNotes&) {
  level_ = l;
  pending_ = true;
  written_ = false;
  view_slot_ = std::clamp(l.player, 0, 31);
  slots_.fill(0);
  slots_[static_cast<size_t>(view_slot_)] = 1;  // the view's player is slot 0
  slots_used_ = 1;
  infos_ = {};
  deathmatch_ = true;
  models_.clear();
  sounds_.clear();
  cd_track_ = 0;
  signon_.b.clear();
  signon_ends_.clear();
  baselines_.clear();
  stats_any_ = false;
  intermission_ = false;
  timed_ = false;
  waiting_.clear();
}

int SingleRecording::slot_of(int s) {
  if (s < 0 || s >= 32) return -1;
  int& k = slots_[static_cast<size_t>(s)];
  if (k == 0 && slots_used_ < kSlots) k = ++slots_used_;
  return k - 1;
}

int SingleRecording::entity_of(int n) const {
  if (n <= 0 || n >= 512) return -1;
  if (n <= 32) {
    const int k = slots_[static_cast<size_t>(n - 1)];
    return k > 0 ? k : kOverflow + ((n - 1) & 15);
  }
  return n - 16;
}

void SingleRecording::user_info(ConvOut& o, int slot, const std::string& info,
                                ConvertNotes& notes) {
  infos_[static_cast<size_t>(slot & 31)] = info;
  if (!info_value(info, "*spectator").empty() && info_value(info, "*spectator") != "0") return;
  const std::string name = info_value(info, "name");
  if (name.empty()) return;
  const int k = slot_of(slot);
  if (k < 0) {
    notes.dropped["a player past the single-player protocol's 16 slots, nameless"]++;
    return;
  }
  o.u8(13);  // updatename
  o.u8(k);
  o.str(name.substr(0, 31));
  const int top = std::atoi(info_value(info, "topcolor").c_str()) & 15;
  const int bottom = std::atoi(info_value(info, "bottomcolor").c_str()) & 15;
  o.u8(17);  // updatecolors
  o.u8(k);
  o.u8(top << 4 | bottom);
}

void SingleRecording::message(ConvOut& o, std::span<const uint8_t> m, bool in_signon,
                              ConvertNotes& notes) {
  if (m.empty()) return;
  const int id = m[0];
  auto none = [&] { notes.dropped["a message the single-player protocol has no place for"]++; };
  switch (id) {
    case 45:    // the lists: kept for the server info
    case 46: {  // (a start, names, an empty name, the next start)
      std::vector<std::string>& list = id == 45 ? models_ : sounds_;
      size_t at = 2;
      for (std::string s = str_at(m, at); !s.empty() && at <= m.size(); s = str_at(m, at))
        list.push_back(s);
      return;
    }
    case 9: {  // stuffed text: the server's info read; only the bonus flash kept
      const std::string text(m.begin() + 1, m.end() - (m.back() == 0 ? 1 : 0));
      std::string kept;
      size_t at = 0;
      while (at < text.size()) {
        size_t nl = text.find('\n', at);
        if (nl == std::string::npos) nl = text.size() - 1;
        const std::string line = text.substr(at, nl - at + 1);
        at = nl + 1;
        if (line.starts_with("fullserverinfo")) {
          deathmatch_ = info_value(line.substr(line.find('"') + 1), "deathmatch") != "0";
        } else if (line.starts_with("bf")) {
          kept += line;
          continue;
        }
        notes.dropped["a stuffed command the single-player client would run"]++;
      }
      if (!kept.empty()) {
        o.u8(9);
        o.str(kept);
      }
      return;
    }
    case 12:  // the light styles, the damage, the centred texts, the statics: as they are
    case 19:
    case 20:
    case 26:
    case 27:
    case 28:
    case 29:
    case 31:
      o.bytes(m);
      return;
    case 32:  // the CD track: the track and the loop
      if (m.size() >= 2) {
        if (in_signon) {
          cd_track_ = m[1];
        } else {
          o.u8(32);
          o.u8(m[1]);
          o.u8(m[1]);
        }
      }
      return;
    case 14: {  // frags
      const int k = m.size() >= 4 ? slot_of(m[1]) : -1;
      if (k < 0) return none();
      o.u8(14);
      o.u8(k);
      o.bytes(m.subspan(2, 2));
      return;
    }
    case 40: {  // a player's info: the name and colours
      if (m.size() < 6) return;
      size_t at = 6;
      user_info(o, m[1], str_at(m, at), notes);
      return;
    }
    case 51: {  // one key of a player's info
      if (m.size() < 2) return;
      const int s = m[1] & 31;
      size_t at = 2;
      const std::string key = str_at(m, at), value = str_at(m, at);
      if (key == "name" || key == "topcolor" || key == "bottomcolor")
        user_info(o, s, set_info_value(infos_[static_cast<size_t>(s)], key, value), notes);
      return;
    }
    case 8: {  // a print: the protocol has no level
      size_t at = 2;
      o.u8(8);
      o.str(str_at(m, at));
      return;
    }
    case 6: {  // a sound: the field mask first, the entity renumbered
      if (m.size() < 3) return;
      const int packed = m[1] | m[2] << 8;
      size_t at = 3;
      const int vol = (packed & 0x8000) != 0 && at < m.size() ? m[at++] : -1;
      const int att = (packed & 0x4000) != 0 && at < m.size() ? m[at++] : -1;
      const int e = entity_of((packed >> 3) & 1023);
      o.u8(6);
      o.u8((vol >= 0 ? 1 : 0) | (att >= 0 ? 2 : 0));
      if (vol >= 0) o.u8(vol);
      if (att >= 0) o.u8(att);
      o.i16((e < 0 ? 0 : e) << 3 | (packed & 7));
      if (at <= m.size()) o.bytes(m.subspan(at));  // the sound, the point
      return;
    }
    case 16: {  // stop a sound
      if (m.size() < 3) return;
      const int packed = m[1] | m[2] << 8;
      const int e = entity_of(packed >> 3);
      if (e < 0) return;
      o.u8(16);
      o.i16(e << 3 | (packed & 7));
      return;
    }
    case 22: {  // a baseline, renumbered; the players' left out (their slots are not known yet,
                // so each frame writes a player whole, from nothing)
      if (m.size() < 16) return;
      const int source = m[1] | m[2] << 8;
      if (source <= 32) return;
      const int e = entity_of(source);
      if (e < 0) return none();
      o.u8(22);
      o.i16(e);
      o.bytes(m.subspan(3));
      return;
    }
    case 23: {  // a temporary entity
      if (m.size() < 2) return;
      const int type = m[1];
      if (type == 2 && m.size() >= 9) {  // gunshot: no count
        o.u8(23);
        o.u8(2);
        o.bytes(m.subspan(3));
        notes.rewritten["a gunshot's count"]++;
      } else if ((type == 12 && m.size() >= 9) || (type == 13 && m.size() >= 8)) {
        // Blood: particles of the blood colours.
        o.u8(18);
        o.bytes(m.subspan(type == 12 ? 3 : 2, 6));
        o.u8(0);
        o.u8(0);
        o.u8(0);
        o.u8(type == 12 ? std::min(m[2] * 20, 254) : 50);
        o.u8(type == 12 ? 73 : 225);
        notes.rewritten["blood as particles"]++;
      } else if ((type == 5 || type == 6 || type == 9) && m.size() >= 4) {  // beams
        const int e = entity_of(m[2] | m[3] << 8);
        o.u8(23);
        o.u8(type);
        o.i16(e < 0 ? 0 : e);
        o.bytes(m.subspan(4));
      } else if (type <= 11) {
        o.bytes(m);
      } else {
        none();
      }
      return;
    }
    case 30:  // the intermission: its spot taken by the view's player from the next frame on
      if (m.size() >= 10) {
        for (size_t i = 0; i < 3; i++) {
          intermission_origin_[i] = static_cast<int16_t>(m[1 + 2 * i] | m[2 + 2 * i] << 8);
          intermission_angles_[i] = static_cast<int8_t>(m[7 + i]);
        }
        intermission_ = true;
      }
      o.u8(30);
      return;
    case 1:  // nothing, the stats (the frames carry them), the view set (the block header wins),
    case 2:  // the end (written at the end), the signon's stage, the player's own speed
    case 3:
    case 10:
    case 25:
    case 38:
      return;
    default:
      none();
  }
}

void SingleRecording::signon(std::vector<uint8_t>&, double, const ConvMessages& m) {
  size_t from = 0;
  for (size_t end : m.ends) {
    message(signon_, std::span<const uint8_t>(m.out.b).subspan(from, end - from), true,
            dropped_notes_);
    from = end;
    if (signon_ends_.empty() || signon_ends_.back() != signon_.size())
      signon_ends_.push_back(signon_.size());
  }
}

void SingleRecording::write_level(std::vector<uint8_t>& out, const ClientFrame& f,
                                  ConvertNotes& notes) {
  for (const auto& [what, n] : dropped_notes_.dropped) notes.dropped[what] += n;
  for (const auto& [what, n] : dropped_notes_.rewritten) notes.rewritten[what] += n;
  dropped_notes_ = {};
  if (!header_) {
    out.push_back('-');
    out.push_back('1');
    out.push_back('\n');
    header_ = true;
  }
  ConvOut a;
  a.u8(11);  // serverinfo
  a.i32(15);
  a.u8(kSlots);
  a.u8(deathmatch_ ? 1 : 0);
  a.str(level_.title.substr(0, 39));
  if (models_.size() > kMostNames || sounds_.size() > kMostNames)
    notes.dropped["a model or sound past the lists' 255"] +=
        static_cast<int64_t>(std::max(models_.size(), kMostNames) - kMostNames +
                             std::max(sounds_.size(), kMostNames) - kMostNames);
  for (size_t i = 0; i < models_.size() && i < kMostNames; i++) a.str(models_[i].substr(0, 63));
  a.u8(0);
  for (size_t i = 0; i < sounds_.size() && i < kMostNames; i++) a.str(sounds_[i].substr(0, 63));
  a.u8(0);
  a.u8(32);  // cdtrack: the track, the loop
  a.u8(cd_track_);
  a.u8(cd_track_);
  a.u8(5);  // setview: the view's player
  a.i16(1);
  a.u8(25);  // signonnum
  a.u8(1);
  block(out, f.view, a);
  // The signon's messages, in blocks cut between them.
  ConvOut b;
  size_t from = 0;
  for (size_t end : signon_ends_) {
    const std::span<const uint8_t> one(signon_.b.data() + from, end - from);
    from = end;
    if (!b.empty() && b.size() + one.size() > kMostBlock) {
      block(out, f.view, b);
      b.b.clear();
    }
    b.bytes(one);
  }
  b.u8(25);
  b.u8(2);
  block(out, f.view, b);
  ConvOut c;
  c.u8(7);  // time
  c.f32(static_cast<float>(f.time));
  c.u8(25);
  c.u8(3);
  block(out, f.view, c);
  pending_ = false;
  written_ = true;
}

void SingleRecording::frame_body(ConvOut& o, const ClientFrame& f, ConvertNotes& notes) {
  const ClientPlayer& me = f.players[static_cast<size_t>(level_.player & 31)];
  // The view's status.
  int bits = kCdItems;
  std::array<int, 3> vel{};
  for (size_t i = 0; i < 3; i++) {
    vel[i] = std::clamp(me.velocity[i] / 16, -128, 127);
    if (vel[i] != 0) bits |= kCdVelocity1 << i;
  }
  if (me.velocity[2] == 0) bits |= kCdOnGround;
  const int weapon_frame = me.weapon_frame != 0 ? me.weapon_frame : f.stats[kStatWeaponFrame];
  if (weapon_frame != 0) bits |= kCdWeaponFrame;
  if (f.stats[4] != 0) bits |= kCdArmour;
  if (f.stats[2] != 0) bits |= kCdWeapon;
  o.u8(15);
  o.i16(bits);
  for (size_t i = 0; i < 3; i++)
    if ((bits & (kCdVelocity1 << i)) != 0) o.u8(vel[i]);
  o.i32(f.stats[kStatItems]);
  if ((bits & kCdWeaponFrame) != 0) o.u8(weapon_frame);
  if ((bits & kCdArmour) != 0) o.u8(std::clamp(f.stats[4], 0, 255));
  if ((bits & kCdWeapon) != 0) o.u8(f.stats[2]);
  o.i16(f.stats[0]);
  for (int k : {3, 6, 7, 8, 9}) o.u8(std::clamp(f.stats[k], 0, 255));
  // The status's bytes hold up to 255: a larger armour or ammunition shows as 255.
  for (int k : {3, 4, 6, 7, 8, 9})
    if (f.stats[k] > 255) {
      notes.rewritten["a status past 255 shown as 255"]++;
      break;
    }
  o.u8(f.stats[10] & 255);
  // The level's counts, as they change.
  for (int k = 11; k <= 14; k++) {
    if (stats_any_ && f.stats[static_cast<size_t>(k)] == stats_[static_cast<size_t>(k)]) continue;
    o.u8(3);  // updatestat
    o.u8(k);
    o.i32(f.stats[static_cast<size_t>(k)]);
  }
  stats_ = f.stats;
  stats_any_ = true;
  // Every entity in view: a delta from its baseline.
  auto update = [&](int n, const ConvEnt& from, const ConvEnt& to) {
    int b = kSignal;
    if (to.model != from.model) b |= kModel;
    if (to.frame != from.frame) b |= kFrame;
    if (to.colormap != from.colormap) b |= kColormap;
    if (to.skin != from.skin) b |= kSkin;
    if (to.effects != from.effects) b |= kEffects;
    const int ob[3] = {kOrigin1, kOrigin2, kOrigin3}, ab[3] = {kAngle1, kAngle2, kAngle3};
    for (size_t i = 0; i < 3; i++) {
      if (to.origin[i] != from.origin[i]) b |= ob[i];
      if (to.angles[i] != from.angles[i]) b |= ab[i];
    }
    if (n >= 256) b |= kLongEntity;
    if ((b & 0xFF00) != 0) b |= kMoreBits;
    o.u8(b & 0xFF);
    if ((b & kMoreBits) != 0) o.u8(b >> 8 & 0xFF);
    if (n >= 256)
      o.i16(n);
    else
      o.u8(n);
    if ((b & kModel) != 0) o.u8(to.model);
    if ((b & kFrame) != 0) o.u8(to.frame);
    if ((b & kColormap) != 0) o.u8(to.colormap);
    if ((b & kSkin) != 0) o.u8(to.skin);
    if ((b & kEffects) != 0) o.u8(to.effects);
    for (size_t i = 0; i < 3; i++) {
      if ((b & ob[i]) != 0) o.i16(to.origin[i]);
      if ((b & ab[i]) != 0) o.u8(static_cast<uint8_t>(to.angles[i]));
    }
  };
  auto base = [&](int source) {
    const auto it = baselines_.find(source);
    return it != baselines_.end() ? it->second : ConvEnt{};
  };
  for (size_t s = 0; s < f.players.size(); s++) {
    const ClientPlayer& p = f.players[s];
    const bool spot = intermission_ && static_cast<int>(s) == level_.player;
    if (!spot && (!p.present || (p.model == 0 && static_cast<int>(s) != level_.player))) continue;
    if (static_cast<int>(s) != level_.player) slot_of(static_cast<int>(s));
    const int n = entity_of(static_cast<int>(s) + 1);
    ConvEnt e;
    e.model = p.model;
    e.frame = p.frame;
    e.colormap = n <= kSlots ? n : 0;
    e.skin = p.skin;
    e.effects = p.effects;
    e.origin = p.origin;
    e.angles = {static_cast<int8_t>(angle8(-p.angles[0] / 3.0f)),
                static_cast<int8_t>(angle8(p.angles[1])), static_cast<int8_t>(angle8(p.angles[2]))};
    if (spot) {
      e.origin = intermission_origin_;
      e.angles = intermission_angles_;
    }
    update(n, ConvEnt{}, e);
  }
  std::vector<std::pair<int, const ConvEnt*>> list;
  list.reserve(f.entities.size());
  for (const auto& [q, e] : f.entities) list.emplace_back(q, &e);
  if (list.size() > kMostDrawn) {
    auto dist = [&](const ConvEnt& e) {
      double d = 0;
      for (size_t i = 0; i < 3; i++) {
        const double v = static_cast<double>(e.origin[i]) - me.origin[i];
        d += v * v;
      }
      return d;
    };
    std::stable_sort(list.begin(), list.end(), [&](const auto& a, const auto& b) {
      return dist(*a.second) < dist(*b.second);
    });
    notes.dropped["an entity past the most a frame draws"] +=
        static_cast<int64_t>(list.size() - kMostDrawn);
    list.resize(kMostDrawn);
    std::sort(list.begin(), list.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
  }
  for (const auto& [q, e] : list) {
    const int n = entity_of(q);
    if (n > 0) update(n, base(q), *e);
  }
}

void SingleRecording::frame(std::vector<uint8_t>& out, const ClientFrame& f,
                            const ConvMessages& events, ConvertNotes& notes) {
  if (pending_) write_level(out, f, notes);
  if (!written_) return;
  // A long gap filled with the last frame again, a tenth of a second apart.
  if (timed_) {
    const int fills = static_cast<int>(std::ceil((f.time - 1e-6 - last_time_) / kMostGap)) - 1;
    for (int i = 1; i <= fills; i++) {
      const double t = last_time_ + kMostGap * i;
      ConvOut again;
      again.u8(7);
      again.f32(static_cast<float>(t));
      again.bytes(last_body_.b);
      block(out, last_view_, again);
    }
  }
  // An intermission in this frame's messages puts the view at its spot in this frame already.
  for (size_t from = 0, i = 0; i < events.ends.size(); from = events.ends[i++]) {
    const auto m = std::span<const uint8_t>(events.out.b).subspan(from, events.ends[i] - from);
    if (m.size() >= 10 && m[0] == 30) {
      ConvOut ignored;
      message(ignored, m, false, notes);
    }
  }
  ConvOut body;
  frame_body(body, f, notes);
  ConvOut o;
  o.u8(7);  // time
  o.f32(static_cast<float>(f.time));
  o.bytes(body.b);
  // The frame's messages, those that fit; the rest wait for the next frame.
  size_t from = 0;
  for (size_t end : events.ends) {
    ConvOut one;
    message(one, std::span<const uint8_t>(events.out.b).subspan(from, end - from), false, notes);
    from = end;
    if (!one.empty()) waiting_.push_back(std::move(one.b));
  }
  size_t used = 0;
  for (; used < waiting_.size(); used++) {
    if (o.size() + waiting_[used].size() > kMostBlock) break;
    o.bytes(waiting_[used]);
  }
  if (used == 0 && !waiting_.empty() && o.size() + waiting_[0].size() > kMostBlock) {
    notes.dropped["a message larger than a block"]++;
    used = 1;
  }
  waiting_.erase(waiting_.begin(), waiting_.begin() + static_cast<std::ptrdiff_t>(used));
  block(out, f.view, o);
  last_body_ = std::move(body);
  last_view_ = f.view;
  last_time_ = f.time;
  timed_ = true;
}

void SingleRecording::end(std::vector<uint8_t>& out, double, bool disconnect, ConvertNotes&) {
  if (!header_) return;
  ConvOut o;
  for (const auto& w : waiting_) {
    if (o.size() + w.size() > kMostBlock) break;
    o.bytes(w);
  }
  waiting_.clear();
  if (!o.empty()) block(out, last_view_, o);
  if (disconnect) {
    ConvOut d;
    d.u8(2);
    block(out, last_view_, d);
  }
}

}  // namespace shooters_edge::client
