// The scripted Quake II client (q2_client.hpp): the connectionless steps, the signon as the
// server stuffs it, the payloads read as Quake II's recording, and the moves.
//
// ge:summary The scripted Quake II client's steps: connect, the stuffed signon, frames, moves
// ge:keywords q2-client q2_client_tick stufftext signon begin move ack-frame
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#include "shooters_edge/net/q2_client.hpp"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "net/voice_trial.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/qw_ext.hpp"
#include "shooters_edge/net/qw_voice_route.hpp"

namespace shooters_edge::net {

namespace cl = shooters_edge::client;

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
  if (c.cfg.spectator)
    info_set(u, "spectator", c.cfg.spectator_password.empty() ? "1" : c.cfg.spectator_password);
  if (!c.cfg.password.empty()) info_set(u, "password", c.cfg.password);
  const uint32_t ext =
      (c.cfg.fetch && c.cfg.ask_origins ? kGeDownloadOrigin : 0u) |
      (c.cfg.ask_moves ? kGeOtherMoves : 0u) | (c.cfg.speedjump ? kGeSpeedJumpShare : 0u) |
      (c.cfg.ask_login ? kGeAccountKey : 0u) | (c.cfg.ask_feel ? kGeMoveFeel : 0u) |
      (c.cfg.voice ? kGeVoice : 0u) | (c.cfg.ask_frames ? kGeEveryFrame : 0u) |
      (c.cfg.ask_back ? kGeItemBack : 0u);
  if (ext != 0) info_set(u, "*ge_ext", std::to_string(ext));
  return u;
}

void send_connect(Q2ClientEnd& c) {
  send_oob(c, "connect " + std::to_string(kQ2NetProtocol) + " " + std::to_string(c.cfg.qport) +
                  " " + std::to_string(c.challenge) + " \"" + userinfo_of(c) + "\"\n");
}

void connected(Q2ClientEnd& c) {
  c.stage = ClientStage::signon;
  c.connected_at = c.now;
  c.ch.trace = c.cfg.trace;
  channel_setup(c.ch, true, c.cfg.qport, c.now);
  channel_set_rate(c.ch, 0);
  q2_client_command(c, "new");
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
    c.stage = ClientStage::connect;
    c.last_ask = c.now;
    send_connect(c);
  } else if (w[0] == "client_connect" && c.stage == ClientStage::connect) {
    connected(c);
  }
}

// THE DOWNLOADS: the next file asked for, or the held `begin` sent once none is left.
void next_fetch(Q2ClientEnd& c) {
  c.fetching.clear();
  c.fetched_bytes.clear();
  c.fetched_percent = 0;
  c.fetched_origin = false;
  if (!c.fetch_queue.empty()) {
    c.fetching = c.fetch_queue.front();
    c.fetch_queue.erase(c.fetch_queue.begin());
    q2_client_command(c, "download " + c.fetching);
    return;
  }
  if (!c.held_begin.empty()) q2_client_command(c, c.held_begin);
  c.held_begin.clear();
}

bool lacks(const Q2ClientEnd& c, const std::string& path) {
  return c.downloaded.count(path) == 0 && (!c.cfg.read_file || c.cfg.read_file(path).empty());
}

