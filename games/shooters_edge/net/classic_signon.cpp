// The classic server's string commands (classic_server.hpp): the signon steps (new, pext, the
// lists, prespawn, spawn, begin), user info, downloads, and everything else handed to the mod.
//
// THE LISTS: a page is at most half a message; it names precache slots n + 1 onward and ends with
// an empty name and the low byte of the last slot it named (0: done). A page never ends on a slot
// that is a multiple of 256, so `(count & 0xff00) + byte` is the next request's start.
// PRESPAWN walks one index space: the static entities, then the baselines (every entity number
// with a model at the map's start), then the signon buffers; each reply fills about half a
// message and stuffs the next request, the last `cmd spawn <sc> 0`.
//
// ge:summary The classic server's signon and string commands: lists, prespawn, spawn, downloads
// ge:keywords signon new pext soundlist modellist prespawn spawn begin skins setinfo download
// ge:keywords nextdl stopdownload full_client_update string_command spawn-count
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#include <algorithm>
#include <cstring>

#include "progs/layout.hpp"
#include "progs/text.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/server/commands.hpp"

namespace shooters_edge::net {

namespace pg = ge::progs;

namespace {

constexpr size_t kPage = kMaxMsgLen / 2;

const char* const kVisibleKeys[] = {"name",   "team",  "skin",  "topcolor", "bottomcolor",
                                    "gender", "*auth", "*flag", "*skill"};

bool visible_key(const std::string& k) {
  for (const char* v : kVisibleKeys)
    if (k == v) return true;
  return false;
}

WireCtx ctx_of(const ClassicClient& c) {
  WireCtx ctx;
  ctx.ext = c.ext;
  ctx.chunked = c.ext.has(ExtFamily::x, kXChunkedDl);
  return ctx;
}

void cmd_new(ClassicServer& s, int slot) {
  ClassicClient& c = s.clients[slot];
  if (c.need_pext) {
    stuff(s, slot, "cmd pext\n");
    return;
  }
  sv::QcServer& q = *s.qc;
  SvcMsg m;
  m.id = svc::serverdata;
  for (ExtFamily f : {ExtFamily::x, ExtFamily::x2, ExtFamily::mvd})
    if (c.ext[f] != 0) m.ext.emplace_back(family_magic(f), c.ext[f]);
  const int number = slot | (c.spectator ? 0x80 : 0);
  m.ints = {kProtocolVersion, s.spawncount, number};
  const char* level = pg::vm_string(
      q.q.vm, static_cast<int32_t>(*pg::entity_field(q.q.vm, 0, q.q.layout->f.message)));
  m.strings = {s.cfg.gamedir, level && level[0] ? level : q.map_name};
  const sh::MoveSettings& mv = q.sv.set.move;
  m.floats = {
      mv.gravity,       mv.stopspeed,       mv.maxspeed, mv.spectatormaxspeed, mv.accelerate,
      mv.airaccelerate, mv.wateraccelerate, mv.friction, mv.waterfriction,     1.0f};
  reliable_svc(s, slot, m);
  SvcMsg cd;
  cd.id = svc::cdtrack;
  cd.ints = {0};
  reliable_svc(s, slot, cd);
  stuff(s, slot, "fullserverinfo \"" + s.serverinfo + "\"\n");
}

void cmd_pext(ClassicServer& s, int slot, const pg::Words& w) {
  ClassicClient& c = s.clients[slot];
  Ext asked;
  asked[ExtFamily::z] = c.ext[ExtFamily::z];
  for (int i = 1; i + 1 < w.count; i += 2) {
    const uint32_t magic = static_cast<uint32_t>(std::strtoul(pg::word(w, i), nullptr, 0));
    ExtFamily f{};
    if (family_of_magic(magic, f))
      asked[f] = static_cast<uint32_t>(std::strtoul(pg::word(w, i + 1), nullptr, 0));
  }
  c.ext = agree(s.cfg.offered, asked);
  c.coords = coords_of(c.ext);
  c.need_pext = false;
  stuff(s, slot, "cmd new\n");
}

void cmd_list(ClassicServer& s, int slot, bool models, int start) {
  const ClassicClient& c = s.clients[slot];
  const std::vector<std::string>& names = models ? s.qc->models : s.qc->sounds;
  const bool dbl = c.ext.has(ExtFamily::x, models ? kXModelDbl : kXSoundDbl);
  const int limit =
      dbl ? static_cast<int>(names.size()) : std::min<int>(256, static_cast<int>(names.size()));
  if (start < 0 || start >= limit) start = limit - 1;
  if (models && start == 0 && !s.qc->vwep.empty() && c.ext.has(ExtFamily::z, kZVwep)) {
    std::string line = "//vwep";
    for (const std::string& v : s.qc->vwep) line += " " + v;
    stuff(s, slot, line + "\n");
  }
  SvcMsg m;
  m.id = start > 255 ? (models ? svc::modellistshort : svc::soundlistshort)
                     : (models ? svc::modellist : svc::soundlist);
  size_t bytes = 4;
  int last = start;
  while (last + 1 < limit) {
    const std::string& n = names[static_cast<size_t>(last) + 1];
    if (bytes + n.size() + 1 > kPage && (last & 0xff) != 0) break;
    bytes += n.size() + 1;
    m.strings.push_back(n);
    last++;
  }
  m.ints = {start, last + 1 < limit ? (last & 0xff) : 0};
  reliable_svc(s, slot, m);
}

void cmd_prespawn(ClassicServer& s, int slot, int n) {
  ClassicClient& c = s.clients[slot];
  const WireCtx ctx = ctx_of(c);
  const bool v2 = c.ext.has(ExtFamily::x, kXSpawnStatic2);
  const int statics = static_cast<int>(s.qc->statics.size());
  std::vector<const EntityState*> bases;
  for (const EntityState& b : s.baselines)
    if (b.modelindex != 0) bases.push_back(&b);
  const int nb = static_cast<int>(bases.size());
  const int total = statics + nb + 1 + static_cast<int>(s.signon.size());
  uint8_t buf[kMaxMsgLen];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  int i = n < 0 ? 0 : n;
  for (; i < total && w.size < kPage; i++) {
    SvcMsg m;
    if (i < statics) {
      const sv::StaticEntity& st = s.qc->statics[static_cast<size_t>(i)];
      if (v2) {
        m.id = svc::spawnstatic2;
        EntityRecord r;
        r.state.modelindex = st.modelindex;
        r.state.frame = static_cast<int>(st.frame);
        r.state.colormap = static_cast<int>(st.colormap);
        r.state.skin = static_cast<int>(st.skin);
        r.state.origin = st.origin;
        r.state.angles = st.angles;
        m.ents.push_back(r);
      } else {
        m.id = svc::spawnstatic;
        m.ints = {st.modelindex, static_cast<int>(st.frame), static_cast<int>(st.colormap),
                  static_cast<int>(st.skin)};
        m.floats = {st.origin.x, st.angles.x, st.origin.y, st.angles.y, st.origin.z, st.angles.z};
      }
      write_svc(w, m, ctx);
    } else if (i < statics + nb) {
      const EntityState& b = *bases[static_cast<size_t>(i - statics)];
      if (v2 || b.modelindex > 255) {
        m.id = svc::spawnbaseline2;
        m.ints = {b.number};
        EntityRecord r;
        r.state = b;
        m.ents.push_back(r);
      } else {
        m.id = svc::spawnbaseline;
        m.ints = {b.number, b.modelindex, b.frame, b.colormap, b.skin};
        m.floats = {b.origin.x, b.angles.x, b.origin.y, b.angles.y, b.origin.z, b.angles.z};
      }
      write_svc(w, m, ctx);
    } else if (i == statics + nb) {
      for (const ClassicServer::StaticSound& ss : s.static_sounds) {
        SvcMsg a;
        a.id = ss.sound > 255 ? svc::spawnstaticsound2 : svc::spawnstaticsound;
        a.floats = {ss.origin.x, ss.origin.y, ss.origin.z};
        a.ints = ss.sound > 255 ? std::vector<int32_t>{0, ss.sound, ss.volume, ss.attenuation}
                                : std::vector<int32_t>{ss.sound, ss.volume, ss.attenuation};
        write_svc(w, a, ctx);
      }
    } else {
      const std::vector<uint8_t>& b = s.signon[static_cast<size_t>(i - statics - nb - 1)];
      gn::write_bytes(w, b.data(), b.size());
    }
  }
  if (w.overflow) {
    classic_drop(s, slot, "a prespawn page overflowed");
    return;
  }
  c.statics_sent = statics;
  reliable_bytes(s, slot, buf, w.size);
  stuff(s, slot,
        i < total ? "cmd prespawn " + std::to_string(s.spawncount) + " " + std::to_string(i) + "\n"
                  : "cmd spawn " + std::to_string(s.spawncount) + " 0\n");
}

void cmd_spawn(ClassicServer& s, int slot, int n) {
  for (int j = n < 0 ? 0 : n; j < sh::kMaxClients; j++) {
    if (channel_reliable_backlog(s.clients[slot].ch) > 4 * kMaxMsgLen) {
      stuff(s, slot, "cmd spawn " + std::to_string(s.spawncount) + " " + std::to_string(j) + "\n");
      return;
    }
    full_client_update(s, j, slot);
  }
  for (int i = 0; i < sv::kLightStyles; i++) {
    SvcMsg m;
    m.id = svc::lightstyle;
    m.ints = {i};
    m.strings = {s.qc->lightstyles[i]};
    reliable_svc(s, slot, m);
  }
  for (int st = 11; st <= 14; st++) {
    SvcMsg m;
    m.id = svc::updatestatlong;
    m.ints = {st, 0};
    reliable_svc(s, slot, m);
  }
  stuff(s, slot, "skins\n");
}

void cmd_begin(ClassicServer& s, int slot) {
  ClassicClient& c = s.clients[slot];
  if (c.state == ConnState::spawned) return;
  c.state = ConnState::spawned;
  c.qc_active = true;
  sv::connect(*s.qc, slot, c.name, c.spectator, false);
  std::vector<std::pair<std::string, std::string>> pairs;
  info_pairs(c.userinfo, pairs);
  for (const auto& [k, v] : pairs)
    if (k != "name") sv::set_user_info(*s.qc, slot, k, v);
  if (s.qc->paused) {
    SvcMsg m;
    m.id = svc::setpause;
    m.ints = {1};
    reliable_svc(s, slot, m);
  }
  for (ServerFrame& f : c.frames) f = ServerFrame{};
  c.delta_byte = -1;
  s.log.push_back("begin " + std::to_string(slot) + " " + c.name);
}

void cmd_setinfo(ClassicServer& s, int slot, const std::string& key, const std::string& value) {
  ClassicClient& c = s.clients[slot];
  if (key.empty() || key[0] == '*' || value.find("&c") != std::string::npos ||
      value.find("&r") != std::string::npos || !info_set(c.userinfo, key, value)) {
    client_print(s, slot, 2, "Can't set " + key + ".\n");
    return;
  }
  std::string v = value;
  if (key == "name") {
    std::vector<std::string> taken;
    for (int i = 0; i < sh::kMaxClients; i++)
      if (i != slot && s.clients[i].state != ConnState::free) taken.push_back(s.clients[i].name);
    v = unique_name(sanitize_name(value), taken);
    c.name = v;
    info_set(c.userinfo, "name", v);
  }
  if (key == "rate" || key == "dupe")
    channel_set_rate(c.ch,
                     std::atoi(info_get(c.userinfo, "rate").c_str()) > 0
                         ? std::atoi(info_get(c.userinfo, "rate").c_str())
                         : 2500,
                     std::atoi(info_get(c.userinfo, "dupe").c_str()));
  if (key == "msg") c.msg_level = std::atoi(v.c_str());
  if (c.qc_active) sv::set_user_info(*s.qc, slot, key, v);
  if (!visible_key(key)) return;
  for (int j = 0; j < sh::kMaxClients; j++) {
    if (s.clients[j].state == ConnState::free) continue;
    SvcMsg m;
    m.id = svc::setinfo;
    m.ints = {slot};
    m.strings = {key, v};
    reliable_svc(s, j, m);
  }
}

// A path the download switches allow; false with the reason.
bool download_allowed(const ClassicServer& s, const std::string& path, std::string& why) {
  if (!s.cfg.allow_download) {
    why = "downloads are off";
    return false;
  }
  if (path.empty() || path[0] == '/' || path[0] == '.' || path.find("..") != std::string::npos ||
      path.find('/') == std::string::npos ||
      (path.size() > 4 && path.compare(path.size() - 4, 4, ".log") == 0)) {
    why = "not a file the server offers";
    return false;
  }
  auto under = [&](const char* dir) { return path.rfind(dir, 0) == 0; };
  const bool ok = under("skins/")   ? s.cfg.allow_skins
                  : under("progs/") ? s.cfg.allow_models
                  : under("sound/") ? s.cfg.allow_sounds
                  : under("maps/")  ? s.cfg.allow_maps
                                    : s.cfg.allow_other;
  if (!ok) why = "that kind of file is not offered";
  return ok;
}

// The classic transfer's next block on the reliable stream.
void download_block(ClassicServer& s, int slot) {
  ClassicClient& c = s.clients[slot];
  ServerDownload& d = c.dl;
  const std::vector<uint8_t>& bytes = s.cfg.files[static_cast<size_t>(d.file)].bytes;
  const int rate = c.ch.rate_s_per_byte > 0 ? static_cast<int>(1.0 / c.ch.rate_s_per_byte) : 25000;
  size_t block = static_cast<size_t>(std::max(10, std::min(1024, rate / 25)));
  if (block > bytes.size() - d.offset) block = bytes.size() - d.offset;
  SvcMsg m;
  m.id = svc::download;
  d.offset += block;
  const int percent = bytes.empty() ? 100 : static_cast<int>(d.offset * 100 / bytes.size());
  m.ints = {static_cast<int>(block), percent};
  m.data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(d.offset - block),
                bytes.begin() + static_cast<std::ptrdiff_t>(d.offset));
  uint8_t buf[kMaxMsgLen];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  WireCtx ctx = ctx_of(c);
  ctx.chunked = false;
  write_svc(w, m, ctx);
  reliable_bytes(s, slot, buf, w.size);
  d.blocks++;
  if (d.offset >= bytes.size()) d = ServerDownload{};
}

void cmd_download(ClassicServer& s, int slot, std::string path) {
  ClassicClient& c = s.clients[slot];
  for (char& ch : path) {
    if (ch == '\\') ch = '/';
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  }
  c.dl = ServerDownload{};
  std::string why;
  int file = -1;
  if (download_allowed(s, path, why)) {
    for (size_t i = 0; i < s.cfg.files.size(); i++)
      if (s.cfg.files[i].path == path) file = static_cast<int>(i);
    if (file < 0) why = "not found";
  }
  const bool chunked = c.ext.has(ExtFamily::x, kXChunkedDl);
  SvcMsg m;
  m.id = svc::download;
  if (chunked) {
    const int size = file < 0
                         ? (why == "not found" ? -1 : -2)
                         : static_cast<int>(s.cfg.files[static_cast<size_t>(file)].bytes.size());
    m.ints = {-1, size};
    m.strings = {path};
    reliable_svc(s, slot, m);
  } else if (file < 0) {
    m.ints = {-1, 0};
    WireCtx ctx = ctx_of(c);
    ctx.chunked = false;
    uint8_t buf[16];
    gn::Writer w = gn::make_writer(buf, sizeof buf);
    write_svc(w, m, ctx);
    reliable_bytes(s, slot, buf, w.size);
  }
  if (file < 0) {
    client_print(s, slot, 2, "Download of " + path + " refused: " + why + ".\n");
    return;
  }
  c.dl.active = true;
  c.dl.chunked = chunked;
  c.dl.file = file;
  if (!chunked) download_block(s, slot);
}

void cmd_nextdl(ClassicServer& s, int slot, const pg::Words& w) {
  ServerDownload& d = s.clients[slot].dl;
  if (!d.active) return;
  if (!d.chunked) {
    download_block(s, slot);
    return;
  }
  const int chunk = std::atoi(pg::word(w, 1));
  d.number = std::atoi(pg::word(w, 3));
  if (chunk < 0) {
    d = ServerDownload{};
    return;
  }
  const size_t size = s.cfg.files[static_cast<size_t>(d.file)].bytes.size();
  if (static_cast<size_t>(chunk) * kChunkBytes >= size || d.want.size() >= 256) return;
  for (int c : d.want)
    if (c == chunk) return;
  d.want.push_back(chunk);
}

}  // namespace

