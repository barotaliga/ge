// The scripted QuakeWorld client (qw_client.hpp): the connectionless steps, the signon as the
// server stuffs it, downloads in both forms, the frames and the moves.
//
// ge:summary The scripted QuakeWorld client's steps, message handling, downloads and moves
// ge:keywords client_tick client_start client_command default_script stufftext skins download
// ge:skill net shooter
// ge:test tests/shooters_edge_qw_server_test.cpp
#include "shooters_edge/net/qw_client.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "classic/rulesets.hpp"
#include "net/voice_trial.hpp"
#include "progs/text.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/qw_mapsum.hpp"
#include "shooters_edge/net/qw_record.hpp"

namespace shooters_edge::net {

namespace cl = shooters_edge::client;

namespace pg = ge::progs;

namespace wd = ge::classic::content;

namespace {

constexpr double kRetry = 1.0;       // s between connectionless retries
constexpr double kChunkRetry = 0.5;  // s before a chunk is asked for again
constexpr int kChunksPerTick = 30;   // chunks asked for in one packet (a stock client's default)
constexpr size_t kChunkAsksBytes = 1024;  // the most a packet's chunk asks fill
constexpr size_t kChunkOobMin = 100;      // an out-of-band chunk is longer (1044 bytes)

std::string hex32(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "0x%x", v);
  return b;
}

void send_raw(QwClientEnd& c, const uint8_t* data, size_t size) {
  gn::transport_send(*c.transport, c.server, data, size, static_cast<int64_t>(c.now * 1e6));
  c.bytes_out += static_cast<int64_t>(size);
}

void send_oob_text(QwClientEnd& c, const std::string& text) {
  std::vector<uint8_t> b = {0xff, 0xff, 0xff, 0xff};
  b.insert(b.end(), text.begin(), text.end());
  b.push_back(0);
  send_raw(c, b.data(), b.size());
}

std::string userinfo_of(const QwClientEnd& c) {
  std::string u;
  info_set(u, "name", c.cfg.name);
  info_set(u, "skin", c.cfg.skin);
  if (!c.cfg.model.empty()) info_set(u, "model", c.cfg.model);
  info_set(u, "topcolor", std::to_string(c.cfg.topcolor));
  info_set(u, "bottomcolor", std::to_string(c.cfg.bottomcolor));
  info_set(u, "rate", std::to_string(c.cfg.rate));
  info_set(u, "msg", std::to_string(c.cfg.msg));
  if (c.cfg.noaim) info_set(u, "noaim", "1");
  if (!c.cfg.team.empty()) info_set(u, "team", c.cfg.team);
  if (!c.cfg.w_switch.empty()) info_set(u, "w_switch", c.cfg.w_switch);
  if (!c.cfg.b_switch.empty()) info_set(u, "b_switch", c.cfg.b_switch);
  if (c.cfg.spectator) info_set(u, "spectator", "1");
  if (c.cfg.speedjump)
    info_set(u, "cl_speedjump", "1");  // a server that does not know it ignores it
  if (!c.cfg.password.empty()) info_set(u, "password", c.cfg.password);
  if (!c.cfg.prx.empty()) info_set(u, "prx", c.cfg.prx);
  if (!c.cfg.ruleset.empty() && c.cfg.ruleset != "default")
    info_set(u, std::string(ge::classic::ruleset_info_key(false)), c.cfg.ruleset);
  for (const auto& [k, v] : c.cfg.extra_info)
    if (info_get(u, k).empty()) info_set(u, k, v);
  if (c.cfg.ask[ExtFamily::z]) info_set(u, "*z_ext", std::to_string(c.cfg.ask[ExtFamily::z]));
  if (c.cfg.ask[ExtFamily::ge]) info_set(u, "*ge_ext", std::to_string(c.cfg.ask[ExtFamily::ge]));
  return u;
}

void send_connect(QwClientEnd& c) {
  std::string t = "connect " + std::to_string(kProtocolVersion) + " " +
                  std::to_string(c.cfg.qport) + " " + std::to_string(c.challenge) + " \"" +
                  userinfo_of(c) + "\"\n";
  if (c.cfg.mode == ClientExtMode::lines)
    for (ExtFamily f : {ExtFamily::x, ExtFamily::x2, ExtFamily::mvd}) {
      const uint32_t bits = c.cfg.ask[f] & c.offered[f];
      if (bits) t += hex32(family_magic(f)) + " " + hex32(bits) + "\n";
    }
  send_oob_text(c, t);
}

bool has_file(const QwClientEnd& c, const std::string& path) {
  for (const std::string& f : c.cfg.files)
    if (f == path) return true;
  if (c.downloaded.count(path) != 0) return true;
  return c.cfg.read_file && !c.cfg.read_file(c.gamedir, path).empty();
}

// A name a player chose that may stand in a path: not empty, no climbing, no drive or backslash;
// a folder's (`model`) no '/' either, a skin's may hold one.
bool plain_name(const std::string& n, bool slash = false) {
  return !n.empty() && n[0] != '.' && n.find("..") == std::string::npos &&
         n.find_first_of(slash ? "\\:" : "/\\:") == std::string::npos;
}

// The game's settings for the web downloads: Quake's kinds of file, its base folder, its scheme;
// our own server is never told `download http`.
wd::WebConfig web_config(const QwClientEnd& c) {
  wd::WebConfig w = c.cfg.web;
  w.gamedir = c.gamedir == "qw" ? std::string() : c.gamedir;
  w.base_folder = "qw";
  w.referer_scheme = "quake";
  w.kinds = wd::quake_web_kinds();
  return w;
}

void next_skin_or_begin(QwClientEnd& c) {
  if (c.web.on) {  // from the web first: `begin` held until it is done
    const wd::WebConfig w = web_config(c);
    for (const std::string& path : std::exchange(c.wanted_skins, {}))
      if (!has_file(c, path)) wd::web_queue(c.web, w, path);
    if (wd::web_busy(c.web)) return;
  }
  while (!c.wanted_skins.empty()) {
    const std::string path = c.wanted_skins.front();
    c.wanted_skins.erase(c.wanted_skins.begin());
    if (has_file(c, path)) continue;
    // A Quake figure asked for only when the server had no second-game one.
    if (path.ends_with("/tris.mdl") && has_file(c, path.substr(0, path.size() - 4) + ".md2"))
      continue;
    c.dl = ClientDownload{};
    c.dl.name = path;
    c.dl.active = true;
    c.dl.chunked = c.ext.has(ExtFamily::x, kXChunkedDl);
    c.dl.number = ++c.downloads_started;
    client_command(c, "download " + path);
    return;
  }
  if (!c.begun) {
    c.begun = true;
    client_command(c, "begin " + std::to_string(c.spawncount));
    // Voice heard from here on, sent to the target asked for (net/qw_talk.hpp).
    if (c.cfg.voice && c.ext.has(ExtFamily::x2, kX2Voice)) {
      client_command(c, "unmuteall");
      const std::string want = c.voice_to.empty() ? c.cfg.voice_to : c.voice_to;
      c.voice_to.clear();
      if (!want.empty()) qw_voice_target(c, want);
    }
  }
}

void start_skins(QwClientEnd& c) {
  c.wanted_skins.clear();
  const auto want = [&c](const std::string& path) {
    for (const std::string& w : c.wanted_skins)
      if (w == path) return;
    if (!has_file(c, path)) c.wanted_skins.push_back(path);
  };
  for (const std::string& u : c.userinfos) {
    if (u.empty() || !c.cfg.fetch_looks) continue;
    std::string skin = info_get(u, "skin"), model = info_get(u, "model");
    for (std::string* n : {&skin, &model})
      for (char& ch : *n) ch = ch >= 'A' && ch <= 'Z' ? char(ch - 'A' + 'a') : ch;
    if (skin.size() > 4 && skin.ends_with(".pcx")) skin.resize(skin.size() - 4);
    if (plain_name(skin, true)) want("skins/" + skin + ".pcx");
    if (!plain_name(model)) continue;
    const std::string folder = "players/" + model + "/";
    want(folder + "tris.md2");
    want(folder + "tris.mdl");
    if (plain_name(skin)) want(folder + skin + ".pcx");
  }
  // The map's extras: its name from the model list's first map ("maps/dm4.bsp").
  const std::string& world = c.models.size() >= 2 ? c.models[1] : std::string();
  if (world.starts_with("maps/") && world.ends_with(".bsp")) {
    const std::string map = world.substr(5, world.size() - 9);
    if (plain_name(map)) {
      if (c.cfg.fetch_lit && !has_file(c, "maps/" + map + ".lit") &&
          !has_file(c, "maps/lits/" + map + ".lit"))
        want("lits/" + map + ".lit");
      if (c.cfg.fetch_locs) want("locs/" + map + ".loc");
    }
  }
  next_skin_or_begin(c);
}

// The web downloads moved on: each file fetched kept as one over the channel; once none is left
// (or the web gave its files back), the channel's turn.
uint32_t version_sum(const std::string& map, const std::vector<uint8_t>& bsp);
void next_content_or_prespawn(QwClientEnd& c);

void step_web(QwClientEnd& c) {
  wd::WebDone done;
  wd::web_step(
      c.web, web_config(c), c.now,
      [&c](const std::string& path, bool) { return !has_file(c, path); }, done);
  const std::string& world = c.models.size() >= 2 ? c.models[1] : std::string();
  for (auto& [path, bytes] : done.files) {
    if (c.web_version && path == world) {  // the server's version of a map the player has
      c.web_version = false;
      const uint32_t sum = version_sum(path, bytes);
      const std::string kept =
          c.cfg.keep_version ? c.cfg.keep_version(path, sum, bytes) : std::string();
      c.downloaded[kept.empty() ? path + "#" + std::to_string(sum) : kept] = std::move(bytes);
      c.map_version = kept;
      c.map_sum = sum;
      continue;
    }
    if (c.prespawn_held && path == world) c.map_sum = version_sum(path, bytes);  // one it lacked
    if (c.cfg.keep_file) c.cfg.keep_file(c.gamedir, path, bytes, nullptr);
    c.downloaded[path] = std::move(bytes);
  }
  // What the web gave back goes over the channel: the map's version asked there again.
  for (const std::string& path : done.back) {
    if (c.web_version && path == world) {
      c.web_version = false;
      c.fetch_version = true;
    } else if (c.prespawn_held) {
      c.wanted_content.push_back(path);
    } else {
      c.wanted_skins.insert(c.wanted_skins.begin(), path);
    }
  }
  if (wd::web_busy(c.web) || c.dl.active) return;
  if (c.prespawn_held)
    next_content_or_prespawn(c);
  else
    next_skin_or_begin(c);
}

void finish_download(QwClientEnd& c, bool ok) {
  if (c.fetch_version && c.dl.name == c.models[1]) {  // the server's version of the map
    c.fetch_version = false;
    if (ok) {
      c.dl.bytes.resize(static_cast<size_t>(c.dl.size));
      const uint32_t sum = version_sum(c.dl.name, c.dl.bytes);
      const std::string path =
          c.cfg.keep_version ? c.cfg.keep_version(c.dl.name, sum, c.dl.bytes) : std::string();
      const std::string kept = path.empty() ? c.dl.name + "#" + std::to_string(sum) : path;
      c.downloaded[kept] = c.dl.bytes;
      if (c.dl.chunked) client_command(c, "nextdl -1 100 " + std::to_string(c.dl.number));
      c.map_version = path;
      c.map_sum = sum;
    } else {
      c.version_unfetched = true;
    }
    c.dl = ClientDownload{};
    next_content_or_prespawn(c);
    return;
  }
  const bool the_map = ok && c.prespawn_held && c.models.size() >= 2 && c.dl.name == c.models[1];
  if (ok) {
    c.dl.bytes.resize(static_cast<size_t>(c.dl.size));
    if (the_map) c.map_sum = version_sum(c.dl.name, c.dl.bytes);  // a map the player lacked
    c.downloaded[c.dl.name] = c.dl.bytes;
    if (c.cfg.keep_file)
      c.cfg.keep_file(c.gamedir, c.dl.name, c.dl.bytes, c.dl.has_origin ? &c.dl.origin : nullptr);
    if (c.dl.chunked) client_command(c, "nextdl -1 100 " + std::to_string(c.dl.number));
  }
  c.dl = ClientDownload{};
  if (c.prespawn_held)
    next_content_or_prespawn(c);
  else
    next_skin_or_begin(c);
}

void download_chunk(QwClientEnd& c, int chunk, const uint8_t* data) {
  if (!c.dl.active || !c.dl.chunked || c.dl.size < 0) return;
  const size_t n = c.dl.have.size();
  if (chunk < 0 || static_cast<size_t>(chunk) >= n || c.dl.have[static_cast<size_t>(chunk)]) return;
  std::memcpy(c.dl.bytes.data() + static_cast<size_t>(chunk) * kChunkBytes, data, kChunkBytes);
  c.dl.have[static_cast<size_t>(chunk)] = true;
  for (bool h : c.dl.have)
    if (!h) return;
  finish_download(c, true);
}

void on_download(QwClientEnd& c, const SvcMsg& m) {
  if (!c.dl.active) return;
  if (c.dl.chunked) {
    if (m.ints[0] == -1) {
      c.dl.size = m.ints.size() > 1 ? m.ints[1] : -1;
      if (c.dl.size < 0) {
        finish_download(c, false);
        return;
      }
      const size_t chunks = (static_cast<size_t>(c.dl.size) + kChunkBytes - 1) / kChunkBytes;
      c.dl.bytes.assign(chunks * kChunkBytes, 0);
      c.dl.have.assign(chunks, false);
      c.dl.asked.assign(chunks, -1);
      if (chunks == 0) finish_download(c, true);
    } else if (m.data.size() == static_cast<size_t>(kChunkBytes)) {
      download_chunk(c, m.ints[0], m.data.data());
    }
    return;
  }
  const int size = m.ints[0];
  if (size < 0) {
    finish_download(c, false);
    return;
  }
  c.dl.bytes.insert(c.dl.bytes.end(), m.data.begin(), m.data.end());
  c.dl.percent = m.ints[1];
  if (m.ints[1] >= 100) {
    c.dl.size = static_cast<int>(c.dl.bytes.size());
    finish_download(c, true);
  } else {
    client_command(c, "nextdl");
  }
}

// The next block of the screenshot the server asked for, with the share done.
void send_upload_block(QwClientEnd& c) {
  ClientUpload& u = c.up;
  if (!u.sending) return;
  const size_t n = std::min(kUploadBlock, u.data.size() - u.at);
  ClcMsg m;
  m.id = clc::upload;
  m.size = static_cast<int>(n);
  m.data.assign(u.data.begin() + static_cast<std::ptrdiff_t>(u.at),
                u.data.begin() + static_cast<std::ptrdiff_t>(u.at + n));
  u.at += n;
  m.percent = static_cast<int>(u.at * 100 / u.data.size());
  uint8_t buf[kUploadBlock + 16];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  write_clc(w, m);
  channel_reliable(c.ch, buf, w.size);
  if (u.at < u.data.size()) return;
  u.said.emplace_back("Upload completed");
  u.sending = false;
  u.data.clear();
}

void on_stufftext(QwClientEnd& c, const std::string& text) {
  c.stufftexts++;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(at, end - at);
    at = end + 1;
    if (c.stuffed.size() < kStuffedKept) c.stuffed.push_back(line);
    pg::Words w;
    pg::tokenize_words(line.c_str(), w);
    const std::string cmd = pg::word(w, 0);
    if (cmd == "cmd") {
      const std::string rest = line.substr(line.find("cmd") + 4);
      if (rest == "snap") {
        // A screenshot asked for: nothing while one is on its way; scr_allowsnap 0 says no.
        if (!c.up.asked && !c.up.sending) {
          if (c.cfg.allow_snap)
            c.up.asked = true;
          else
            client_command(c, "snap");
        }
      } else if (rest == "pext") {
        std::string ans = "pext";
        if (c.cfg.mode == ClientExtMode::pext)
          for (ExtFamily f : {ExtFamily::x, ExtFamily::x2, ExtFamily::mvd})
            if (c.cfg.ask[f]) ans += " " + hex32(family_magic(f)) + " " + hex32(c.cfg.ask[f]);
        client_command(c, ans);
      } else {
        client_command(c, rest);
      }
    } else if (cmd == "nextul") {
      send_upload_block(c);
    } else if (cmd == "stopul") {
      c.up.sending = false;
      c.up.data.clear();
    } else if (cmd == "packet" && w.count >= 3) {
      // A connectionless line the server asks to have sent back (a check of the address it
      // reaches us from): sent to the address named, or to the server when the name does not read.
      gn::Address to = c.server;
      if (!gn::parse_address(pg::word(w, 1), to)) to = c.server;
      std::vector<uint8_t> b = {0xff, 0xff, 0xff, 0xff};
      const std::string line_out = pg::word(w, 2);
      b.insert(b.end(), line_out.begin(), line_out.end());
      gn::transport_send(*c.transport, to, b.data(), b.size(), static_cast<int64_t>(c.now * 1e6));
      c.bytes_out += static_cast<int64_t>(b.size());
    } else if (cmd == "fullserverinfo") {
      c.serverinfo = pg::word(w, 1);
      // The web address files are fetched from: the server's, else the player's own.
      // A server that names its address the other engines' way (`sv_dlURL`) is read too.
      std::string dl = info_get(c.serverinfo, "*dlserver");
      if (dl.empty()) dl = info_get(c.serverinfo, "sv_dlURL");
      wd::web_connect(c.web, web_config(c), dl, c.cfg.fetch_looks ? 1 : 0, gn::to_string(c.server),
                      info_get(c.serverinfo, "*dlpin"));
      const std::string z = info_get(c.serverinfo, "*z_ext");
      c.ext[ExtFamily::z] = c.cfg.ask[ExtFamily::z] & static_cast<uint32_t>(std::atoi(z.c_str()));
      const std::string ge = info_get(c.serverinfo, "*ge_ext");
      c.ext[ExtFamily::ge] =
          c.cfg.ask[ExtFamily::ge] & static_cast<uint32_t>(std::atoi(ge.c_str()));
    } else if (cmd == "skins") {
      start_skins(c);
    } else if (cmd == "color" && w.count >= 2) {
      // The player's colours, as a mod sets them (a team's): the user info and the server told.
      c.cfg.topcolor = std::atoi(pg::word(w, 1));
      c.cfg.bottomcolor = w.count >= 3 ? std::atoi(pg::word(w, 2)) : c.cfg.topcolor;
      client_command(c, "setinfo topcolor " + std::to_string(c.cfg.topcolor));
      client_command(c, "setinfo bottomcolor " + std::to_string(c.cfg.bottomcolor));
    } else if ((cmd == "team" || cmd == "skin" || cmd == "name" || cmd == "topcolor" ||
                cmd == "bottomcolor") &&
               w.count >= 2) {
      const std::string value = pg::word(w, 1);
      if (cmd == "team") c.cfg.team = value;
      if (cmd == "skin") c.cfg.skin = value;
      if (cmd == "name") c.cfg.name = value;
      if (cmd == "topcolor") c.cfg.topcolor = std::atoi(value.c_str());
      if (cmd == "bottomcolor") c.cfg.bottomcolor = std::atoi(value.c_str());
      std::string ask = "setinfo ";
      ask += cmd;
      ask += " \"";
      ask += value;
      ask += '"';
      client_command(c, ask);
    } else if (cmd == "setinfo" && w.count >= 3) {
      std::string ask = "setinfo ";
      ask += pg::word(w, 1);
      ask += " \"";
      ask += pg::word(w, 2);
      ask += '"';
      client_command(c, ask);
    } else if (cmd == "connect" && w.count >= 2) {  // sent elsewhere (sv_redirect)
      c.sent_on = pg::word(w, 1);
    } else if (cmd == "reconnect" || cmd == "changing") {
      // The server changed the map: the signon again, to `begin` on the new one.
      if (cmd == "reconnect") {
        c.begun = false;
        if (c.stage == ClientStage::active) c.stage = ClientStage::signon;
        client_command(c, "new");
      }
    } else if (!cmd.empty() && cmd != "bf" && !line.starts_with("//") &&
               c.to_run.size() < kStuffedKept) {
      c.to_run.push_back(line);  // the rest runs in the window's console, as the server's
    }
  }
}

