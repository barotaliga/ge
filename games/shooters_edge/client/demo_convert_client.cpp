// The client recording written for the oldest clients (demo_convert_client.hpp): the signon
// packets, the sequences record, a command record and a packet a frame, the end.
//
// ge:summary The client recording's writer for the oldest clients: signon, commands, frames
// ge:keywords demo-convert client-recording protocol-28 old-client writer conv_write_delta
// ge:skill shooter
// ge:test tests/shooters_edge_demo_convert_test.cpp
#include "shooters_edge/client/demo_convert_client.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace shooters_edge::client {

namespace {

constexpr size_t kSignonPacket = 725;  // a signon packet is flushed past this, bytes
constexpr size_t kFramePacket = 1400;  // a frame's packet holds at most this, bytes
constexpr size_t kMostEntities = 64;   // a frame's list holds at most this many
constexpr double kListsLater = 0.05;   // the model list after the server's info, seconds

// The entity list's flags: the word's, then the more byte's.
constexpr int kOrigin1 = 1 << 9, kOrigin2 = 1 << 10, kOrigin3 = 1 << 11, kAngle2 = 1 << 12;
constexpr int kFrame = 1 << 13, kMoreBits = 1 << 15;
constexpr int kAngle1 = 1 << 0, kAngle3 = 1 << 1, kModel = 1 << 2, kColormap = 1 << 3;
constexpr int kSkin = 1 << 4, kEffects = 1 << 5;

// The client form's player bits.
constexpr int kPfMsec = 0x1, kPfCommand = 0x2, kPfVelocity1 = 0x4, kPfModel = 0x20;
constexpr int kPfSkin = 0x40, kPfEffects = 0x80, kPfWeaponFrame = 0x100, kPfDead = 0x200;
constexpr int kPfGib = 0x400;

void put_f32(std::vector<uint8_t>& out, float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, sizeof u);
  for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(u >> (8 * i)));
}

void put_i32(std::vector<uint8_t>& out, int32_t v) {
  const auto u = static_cast<uint32_t>(v);
  for (int i = 0; i < 4; i++) out.push_back(static_cast<uint8_t>(u >> (8 * i)));
}

// A list message (the sounds' or the models') in chunks of about a signon packet each.
std::vector<ConvOut> name_lists(int id, const std::vector<std::string>& names) {
  std::vector<ConvOut> chunks;
  const size_t most = std::min<size_t>(names.size(), 255);
  size_t i = 0;
  do {
    ConvOut m;
    m.u8(id);
    m.u8(static_cast<int>(i));
    for (; i < most && m.size() < kSignonPacket; i++) m.str(names[i]);
    m.str("");
    m.u8(i < most ? static_cast<int>(i) : 0);  // the next chunk's start; 0 on the last
    chunks.push_back(std::move(m));
  } while (i < most);
  return chunks;
}

}  // namespace

void ConvOut::i16(int v) {
  u8(v & 0xFF);
  u8((v >> 8) & 0xFF);
}

void ConvOut::i32(int32_t v) { put_i32(b, v); }

void ConvOut::f32(float f) { put_f32(b, f); }

void ConvOut::str(const std::string& s) {
  b.insert(b.end(), s.begin(), s.end());
  b.push_back(0);
}

int conv_angle16(float deg) {
  return static_cast<int>(std::lround(deg * 65536.0f / 360.0f)) & 0xFFFF;
}

void conv_write_spawn(ConvOut& o, const ConvEnt& e) {
  o.u8(e.model);
  o.u8(e.frame);
  o.u8(e.colormap);
  o.u8(e.skin);
  for (size_t i = 0; i < 3; i++) {
    o.i16(e.origin[i]);
    o.u8(static_cast<uint8_t>(e.angles[i]));
  }
}