void full_client_update(ClassicServer& s, int about, int to) {
  const ClassicClient& c = s.clients[about];
  int frags = 0;
  if (c.qc_active)
    frags = static_cast<int>(
        pg::as_float(*pg::entity_field(s.qc->q.vm, about + 1, s.qc->q.layout->f.frags)));
  std::vector<SvcMsg> ms(5);
  ms[0].id = svc::updatefrags;
  ms[0].ints = {about, frags};
  ms[1].id = svc::updateping;
  ms[1].ints = {about, static_cast<int>(c.ping_s * 1000)};
  ms[2].id = svc::updatepl;
  ms[2].ints = {about, c.loss_pct};
  ms[3].id = svc::updateentertime;
  ms[3].ints = {about};
  ms[3].floats = {static_cast<float>(s.qc->sv.time - c.connect_time)};
  ms[4].id = svc::updateuserinfo;
  ms[4].ints = {about, c.state == ConnState::free ? 0 : c.userid};
  ms[4].strings = {c.state == ConnState::free ? std::string() : info_public(c.userinfo)};
  for (int j = 0; j < sh::kMaxClients; j++) {
    if (s.clients[j].state == ConnState::free || (to >= 0 && j != to)) continue;
    for (const SvcMsg& m : ms) reliable_svc(s, j, m);
  }
}

