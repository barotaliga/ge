// Recording a multiplayer game, with its voices (classic_record.hpp).
//
// ge:summary The client's and the server's recordings written: packets, commands, signon, frames
// ge:keywords record recorder client-recording multiview signon frame stats events voice
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_record_test.cpp
#include "shooters_edge/net/classic_record.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "progs/layout.hpp"
#include "progs/vm.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_server.hpp"

namespace shooters_edge::net {

namespace pg = ge::progs;
namespace cl = shooters_edge::client;

namespace {

constexpr int kRecRead = 1, kRecMultiple = 3, kRecSingle = 4, kRecStats = 5, kRecAll = 6;
constexpr size_t kBuffer = size_t{1} << 18;  // one record's messages at most, bytes

void put_u8(std::vector<uint8_t>& b, int v) { b.push_back(static_cast<uint8_t>(v)); }
void put_i32(std::vector<uint8_t>& b, int32_t v) {
  const auto u = static_cast<uint32_t>(v);
  for (int i = 0; i < 4; i++) put_u8(b, static_cast<int>((u >> (8 * i)) & 0xFF));
}
void put_f32(std::vector<uint8_t>& b, float f) {
  uint32_t u = 0;
  std::memcpy(&u, &f, sizeof u);
  put_i32(b, static_cast<int32_t>(u));
}
void put_i16(std::vector<uint8_t>& b, int v) {
  put_u8(b, v & 0xFF);
  put_u8(b, (v >> 8) & 0xFF);
}

// A multiview record: the milliseconds, the kind and player, the mask of a multiple, the length
// and the messages.
void put_record(std::vector<uint8_t>& file, int ms, int kind, int target, uint32_t mask,
                std::span<const uint8_t> msg) {
  put_u8(file, ms);
  put_u8(file, kind | target << 3);
  if (kind == kRecMultiple) put_i32(file, static_cast<int32_t>(mask));
  put_i32(file, static_cast<int32_t>(msg.size()));
  file.insert(file.end(), msg.begin(), msg.end());
}

// A scratch writer of up to kBuffer bytes.
struct Scratch {
  std::vector<uint8_t> buf = std::vector<uint8_t>(kBuffer);
  gn::Writer w = gn::make_writer(buf.data(), buf.size());
  std::span<const uint8_t> bytes() const { return {buf.data(), w.overflow ? 0 : w.size}; }
};

float field(const ClassicServer& s, int e, int offset) {
  if (offset < 0) return 0;
  return pg::as_float(*pg::entity_field(s.qc->q.vm, e, offset));
}

bool playing(const ClassicServer& s, int k) {
  return s.clients[k].qc_active && !s.clients[k].spectator &&
         s.qc->sv.clients[static_cast<size_t>(k)].active;
}

// The mask of every spawned player.
uint32_t everyone(const ClassicServer& s) {
  uint32_t m = 0;
  for (int k = 0; k < sh::kMaxClients; k++)
    if (s.clients[k].state == ConnState::spawned) m |= 1u << k;
  return m;
}

void hold(MultiviewRecorder& r, uint32_t to, uint32_t all, std::span<const uint8_t> msg) {
  if (msg.empty() || to == 0) return;
  MultiviewRecorder::Held h;
  if ((to & all) == all) {
    h.kind = kRecAll;
  } else if ((to & (to - 1)) == 0) {
    h.kind = kRecSingle;
    for (uint32_t m = to; (m & 1u) == 0; m >>= 1) h.target++;
  } else {
    h.kind = kRecMultiple;
    h.mask = to;
  }
  h.msg.assign(msg.begin(), msg.end());
  r.held.push_back(std::move(h));
  r.events++;
}

void hold_svc(MultiviewRecorder& r, uint32_t to, uint32_t all, const SvcMsg& m) {
  Scratch o;
  write_svc(o.w, m, WireCtx{});
  hold(r, to, all, o.bytes());
}

// Raw bytes a mod wrote, kept only when they read as whole messages.
bool whole_messages(std::span<const uint8_t> b) {
  gn::Reader rd = gn::make_reader(b.data(), b.size());
  while (remaining(rd) > 0) {
    SvcMsg m;
    std::string error;
    if (!read_svc(rd, m, WireCtx{}, error)) return false;
  }
  return true;
}

void signon(ClassicServer& s, MultiviewRecorder& r) {
  const sv::QcServer& q = *s.qc;
  Scratch o;
  gn::Writer& w = o.w;
  write_byte(w, svc::serverdata);  // no extension families: shorts for coordinates
  write_long(w, kProtocolVersion);
  write_long(w, s.spawncount);
  write_string(w, s.cfg.gamedir.c_str());
  write_float(w, static_cast<float>(q.sv.time));
  write_string(w, q.map_name.c_str());
  const sh::MoveSettings& mv = q.sv.set.move;
  for (const float f : {mv.gravity, mv.stopspeed, mv.maxspeed, mv.spectatormaxspeed, mv.accelerate,
                        mv.airaccelerate, mv.wateraccelerate, mv.friction, mv.waterfriction, 1.0f})
    write_float(w, f);
  for (const bool models : {true, false}) {
    const std::vector<std::string>& names = models ? q.models : q.sounds;
    write_byte(w, models ? svc::modellist : svc::soundlist);
    write_byte(w, 0);
    for (size_t i = 1; i < names.size() && i < 256; i++) write_string(w, names[i].c_str());
    write_byte(w, 0);
    write_byte(w, 0);
  }
  for (const EntityState& b : s.baselines) {
    if (b.modelindex == 0) continue;
    SvcMsg m;
    m.id = svc::spawnbaseline;
    m.ints = {b.number, b.modelindex, b.frame, b.colormap, b.skin};
    m.floats = {b.origin.x, b.angles.x, b.origin.y, b.angles.y, b.origin.z, b.angles.z};
    write_svc(w, m, WireCtx{});
  }
  put_record(r.bytes, 0, kRecAll, 0, 0, o.bytes());
  r.spawncount = s.spawncount;
  r.statics = r.static_sounds = 0;
  r.start_time = q.sv.time;
  r.written_ms = 0;
  for (auto& known : r.stats_known) std::fill(std::begin(known), std::end(known), false);
}

// The statics and static sounds the level has that the recording has not.
void statics(ClassicServer& s, MultiviewRecorder& r) {
  Scratch o;
  const int n = static_cast<int>(s.qc->statics.size());
  for (; r.statics < n; r.statics++) {
    const sv::StaticEntity& st = s.qc->statics[static_cast<size_t>(r.statics)];
    SvcMsg m;
    m.id = svc::spawnstatic;
    m.ints = {st.modelindex, static_cast<int>(st.frame), static_cast<int>(st.colormap),
              static_cast<int>(st.skin)};
    m.floats = {st.origin.x, st.angles.x, st.origin.y, st.angles.y, st.origin.z, st.angles.z};
    write_svc(o.w, m, WireCtx{});
  }
  const int ns = static_cast<int>(s.static_sounds.size());
  for (; r.static_sounds < ns; r.static_sounds++) {
    const ClassicServer::StaticSound& st = s.static_sounds[static_cast<size_t>(r.static_sounds)];
    SvcMsg m;
    m.id = svc::spawnstaticsound;
    m.floats = {st.origin.x, st.origin.y, st.origin.z};
    m.ints = {st.sound, st.volume, st.attenuation};
    write_svc(o.w, m, WireCtx{});
  }
  if (o.w.size > 0) put_record(r.bytes, 0, kRecAll, 0, 0, o.bytes());
}

// A player in the recorder's form: every field, every frame.
void player(const ClassicServer& s, int k, gn::Writer& w) {
  const sh::Edict& e = s.qc->sv.edicts[static_cast<size_t>(k) + 1];
  const sh::MoveCmd& cmd = s.qc->sv.clients[static_cast<size_t>(k)].last_cmd;
  const auto& f = s.qc->q.layout->f;
  constexpr int kAllFields = 1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 1024 | 2048;
  write_byte(w, svc::playerinfo);
  write_byte(w, k);
  write_short(w, kAllFields);
  write_byte(w, static_cast<int>(field(s, k + 1, f.frame)));
  for (int i = 0; i < 3; i++) write_coord(w, e.origin[i]);
  for (int i = 0; i < 3; i++) write_angle16(w, cmd.angles[i]);
  write_byte(w, static_cast<int>(field(s, k + 1, f.modelindex)));
  write_byte(w, static_cast<int>(field(s, k + 1, f.skin)));
  write_byte(w, static_cast<int>(field(s, k + 1, f.effects)));
  write_byte(w, static_cast<int>(field(s, k + 1, s.f_weaponframe)));
}

// A packed recorder's new plain bytes fed to its stream (flushed every kPackFlushBytes).
template <typename R>
void feed(R& r) {
  if (!r.packed || r.bytes.empty()) return;
  ge::vfs::zstd_write(r.pack, r.bytes);
  r.plain += static_cast<int64_t>(r.bytes.size());
  r.bytes.clear();
  if (r.plain - r.flushed_at >= kPackFlushBytes) {
    ge::vfs::zstd_flush(r.pack);
    r.flushed_at = r.plain;
  }
}

template <typename R>
void begin_pack(R& r, int level) {
  r.packed = level > 0;
  if (r.packed) ge::vfs::zstd_begin(r.pack, {std::clamp(level, 1, 22), 0, false});
}

// A packed recorder's stream ended and its file moved into `bytes`.
template <typename R>
void end_pack(R& r) {
  if (!r.packed || r.pack.ended) return;
  feed(r);
  ge::vfs::zstd_end(r.pack);
  r.bytes = std::move(r.pack.out);
}

}  // namespace

// ---- the client's recording ----

void record_start(ClientRecorder& r, bool voice, int pack_level) {
  r = ClientRecorder{};
  r.voice = voice;
  if (voice) r.bytes = cl::voice_header(cl::DemoFormat::client);
  begin_pack(r, pack_level);
}

void record_stop(ClientRecorder& r) { end_pack(r); }

void record_packet(ClientRecorder& r, double time, std::span<const uint8_t> header,
                   std::span<const uint8_t> payload,
                   std::span<const std::pair<size_t, size_t>> messages) {
  std::vector<uint8_t> kept(header.begin(), header.end());
  for (const auto& [from, to] : messages) {
    if (payload[from] == svc::voice) {
      if (!r.voice) {
        r.stripped++;
        continue;
      }
      r.voice_messages++;
    }
    kept.insert(kept.end(), payload.begin() + static_cast<std::ptrdiff_t>(from),
                payload.begin() + static_cast<std::ptrdiff_t>(to));
  }
  kept.insert(kept.end(), r.own.begin(), r.own.end());
  r.own.clear();
  put_f32(r.bytes, static_cast<float>(time));
  put_u8(r.bytes, kRecRead);
  put_i32(r.bytes, static_cast<int32_t>(kept.size()));
  r.bytes.insert(r.bytes.end(), kept.begin(), kept.end());
  r.packets++;
  feed(r);
}

void record_command(ClientRecorder& r, double time, const sh::MoveCmd& cmd) {
  put_f32(r.bytes, static_cast<float>(time));
  put_u8(r.bytes, 0);
  // The command struct: msec, three bytes of padding, the angles, the moves, buttons, impulse.
  put_u8(r.bytes, cmd.msec);
  for (int i = 0; i < 3; i++) put_u8(r.bytes, 0);
  for (int i = 0; i < 3; i++) put_f32(r.bytes, cmd.angles[i]);
  put_i16(r.bytes, cmd.forward);
  put_i16(r.bytes, cmd.side);
  put_i16(r.bytes, cmd.up);
  put_u8(r.bytes, cmd.buttons);
  put_u8(r.bytes, cmd.impulse);
  for (int i = 0; i < 3; i++) put_f32(r.bytes, cmd.angles[i]);  // the view's
  r.commands++;
  feed(r);
}

void record_own_voice(ClientRecorder& r, int slot, const ClcMsg& m) {
  if (!r.voice || slot < 0) return;
  SvcMsg v;
  v.id = svc::voice;
  v.ints = {slot, m.gen, m.seq};
  v.data = m.data;
  Scratch o;
  write_svc(o.w, v, WireCtx{});
  const std::span<const uint8_t> b = o.bytes();
  r.own.insert(r.own.end(), b.begin(), b.end());
  r.own_messages++;
}

// ---- the server's multiview recording ----

void mvd_start(MultiviewRecorder& r, bool voice, int pack_level) {
  r = MultiviewRecorder{};
  r.voice = voice;
  if (voice) r.bytes = cl::voice_header(cl::DemoFormat::multiview);
  begin_pack(r, pack_level);
}

void mvd_events(ClassicServer& s, MultiviewRecorder& r) {
  if (r.spawncount != s.spawncount) return;  // before the signon: the level's start sends it all
  const uint32_t all = everyone(s);
  const sv::MessageSink& out = s.qc->out;
  for (const sv::NetEvent& ev : out.events) {
    SvcMsg m;
    switch (ev.kind) {
      case sv::EventKind::print:
        m.id = svc::print;
        m.ints = {ev.level};
        m.strings = {ev.text};
        break;
      case sv::EventKind::centerprint:
        m.id = svc::centerprint;
        m.strings = {ev.text};
        break;
      case sv::EventKind::lightstyle:
        m.id = svc::lightstyle;
        m.ints = {ev.level};
        m.strings = {ev.text};
        break;
      case sv::EventKind::sound: {
        int snd = 0;
        for (size_t i = 1; i < s.qc->sounds.size(); i++)
          if (s.qc->sounds[i] == ev.text) snd = static_cast<int>(i);
        if (snd == 0 || snd > 255 || ev.ent > 1023) {
          r.dropped++;
          continue;
        }
        m.id = svc::sound;
        m.ints = {ev.ent, ev.level, static_cast<int>(ev.values[0]),
                  static_cast<int>(ev.values[1] * 64), snd};
        m.floats = {ev.origin.x, ev.origin.y, ev.origin.z};
        break;
      }
      case sv::EventKind::particle:
        m.id = svc::temp_entity;
        if (ev.text == "explosion") {
          m.ints = {te::explosion};
        } else if (ev.text == "blood") {
          m.ints = {te::blood, static_cast<int>(ev.values[1]) / 2};
        } else if (ev.text == "lightning blood") {
          m.ints = {te::lightningblood};
        } else {
          r.dropped++;
          continue;
        }
        m.floats = {ev.origin.x, ev.origin.y, ev.origin.z};
        break;
      case sv::EventKind::multicast:
        if (whole_messages(ev.bytes))
          hold(r, ev.to, all, ev.bytes);
        else
          r.dropped++;
        continue;
      default:
        continue;  // stuffed commands, pauses, statics (from the list): nothing a viewer replays
    }
    hold_svc(r, ev.to, all, m);
  }
  if (whole_messages(out.all)) hold(r, all, all, out.all);
  if (whole_messages(out.unreliable)) hold(r, all, all, out.unreliable);
  for (int k = 0; k < sh::kMaxClients; k++)
    if (!out.one[k].empty() && whole_messages(out.one[k])) hold(r, 1u << k, all, out.one[k]);
}

void mvd_voice(MultiviewRecorder& r, const SvcMsg& m) {
  if (!r.voice || r.spawncount < 0) return;
  Scratch o;
  write_svc(o.w, m, WireCtx{});
  MultiviewRecorder::Held h;
  h.kind = kRecAll;
  const std::span<const uint8_t> b = o.bytes();
  h.msg.assign(b.begin(), b.end());
  r.held.push_back(std::move(h));
  r.voice_messages++;
}

void mvd_frame(ClassicServer& s, MultiviewRecorder& r) {
  if (r.spawncount != s.spawncount) {
    signon(s, r);
    r.held.clear();
  }
  statics(s, r);
  // The frame's clock: whole milliseconds since the signon, at least one a frame.
  const int64_t now_ms = std::llround((s.qc->sv.time - r.start_time) * 1000);
  const int ms = static_cast<int>(std::clamp<int64_t>(now_ms - r.written_ms, 1, 255));
  r.written_ms += ms;
  Scratch o;
  for (int k = 0; k < sh::kMaxClients; k++)
    if (playing(s, k)) player(s, k, o.w);
  std::vector<EntityState> ents;
  const sh::Server& sv = s.qc->sv;
  for (int e = sv.set.maxclients + 1; e < sv.num_edicts && e < 512; e++) {
    if (sv.edicts[static_cast<size_t>(e)].free) continue;
    const EntityState st = entity_state(s, e);
    if (st.modelindex != 0) ents.push_back(grid_entity(st, Coords::shorts));
  }
  write_byte(o.w, svc::packetentities);
  write_packet_entities(o.w, nullptr, ents, s.baselines, Ext{});
  put_record(r.bytes, ms, kRecAll, 0, 0, o.bytes());
  for (int k = 0; k < sh::kMaxClients; k++) {
    if (!playing(s, k)) continue;
    int32_t st[kStats] = {};
    player_stats(s, k, st, false);
    Scratch so;
    for (int i = 0; i < kStats; i++) {
      if (r.stats_known[k][i] && r.stats[k][i] == st[i]) continue;
      SvcMsg m;
      m.id = st[i] >= 0 && st[i] <= 255 ? svc::updatestat : svc::updatestatlong;
      m.ints = {i, st[i]};
      write_svc(so.w, m, WireCtx{});
      r.stats[k][i] = st[i];
      r.stats_known[k][i] = true;
    }
    if (so.w.size > 0) put_record(r.bytes, 0, kRecStats, k, 0, so.bytes());
  }
  for (const MultiviewRecorder::Held& h : r.held)
    put_record(r.bytes, 0, h.kind, h.target, h.mask, h.msg);
  r.held.clear();
  r.frames++;
  feed(r);
}

void mvd_stop(MultiviewRecorder& r) {
  Scratch o;
  write_byte(o.w, svc::disconnect);
  write_string(o.w, "EndOfDemo");
  put_record(r.bytes, 1, kRecAll, 0, 0, o.bytes());
  end_pack(r);
}

}  // namespace shooters_edge::net