void conv_write_delta(ConvOut& o, int q, const ConvEnt& from, const ConvEnt& to, bool always,
                      bool whole) {
  int bits = 0;
  if (whole || to.origin[0] != from.origin[0]) bits |= kOrigin1;
  if (whole || to.origin[1] != from.origin[1]) bits |= kOrigin2;
  if (whole || to.origin[2] != from.origin[2]) bits |= kOrigin3;
  if (whole || to.angles[0] != from.angles[0]) bits |= kAngle1;
  if (whole || to.angles[1] != from.angles[1]) bits |= kAngle2;
  if (whole || to.angles[2] != from.angles[2]) bits |= kAngle3;
  if (whole || to.frame != from.frame) bits |= kFrame;
  if (whole || to.model != from.model) bits |= kModel;
  if (whole || to.colormap != from.colormap) bits |= kColormap;
  if (whole || to.skin != from.skin) bits |= kSkin;
  if (whole || to.effects != from.effects) bits |= kEffects;
  if (bits == 0 && !always) return;
  const int more = bits & 0xFF;
  if (more != 0) bits |= kMoreBits;
  o.i16(q | (bits & 0xFE00));
  if (more != 0) o.u8(more);
  if ((bits & kModel) != 0) o.u8(to.model);
  if ((bits & kFrame) != 0) o.u8(to.frame);
  if ((bits & kColormap) != 0) o.u8(to.colormap);
  if ((bits & kSkin) != 0) o.u8(to.skin);
  if ((bits & kEffects) != 0) o.u8(to.effects);
  const int ob[3] = {kOrigin1, kOrigin2, kOrigin3};
  const int ab[3] = {kAngle1, kAngle2, kAngle3};
  for (size_t i = 0; i < 3; i++) {
    if ((bits & ob[i]) != 0) o.i16(to.origin[i]);
    if ((bits & ab[i]) != 0) o.u8(static_cast<uint8_t>(to.angles[i]));
  }
}

void ClientRecording::packet(std::vector<uint8_t>& out, double time, const ConvOut& body) {
  seq_++;
  put_f32(out, static_cast<float>(time));
  out.push_back(1);
  put_i32(out, static_cast<int32_t>(body.size() + 8));
  put_i32(out, static_cast<int32_t>(seq_));  // the packet's sequence
  put_i32(out, static_cast<int32_t>(seq_));  // the acknowledgement: the same
  out.insert(out.end(), body.b.begin(), body.b.end());
}

void ClientRecording::flush_signon(std::vector<uint8_t>& out, double time) {
  if (pending_.empty()) return;
  packet(out, time, pending_);
  pending_.b.clear();
}

void ClientRecording::level(std::vector<uint8_t>& out, double time, const ClientLevel& l,
                            ConvertNotes& notes) {
  flush_signon(out, time);
  if (!waiting_.empty()) {
    notes.dropped["a message left over when the level changed"] +=
        static_cast<int64_t>(waiting_.size());
    waiting_.clear();
  }
  in_signon_ = true;
  player_ = l.player;
  stats_any_ = false;
  baselines_.clear();
  ConvOut sd;
  sd.u8(11);  // serverdata: protocol, server count, game folder, player, title, movement
  sd.i32(28);
  sd.i32(l.server_count);
  sd.str(l.game);
  sd.u8(l.player);
  sd.str(l.title.substr(0, 39));
  for (float f : l.movevars) sd.f32(f);
  sd.u8(9);  // stufftext: the server's info
  sd.str("fullserverinfo \"" + l.server_info + "\"\n");
  packet(out, time, sd);
  if (l.models.size() > 255 || l.sounds.size() > 255)
    notes.dropped["a model or sound past the lists' 255"] +=
        static_cast<int64_t>(std::max<size_t>(l.models.size(), 255) - 255 +
                             std::max<size_t>(l.sounds.size(), 255) - 255);
  for (const ConvOut& c : name_lists(46, l.sounds)) packet(out, time, c);  // soundlist
  const std::vector<ConvOut> models = name_lists(45, l.models);            // modellist
  for (size_t i = 0; i < models.size(); i++)
    packet(out, i + 1 == models.size() ? time + kListsLater : time, models[i]);
  last_time_ = time + kListsLater;
}