void string_command(ClassicServer& s, int slot, const std::string& line) {
  ClassicClient& c = s.clients[slot];
  pg::Words w;
  pg::tokenize_words(line.c_str(), w);
  const std::string cmd = pg::word(w, 0);
  const bool stale = w.count >= 2 && std::atoi(pg::word(w, 1)) != s.spawncount;
  auto signon_ok = [&] { return !stale && c.state == ConnState::connected; };
  if (cmd == "new") {
    if (c.state == ConnState::connected) cmd_new(s, slot);
  } else if (cmd == "pext") {
    if (c.state == ConnState::connected) cmd_pext(s, slot, w);
  } else if (cmd == "soundlist" || cmd == "modellist") {
    if (signon_ok()) cmd_list(s, slot, cmd == "modellist", std::atoi(pg::word(w, 2)));
  } else if (cmd == "prespawn") {
    if (signon_ok()) cmd_prespawn(s, slot, std::atoi(pg::word(w, 2)));
  } else if (cmd == "spawn") {
    if (signon_ok()) cmd_spawn(s, slot, std::atoi(pg::word(w, 2)));
  } else if (cmd == "begin") {
    if (signon_ok()) cmd_begin(s, slot);
  } else if (cmd == "drop") {
    classic_drop(s, slot, "left the server");
  } else if (cmd == "pings") {
    for (int j = 0; j < sh::kMaxClients; j++)
      if (s.clients[j].state != ConnState::free) {
        SvcMsg m;
        m.id = svc::updateping;
        m.ints = {j, static_cast<int>(s.clients[j].ping_s * 1000)};
        reliable_svc(s, slot, m);
        m.id = svc::updatepl;
        m.ints = {j, s.clients[j].loss_pct};
        reliable_svc(s, slot, m);
      }
  } else if (cmd == "setinfo") {
    if (w.count >= 3) cmd_setinfo(s, slot, pg::word(w, 1), pg::word(w, 2));
  } else if (cmd == "serverinfo") {
    std::vector<std::pair<std::string, std::string>> pairs;
    info_pairs(s.serverinfo, pairs);
    std::string t;
    for (const auto& [k, v] : pairs) {
      t += k;
      t += ' ';
      t += v;
      t += '\n';
    }
    client_print(s, slot, 2, t);
  } else if (cmd == "download") {
    cmd_download(s, slot, pg::word(w, 1));
  } else if (cmd == "nextdl") {
    cmd_nextdl(s, slot, w);
  } else if (cmd == "stopdownload" || cmd == "stopdl") {
    c.dl = ServerDownload{};
  } else if (cmd == "ptrack" || cmd == "snap" || cmd == "upload") {
    // Spectator tracking, screenshots and uploads: not built.
  } else if (c.qc_active) {
    sv::client_command(*s.qc, slot, line);
  }
}

