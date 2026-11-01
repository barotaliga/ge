// The listen game recorded (listen_record.hpp): the multiview form's messages written from the
// server's state, given to the zstd stream or kept plain, and the file side that writes them out.
//
// ge:summary The listen recorder's body: signon, frames and stats as multiview records, the file
// ge:keywords record stop listen-record multiview signon playerinfo packetentities updatefrags
// ge:keywords updatestatlong temp-entity explosion print zstd flush
// ge:skill shooter replay
// ge:test tests/shooters_edge_listen_record_test.cpp
#include "shooters_edge/client/listen_record.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <system_error>

namespace shooters_edge::client {

namespace fs = std::filesystem;

namespace {

constexpr int kAll = 6, kStats = 5;        // the record kinds written
constexpr int kMultiviewTag = 0x3144564D;  // the multiview extension family's tag
constexpr int kProtocol = 28;              // the protocol's version
constexpr int kMostNumbered = 512;         // entities above are left out (no extension byte)
constexpr int kStatHealth = 0, kStatItems = 15;

struct Out {
  std::vector<uint8_t> b;
  void u8(int v) { b.push_back(static_cast<uint8_t>(v)); }
  void i16(int v) {
    u8(v & 0xFF);
    u8((v >> 8) & 0xFF);
  }
  void i32(int32_t v) {
    const auto u = static_cast<uint32_t>(v);
    for (int i = 0; i < 4; i++) u8(static_cast<int>((u >> (8 * i)) & 0xFF));
  }
  void f32(float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof u);
    i32(static_cast<int32_t>(u));
  }
  void str(const std::string& s) {
    b.insert(b.end(), s.begin(), s.end());
    u8(0);
  }
  // A coordinate in eighths of a unit, an angle in 1/256 of a turn, a player's angle in 1/65536.
  void coord(float f) { i16(static_cast<int>(std::lround(f * 8))); }
  void angle(float deg) { u8(static_cast<int>(std::lround(deg * 256 / 360)) & 255); }
  void angle16(float deg) { i16(static_cast<int>(std::lround(deg * 65536 / 360)) & 0xFFFF); }
  void point(sh::Vec3f p) {
    coord(p.x);
    coord(p.y);
    coord(p.z);
  }
};

// The recording's next plain bytes: kept, or given to the stream (flushed every
// kListenFlushBytes), its output moved to `ready`.
void emit(ListenRecorder& r, const Out& o) {
  r.plain += static_cast<int64_t>(o.b.size());
  if (!r.packed) {
    r.ready.insert(r.ready.end(), o.b.begin(), o.b.end());
    return;
  }
  ge::vfs::zstd_write(r.pack, o.b);
  if (r.plain - r.flushed_at >= kListenFlushBytes ||
      r.last_time - r.flushed_time >= kListenFlushSeconds) {
    ge::vfs::zstd_flush(r.pack);
    r.flushed_at = r.plain;
    r.flushed_time = r.last_time;
  }
  r.ready.insert(r.ready.end(), r.pack.out.begin(), r.pack.out.end());
  r.pack.out.clear();
}

// A record: the milliseconds, the kind and the player, the length and the messages.
void record(ListenRecorder& r, int ms, int kind, int target, const Out& msg) {
  Out o;
  o.u8(ms);
  o.u8(kind | target << 3);
  o.i32(static_cast<int32_t>(msg.b.size()));
  o.b.insert(o.b.end(), msg.b.begin(), msg.b.end());
  emit(r, o);
}

int field_int(const Listen& l, int e, const char* name) {
  return static_cast<int>(field_named(l, e, name));
}

// A player's view angles: the local player's own, a bot's from its `v_angle`, else its body's.
sh::Vec3f player_angles(const Listen& l, int e) {
  if (e == player_state(l).entity) return l.view;
  const sh::Vec3f v{field_named(l, e, "v_angle_x"), field_named(l, e, "v_angle_y"),
                    field_named(l, e, "v_angle_z")};
  if (v.x != 0 || v.y != 0 || v.z != 0) return v;
  return l.server->sv.edicts[static_cast<size_t>(e)].angles;
}

std::string user_info(const sv::ClientInfo& c) {
  std::string s;
  for (const auto& [k, v] : c.userinfo) s.append("\\").append(k).append("\\").append(v);
  return s;
}

// The list message `id` (the model or sound list) of `names` past their empty first.
void name_list(Out& m, int id, const std::vector<std::string>& names) {
  m.u8(id);
  m.u8(0);
  for (size_t i = 1; i < names.size() && i < 256; i++) m.str(names[i]);
  m.str("");
  m.u8(0);
}

}  // namespace