std::vector<uint8_t> game_file(const QwClientEnd& c, const std::string& path);

// The map's second checksum as the first prespawn sends it (signed, as the server prints it).
int32_t map_checksum(const QwClientEnd& c) {
  if (c.models.size() < 2) return 0;
  return static_cast<int32_t>(c.map_sum);
}

// The checksum a version of the map sends (0: not a map that reads).
uint32_t version_sum(const std::string& map, const std::vector<uint8_t>& bsp) {
  const MapChecksums sums = map_checksums(bsp);
  return sums.ok ? sent_checksum(map, sums) : 0;
}

// A server's reason for a drop in a player's words: a map whose checksum the server refused (its
// copy and the player's are other versions of it, as a later release's maps are) said plainly. It
// is the last word only when no other version was left to join with (map_versions.hpp).
std::string refusal_words(const std::string& why) {
  const size_t at = why.find("does not match (maps/");
  if (at == std::string::npos) return why;
  const size_t from = at + 21, end = why.find(".bsp", from);
  const std::string map = why.substr(from, end == std::string::npos ? 0 : end - from);
  return "the server plays another version of " + (map.empty() ? std::string("its map") : map) +
         " than yours and does not hand its version out, so it would not let you in";
}

// A file of the game's, read from its files or fetched this join (empty: neither).
std::vector<uint8_t> game_file(const QwClientEnd& c, const std::string& path) {
  if (const auto it = c.downloaded.find(path); it != c.downloaded.end()) return it->second;
  return c.cfg.read_file ? c.cfg.read_file(c.gamedir, path) : std::vector<uint8_t>{};
}