void send_download_chunks(ClassicServer& s, int slot, gn::Writer& dgram) {
  ClassicClient& c = s.clients[slot];
  ServerDownload& d = c.dl;
  if (!d.active || !d.chunked) return;
  const std::vector<uint8_t>& bytes = s.cfg.files[static_cast<size_t>(d.file)].bytes;
  int sent = 0;
  while (!d.want.empty() && sent < s.cfg.chunks_per_frame) {
    const int chunk = d.want.front();
    d.want.erase(d.want.begin());
    SvcMsg m;
    m.id = svc::download;
    m.ints = {chunk};
    const size_t at = static_cast<size_t>(chunk) * kChunkBytes;
    const size_t n = std::min<size_t>(kChunkBytes, bytes.size() - at);
    m.data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(at),
                  bytes.begin() + static_cast<std::ptrdiff_t>(at + n));
    WireCtx ctx = ctx_of(c);
    ctx.chunked = true;
    if (sent == 0 && dgram.capacity - dgram.size > kChunkBytes + 8) {
      write_svc(dgram, m, ctx);
    } else {
      uint8_t buf[kChunkBytes + 32];
      gn::Writer w = gn::make_writer(buf, sizeof buf);
      gn::write_u32(w, 0xffffffffu);
      gn::write_bytes(w, "\\chunk", 6);
      write_long(w, d.number);
      gn::Writer body = gn::make_writer(buf + w.size, sizeof buf - w.size);
      write_svc(body, m, ctx);
      // The out-of-band form carries the body without its id byte.
      std::memmove(buf + w.size, buf + w.size + 1, body.size - 1);
      send_oob(s, c.address, buf, w.size + body.size - 1);
      s.counts.chunks_oob++;
    }
    d.blocks++;
    sent++;
  }
}

}  // namespace shooters_edge::net