ListenRecorder::~ListenRecorder() { record_close(*this); }

void record_listen_begin(ListenRecorder& r, const Listen& l, int pack_level) {
  r.on = true;
  r.packed = pack_level > 0;
  r.ready.clear();
  r.plain = r.flushed_at = r.frames = 0;
  if (r.packed) ge::vfs::zstd_begin(r.pack, {std::clamp(pack_level, 1, 22), 0, false});
  const sv::QcServer& s = *l.server;
  r.last_time = r.flushed_time = s.sv.time;
  r.prints_seen = l.prints.size();
  r.explosions_seen = l.explosions.size();
  r.frags.fill(-100000);
  for (auto& st : r.stats) st.fill(-100000);
  Out sign;
  sign.u8(11);  // serverdata: the multiview tag and its bits, the protocol, the server count
  sign.i32(kMultiviewTag);
  sign.i32(0);
  sign.i32(kProtocol);
  sign.i32(1);
  sign.str(r.game_dir);  // the game folder, which the reader opens the demo's files from
  sign.f32(static_cast<float>(s.sv.time));  // a multiview recording's: the time, not a slot
  sign.str(map_title(l));
  for (int k = 0; k < 10; k++) sign.f32(k == 0 ? 800.0f : 320.0f);  // gravity, the rest
  record(r, 0, kAll, 0, sign);
  Out lists;
  name_list(lists, 45, s.models);  // modellist
  name_list(lists, 46, s.sounds);  // soundlist
  for (int k = 0; k < sv::kLightStyles; k++) {
    if (s.lightstyles[k].empty()) continue;
    lists.u8(12);
    lists.u8(k);
    lists.str(s.lightstyles[k]);
  }
  for (const sv::StaticEntity& st : s.statics) {
    if (st.modelindex <= 0 || st.modelindex > 255) continue;
    lists.u8(20);  // spawnstatic: model, frame, colormap, skin, then each origin and angle
    lists.u8(st.modelindex);
    lists.u8(static_cast<int>(st.frame));
    lists.u8(static_cast<int>(st.colormap));
    lists.u8(static_cast<int>(st.skin));
    const float o[3] = {st.origin.x, st.origin.y, st.origin.z};
    const float a[3] = {st.angles.x, st.angles.y, st.angles.z};
    for (int k = 0; k < 3; k++) {
      lists.coord(o[k]);
      lists.angle(a[k]);
    }
  }
  for (int k = 0; k < sh::kMaxClients; k++) {
    const sv::ClientInfo& c = s.clients[k];
    if (!c.active) continue;
    lists.u8(40);  // updateuserinfo
    lists.u8(k);
    lists.i32(c.userid);
    lists.str(user_info(c));
  }
  record(r, 0, kAll, 0, lists);
}