// The model checks (the player's and the eyes' whole files, CRC-16), then the map's checksum on
// the first `prespawn`.
void send_prespawn(QwClientEnd& c) {
  if (c.cfg.use_map && c.models.size() >= 2) c.cfg.use_map(c.models[1], c.map_version);
  for (const auto& [key, path] :
       {std::pair{"pmodel", "progs/player.mdl"}, std::pair{"emodel", "progs/eyes.mdl"}}) {
    const std::vector<uint8_t> mdl = game_file(c, path);
    if (!mdl.empty())
      client_command(
          c, std::string("setinfo ") + key + " " + std::to_string(crc16(mdl.data(), mdl.size())));
  }
  client_command(
      c, "prespawn " + std::to_string(c.spawncount) + " 0 " + std::to_string(map_checksum(c)));
}

// The next of the content lacked asked for, or, with none left, `prespawn`.
void next_content_or_prespawn(QwClientEnd& c) {
  // From the server's web address first, the map and the rest at once: the channel waits until
  // the web is done and takes what it gave back.
  if (c.web.on && !c.dl.active) {
    const wd::WebConfig w = web_config(c);
    if (c.fetch_version && !c.web.asked.contains(c.models[1])) {
      c.fetch_version = false;
      c.web_version = true;
      wd::web_queue(c.web, w, c.models[1]);
    }
    // A path the web was asked for already came back from it: the channel's now.
    std::vector<std::string> channel;
    for (const std::string& path : c.wanted_content) {
      if (has_file(c, path)) continue;
      if (c.web.asked.contains(path))
        channel.push_back(path);
      else
        wd::web_queue(c.web, w, path);
    }
    c.wanted_content = std::move(channel);
    if (wd::web_busy(c.web)) return;
  }
  if (c.fetch_version && !c.dl.active) {  // the server's version of the map, before the rest
    c.dl = ClientDownload{};
    c.dl.name = c.models[1];
    c.dl.active = true;
    c.dl.chunked = c.ext.has(ExtFamily::x, kXChunkedDl);
    c.dl.number = ++c.downloads_started;
    client_command(c, "download " + c.dl.name);
    return;
  }
  while (!c.wanted_content.empty()) {
    const std::string path = c.wanted_content.front();
    c.wanted_content.erase(c.wanted_content.begin());
    if (has_file(c, path)) continue;
    c.dl = ClientDownload{};
    c.dl.name = path;
    c.dl.active = true;
    c.dl.chunked = c.ext.has(ExtFamily::x, kXChunkedDl);
    c.dl.number = ++c.downloads_started;
    client_command(c, "download " + path);
    return;
  }
  c.prespawn_held = false;
  send_prespawn(c);
}

