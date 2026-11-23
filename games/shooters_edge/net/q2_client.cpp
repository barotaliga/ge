// The scripted Quake II client (q2_client.hpp): the connectionless steps, the signon as the
// server stuffs it, the payloads read as Quake II's recording, and the moves.
//
// ge:summary The scripted Quake II client's steps: connect, the stuffed signon, frames, moves
// ge:keywords q2-client q2_client_tick stufftext signon begin move ack-frame
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#include "shooters_edge/net/q2_client.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <utility>

#include "classic/rulesets.hpp"
#include "net/voice_trial.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/q2_fetch.hpp"
#include "shooters_edge/net/qw_ext.hpp"
#include "shooters_edge/net/qw_voice_route.hpp"

namespace shooters_edge::net {

namespace cl = shooters_edge::client;
namespace wd = ge::classic::content;

namespace {

constexpr double kRetry = 1.0;  // s between connectionless retries, and keep-alives in the signon
constexpr size_t kKept = 256;   // stuffed lines and prints kept

void send_raw(Q2ClientEnd& c, const uint8_t* data, size_t size) {
  gn::transport_send(*c.transport, c.server, data, size, static_cast<int64_t>(c.now * 1e6));
  c.bytes_out += static_cast<int64_t>(size);
}

void send_oob(Q2ClientEnd& c, const std::string& text) {
  const std::vector<uint8_t> b = q2_oob(text);
  send_raw(c, b.data(), b.size());
}

std::string userinfo_of(const Q2ClientEnd& c) {
  std::string u;
  info_set(u, "name", c.cfg.name);
  info_set(u, "skin", c.cfg.skin);
  info_set(u, "rate", std::to_string(c.cfg.rate));
  info_set(u, "msg", "1");
  info_set(u, "hand", std::to_string(c.cfg.hand));
  info_set(u, "fov", std::to_string(c.cfg.fov));
  if (!c.cfg.gender.empty()) info_set(u, "gender", c.cfg.gender);
  info_set(u, "version", kProgramVersion);  // what the server's players and admins see us as
  if (c.cfg.speedjump) info_set(u, "cl_speedjump", "1");
  if (!c.cfg.ruleset.empty() && c.cfg.ruleset != "default")
    info_set(u, std::string(ge::classic::ruleset_info_key(true)), c.cfg.ruleset);
  if (c.cfg.spectator)
    info_set(u, "spectator", c.cfg.spectator_password.empty() ? "1" : c.cfg.spectator_password);
  if (!c.cfg.password.empty()) info_set(u, "password", c.cfg.password);
  const uint32_t ext =
      (c.cfg.fetch && c.cfg.ask_origins ? kGeDownloadOrigin : 0u) |
      (c.cfg.ask_moves ? kGeOtherMoves : 0u) | (c.cfg.speedjump ? kGeSpeedJumpShare : 0u) |
      (c.cfg.ask_login ? kGeAccountKey : 0u) | (c.cfg.ask_feel ? kGeMoveFeel : 0u) |
      (c.cfg.voice ? kGeVoice : 0u) | (c.cfg.ask_frames ? kGeEveryFrame : 0u) |
      (c.cfg.ask_back ? kGeItemBack : 0u) | (c.cfg.ask_hits ? kGeHitReports : 0u) |
      (c.cfg.ask_sheet ? kGeSettings : 0u);
  if (ext != 0) info_set(u, "*ge_ext", std::to_string(ext));
  return u;
}

// The later protocols' qport: its low byte alone.
uint16_t qport_of(const Q2ClientEnd& c) {
  return c.protocol == kQ2NetProtocol ? c.cfg.qport : static_cast<uint16_t>(c.cfg.qport & 0xFFu);
}

// The protocol taken from the challenge's `p=` lists (`w` after the challenge number): the asked
// one when offered, else the next one down; 34 when nothing later is offered or asked.
int choose_protocol(int asked, const std::vector<std::string>& w) {
  bool has35 = false, has36 = false;
  for (size_t k = 2; k < w.size(); k++) {
    if (w[k].rfind("p=", 0) != 0) continue;
    const std::string list = w[k].substr(2);
    for (size_t at = 0; at <= list.size();) {
      const size_t comma = std::min(list.find(',', at), list.size());
      const int v = std::atoi(list.substr(at, comma - at).c_str());
      has35 = has35 || v == kQ2Protocol35;
      has36 = has36 || v == kQ2Protocol36;
      at = comma + 1;
    }
  }
  if (asked == 0) asked = kQ2Protocol36;
  if (asked == kQ2Protocol36) {
    if (has36) return kQ2Protocol36;
    asked = kQ2Protocol35;
  }
  if (asked == kQ2Protocol35 && has35) return kQ2Protocol35;
  return kQ2NetProtocol;
}

// The largest packet asked of the server: `net_maxmsglen`, 0 sent as it is.
int msglen_of(const Q2ClientConfig& cfg) {
  return cfg.msglen == 0 ? 0 : std::clamp(cfg.msglen, kQ2MsgLenLeast, kQ2MsgLenMost);
}

void send_connect(Q2ClientEnd& c) {
  std::string tail;
  if (c.protocol == kQ2Protocol35)
    tail = " " + std::to_string(msglen_of(c.cfg)) + " " + std::to_string(kQ2MinorLast35);
  else if (c.protocol == kQ2Protocol36)
    tail = " " + std::to_string(msglen_of(c.cfg)) + " " + std::to_string(c.cfg.chantype) + " 1 " +
           std::to_string(kQ2MinorLast36);
  send_oob(c, "connect " + std::to_string(c.protocol) + " " + std::to_string(qport_of(c)) + " " +
                  std::to_string(c.challenge) + " \"" + userinfo_of(c) + "\"" + tail + "\n");
}

// A client setting (clc 5) on the reliable channel: the later protocols' only.
void send_setting(Q2ClientEnd& c, int index, int value) {
  const uint8_t b[5] = {
      kClcSetting, static_cast<uint8_t>(index & 0xFF), static_cast<uint8_t>((index >> 8) & 0xFF),
      static_cast<uint8_t>(value & 0xFF), static_cast<uint8_t>((value >> 8) & 0xFF)};
  channel_reliable(c.ch, b, sizeof b);
}

void connected(Q2ClientEnd& c, int chan) {
  c.stage = ClientStage::signon;
  c.connected_at = c.now;
  c.ch.trace = c.cfg.trace;
  channel_setup(c.ch, true, qport_of(c), c.now);
  if (c.protocol != kQ2NetProtocol) {  // a byte of qport (none when 0), the newer channel on 36
    c.ch.qport_bytes = qport_of(c) != 0 ? 1 : 0;
    c.ch.fragments = chan == 1;
    q2_demo(c).wide_blocks = true;
  }
  channel_set_rate(c.ch, 0);
  q2_client_command(c, "new");
  // The server is told the client records whatever it does, so it leaves out nothing the client
  // could work out itself: every recording of a later protocol's server is whole.
  if (c.protocol != kQ2NetProtocol) send_setting(c, kQ2SetRecording, 1);
}

void read_oob(Q2ClientEnd& c, const uint8_t* data, size_t size) {
  const std::string text = q2_oob_text(data, size);
  if (text.rfind("print\n", 0) == 0) {
    const std::string said = text.substr(6);
    if (c.stage == ClientStage::challenge || c.stage == ClientStage::connect) {
      c.refused = said;
      c.stage = ClientStage::dropped;
    } else if (c.prints.size() < kKept) {
      c.prints.push_back(said);
    }
    return;
  }
  const std::vector<std::string> w = q2_words(text);
  if (w.empty()) return;
  if (w[0] == "challenge" && w.size() >= 2 && c.stage == ClientStage::challenge) {
    c.challenge = static_cast<int32_t>(std::strtol(w[1].c_str(), nullptr, 10));
    c.protocol = choose_protocol(c.cfg.protocol, w);
    c.stage = ClientStage::connect;
    c.last_ask = c.now;
    send_connect(c);
  } else if (w[0] == "client_connect" && c.stage == ClientStage::connect) {
    // The channel: the server's ` nc=` answer, else the newer one on 36.
    int chan = c.protocol == kQ2Protocol36 ? 1 : 0;
    int anticheat = 0;  // the server's ` ac=` ask: 2 and up insists on it
    for (size_t k = 1; k < w.size(); k++) {
      if (w[k].rfind("nc=", 0) == 0) chan = std::atoi(w[k].c_str() + 3);
      if (w[k].rfind("ac=", 0) == 0) anticheat = std::atoi(w[k].c_str() + 3);
    }
    if (anticheat >= 2 && c.prints.size() < kKept)  // none here: said, and the game goes on
      c.prints.emplace_back(
          "The server asks for anticheat, which this game does not have; joining without it.\n");
    if (chan != 0 && chan != 1) {
      c.refused = "Server returned invalid netchan type";
      c.stage = ClientStage::dropped;
      return;
    }
    wd::web_connect(c.http, c.cfg.http, wd::web_dlserver(w), c.cfg.allow.all,
                    gn::to_string(c.server));
    connected(c, chan);
  }
}

bool lacks(const Q2ClientEnd& c, const std::string& path) {
  return c.downloaded.count(path) == 0 && (!c.cfg.read_file || c.cfg.read_file(path).empty());
}

// Lacked, never asked for on this connection, and let through by the player's download switches.
bool wanted(const Q2ClientEnd& c, const std::string& path) {
  return c.asked.count(path) == 0 && q2_download_allowed(c.cfg.allow, path) && lacks(c, path);
}

void queue_fetch(Q2ClientEnd& c, const std::string& path) {
  if (!wanted(c, path)) return;
  c.asked.insert(path);
  c.fetch_queue.push_back(path);
}

// A file as held now: fetched on this connection, else the player's own.
std::vector<uint8_t> held_file(const Q2ClientEnd& c, const std::string& path) {
  if (const auto it = c.downloaded.find(path); it != c.downloaded.end()) return it->second;
  return c.cfg.read_file ? c.cfg.read_file(path) : std::vector<uint8_t>{};
}

// The level's next stage queued (net/q2_fetch.hpp), once the one before has come: what the models
// and the level name, then the map's wall textures.
void queue_next_stage(Q2ClientEnd& c) {
  while (c.fetch_queue.empty() && c.cfg.fetch_level && c.fetch_stage >= 1 && c.fetch_stage < 3) {
    const cl::DemoState& st = q2_demo(c).state;
    const Q2FetchRead read = [&c](const std::string& path) { return held_file(c, path); };
    if (++c.fetch_stage == 2) {
      for (const std::string& path : q2_fetch_level(st, read, c.cfg.vweps)) queue_fetch(c, path);
    } else if (st.models.size() > 1) {
      for (const std::string& path : q2_fetch_textures(held_file(c, q2_fetch_path(st.models[1]))))
        queue_fetch(c, path);
    }
  }
}

// THE DOWNLOADS: the next file asked for, or the held `begin` sent once none is left.
void next_fetch(Q2ClientEnd& c) {
  c.fetching.clear();
  c.fetched_bytes.clear();
  c.fetched_percent = 0;
  c.fetched_from = 0;
  c.fetched_origin = false;
  for (;;) {
    if (c.http.on) {                               // over HTTP first: `begin` held until it is done
      c.cfg.http.gamedir = q2_demo(c).state.game;  // the server's game folder ("": baseq2)
      for (const std::string& path : std::exchange(c.fetch_queue, {}))
        if (wd::web_queue(c.http, c.cfg.http, path)) q2_client_command(c, "download http");
      if (wd::web_busy(c.http)) return;
    }
    if (!c.fetch_queue.empty()) break;
    if (c.fetch_stage < 1 || c.fetch_stage >= 3 || !c.cfg.fetch_level) {
      if (!c.held_begin.empty()) q2_client_command(c, c.held_begin);
      c.held_begin.clear();
      return;
    }
    queue_next_stage(c);
  }
  c.fetching = c.fetch_queue.front();
  c.fetch_queue.erase(c.fetch_queue.begin());
  // A download cut short before: the rest asked for from what is held.
  if (c.cfg.held.read) c.fetched_bytes = c.cfg.held.read(c.fetching);
  c.fetched_from = c.fetched_bytes.size();
  q2_client_command(
      c,
      "download " + c.fetching +
          (c.fetched_bytes.empty() ? std::string() : " " + std::to_string(c.fetched_bytes.size())));
}

// `precache`: the files lacked queued, `begin` held until they are fetched; with `fetch_level`
// the level's stages after the map and its models follow (net/q2_fetch.hpp).
void start_fetch(Q2ClientEnd& c, const std::string& begin) {
  c.fetch_queue.clear();
  c.fetch_stage = 1;
  const std::vector<std::string>& models = q2_demo(c).state.models;
  if (c.cfg.fetch_level) {
    for (const std::string& path : q2_fetch_models(q2_demo(c).state)) queue_fetch(c, path);
  } else if (models.size() > 1 && models[1].rfind("maps/", 0) == 0) {
    queue_fetch(c, models[1]);
  }
  for (const std::string& w : c.cfg.want) queue_fetch(c, w);
  c.held_begin = begin;
  next_fetch(c);
}

// "[3 remaining files]": the files still to ask for over the channel.
std::string files_left(const Q2ClientEnd& c) {
  return wd::web_files_left(static_cast<int>(c.fetch_queue.size()));
}

// The download pieces and archives the reader took off this payload.
void take_downloads(Q2ClientEnd& c) {
  for (auto& [path, origin] : q2_demo(c).download_origins)
    if (path == c.fetching) {
      c.fetched_origin = true;
      c.fetching_origin = std::move(origin);
    }
  q2_demo(c).download_origins.clear();
  for (cl::Demo::DownloadPiece& p : q2_demo(c).download_pieces) {
    if (c.fetching.empty()) continue;
    if (p.size < 0) {
      c.downloads_refused++;
      c.http.said.push_back("[UDP] " + c.fetching +
                            (c.fetched_percent > 0 ? " [STOP] " : " [FAIL] ") + files_left(c));
      next_fetch(c);
      continue;
    }
    if (c.cfg.held.add && !p.bytes.empty())
      c.cfg.held.add(c.fetching, p.bytes, c.fetched_bytes.empty());
    c.fetched_bytes.insert(c.fetched_bytes.end(), p.bytes.begin(), p.bytes.end());
    if (p.percent == 100 && c.in.download_ready) {  // 36's compressed download, inflated whole
      c.fetched_bytes.resize(c.fetched_from);       // the rest after what was held
      c.fetched_bytes.insert(c.fetched_bytes.end(), c.in.download.begin(), c.in.download.end());
      c.in.download.clear();
      c.in.download_ready = false;
    }
    c.fetched_percent = p.percent;
    if (p.percent < 100) {
      q2_client_command(c, "nextdl");
      continue;
    }
    c.downloaded[c.fetching] = c.fetched_bytes;
    if (c.cfg.held.drop) c.cfg.held.drop(c.fetching);
    c.http.said.push_back("[UDP] " + c.fetching + " [DONE] " + files_left(c));
    if (c.cfg.keep_file)
      c.cfg.keep_file(c.fetching, c.fetched_bytes, c.fetched_origin ? &c.fetching_origin : nullptr);
    next_fetch(c);
  }
  q2_demo(c).download_pieces.clear();
}

// The HTTP downloads moved on: each file fetched kept as one over the channel; once none is left
// (or HTTP gave its files back), the channel's turn.
void step_http(Q2ClientEnd& c) {
  wd::WebDone done;
  wd::web_step(
      c.http, c.cfg.http, c.now,
      [&c](const std::string& path, bool) {
        return q2_download_allowed(c.cfg.allow, path) && lacks(c, path);
      },
      done);
  for (auto& [path, bytes] : done.files) {
    if (c.cfg.keep_file) c.cfg.keep_file(path, bytes, nullptr);
    c.downloaded[path] = std::move(bytes);
  }
  c.fetch_queue.insert(c.fetch_queue.begin(), done.back.begin(), done.back.end());
  if (!wd::web_busy(c.http) && c.fetching.empty()) next_fetch(c);
}

// `line` with each `$<name>` outside quotes made the setting's value, as a console expands it.
std::string expand_settings(const Q2ClientEnd& c, const std::string& line) {
  std::string out;
  bool quoted = false;
  for (size_t i = 0; i < line.size(); i++) {
    const char ch = line[i];
    if (ch == '"') quoted = !quoted;
    if (ch != '$' || quoted) {
      out += ch;
      continue;
    }
    size_t end = i + 1;
    while (end < line.size() && line[end] != ' ' && line[end] != '\t' && line[end] != '"') end++;
    const std::string name = line.substr(i + 1, end - i - 1);
    if (const auto it = c.stuffed_set.find(name); it != c.stuffed_set.end())
      out += it->second;
    else
      out += c.cfg.setting ? c.cfg.setting(name) : std::string();
    i = end - 1;
  }
  return out;
}

// The lines the server stuffed: the signon's prompts answered, the rest kept.
void run_stuffed(Q2ClientEnd& c) {
  for (const std::string& text : q2_demo(c).stuffed) {
    size_t from = 0;
    while (from < text.size()) {
      size_t end = text.find_first_of("\n;", from);
      if (end == std::string::npos) end = text.size();
      const std::string raw = text.substr(from, end - from);
      from = end + 1;
      if (raw.empty()) continue;
      if (c.stuffed.size() < kKept) c.stuffed.push_back(raw);
      if (c.cfg.stuff != nullptr && !ge::classic::stuff_allowed(*c.cfg.stuff, raw)) {
        if (std::string said = ge::classic::stuff_ignored_line(*c.cfg.stuff, raw); !said.empty())
          c.ignored.push_back(std::move(said));
        continue;
      }
      const std::string line = expand_settings(c, raw);  // as a console runs it
      const std::vector<std::string> w = q2_words(line);
      if (w.empty()) continue;
      if (w[0] == "set" && w.size() > 1) {  // the server's own settings (the forced reconnect)
        c.stuffed_set[w[1]] = w.size() > 2 ? w[2] : std::string();
        if (c.to_run.size() < kKept) c.to_run.push_back(raw);
      } else if (w[0] == "changing") {  // the level changes: the next `reconnect` follows
      } else if (w[0] == "cmd" && line.size() > 4) {
        q2_client_command(c, line.substr(4));
        c.signon_steps++;
      } else if (w[0] == "precache") {
        const std::string begin = "begin" + (w.size() > 1 ? " " + w[1] : std::string());
        if (c.cfg.fetch)
          start_fetch(c, begin);
        else
          q2_client_command(c, begin);
        c.no_footsteps_told = -1;  // told again with each `begin`
        q2_client_settings(c);
        c.signon_steps++;
      } else if (w[0] == "reconnect") {
        c.stage = ClientStage::signon;
        q2_client_command(c, "new");
      } else if (w[0] == "connect" && w.size() > 1) {  // sent elsewhere (sv_redirect)
        c.sent_on = w[1];
      } else if (c.to_run.size() < kKept) {  // the rest runs in the console as if typed
        c.to_run.push_back(raw);
      }
    }
  }
  q2_demo(c).stuffed.clear();
}

// One payload as one block of the recording, then read.
void read_payload(Q2ClientEnd& c, const uint8_t* data, size_t size) {
  if (c.cfg.shownet > 0 && c.cfg.trace != nullptr && c.cfg.trace->print)
    c.cfg.trace->print(std::to_string(size));  // cl_shownet
  if (size == 0) return;                       // a keep-alive
  const int levels = q2_demo(c).state.levels;
  const size_t block_at = c.level_record != nullptr ? c.level_record->size() : 0;
  const auto keep = [&c](const uint8_t* bytes, size_t n) {  // one block, read and recorded
    const auto len = static_cast<uint32_t>(n);
    for (int k = 0; k < 4; k++) q2_demo(c).bytes.push_back(static_cast<uint8_t>(len >> (8 * k)));
    q2_demo(c).bytes.insert(q2_demo(c).bytes.end(), bytes, bytes + n);
    if (c.level_record != nullptr) {
      for (int k = 0; k < 4; k++) c.level_record->push_back(static_cast<uint8_t>(len >> (8 * k)));
      c.level_record->insert(c.level_record->end(), bytes, bytes + n);
    }
  };
  if (c.protocol == kQ2NetProtocol) {
    keep(data, size);
  } else {  // a later protocol's, as protocol 34's blocks (q2_proto_in.hpp)
    std::vector<std::vector<uint8_t>> blocks;
    if (!q2_proto_in(c.in, std::span<const uint8_t>(data, size), blocks, c.error)) {
      c.stage = ClientStage::dropped;
      c.refused = c.error;
      return;
    }
    if (c.in.has_server_data && c.in.protocol != c.protocol) {
      c.stage = ClientStage::dropped;
      c.refused = "Requested protocol version " + std::to_string(c.protocol) +
                  ", but server returned " + std::to_string(c.in.protocol) + ".";
      return;
    }
    for (std::string& line : std::exchange(c.in.warnings, {}))
      if (c.prints.size() < kKept) c.prints.push_back(std::move(line));
    for (const std::vector<uint8_t>& b : blocks) keep(b.data(), b.size());
    q2_demo(c).strafe_hack = c.in.strafe_hack;
  }
  for (;;) {
    const cl::DemoRead got = cl::next_q2(q2_demo(c), c.error);
    if (got == cl::DemoRead::error) {
      c.stage = ClientStage::dropped;
      c.refused = "could not read the server: " + c.error;
      return;
    }
    if (got == cl::DemoRead::end) break;
    c.frames++;
    if (c.stage == ClientStage::signon) {
      c.stage = ClientStage::active;
      c.active_at = c.now;
    }
  }
  if (q2_demo(c).state.levels != levels) {
    c.levels += q2_demo(c).state.levels - levels;
    c.level_from = block_at;  // a new level begins with this payload
  }
  if (q2_demo(c).other_moves_read != c.moves_read) {
    c.moves_read = q2_demo(c).other_moves_read;
    c.moves_at = c.now;
  }
  cl::DemoState& st = q2_demo(c).state;
  for (const cl::DemoPrint& p : st.prints)
    if (c.prints.size() < kKept) c.prints.push_back(p.text);
  if (c.cfg.into != nullptr) {  // the window's: kept until it takes them, the read bytes dropped
    Q2News& n = c.news;
    n.prints.insert(n.prints.end(), st.prints.begin(), st.prints.end());
    n.effects.insert(n.effects.end(), st.effects.begin(), st.effects.end());
    n.started.insert(n.started.end(), st.started.begin(), st.started.end());
    n.stopped.insert(n.stopped.end(), st.stopped.begin(), st.stopped.end());
    n.voice.insert(n.voice.end(), st.voice.begin(), st.voice.end());
    if (!st.centre.empty()) n.centre = std::move(st.centre);
    st.centre.clear();
    cl::Demo& d = q2_demo(c);
    d.bytes.erase(d.bytes.begin(), d.bytes.begin() + static_cast<std::ptrdiff_t>(d.pos));
    d.pos = 0;
  }
  for (const cl::DemoVoice& v : st.voice)
    if (v.sender >= 0 && v.sender < 32) c.voice_heard[v.sender]++;
  q2_demo(c).state.voice.clear();
  if (q2_demo(c).voice_taken && !c.voice_on && c.cfg.voice) {  // heard from here on
    c.voice_on = true;
    q2_client_command(c, "unmuteall");
    const std::string want = c.voice_to.empty() ? c.cfg.voice_to : c.voice_to;
    c.voice_to.clear();
    if (!want.empty()) q2_voice_target(c, want);
  }
  q2_demo(c).state.prints.clear();
  q2_demo(c).state.effects.clear();
  q2_demo(c).state.started.clear();
  q2_demo(c).state.stopped.clear();
  take_downloads(c);
  run_stuffed(c);
  if (q2_demo(c).reconnect) {  // the server restarted: a fresh connect
    q2_demo(c).reconnect = false;
    c.stage = ClientStage::challenge;
    c.last_ask = -1;
  }
  if (q2_demo(c).ended) {
    c.stage = ClientStage::dropped;
    if (c.refused.empty()) c.refused = "the server ended the connection";
  }
}

// The voice cut since the last packet (`samples` of it) and, while talking, written after the
// move as one voice message (clc 83, net/qw_voice_route.hpp).
void speak(Q2ClientEnd& c, gn::Writer& w, int samples) {
  if (!c.voice_on || !c.voice.encoder) return;
  const bool talking =
      c.cfg.live_mic ? c.mic_talking : c.cfg.talk != nullptr && c.cfg.talk(c.tick, c.cfg.seed);
  std::vector<float> said;
  cl::voice_out_said(c.mic_in, c.speech, c.speech_at, c.cfg.live_mic, talking, samples, said);
  std::vector<uint8_t> frames;
  int seq = 0;
  const int n = cl::voice_out_cut(c.voice, said, talking, frames, seq);
  if (n == 0 || frames.size() > kMostVoiceBytes) return;
  gn::write_u8(w, kClcGeVoice);
  gn::write_u8(w, static_cast<uint8_t>(cl::kVoiceOpus << 4));
  gn::write_u8(w, static_cast<uint8_t>(seq));
  gn::write_u16(w, static_cast<uint16_t>(frames.size()));
  gn::write_bytes(w, frames.data(), frames.size());
  c.voice_sent += n;
}

// 36's batched moves: whether a packet is due (`cl_maxpackets`'s gap, rounded down to a whole
// share of 100 ms, or reliable data waiting).
bool batch_due(const Q2ClientEnd& c) {
  if (c.cfg.maxpackets <= 0 || c.last_packet < 0 || c.voice_on ||
      channel_reliable_backlog(c.ch) > 0)
    return true;
  // cl_instantpacket: attack or use just pressed goes out at once.
  if (c.cfg.instantpacket && (c.cmds[2].buttons & 3 & ~c.cmds[1].buttons) != 0) return true;
  const int ms = 1000 / std::max(c.cfg.maxpackets, 10);
  const int gap = 100 / std::max(1, 100 / std::max(ms, 1));
  return (c.now - c.last_packet) * 1000.0 >= gap - 0.5;
}

void send_packet(Q2ClientEnd& c, int64_t now_us) {
  const bool active = c.stage == ClientStage::active;
  if (active && c.cfg.send_every > 1 && c.tick % c.cfg.send_every != 0) return;
  if (!active && channel_reliable_backlog(c.ch) == 0 && c.now - c.last_ask < kRetry) return;
  c.last_ask = c.now;
  uint8_t payload[kQ2Packet];
  gn::Writer w = gn::make_writer(payload, sizeof payload - kClientHeader);
  if (active) {
    sh::MoveCmd m = c.cfg.brain ? c.cfg.brain(c.tick) : sh::MoveCmd{};
    const double ms = c.last_tick < 0 ? 13 : (c.now - c.last_tick) * 1000.0;
    m.msec = static_cast<uint8_t>(std::clamp(ms, 1.0, 250.0));
    c.cmds[0] = c.cmds[1];
    c.cmds[1] = c.cmds[2];
    c.cmds[2] = q2_cmd(m);
    c.sent[c.made & 63] = m;
    c.made++;
    int32_t acked = q2_demo(c).last_frame;
    if (c.cfg.whole_every > 0 && c.now - c.last_whole >= c.cfg.whole_every) c.last_whole = c.now;
    // For 150 ms (past the server's next 10 Hz frame, whatever moves come between) no frame is
    // acknowledged, so the server's next one is whole.
    if (c.cfg.whole_every > 0 && c.now - c.last_whole < 0.15) acked = -1;
    if (c.cfg.nodelta) acked = -1;  // cl_nodelta: every frame asked for whole
    if (c.protocol == kQ2Protocol36 && c.cfg.batch) {
      c.batch_now.push_back(c.cmds[2]);
      if (!batch_due(c)) return;  // held for the next packet
      Q2Batch b;
      b.frame = acked;
      const size_t dup = static_cast<size_t>(std::clamp(c.cfg.packetdup, 0, 3));
      const size_t from = c.batch_sent.size() > dup ? c.batch_sent.size() - dup : 0;
      b.frames.assign(c.batch_sent.begin() + static_cast<std::ptrdiff_t>(from), c.batch_sent.end());
      b.frames.push_back(c.batch_now);
      bool full = false;
      for (const auto& f : b.frames) full = full || f.size() >= 32;
      if (!full) write_q2_batch(w, b);  // a frame of 32 moves or more: the packet goes without
      c.batch_sent.push_back(std::move(c.batch_now));
      c.batch_now.clear();
      if (c.batch_sent.size() > 3) c.batch_sent.erase(c.batch_sent.begin());
    } else if (c.protocol != kQ2NetProtocol) {
      write_q2_move_ext(w, c.protocol, c.in.minor, acked, c.cmds);
    } else {
      write_q2_move(w, acked, c.cmds, c.ch.outgoing_sequence);
    }
    c.moves++;
    speak(c, w, m.msec * gn::kVoiceRate / 1000);
  }
  c.made_at[c.ch.outgoing_sequence & 63] = c.made;
  c.sent_at[c.ch.outgoing_sequence & 63] = c.now;
  c.last_packet = c.now;
  uint8_t out[kMaxPacket];
  const size_t n = channel_transmit(c.ch, payload, w.overflow ? 0 : w.size, c.now, out);
  gn::transport_send(*c.transport, c.server, out, n, now_us);
  c.bytes_out += static_cast<int64_t>(n);
}

}  // namespace

void q2_client_start(Q2ClientEnd& c, gn::Transport& t, const gn::Address& server,
                     const Q2ClientConfig& cfg, double now) {
  c = Q2ClientEnd{};
  c.cfg = cfg;
  c.transport = &t;
  c.server = server;
  c.now = now;
  cl::q2_stream_begin(q2_demo(c));
  if (cfg.voice) {
    cl::voice_out_open(c.voice);
    if (!cfg.live_mic)
      c.speech = cfg.speech ? *cfg.speech
                            : gn::voice_trial_speech(static_cast<uint32_t>(cfg.seed) * 7919u + 17u,
                                                     6.0, 100.0 + 25.0 * (cfg.seed % 5));
  }
  if (!cfg.via.empty()) {
    std::vector<gn::Address> chain = cfg.via;
    chain.push_back(server);
    ge::forward::dial_start(c.dial, chain, static_cast<uint64_t>(cfg.seed) * 7919u + cfg.qport,
                            static_cast<int64_t>(now * 1e6));
    c.dialing = true;
  }
}

void q2_voice_target(Q2ClientEnd& c, const std::string& word) {
  if (word.empty() || word == c.voice_to) return;
  c.voice_to = word;
  if (c.voice_on) q2_client_command(c, "voicetarg " + word);
}

void q2_client_command(Q2ClientEnd& c, const std::string& line) {
  std::vector<uint8_t> b = {kClcStringCmd};
  b.insert(b.end(), line.begin(), line.end());
  b.push_back(0);
  channel_reliable(c.ch, b.data(), b.size());
}

void q2_client_settings(Q2ClientEnd& c) {
  if (c.protocol != kQ2Protocol36) return;
  if (c.stage != ClientStage::signon && c.stage != ClientStage::active) return;
  const int nf = c.cfg.no_footsteps ? 1 : 0;
  if (nf == c.no_footsteps_told) return;
  c.no_footsteps_told = nf;
  send_setting(c, kQ2SetNoFootsteps, nf);
}

void q2_client_userinfo(Q2ClientEnd& c) {
  const std::string u = userinfo_of(c);
  std::vector<uint8_t> b = {kClcUserInfo};
  b.insert(b.end(), u.begin(), u.end());
  b.push_back(0);
  channel_reliable(c.ch, b.data(), b.size());
}

void q2_client_tick(Q2ClientEnd& c, int64_t now_us) {
  c.now = static_cast<double>(now_us) / 1e6;
  c.tick++;
  gn::transport_pump(*c.transport, now_us);
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  for (;;) {
    const int n = gn::transport_receive(*c.transport, buf, sizeof buf, from, now_us);
    if (n <= 0) break;
    const auto size = static_cast<size_t>(n);
    c.datagrams_in++;
    c.bytes_in += n;
    if (!c.cfg.via.empty() && ge::forward::dial_read(c.dial, buf, size, from, now_us)) continue;
    if (c.dialing || from != c.server || c.stage == ClientStage::dropped) continue;
    if (is_out_of_band(buf, size)) {
      read_oob(c, buf, size);
      continue;
    }
    if (c.stage != ClientStage::signon && c.stage != ClientStage::active) continue;
    std::span<const uint8_t> payload;
    if (!channel_read(c.ch, buf, size, c.now, payload)) continue;
    if (c.stage == ClientStage::active && c.moves > 0)
      c.latency = c.now - c.sent_at[c.ch.incoming_acknowledged & 63];
    read_payload(c, payload.data(), payload.size());
    ge::hud::net_lost(c.net, c.ch.last_drop);  // the lag graph's (scr_lag_draw) and r_netgraph's
    ge::hud::net_arrived(c.net,
                         c.stage == ClientStage::active && c.moves > 0 ? c.latency * 1e3 : -1);
  }
  if (!c.sent_on.empty()) {  // a server sent it elsewhere: the handshake again, there
    gn::Address to;
    const int sent = c.redirects + 1;
    if (sent > 3 || !gn::parse_address(c.sent_on, to)) {
      c.stage = ClientStage::dropped;
      c.refused = sent > 3 ? "sent from server to server too many times"
                           : "sent to an address that cannot be read: " + c.sent_on;
      c.sent_on.clear();
    } else {
      const Q2ClientConfig cfg = c.cfg;
      // The settings the server stuffed go along: a forced reconnect's are what it asks back.
      std::map<std::string, std::string> kept = std::move(c.stuffed_set);
      q2_client_start(c, *c.transport, to, cfg, c.now);
      c.stuffed_set = std::move(kept);
      c.redirects = sent;
      return;
    }
  }
  if (!c.cfg.via.empty()) {
    ge::forward::dial_update(c.dial, *c.transport, now_us);
    if (c.dialing && c.dial.state == ge::forward::DialState::routed) {
      c.dialing = false;
      c.server = c.dial.first;  // the game's handshake goes to the first hop
    } else if (c.dialing && (c.dial.state == ge::forward::DialState::refused ||
                             c.dial.state == ge::forward::DialState::timed_out)) {
      c.dialing = false;
      c.stage = ClientStage::dropped;
      c.refused = "the route through the forwarders failed: " + c.dial.why;
    }
    if (c.dialing) {
      c.last_tick = c.now;
      return;
    }
  }
  if (wd::web_busy(c.http)) step_http(c);
  // A connected server that has gone quiet: given up, so the game can say so and leave.
  if ((c.stage == ClientStage::signon || c.stage == ClientStage::active) && c.cfg.timeout > 0 &&
      c.now - c.ch.last_received > c.cfg.timeout) {
    c.stage = ClientStage::dropped;
    c.refused = "the server stopped answering";
  }
  switch (c.stage) {
    case ClientStage::challenge:
      if (c.last_ask < 0 || c.now - c.last_ask >= kRetry) {
        c.last_ask = c.now;
        send_oob(c, "getchallenge\n");
      }
      break;
    case ClientStage::connect:
      if (c.now - c.last_ask >= kRetry) {
        c.last_ask = c.now;
        send_connect(c);
      }
      break;
    case ClientStage::signon:
    case ClientStage::active:
      send_packet(c, now_us);
      break;
    case ClientStage::dropped:
      break;
  }
  c.last_tick = c.now;
}

std::string q2_download(Q2ClientEnd& c, const std::string& file) {
  if (file.size() > 63) return "File name too long";
  std::string path = file;
  for (char& ch : path) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  for (char& ch : path)
    if (ch == '\\') ch = '/';
  const auto name_char = [](char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '-';
  };
  static constexpr const char* kKinds[] = {"bsp", "dm2", "ent", "jpg", "loc", "md2", "md3", "ogg",
                                           "pcx", "png", "sp2", "tga", "txt", "wal", "wav"};
  const size_t dot = path.rfind('.');
  const std::string kind = dot == std::string::npos ? std::string() : path.substr(dot + 1);
  const bool known = std::any_of(std::begin(kKinds), std::end(kKinds),
                                 [&kind](const char* k) { return kind == k; });
  if (path.empty() || !name_char(path.front()) || !name_char(path.back()) ||
      path.find("..") != std::string::npos || path.find('/') == std::string::npos || !known ||
      path.find("//") != std::string::npos)
    return "Invalid quake path";
  if (!lacks(c, path) || path == c.fetching ||
      std::find(c.fetch_queue.begin(), c.fetch_queue.end(), path) != c.fetch_queue.end())
    return "File exists";
  if (path.rfind("save/", 0) == 0 || path.rfind("music/", 0) == 0 || path.rfind("video/", 0) == 0)
    return "Operation not permitted";
  c.fetch_queue.push_back(path);
  if (c.fetching.empty()) next_fetch(c);
  return {};
}

void q2_client_stop(Q2ClientEnd& c) {
  wd::web_stop(c.http);
  if (c.stage == ClientStage::signon || c.stage == ClientStage::active) {
    const uint8_t bye[] = {kClcStringCmd, 'd', 'i', 's', 'c', 'o', 'n', 'n', 'e', 'c', 't', 0};
    for (int k = 0; k < 3; k++) {
      uint8_t out[kMaxPacket];
      const size_t n = channel_transmit(c.ch, bye, sizeof bye, c.now, out);
      send_raw(c, out, n);
    }
    c.stage = ClientStage::dropped;
    if (c.refused.empty()) c.refused = "left";
  }
  if (c.recording) {
    for (int k = 0; k < 4; k++) q2_demo(c).bytes.push_back(0xFF);
    c.recording = false;
  }
}

}  // namespace shooters_edge::net