void ClientRecording::signon(std::vector<uint8_t>& out, double time, const ConvMessages& m) {
  time = std::max(time, last_time_);
  size_t from = 0;
  for (size_t end : m.ends) {
    const std::span<const uint8_t> one(m.out.b.data() + from, end - from);
    from = end;
    if (!pending_.empty() && pending_.size() + one.size() > kSignonPacket) flush_signon(out, time);
    pending_.bytes(one);
    if (pending_.size() >= kSignonPacket) flush_signon(out, time);
  }
}

void ClientRecording::command(std::vector<uint8_t>& out, double time, int msec,
                              const std::array<float, 3>& view, const ClientPlayer& p) {
  put_f32(out, static_cast<float>(time));
  out.push_back(0);
  // The command: msec, padding, the angles, the moves, the buttons, the impulse.
  out.push_back(static_cast<uint8_t>(std::clamp(msec, 0, 255)));
  for (int i = 0; i < 3; i++) out.push_back(0);
  for (float a : view) put_f32(out, a);
  // The moves: the view's velocity along its own forward and right on the ground.
  int forward = 0, side = 0;
  if (!p.dead) {
    const double yaw = static_cast<double>(view[1]) * (3.14159265358979323846 / 180.0);
    const double vx = p.velocity[0], vy = p.velocity[1];
    forward = static_cast<int>(std::lround(vx * std::cos(yaw) + vy * std::sin(yaw)));
    side = static_cast<int>(std::lround(vx * std::sin(yaw) - vy * std::cos(yaw)));
  }
  for (int v : {std::clamp(forward, -800, 800), std::clamp(side, -800, 800), 0}) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  }
  out.push_back(0);  // buttons
  out.push_back(0);  // impulse
  for (float a : view) put_f32(out, a);
}

void ClientRecording::player_info(ConvOut& m, int slot, const ClientPlayer& p) const {
  int flags = 0;
  const bool view = slot == player_;
  if (!view) flags |= kPfMsec | kPfCommand;
  for (int i = 0; i < 3; i++)
    if (p.velocity[static_cast<size_t>(i)] != 0) flags |= kPfVelocity1 << i;
  if (p.model != 0) flags |= kPfModel;
  if (p.skin != 0) flags |= kPfSkin;
  if (p.effects != 0) flags |= kPfEffects;
  if (p.weapon_frame != 0) flags |= kPfWeaponFrame;
  if (p.dead) flags |= kPfDead;
  if (p.gib) flags |= kPfGib;
  m.u8(42);  // playerinfo: the slot, the flags, the origin, the frame, then what the flags name
  m.u8(slot);
  m.i16(flags);
  for (int16_t v : p.origin) m.i16(v);
  m.u8(p.frame);
  if ((flags & kPfMsec) != 0) m.u8(0);
  if ((flags & kPfCommand) != 0) {
    m.u8(0x01 | 0x80);  // the pitch and the yaw, which the player is drawn with
    m.i16(conv_angle16(p.angles[0]));
    m.i16(conv_angle16(p.angles[1]));
    m.u8(0);  // msec
  }
  for (int i = 0; i < 3; i++)
    if ((flags & (kPfVelocity1 << i)) != 0) m.i16(p.velocity[static_cast<size_t>(i)]);
  if ((flags & kPfModel) != 0) m.u8(p.model);
  if ((flags & kPfSkin) != 0) m.u8(p.skin);
  if ((flags & kPfEffects) != 0) m.u8(p.effects);
  if ((flags & kPfWeaponFrame) != 0) m.u8(p.weapon_frame);
}