// The version of the map the first `prespawn` sends (ClientConfig::map_versions): the one this
// server took last, else the player's own copy, else the next kept one, none whose checksum the
// server refused; with none left, the server's own is wanted first (`fetch_version`).
void choose_map_version(QwClientEnd& c) {
  c.map_version.clear();
  c.map_sum = 0;
  c.fetch_version = false;
  if (c.models.size() < 2 || c.models[1].empty()) return;
  const std::string& map = c.models[1];
  const auto refused = [&c](uint32_t sum) {
    return std::find(c.cfg.refused.begin(), c.cfg.refused.end(), sum) != c.cfg.refused.end();
  };
  std::vector<std::pair<std::string, uint32_t>> have;  // path ("" own), checksum
  if (const std::vector<uint8_t> own = game_file(c, map); !own.empty())
    have.emplace_back("", version_sum(map, own));
  if (c.cfg.map_versions)
    for (const auto& [path, bytes] : c.cfg.map_versions(map))
      have.emplace_back(path, version_sum(map, bytes));
  const std::pair<std::string, uint32_t>* pick = nullptr;
  const std::optional<uint32_t> last =
      c.cfg.remembered ? c.cfg.remembered(map) : std::optional<uint32_t>();
  for (const auto& h : have)
    if (last && h.second == *last && !refused(h.second)) pick = &h;
  for (const auto& h : have)
    if (pick == nullptr && !refused(h.second)) pick = &h;
  if (pick != nullptr) {
    c.map_version = pick->first;
    c.map_sum = pick->second;
    return;
  }
  // None the server takes (or none at all): its own fetched, kept as a version when the player
  // has a copy of his own.
  c.fetch_version = !have.empty();
  if (!have.empty()) c.map_sum = have.front().second;
}

// The lists complete: the map first, then every model and sound the game lacks, then `prespawn`.
void start_content(QwClientEnd& c) {
  c.wanted_content.clear();
  c.prespawn_held = true;
  choose_map_version(c);
  if (c.cfg.fetch_content && c.cfg.read_file) {  // a client with no files cannot tell
    const auto want = [&c](const std::string& path) {
      if (!plain_name(path, true) || has_file(c, path)) return;
      for (const std::string& w : c.wanted_content)
        if (w == path) return;
      c.wanted_content.push_back(path);
    };
    for (size_t i = 1; i < c.models.size(); i++)
      if (!c.models[i].empty() && c.models[i][0] != '*') want(c.models[i]);
    for (size_t i = 1; i < c.sounds.size(); i++)
      if (!c.sounds[i].empty()) want("sound/" + c.sounds[i]);
  }
  next_content_or_prespawn(c);
}

