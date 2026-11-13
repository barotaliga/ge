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
#include <cstdlib>
#include <cstring>

#include "progs/layout.hpp"
#include "progs/text.hpp"
#include "progs/vm.hpp"
#include "shooters_edge/net/classic_admin.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_looks.hpp"
#include "shooters_edge/net/classic_server.hpp"
#include "shooters_edge/net/classic_talk.hpp"
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
  sv::RulesCommon& q = *s.game;
  SvcMsg m;
  m.id = svc::serverdata;
  for (ExtFamily f : {ExtFamily::x, ExtFamily::x2, ExtFamily::mvd})
    if (c.ext[f] != 0) m.ext.emplace_back(family_magic(f), c.ext[f]);
  const int number = slot | (c.spectator ? 0x80 : 0);
  m.ints = {kProtocolVersion, s.spawncount, number};
  const std::string& level = q.world.map_title;
  m.strings = {s.cfg.gamedir, !level.empty() ? level : q.map_name};
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
  asked[ExtFamily::ge] = c.ext[ExtFamily::ge];
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
  const std::vector<std::string>& names = models ? s.game->models : s.game->sounds;
  const bool dbl = c.ext.has(ExtFamily::x, models ? kXModelDbl : kXSoundDbl);
  const int limit =
      dbl ? static_cast<int>(names.size()) : std::min<int>(256, static_cast<int>(names.size()));
  if (start < 0 || start >= limit) start = limit - 1;
  if (models && start == 0 && !s.game->vwep.empty() && c.ext.has(ExtFamily::z, kZVwep)) {
    std::string line = "//vwep";
    for (const std::string& v : s.game->vwep) line += " " + v;
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
  const int statics = static_cast<int>(s.game->statics.size());
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
      const sv::StaticEntity& st = s.game->statics[static_cast<size_t>(i)];
      if (v2) {
        m.id = svc::spawnstatic2;
        EntityRecord r;
        r.state.modelindex = st.modelindex;
        r.state.frame = static_cast<int>(st.frame);
        r.state.colormap = static_cast<int>(st.colormap);
        r.state.skin = static_cast<int>(st.skin);
        r.state.origin = st.origin;
        r.state.angles = st.angles;
        const sv::WorldStatic look = sv::world_static(st);
        entity_look(r.state, look.alpha, 1, look.colormod);
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
    m.strings = {s.game->lightstyles[i]};
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
  c.rules_active = true;
  // A player whose ping is past the server's most (sv_maxping) watches instead.
  const int ping_ms = static_cast<int>(c.ping_s * 1000);
  if (!c.local && !c.spectator && s.cfg.max_ping_ms > 0 && c.ping_samples > 0 &&
      ping_ms > s.cfg.max_ping_ms) {
    c.spectator = true;
    info_set(c.userinfo, "*spectator", "1");
    client_print(s, slot, 2,
                 "Your ping of " + std::to_string(ping_ms) + " ms is over this server's most (" +
                     std::to_string(s.cfg.max_ping_ms) + " ms), so you join as a spectator.\n");
  }
  if (s.seat != nullptr)
    s.seat(s.seat_ctx, slot, c.name, c.spectator);
  else
    s.rules->connect(slot, c.name, c.spectator, false);
  std::vector<std::pair<std::string, std::string>> pairs;
  info_pairs(c.userinfo, pairs);
  for (const auto& [k, v] : pairs)
    if (k != "name") s.rules->user_info(slot, k, v);
  if (s.game->paused) {
    SvcMsg m;
    m.id = svc::setpause;
    m.ints = {1};
    reliable_svc(s, slot, m);
  }
  for (ServerFrame& f : c.frames) f = ServerFrame{};
  c.delta_byte = -1;
  s.log.push_back("begin " + std::to_string(slot) + " " + c.name);
  if (!c.aids_told) {  // a player who joins is told every aid granted (classic_admin.hpp)
    c.aids_told = true;
    classic_tell_aids(s, slot);
  }
}

// A window of changes that does not slide: one more change in it, true while there are at most
// `most` (a change after `seconds` opens a new window).
bool within(double& since, int& count, double now, double seconds, int most) {
  if (since < 0 || now - since > seconds) {
    since = now;
    count = 0;
  }
  return ++count <= most;
}

// The guards on a client's user info (never a local seat): true when the client was dropped.
bool info_guards(ClassicServer& s, int slot, const std::string& key, const std::string& value) {
  ClassicClient& c = s.clients[slot];
  if (c.local) return false;
  const double now = s.game->sv.time;
  std::string why;
  if (s.cfg.info_flood_count > 0 && s.cfg.info_flood_s > 0 &&
      !within(c.info_since, c.info_changes, now, s.cfg.info_flood_s, s.cfg.info_flood_count))
    why = "userinfo spam";
  else if (s.cfg.colour_kick && key == "topcolor" && value != info_get(c.userinfo, key) &&
           !within(c.colour_since, c.colour_changes, now, 8, 7))
    why = "topcolor spam";
  if (why.empty()) return false;
  const std::string name = c.name;
  client_print(s, slot, 2, "You were kicked from the game for " + why + "\n");
  classic_drop(s, slot, "was kicked for " + why);
  classic_print_all(s, name + " was kicked for " + why + "\n");
  return true;
}

void cmd_setinfo(ClassicServer& s, int slot, const std::string& key, const std::string& value) {
  ClassicClient& c = s.clients[slot];
  if (info_guards(s, slot, key, value)) return;
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
    v = unique_name(sanitize_name(value, s.cfg.default_name), taken);
    if (s.name_rule != nullptr) s.name_rule(s.name_ctx, slot, v, false);
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
  if (c.rules_active) s.rules->user_info(slot, key, v);
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
  // A player figure's folder: its pictures are skins, its figures models.
  const bool figure = under("players/") && (path.ends_with(".md2") || path.ends_with(".mdl"));
  // A map's coloured light and place names go with the map.
  const bool extra =
      (under("lits/") && path.ends_with(".lit")) || (under("locs/") && path.ends_with(".loc"));
  const bool ok = under("skins/") || (under("players/") && !figure) ? s.cfg.allow_skins
                  : under("progs/") || figure                       ? s.cfg.allow_models
                  : under("sound/")                                 ? s.cfg.allow_sounds
                  : under("maps/") || extra                         ? s.cfg.allow_maps
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
    std::vector<uint8_t> bytes;
    // A map's coloured light asked for in lits/ may sit beside the map instead.
    const bool lit = path.starts_with("lits/") && path.ends_with(".lit");
    if (file < 0 && s.cfg.read_file &&
        ((s.cfg.read_file(path, &bytes) && !bytes.empty()) ||
         (lit && s.cfg.read_file("maps/" + path.substr(5), &bytes) && !bytes.empty()))) {
      s.cfg.files.push_back({path, std::move(bytes)});  // kept for the next who asks
      file = static_cast<int>(s.cfg.files.size() - 1);
    }
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
  ClassicClient& c = s.clients[about];
  int frags = 0;
  if (c.rules_active) frags = static_cast<int>(s.game->world.clients[about].numbers.frags);
  c.frags_told = frags;
  std::vector<SvcMsg> ms(5);
  ms[0].id = svc::updatefrags;
  ms[0].ints = {about, frags};
  ms[1].id = svc::updateping;
  ms[1].ints = {about, static_cast<int>(c.ping_s * 1000)};
  ms[2].id = svc::updatepl;
  ms[2].ints = {about, c.loss_pct};
  ms[3].id = svc::updateentertime;
  ms[3].ints = {about};
  ms[3].floats = {static_cast<float>(s.game->sv.time - c.connect_time)};
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
  } else if (cmd == "minping") {  // a player's own floor on everyone's ping (sv_minpingcmd)
    if (w.count < 2) {
      client_print(s, slot, 2,
                   "The minimum ping is " + std::to_string(s.cfg.min_ping_ms) + " ms.\n");
    } else if (!s.cfg.min_ping_cmd) {
      client_print(s, slot, 2, "This server doesn't let players change the minimum ping.\n");
    } else if (s.match_on) {
      client_print(s, slot, 2, "The minimum ping can't change while a match is on.\n");
    } else {
      char* end = nullptr;
      const long v = std::strtol(pg::word(w, 1), &end, 10);
      if (end == pg::word(w, 1) || *end != '\0' || v < 0 || v > 300) {
        client_print(s, slot, 2, "The minimum ping must be 0 to 300 ms.\n");
      } else {
        s.cfg.min_ping_ms = static_cast<int>(v);
        client_print(s, slot, 2, "Minimum ping set to " + std::to_string(v) + " ms.\n");
      }
    }
  } else if (cmd == "users") {
    client_print(s, slot, 2, classic_users(s));  // who is on, by user id (classic_admin.hpp)
  } else if (cmd == "listskins") {  // the skins and player figures offered (classic_looks.hpp)
    std::vector<std::string> names;
    if (s.cfg.list_files) names = s.cfg.list_files();
    for (const ServedFile& f : s.cfg.files) names.push_back(f.path);
    client_print(s, slot, 2, looks_offered(names, s.cfg.allow_download));
  } else if (cmd == "ptrack" || cmd == "snap" || cmd == "upload") {
    // Spectator tracking, screenshots and uploads: not built.
  } else {
    // A chat line held to the flood rule first (a dropped one tells only its sender why).
    // A `login` line counts too (classic_login.hpp): it is a chat line a client sent as a command.
    const bool chat = cmd == "say" || cmd == "say_team" || cmd == "say_near";
    const bool voice =
        cmd == "voicetarg" || cmd == "unmuteall" || cmd == "muteall" || cmd == "vignore";
    if (voice) {  // the voice's target and mutes (classic_talk.hpp): the server's own
      classic_talk_command(s, slot, w);
      return;
    }
    if ((chat || cmd == "login") && !c.local) {
      const gs::FloodVerdict v = gs::flood_check(c.flood, s.cfg.flood, s.game->sv.time);
      if (v != gs::FloodVerdict::allowed) {
        client_print(s, slot, 3,
                     gs::flood_text(v, c.flood, s.cfg.flood, s.cfg.flood_msg, s.game->sv.time));
        s.counts.chat_flooded++;
        return;
      }
    }
    // A public line's carriage returns could make it pass for another player's (sv_unfake).
    std::string said = line;
    if (cmd == "say" && s.cfg.unfake)
      for (char& ch : said)
        if (ch == '\r') ch = '#';
    // The game's own words (a lobby's ready, team, start) first, then the rules'. Nearby chat
    // is the server's own (no rules know of it); a say the rules took nothing of (the sequel's
    // have no chat) is routed by the server's own chat.
    const bool taken = s.game_command != nullptr && s.game_command(s.game_ctx, slot, said);
    if (taken) return;
    if (cmd == "say_near") {
      classic_talk_command(s, slot, w);
      return;
    }
    if (c.rules_active && s.rules->client_command(slot, said) == sv::CommandResult::bad && chat)
      classic_server_say(s, slot, cmd == "say_team", said);
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