void record_listen_frame(ListenRecorder& r, const Listen& l) {
  if (!r.on || l.server == nullptr) return;
  const sv::QcServer& s = *l.server;
  const int ms =
      std::clamp(static_cast<int>(std::lround((s.sv.time - r.last_time) * 1000)), 1, 255);
  r.last_time = s.sv.time;
  Out m;
  for (int k = 0; k < sh::kMaxClients; k++) {
    if (!s.clients[k].active) continue;
    const int e = k + 1;
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    const int health = field_int(l, e, "health");
    // Origin and angles (1..32), effects 64, skin 128, dead 256, weapon frame 1024, model 2048.
    const int bits = 63 | 64 | 128 | (health <= 0 ? 256 : 0) | 1024 | 2048;
    m.u8(42);  // playerinfo, the recorder's form: slot, bits, frame, the fields
    m.u8(k);
    m.i16(bits);
    m.u8(field_int(l, e, "frame"));
    m.point(ed.origin);
    const sh::Vec3f a = player_angles(l, e);
    m.angle16(a.x);
    m.angle16(a.y);
    m.angle16(a.z);
    m.u8(std::clamp(ed.modelindex, 0, 255));
    m.u8(field_int(l, e, "skin"));
    m.u8(field_int(l, e, "effects"));
    m.u8(field_int(l, e, "weaponframe"));
  }
  m.u8(47);  // packetentities: the whole list
  const int models = static_cast<int>(s.models.size());
  for (int e = sh::kMaxClients + 1; e < std::min(s.sv.num_edicts, kMostNumbered); e++) {
    const sh::Edict& ed = s.sv.edicts[static_cast<size_t>(e)];
    if (ed.free || ed.modelindex <= 0 || ed.modelindex >= models || ed.modelindex > 255) continue;
    // The number, every origin and angle, the frame, then the more byte: angles 1 and 3, the
    // model, the skin, the effects.
    m.i16(e | 1 << 9 | 1 << 10 | 1 << 11 | 1 << 12 | 1 << 13 | 1 << 15);
    m.u8(1 | 2 | 4 | 16 | 32);
    m.u8(ed.modelindex);
    m.u8(field_int(l, e, "frame"));
    m.u8(field_int(l, e, "skin"));
    m.u8(field_int(l, e, "effects"));
    m.coord(ed.origin.x);
    m.angle(ed.angles.x);
    m.coord(ed.origin.y);
    m.angle(ed.angles.y);
    m.coord(ed.origin.z);
    m.angle(ed.angles.z);
  }
  m.i16(0);
  for (int k = 0; k < sh::kMaxClients; k++) {
    if (!s.clients[k].active) continue;
    const int frags = field_int(l, k + 1, "frags");
    if (frags == r.frags[static_cast<size_t>(k)]) continue;
    r.frags[static_cast<size_t>(k)] = frags;
    m.u8(14);  // updatefrags
    m.u8(k);
    m.i16(frags);
  }
  r.explosions_seen = std::min(r.explosions_seen, l.explosions.size());
  for (; r.explosions_seen < l.explosions.size(); r.explosions_seen++) {
    m.u8(23);  // temp entity: an explosion where it went off
    m.u8(3);
    m.point(l.explosions[r.explosions_seen]);
  }
  r.prints_seen = std::min(r.prints_seen, l.prints.size());
  for (; r.prints_seen < l.prints.size(); r.prints_seen++) {
    m.u8(8);  // print, at the high level
    m.u8(2);
    m.str(l.prints[r.prints_seen] + (l.prints[r.prints_seen].ends_with('\n') ? "" : "\n"));
  }
  record(r, ms, kAll, 0, m);
  // Each player's stats that changed: health, ammunition, armour, the shells to cells, the
  // weapon in hand, the items.
  static const std::pair<int, const char*> kStatFields[] = {
      {kStatHealth, "health"}, {3, "currentammo"}, {4, "armorvalue"},
      {6, "ammo_shells"},      {7, "ammo_nails"},  {8, "ammo_rockets"},
      {9, "ammo_cells"},       {10, "weapon"},     {kStatItems, "items"}};
  for (int k = 0; k < sh::kMaxClients; k++) {
    if (!s.clients[k].active) continue;
    Out st;
    for (const auto& [stat, name] : kStatFields) {
      const int32_t v = field_int(l, k + 1, name);
      int32_t& last = r.stats[static_cast<size_t>(k)][static_cast<size_t>(stat)];
      if (v == last) continue;
      last = v;
      st.u8(38);  // updatestatlong
      st.u8(stat);
      st.i32(v);
    }
    if (!st.b.empty()) record(r, 0, kStats, k, st);
  }
  r.frames++;
}