void on_list(QwClientEnd& c, const SvcMsg& m, bool models) {
  std::vector<std::string>& list = models ? c.models : c.sounds;
  int& last = models ? c.model_last : c.sound_last;
  const int start = m.ints[0];
  if (list.empty()) list.push_back("");
  for (size_t i = 0; i < m.strings.size(); i++) {
    const size_t slot = static_cast<size_t>(start) + 1 + i;
    if (list.size() <= slot) list.resize(slot + 1);
    list[slot] = m.strings[i];
  }
  last = start + static_cast<int>(m.strings.size());
  const int next = m.ints[1];
  const std::string sc = std::to_string(c.spawncount);
  if (next != 0) {
    client_command(c, std::string(models ? "modellist " : "soundlist ") + sc + " " +
                          std::to_string((last & 0xff00) + next));
  } else if (!models) {
    client_command(c, "modellist " + sc + " 0");
  } else {
    start_content(c);
  }
}

void on_entities(QwClientEnd& c, gn::Reader& r, bool delta) {
  const uint32_t seq = c.ch.incoming_sequence;
  const std::vector<EntityState>* from = nullptr;
  bool ok = true;
  if (delta) {
    const auto b = static_cast<uint32_t>(read_byte(r));
    const uint32_t full = seq - ((seq - b) & 0xffu);
    const ClientFrame& f = c.frames[full & 63];
    if (f.valid && f.sequence == full)
      from = &f.ents;
    else
      ok = false;
  }
  std::vector<EntityState> out;
  const std::vector<EntityState> none;
  if (!read_packet_entities(r, from, c.baselines, c.ext, out)) {
    c.error = "a damaged entity list";
    return;
  }
  ClientFrame& f = c.frames[seq & 63];
  f.sequence = seq;
  f.valid = ok;
  f.ents = std::move(out);
  if (ok) {
    c.valid_frame = static_cast<int>(seq);
    c.entities_last = static_cast<int64_t>(f.ents.size());
    (delta ? c.delta_frames : c.full_frames)++;
  } else {
    c.delta_misses++;
    c.valid_frame = -1;
  }
}

sh::MoveType move_type_of(const PlayerState& p) {
  switch (p.pm) {
    case PmType::spectator:
      return sh::MoveType::spectator;
    case PmType::old_spectator:
      return sh::MoveType::old_spectator;
    case PmType::fly:
      return sh::MoveType::fly;
    case PmType::none:
      return sh::MoveType::none;
    case PmType::lock:
      return sh::MoveType::lock;
    default:
      return p.dead ? sh::MoveType::dead : sh::MoveType::normal;
  }
}

void apply(QwClientEnd& c, const SvcMsg& m) {
  switch (m.id) {
    case svc::serverdata:
      for (const auto& [magic, bits] : m.ext) {
        ExtFamily f{};
        if (family_of_magic(magic, f)) c.ext[f] = bits;
      }
      for (size_t i = 0; i < 10 && i < m.floats.size(); i++) c.movevars[i] = m.floats[i];
      if (c.level_record) c.level_from = c.level_record->bytes.size();  // a new level
      c.spawncount = m.ints[1];
      c.playernum = m.ints[2] & 0x7f;
      c.gamedir = m.strings[0];
      c.level = m.strings[1];
      c.sounds.clear();
      c.models.clear();
      client_command(c, "soundlist " + std::to_string(c.spawncount) + " 0");
      break;
    case svc::soundlist:
    case svc::soundlistshort:
      on_list(c, m, false);
      break;
    case svc::modellist:
    case svc::modellistshort:
      on_list(c, m, true);
      break;
    case svc::stufftext:
      on_stufftext(c, m.strings[0]);
      break;
    case svc::spawnbaseline:
    case svc::spawnbaseline2: {
      EntityState b;
      b.number = m.ints[0];
      if (m.id == svc::spawnbaseline) {
        b.modelindex = m.ints[1];
        b.frame = m.ints[2];
        b.colormap = m.ints[3];
        b.skin = m.ints[4];
        b.origin = {m.floats[0], m.floats[2], m.floats[4]};
        b.angles = {m.floats[1], m.floats[3], m.floats[5]};
      } else {
        b = m.ents[0].state;
        b.number = m.ints[0];
      }
      if (c.baselines.size() <= static_cast<size_t>(b.number))
        c.baselines.resize(static_cast<size_t>(b.number) + 1);
      for (size_t i = 0; i < c.baselines.size(); i++) c.baselines[i].number = static_cast<int>(i);
      c.baselines[static_cast<size_t>(b.number)] = b;
      break;
    }
    case svc::spawnstatic:
    case svc::spawnstatic2:
      c.statics++;
      break;
    case svc::spawnstaticsound:
    case svc::spawnstaticsound2:
      c.static_sounds++;
      break;
    case svc::lightstyle:
      c.lightstyles++;
      break;
    case svc::updateuserinfo:
      if (m.ints[0] >= 0 && m.ints[0] < 32) {
        c.userinfos[m.ints[0]] = m.strings[0];
        c.userids[m.ints[0]] = m.ints.size() > 1 ? m.ints[1] : 0;
      }
      break;
    case svc::setinfo:
      if (m.ints[0] >= 0 && m.ints[0] < 32)
        info_set(c.userinfos[m.ints[0]], m.strings[0], m.strings[1]);
      break;
    case svc::updatefrags:
      if (m.ints[0] >= 0 && m.ints[0] < 32) c.frags[m.ints[0]] = m.ints[1];
      break;
    case svc::updatestat:
    case svc::updatestatlong:
      if (m.ints[0] >= 0 && m.ints[0] < kStats) c.stats[m.ints[0]] = m.ints[1];
      break;
    case svc::print:
      c.prints.push_back(m.strings[0]);
      break;
    case svc::sound:
    case svc::soundextended:
      if (m.ints.size() >= 5 && m.floats.size() >= 3)
        c.heard.push_back({m.ints[0],
                           m.ints[1],
                           m.ints[4],
                           static_cast<float>(m.ints[2]) / 255.f,
                           static_cast<float>(m.ints[3]) / 64.f,
                           {m.floats[0], m.floats[1], m.floats[2]}});
      break;
    case svc::centerprint:
      c.centerprints.push_back(m.strings[0]);
      break;
    case svc::playerinfo:
      if (m.player.slot >= 0 && m.player.slot < 32) {
        c.players[m.player.slot] = m.player;
        c.player_seen[m.player.slot] = true;
        c.player_at[m.player.slot] = c.now;
      }
      break;
    case svc::setangle:
      for (size_t i = 0; i < 3 && i < m.floats.size(); i++) c.set_angles[i] = m.floats[i];
      c.setangles++;
      break;
    case svc::download:
      on_download(c, m);
      break;
    case svc::ge_origin:
      // The archive the download asked for comes out of; told before the download's first block.
      if (c.dl.active && m.strings.size() == 3 && m.ints.size() == 5 && m.strings[0] == c.dl.name) {
        c.dl.has_origin = true;
        c.dl.origin = {m.strings[1],
                       m.strings[2],
                       static_cast<uint32_t>(m.ints[0]),
                       static_cast<uint32_t>(m.ints[1]),
                       static_cast<uint32_t>(m.ints[2]),
                       static_cast<uint32_t>(m.ints[3]),
                       static_cast<uint32_t>(m.ints[4])};
      }
      break;
    case svc::ge_sheet:  // the server's settings sheet, a part at a time (qw_sheet.hpp)
      if (!m.ints.empty() && !m.strings.empty())
        ge::server::sheet_take(c.sheet, m.ints[0], m.strings[0]);
      break;
    case svc::serverinfo:  // one key changed (a match's `status`, a new limit)
      if (m.strings.size() == 2)
        info_set(c.serverinfo, m.strings[0], m.strings[1], kServerInfoBytes);
      break;
    case svc::disconnect: {
      // The server's reason, printed in the same packet as the drop ("Map model file does not
      // match"), else the drop alone.
      std::string why;
      for (size_t i = c.packet_prints; i < c.prints.size(); i++) why += c.prints[i];
      while (!why.empty() && (why.back() == '\n' || why.back() == ' ')) why.pop_back();
      for (char& ch : why) ch = ch == '\n' ? ' ' : ch;
      c.stage = ClientStage::dropped;
      c.map_refused = why.find("does not match (maps/") != std::string::npos;
      c.refused = why.empty() ? "disconnected by the server" : refusal_words(why);
      break;
    }
    default:
      break;
  }
}