// `precache`: the files lacked queued, `begin` held until they are fetched.
void start_fetch(Q2ClientEnd& c, const std::string& begin) {
  c.fetch_queue.clear();
  const std::vector<std::string>& models = q2_demo(c).state.models;
  if (models.size() > 1 && models[1].rfind("maps/", 0) == 0 && lacks(c, models[1]))
    c.fetch_queue.push_back(models[1]);
  if (c.cfg.fetch_level) {  // what the level names: models, sounds and the players' figures
    const auto queue = [&c](const std::string& path) {
      if (path.empty() || path[0] == '*' || path[0] == '#' || path.find('/') == std::string::npos)
        return;
      if (lacks(c, path) &&
          std::find(c.fetch_queue.begin(), c.fetch_queue.end(), path) == c.fetch_queue.end())
        c.fetch_queue.push_back(path);
    };
    const cl::DemoState& st = q2_demo(c).state;
    for (size_t i = 2; i < models.size(); i++) queue(models[i]);
    for (size_t i = 1; i < st.sounds.size(); i++)
      if (!st.sounds[i].empty() && st.sounds[i][0] != '*') queue("sound/" + st.sounds[i]);
    for (const std::string& look : st.looks) {
      const size_t slash = look.find('/');
      if (slash == std::string::npos || slash == 0 || slash + 1 >= look.size()) continue;
      const std::string figure = look.substr(0, slash);
      queue("players/" + figure + "/tris.md2");
      queue("players/" + figure + "/" + look.substr(slash + 1) + ".pcx");
    }
  }
  for (const std::string& w : c.cfg.want)
    if (lacks(c, w) &&
        std::find(c.fetch_queue.begin(), c.fetch_queue.end(), w) == c.fetch_queue.end())
      c.fetch_queue.push_back(w);
  c.held_begin = begin;
  next_fetch(c);
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
      next_fetch(c);
      continue;
    }
    c.fetched_bytes.insert(c.fetched_bytes.end(), p.bytes.begin(), p.bytes.end());
    c.fetched_percent = p.percent;
    if (p.percent < 100) {
      q2_client_command(c, "nextdl");
      continue;
    }
    c.downloaded[c.fetching] = c.fetched_bytes;
    if (c.cfg.keep_file)
      c.cfg.keep_file(c.fetching, c.fetched_bytes, c.fetched_origin ? &c.fetching_origin : nullptr);
    next_fetch(c);
  }
  q2_demo(c).download_pieces.clear();
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
      const std::string line = text.substr(from, end - from);
      from = end + 1;
      if (line.empty()) continue;
      if (c.stuffed.size() < kKept) c.stuffed.push_back(line);
      const std::vector<std::string> w = q2_words(line);
      if (w.empty()) continue;
      if (w[0] == "cmd" && line.size() > 4) {
        q2_client_command(c, expand_settings(c, line.substr(4)));
        c.signon_steps++;
      } else if (w[0] == "precache") {
        const std::string begin = "begin" + (w.size() > 1 ? " " + w[1] : std::string());
        if (c.cfg.fetch)
          start_fetch(c, begin);
        else
          q2_client_command(c, begin);
        c.signon_steps++;
      } else if (w[0] == "reconnect") {
        c.stage = ClientStage::signon;
        q2_client_command(c, "new");
      } else if (w[0] == "connect" && w.size() > 1) {  // sent elsewhere (sv_redirect)
        c.sent_on = w[1];
      }
    }
  }
  q2_demo(c).stuffed.clear();
}

// One payload as one block of the recording, then read.
void read_payload(Q2ClientEnd& c, const uint8_t* data, size_t size) {
  if (size == 0) return;  // a keep-alive
  const int levels = q2_demo(c).state.levels;
  const auto len = static_cast<uint32_t>(size);
  for (int k = 0; k < 4; k++) q2_demo(c).bytes.push_back(static_cast<uint8_t>(len >> (8 * k)));
  q2_demo(c).bytes.insert(q2_demo(c).bytes.end(), data, data + size);
  const size_t block_at = c.level_record != nullptr ? c.level_record->size() : 0;
  if (c.level_record != nullptr) {
    for (int k = 0; k < 4; k++) c.level_record->push_back(static_cast<uint8_t>(len >> (8 * k)));
    c.level_record->insert(c.level_record->end(), data, data + size);
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
    c.sent[c.ch.outgoing_sequence & 63] = m;
    c.sent_at[c.ch.outgoing_sequence & 63] = c.now;
    int32_t acked = q2_demo(c).last_frame;
    if (c.cfg.whole_every > 0 && c.now - c.last_whole >= c.cfg.whole_every) c.last_whole = c.now;
    // For 150 ms (past the server's next 10 Hz frame, whatever moves come between) no frame is
    // acknowledged, so the server's next one is whole.
    if (c.cfg.whole_every > 0 && c.now - c.last_whole < 0.15) acked = -1;
    write_q2_move(w, acked, c.cmds, c.ch.outgoing_sequence);
    c.moves++;
    speak(c, w, m.msec * gn::kVoiceRate / 1000);
  }
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
    size_t at = 0;
    if (!channel_process(c.ch, buf, size, c.now, at)) continue;
    if (c.stage == ClientStage::active && c.moves > 0)
      c.latency = c.now - c.sent_at[c.ch.incoming_acknowledged & 63];
    read_payload(c, buf + at, size - at);
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
      q2_client_start(c, *c.transport, to, cfg, c.now);
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

void q2_client_stop(Q2ClientEnd& c) {
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