void record_listen_end(ListenRecorder& r) {
  if (!r.on) return;
  Out end;
  end.u8(2);  // disconnect
  end.str("EndOfDemo");
  record(r, 1, kAll, 0, end);
  if (r.packed) {
    ge::vfs::zstd_end(r.pack);
    r.ready.insert(r.ready.end(), r.pack.out.begin(), r.pack.out.end());
    r.pack.out.clear();
  }
  r.on = false;
}

namespace {

void write_ready(ListenRecorder& r) {
  if (!r.file.is_open() || r.ready.empty()) return;
  r.file.write(reinterpret_cast<const char*>(r.ready.data()),
               static_cast<std::streamsize>(r.ready.size()));
  r.file.flush();
  r.written += static_cast<int64_t>(r.ready.size());
  r.ready.clear();
}

}  // namespace

bool record_open(ListenRecorder& r, const Listen& l, const SearchPath& files,
                 const std::string& name, const RecordSettings& s, std::string& error) {
  if (r.on) {
    error = "record: already recording " + r.path + "; next: `stop` first";
    return false;
  }
  bool ok = !name.empty() && name.size() <= 64;
  for (const char c : name)
    ok = ok && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_' || c == '-');
  if (!ok) {
    error = "record: the name `" + name +
            "` is not 1..64 letters, digits, _ or -; next: `record match1`";
    return false;
  }
  if (files.folder.empty()) {
    error = "record: no game folder to keep recordings in; next: start the game with -basedir";
    return false;
  }
  if (l.server == nullptr || l.campaign_host) {
    error = "record: only a match records (a campaign keeps saves); next: start a match with `map`";
    return false;
  }
  const int level = record_pack_level(s);
  const fs::path file = fs::path(files.folder) / (name + (level > 0 ? ".mvdz" : ".mvd"));
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  r.file.open(file, std::ios::binary | std::ios::trunc);
  if (!r.file) {
    error = "record: could not write " + file.string() +
            "; next: check the folder is writable and the disk has room";
    return false;
  }
  r.path = file.string();
  r.game_dir = fs::path(files.folder).filename().string();
  r.written = 0;
  record_listen_begin(r, l, level);
  write_ready(r);
  return true;
}

std::string record_close(ListenRecorder& r) {
  if (!r.on && !r.file.is_open()) return "";
  record_listen_end(r);
  write_ready(r);
  r.file.close();
  const double kib = static_cast<double>(r.written) / 1024;
  const double plain_kib = static_cast<double>(r.plain) / 1024;
  const std::string line =
      "recorded " + r.path + ": " + std::to_string(r.frames) + " frames, " +
      std::to_string(static_cast<int>(std::lround(kib))) + " KiB" +
      (r.packed ? " (" + std::to_string(static_cast<int>(std::lround(plain_kib))) + " KiB unpacked)"
                : "") +
      "; play it with `playdemo " + r.path + "`";
  return line;
}

void record_tick(ListenRecorder& r, const Listen& l, int frames, ge::console::Console& console) {
  if (!r.on) return;
  if (l.map_changed || l.restarted || l.server == nullptr) {
    console.print("record: the map changed, so the recording ends here");
    console.print(record_close(r));
    return;
  }
  if (frames <= 0) return;
  record_listen_frame(r, l);
  write_ready(r);
}

void add_record_commands(ge::console::Console& console, ListenRecorder& r, const Listen& listen,
                         const SearchPath& files, const RecordSettings& s) {
  using Con = ge::console::Console;
  using CArgs = ge::console::Args;
  console.add_command(
      "record",
      "record <name>: the match recorded as <game folder>/<name>.mvdz (packed: demo_pack) until "
      "`stop`",
      [&r, &listen, &files, &s](Con& c, const CArgs& a) {
        if (a.size() < 2) {
          c.print("record: name the recording (record match1)");
          return;
        }
        std::string error;
        if (!record_open(r, listen, files, a[1], s, error))
          c.print(error);
        else
          c.print("recording " + r.path);
      });
  console.add_command("stop", "stop: the recording `record` began ended and its file closed",
                      [&r](Con& c, const CArgs&) {
                        const std::string line = record_close(r);
                        c.print(line.empty() ? "stop: not recording" : line);
                      });
}

}  // namespace shooters_edge::client