void read_payload(QwClientEnd& c, const uint8_t* data, size_t size, size_t at) {
  c.packet_prints = c.prints.size();
  gn::Reader r = gn::make_reader(data, size);
  r.at = at;
  WireCtx ctx;
  std::vector<std::pair<size_t, size_t>> messages;  // each message's bytes, for a recording
  const auto recorded = [&] {
    if (c.record) record_packet(*c.record, c.now, {data, at}, {data, size}, messages);
    if (c.level_record) record_packet(*c.level_record, c.now, {data, at}, {data, size}, messages);
  };
  while (remaining(r) > 0 && c.stage != ClientStage::dropped) {
    const size_t from = r.at;
    const uint8_t id = r.data[r.at];
    c.svc_counts[id]++;
    if (id == svc::packetentities || id == svc::deltapacketentities) {
      r.at++;
      on_entities(c, r, id == svc::deltapacketentities);
      if (!c.error.empty()) return recorded();
      messages.emplace_back(from, r.at);
      continue;
    }
    ctx.ext = c.ext;
    ctx.chunked = c.ext.has(ExtFamily::x, kXChunkedDl);
    SvcMsg m;
    if (!read_svc(r, m, ctx, c.error)) return recorded();
    messages.emplace_back(from, r.at);
    if (m.id == svc::voice && !m.ints.empty() && m.ints[0] >= 0 && m.ints[0] < 32)
      c.voice_heard[m.ints[0]]++;
    apply(c, m);
  }
  recorded();
}

// The voice cut since the last tick (`samples` of it) and, while talking, sent as one message.
void speak(QwClientEnd& c, int samples) {
  const bool talking =
      c.cfg.live_mic ? c.mic_talking : c.cfg.talk != nullptr && c.cfg.talk(c.tick, c.cfg.seed);
  std::vector<float> said;  // the samples cut this tick
  cl::voice_out_said(c.mic_in, c.speech, c.speech_at, c.cfg.live_mic, talking, samples, said);
  ClcMsg m;
  m.id = clc::voice;
  m.gen = cl::kVoiceOpus << 4;
  const int frames = cl::voice_out_cut(c.voice, said, talking, m.data, m.seq);
  if (frames == 0) return;
  c.voice_sent += frames;
  uint8_t buf[kMaxMsgLen];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  write_clc(w, m);
  if (w.overflow) return;
  // Unreliable, like a move: a frame late is a frame lost.
  c.voice_out.insert(c.voice_out.end(), buf, buf + w.size);
  if (c.record && c.playernum >= 0) record_own_voice(*c.record, c.playernum, m);
}

// From this machine: the loopback, or the client's own address (any port).
bool from_here(const gn::Address& a, const gn::Address& own) {
  if (a.family == own.family && std::memcmp(a.bytes, own.bytes, sizeof a.bytes) == 0) return true;
  if (a.family == gn::Family::ipv4) return a.bytes[0] == 127;
  if (a.family != gn::Family::ipv6) return false;
  for (int k = 0; k < 15; k++)
    if (a.bytes[k] != 0) return false;
  return a.bytes[15] == 1;
}

std::string trimmed(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return "";
  return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

// A command packet ('B': a command, then an id) from a server browser on this machine: run only
// with the id `localid` holds (any, before the first accept); a wrong id clears `localid`.
void command_packet(QwClientEnd& c, const gn::Address& from, const uint8_t* data, size_t size) {
  c.oob_prints.push_back(gn::to_string(from) + ": client command\n");
  if (!from_here(from, c.transport->local)) {
    c.oob_prints.emplace_back("Command packet from remote host.  Ignored.\n");
    return;
  }
  gn::Reader r = gn::make_reader(data, size);
  r.at = 5;
  const std::string command = read_string(r);
  const std::string id = trimmed(read_string(r));
  const std::string bar(27, '=');
  if (!c.commands_unchecked) {
    if (c.localid.empty()) {
      c.oob_prints.push_back(bar +
                             "\nCommand packet received from local host, but no localid has "
                             "been set.  You may need to upgrade your server browser.\n" +
                             bar + "\n");
      return;
    }
    if (id != c.localid) {
      c.oob_prints.push_back(bar +
                             "\nInvalid localid on command packet received from local host. "
                             "\n|" +
                             id + "| != |" + c.localid +
                             "|\nYou may need to reload your server browser and QuakeWorld.\n" +
                             bar + "\n");
      c.localid.clear();
      c.localid_refused = true;
      return;
    }
  }
  c.local_commands.push_back(command);
}

void read_oob(QwClientEnd& c, const uint8_t* data, size_t size) {
  if (size == 1 && data[0] == 'l') return;
  if (size < 5) return;
  const char type = static_cast<char>(data[4]);
  // A download's chunk sent out of band: a print ('n') whose text starts `\chunk`, then the
  // download's number, the download message's id and its chunk. Never printed: one from an
  // earlier download (its number another) is dropped, so it never lands in this one's bytes.
  if (size > kChunkOobMin && type == 'n' && std::memcmp(data + 5, "\\chunk", 6) == 0) {
    gn::Reader r = gn::make_reader(data, size);
    r.at = 11;
    const int32_t number = read_long(r);
    if (number != c.dl.number || gn::read_u8(r) != svc::download) return;
    const int32_t chunk = read_long(r);
    const uint8_t* p = gn::read_view(r, kChunkBytes);
    c.chunks_oob++;
    if (p) download_chunk(c, chunk, p);
    return;
  }
  if (type == 'c' && c.stage == ClientStage::challenge) {
    gn::Reader r = gn::make_reader(data, size);
    r.at = 5;
    c.challenge = static_cast<int32_t>(std::atol(read_string(r).c_str()));
    while (remaining(r) >= 8) {
      const uint32_t magic = gn::read_u32(r), bits = gn::read_u32(r);
      ExtFamily f{};
      if (family_of_magic(magic, f)) c.offered[f] = bits;
    }
    c.stage = ClientStage::connect;
    c.last_ask = c.now;
    send_connect(c);
  } else if (type == 'j' && c.stage == ClientStage::connect) {
    c.commands_unchecked = false;  // every later command packet needs the id
    c.stage = ClientStage::signon;
    c.connected_at = c.now;
    c.ch.trace = c.cfg.trace;
    channel_setup(c.ch, true, c.cfg.qport, c.now);
    channel_set_rate(c.ch, 0);
    client_command(c, "new");
  } else if (type == 'n' && c.stage == ClientStage::connect) {
    std::string text(reinterpret_cast<const char*>(data) + 5,
                     strnlen(reinterpret_cast<const char*>(data) + 5, size - 5));
    // A full server seating the client as a spectator says so and goes on to the accept.
    if (text.find("connecting as spectator") != std::string::npos) {
      c.oob_prints.push_back(std::move(text));
      return;
    }
    c.refused = std::move(text);
    c.stage = ClientStage::dropped;
  } else if (type == 'n') {  // after the connect: a remote console's answer
    c.oob_prints.emplace_back(reinterpret_cast<const char*>(data) + 5,
                              strnlen(reinterpret_cast<const char*>(data) + 5, size - 5));
  }
}

}  // namespace