void ClientRecording::frame(std::vector<uint8_t>& out, const ClientFrame& f,
                            const ConvMessages& events, ConvertNotes& notes) {
  const double time = std::max(f.time, last_time_);
  int msec = 0;
  if (in_signon_) {
    ConvOut skins;
    skins.u8(9);
    skins.str("skins\n");
    pending_.bytes(skins.b);
    flush_signon(out, last_time_);
    in_signon_ = false;
    // The commands one sequence ahead of the packets: the next packet's, plus one.
    put_f32(out, static_cast<float>(last_time_));
    out.push_back(2);
    put_i32(out, static_cast<int32_t>(seq_ + 2));
    put_i32(out, static_cast<int32_t>(seq_));
  } else if (timed_) {
    msec = static_cast<int>(std::lround((time - last_time_) * 1000.0));
  }
  timed_ = true;
  last_time_ = time;
  const ClientPlayer& me = f.players[static_cast<size_t>(player_ & 31)];
  command(out, time, msec, f.view, me);

  ConvOut m;
  for (size_t s = 0; s < f.players.size(); s++) {
    const ClientPlayer& p = f.players[s];
    if (!p.present) continue;
    if (static_cast<int>(s) != player_ && p.model == 0) continue;  // nothing to draw
    player_info(m, static_cast<int>(s), p);
  }
  // The list: the nearest the view when there are more than the client holds.
  std::vector<std::pair<int, const ConvEnt*>> list;
  list.reserve(f.entities.size());
  for (const auto& [q, e] : f.entities) list.emplace_back(q, &e);
  if (list.size() > kMostEntities) {
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
    notes.dropped["an entity past the 64 a frame holds"] +=
        static_cast<int64_t>(list.size() - kMostEntities);
    list.resize(kMostEntities);
    std::sort(list.begin(), list.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
  }
  m.u8(47);  // packetentities: the whole list, each a delta from its baseline
  for (const auto& [q, e] : list) {
    const auto base = baselines_.find(q);
    if (base != baselines_.end())
      conv_write_delta(m, q, base->second, *e, true);
    else
      conv_write_delta(m, q, ConvEnt{}, *e, true, true);
  }
  m.i16(0);
  for (size_t k = 0; k < f.stats.size(); k++) {
    if (stats_any_ && f.stats[k] == stats_written_[k]) continue;
    stats_written_[k] = f.stats[k];
    if (f.stats[k] >= 0 && f.stats[k] < 256) {
      m.u8(3);  // updatestat
      m.u8(static_cast<int>(k));
      m.u8(f.stats[k]);
    } else {
      m.u8(38);  // updatestatlong
      m.u8(static_cast<int>(k));
      m.i32(f.stats[k]);
    }
  }
  stats_any_ = true;
  // The messages, those waiting first, as many whole ones as the packet holds.
  size_t from = 0;
  for (size_t end : events.ends) {
    waiting_.emplace_back(events.out.b.begin() + static_cast<std::ptrdiff_t>(from),
                          events.out.b.begin() + static_cast<std::ptrdiff_t>(end));
    from = end;
  }
  size_t used = 0;
  for (; used < waiting_.size(); used++) {
    if (m.size() + waiting_[used].size() > kFramePacket) break;
    m.bytes(waiting_[used]);
  }
  waiting_.erase(waiting_.begin(), waiting_.begin() + static_cast<std::ptrdiff_t>(used));
  // A message that did not fit beside the frame when it was first in line never will.
  if (used == 0 && !waiting_.empty()) {
    notes.dropped["a message too large for a packet beside its frame"]++;
    waiting_.erase(waiting_.begin());
  }
  packet(out, time, m);
}

void ClientRecording::end(std::vector<uint8_t>& out, double time, bool disconnect,
                          ConvertNotes& notes) {
  time = std::max(time, last_time_);
  flush_signon(out, time);
  ConvOut m;
  size_t used = 0;
  for (; used < waiting_.size(); used++) {
    if (m.size() + waiting_[used].size() > kFramePacket) break;
    m.bytes(waiting_[used]);
  }
  if (used < waiting_.size())
    notes.dropped["a message left over at the end"] += static_cast<int64_t>(waiting_.size() - used);
  waiting_.clear();
  if (disconnect) {
    m.u8(2);  // disconnect: the client stops the recording there
  }
  if (!m.empty()) packet(out, time, m);
}

}  // namespace shooters_edge::client