const char* client_stage_name(ClientStage s) {
  switch (s) {
    case ClientStage::challenge:
      return "challenge";
    case ClientStage::connect:
      return "connect";
    case ClientStage::signon:
      return "signon";
    case ClientStage::active:
      return "active";
    case ClientStage::dropped:
      return "dropped";
  }
  return "?";
}

sh::MoveCmd default_script(int tick, int seed) {
  // Runs and turns in a slow circle, strafes every other second, jumps and fires now and then.
  sh::MoveCmd c;
  const int t = tick + seed * 37;
  c.angles = {sh::quantize_angle(static_cast<float>((t / 7) % 20) - 10.0f),
              sh::quantize_angle(static_cast<float>((t * 2) % 360) - 180.0f), 0};
  c.forward = sh::quantize_axis(400);
  c.side = sh::quantize_axis((t / 77) % 2 ? 200.0f : -200.0f);
  if (t % 154 < 3) c.buttons |= sh::kButtonJump;
  if (t % 100 < 2) c.buttons |= sh::kButtonAttack;
  return c;
}

void client_start(QwClientEnd& c, gn::Transport& t, const gn::Address& server,
                  const ClientConfig& cfg, double now) {
  c = QwClientEnd{};
  c.cfg = cfg;
  c.transport = &t;
  c.server = server;
  c.now = now;
  if (cfg.voice) {
    c.cfg.ask[ExtFamily::x2] |= kX2Voice;
    cl::voice_out_open(c.voice);
    if (!cfg.live_mic)
      c.speech = cfg.speech ? *cfg.speech
                            : gn::voice_trial_speech(static_cast<uint32_t>(cfg.seed) * 7919u + 17u,
                                                     6.0, 100.0 + 25.0 * (cfg.seed % 5));
  }
}

void client_rcon(QwClientEnd& c, const std::string& password, const std::string& line) {
  send_oob_text(c, "rcon \"" + password + "\" " + line + "\n");
}

void qw_voice_target(QwClientEnd& c, const std::string& word) {
  if (word.empty() || word == c.voice_to) return;
  c.voice_to = word;
  if (c.begun) client_command(c, "voicetarg " + word);
}

void client_command(QwClientEnd& c, const std::string& line) {
  uint8_t buf[1100];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  write_byte(w, clc::stringcmd);
  write_string(w, line);
  channel_reliable(c.ch, buf, w.size);
}

void client_upload_start(QwClientEnd& c, std::vector<uint8_t> picture) {
  c.up.asked = false;
  if (picture.empty()) {
    client_command(c, "snap");
    return;
  }
  c.up.said.push_back("Upload starting of " + std::to_string(picture.size()) + "...");
  c.up.data = std::move(picture);
  c.up.at = 0;
  c.up.sending = true;
  send_upload_block(c);
}

sh::MoveSettings client_move_settings(const QwClientEnd& c) {
  sh::MoveSettings m;
  const float* v = c.movevars;
  m.gravity = v[0];
  m.stopspeed = v[1];
  m.maxspeed = v[2];
  m.spectatormaxspeed = v[3];
  m.accelerate = v[4];
  m.airaccelerate = v[5];
  m.wateraccelerate = v[6];
  m.friction = v[7];
  m.waterfriction = v[8];
  return m;
}

sh::NetPlayer client_net_player(const PlayerState& p) {
  sh::NetPlayer n;
  const float at[3] = {p.origin.x, p.origin.y, p.origin.z};
  const float speed[3] = {p.velocity.x, p.velocity.y, p.velocity.z};
  for (int i = 0; i < 3; i++) {
    n.origin8[i] = static_cast<int32_t>(at[i] * 8);
    n.velocity[i] = static_cast<int16_t>(speed[i]);
  }
  n.type = move_type_of(p);
  n.onground = p.onground;
  n.jump_held = p.pm == PmType::normal_jump_held;
  return n;
}

double client_move_rate(const QwClientEnd& c) {
  double server = std::atof(info_get(c.serverinfo, "maxfps").c_str());
  if (server <= 0) server = kServerMovesDefault;
  const int own = c.cfg.spectator ? c.cfg.physfps_spectator : c.cfg.physfps;
  const double rate = own > 0 ? std::min(static_cast<double>(own), server) : server;
  return std::max(rate, kLeastMoves);
}

bool client_hold_move(QwClientEnd& c, double move_s) {
  if (c.cfg.c2spps <= 0) return false;
  const double cost = 1.0 / std::clamp(c.cfg.c2spps, kLeastPackets, kMostPackets);
  c.send_balance = std::clamp(c.send_balance + move_s, -kPacketBalanceS, kPacketBalanceS);
  bool impulse = false;
  for (int i = 0; i < std::clamp(c.cfg.c2s_impulse_backup, 0, 3); i++)
    impulse = impulse || c.cmds[2 - i].impulse != 0;
  if (c.send_balance <= 0 && c.held < kMostHeld && !impulse) {
    c.held++;
    c.moves_held++;
    return true;
  }
  c.held = 0;
  c.send_balance = std::max(c.send_balance - cost, -kPacketBalanceS);
  return false;
}

void client_tick(QwClientEnd& c, int64_t now_us) {
  c.now = static_cast<double>(now_us) / 1e6;
  c.tick++;
  c.heard.clear();
  gn::transport_pump(*c.transport, now_us);
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  while (true) {
    const int n = gn::transport_receive(*c.transport, buf, sizeof buf, from, now_us);
    if (n <= 0) break;
    const auto size = static_cast<size_t>(n);
    c.datagrams_in++;
    c.bytes_in += n;
    if (from != c.server) {
      if (is_out_of_band(buf, size) && size > 4 && buf[4] == 'B')
        command_packet(c, from, buf, size);
      continue;
    }
    if (is_out_of_band(buf, size) || (size == 1 && buf[0] == 'l')) {
      read_oob(c, buf, size);
      continue;
    }
    if (c.stage != ClientStage::signon && c.stage != ClientStage::active) continue;
    size_t at = 0;
    const uint32_t acked = c.ch.incoming_acknowledged;
    if (!channel_process(c.ch, buf, size, c.now, at)) continue;
    const uint32_t ack = c.ch.incoming_acknowledged;
    if (ack != acked && c.sent_seq[ack & 63] == ack) {
      c.latency = c.now - c.sent_at[ack & 63];
      c.latency_avg =
          c.latency_avg <= 0 ? c.latency : c.latency_avg + (c.latency - c.latency_avg) / 16;
    }
    const int64_t misses = c.delta_misses;
    read_payload(c, buf, size, at);
    ge::hud::net_lost(c.net, c.ch.last_drop);
    ge::hud::net_arrived(c.net, c.latency > 0 ? c.latency * 1e3 : -1, c.delta_misses != misses);
    if (!c.error.empty()) {
      c.stage = ClientStage::dropped;
      c.refused = "could not read the server: " + c.error;
    }
  }
  if (!c.sent_on.empty()) {  // a server sent it elsewhere: the handshake again, there
    gn::Address to;
    const int sent = c.redirects + 1;
    if (sent > 3 || !gn::parse_address(c.sent_on, to)) {
      c.stage = ClientStage::dropped;
      c.refused = sent > 3 ? "sent from server to server too many times"
                           : "sent to an address that cannot be read: " + c.sent_on;
      c.sent_on.clear();
      return;
    }
    const ClientConfig cfg = c.cfg;
    client_start(c, *c.transport, to, cfg, c.now);
    c.redirects = sent;
    return;
  }
  if (wd::web_busy(c.web)) step_web(c);
  // A connected server that has gone quiet: given up, so the game can say so and leave.
  if ((c.stage == ClientStage::signon || c.stage == ClientStage::active) && c.cfg.timeout > 0 &&
      c.now - c.ch.last_received > c.cfg.timeout) {
    c.stage = ClientStage::dropped;
    c.refused = "the server stopped answering";
    return;
  }
  if (c.begun && c.stage == ClientStage::signon && c.ch.reliable.size == 0 &&
      c.ch.pending_count == 0) {
    c.stage = ClientStage::active;
    c.active_at = c.now;
  }

  switch (c.stage) {
    case ClientStage::challenge:
      if (c.last_ask < 0 || c.now - c.last_ask >= kRetry) {
        c.last_ask = c.now;
        send_oob_text(c, "getchallenge\n");
      }
      return;
    case ClientStage::connect:
      if (c.now - c.last_ask >= kRetry) {
        c.last_ask = c.now;
        send_connect(c);
      }
      return;
    case ClientStage::dropped:
      return;
    default:
      break;
  }

  uint8_t payload[kMaxMsgLen];
  gn::Writer w = gn::make_writer(payload, sizeof payload);
  if (c.stage == ClientStage::active) {
    c.cmds[0] = c.cmds[1];
    c.cmds[1] = c.cmds[2];
    sh::MoveCmd cmd =
        usercmd_grid(c.cfg.brain ? c.cfg.brain(c.tick) : c.cfg.script(c.tick, c.cfg.seed));
    c.clock.period = 1 / client_move_rate(c);
    cmd.msec = sh::next_msec(c.clock);  // whole milliseconds, the fraction carried
    c.cmds[2] = cmd;
    if (client_hold_move(c, cmd.msec / 1000.0)) {  // its sequence spent; the next packet carries it
      const uint32_t seq = c.ch.outgoing_sequence++;
      c.sent[seq & 63] = cmd;
      c.sent_seq[seq & 63] = seq;
      c.sent_at[seq & 63] = c.now;
      if (c.record) record_command(*c.record, c.now, cmd);
      if (c.level_record) record_command(*c.level_record, c.now, cmd);
      if (c.cfg.voice && c.ext.has(ExtFamily::x2, kX2Voice))
        speak(c, cmd.msec * gn::kVoiceRate / 1000);  // sent with the next packet
      return;
    }
    const int loss =
        c.ch.accepted ? static_cast<int>(100 * c.ch.dropped / (c.ch.accepted + c.ch.dropped)) : 0;
    write_move(w, c.cmds, static_cast<uint8_t>(loss), c.ch.outgoing_sequence);
    c.sent[c.ch.outgoing_sequence & 63] = cmd;
    c.sent_seq[c.ch.outgoing_sequence & 63] = c.ch.outgoing_sequence;
    c.sent_at[c.ch.outgoing_sequence & 63] = c.now;
    if (c.record) record_command(*c.record, c.now, cmd);
    if (c.level_record) record_command(*c.level_record, c.now, cmd);
    if (c.tmove) {  // a spectator's teleport (qw_follow.hpp)
      ClcMsg t;
      t.id = clc::tmove;
      t.origin = c.tmove_to;
      write_clc(w, t);
      c.tmove = false;
    }
    if (c.valid_frame >= 0) {
      write_byte(w, clc::delta);
      write_byte(w, c.valid_frame & 0xff);
    }
    if (c.cfg.voice && c.ext.has(ExtFamily::x2, kX2Voice)) {
      speak(c, cmd.msec * gn::kVoiceRate / 1000);
      if (w.size + c.voice_out.size() <= w.capacity)
        gn::write_bytes(w, c.voice_out.data(), c.voice_out.size());
      c.voice_out.clear();
    }
  } else {
    write_byte(w, clc::nop);
  }
  if (c.dl.active && c.dl.chunked && c.dl.size > 0) {
    int asked = 0;
    for (size_t i = 0;
         i < c.dl.have.size() && asked < kChunksPerTick && w.size + 32 <= kChunkAsksBytes; i++) {
      if (c.dl.have[i] || (c.dl.asked[i] >= 0 && c.now - c.dl.asked[i] < kChunkRetry)) continue;
      c.dl.asked[i] = c.now;
      int held = 0;
      for (bool h : c.dl.have) held += h ? 1 : 0;
      const int percent =
          static_cast<int>(size_t{100} * static_cast<size_t>(held) / c.dl.have.size());
      write_byte(w, clc::stringcmd);
      write_string(w, "nextdl " + std::to_string(i) + " " + std::to_string(percent) + " " +
                          std::to_string(c.dl.number));
      asked++;
    }
  }
  uint8_t packet[kMaxPacket];
  const size_t n = channel_transmit(c.ch, payload, w.overflow ? 0 : w.size, c.now, packet);
  send_raw(c, packet, n);
}

}  // namespace shooters_edge::net
