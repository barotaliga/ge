// The scripted classic client (classic_client.hpp): the connectionless steps, the signon as the
// server stuffs it, downloads in both forms, the frames and the moves.
//
// ge:summary The scripted classic client's steps, message handling, downloads and moves
// ge:keywords client_tick client_start client_command default_script stufftext skins download
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_server_test.cpp
#include "shooters_edge/net/classic_client.hpp"

#include <cstdio>
#include <cstring>

#include "net/voice_trial.hpp"
#include "progs/text.hpp"
#include "shooter/usercmd.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "shooters_edge/net/classic_record.hpp"

namespace shooters_edge::net {

namespace cl = shooters_edge::client;

namespace pg = ge::progs;

namespace {

constexpr double kRetry = 1.0;       // s between connectionless retries
constexpr double kChunkRetry = 0.5;  // s before a chunk is asked for again
constexpr int kChunksPerTick = 8;

std::string hex32(uint32_t v) {
  char b[16];
  std::snprintf(b, sizeof b, "0x%x", v);
  return b;
}

void send_raw(ClassicClientEnd& c, const uint8_t* data, size_t size) {
  gn::transport_send(*c.transport, c.server, data, size, static_cast<int64_t>(c.now * 1e6));
  c.bytes_out += static_cast<int64_t>(size);
}

void send_oob_text(ClassicClientEnd& c, const std::string& text) {
  std::vector<uint8_t> b = {0xff, 0xff, 0xff, 0xff};
  b.insert(b.end(), text.begin(), text.end());
  b.push_back(0);
  send_raw(c, b.data(), b.size());
}

std::string userinfo_of(const ClassicClientEnd& c) {
  std::string u;
  info_set(u, "name", c.cfg.name);
  info_set(u, "skin", c.cfg.skin);
  info_set(u, "topcolor", std::to_string(c.cfg.topcolor));
  info_set(u, "bottomcolor", std::to_string(c.cfg.bottomcolor));
  info_set(u, "rate", std::to_string(c.cfg.rate));
  info_set(u, "msg", "1");
  if (!c.cfg.team.empty()) info_set(u, "team", c.cfg.team);
  if (c.cfg.spectator) info_set(u, "spectator", "1");
  if (!c.cfg.password.empty()) info_set(u, "password", c.cfg.password);
  if (c.cfg.ask[ExtFamily::z]) info_set(u, "*z_ext", std::to_string(c.cfg.ask[ExtFamily::z]));
  return u;
}

void send_connect(ClassicClientEnd& c) {
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

bool has_file(const ClassicClientEnd& c, const std::string& path) {
  for (const std::string& f : c.cfg.files)
    if (f == path) return true;
  return c.downloaded.count(path) != 0;
}

void next_skin_or_begin(ClassicClientEnd& c) {
  while (!c.wanted_skins.empty()) {
    const std::string path = c.wanted_skins.front();
    c.wanted_skins.erase(c.wanted_skins.begin());
    if (has_file(c, path)) continue;
    c.dl = ClientDownload{};
    c.dl.name = path;
    c.dl.active = true;
    c.dl.chunked = c.ext.has(ExtFamily::x, kXChunkedDl);
    c.dl.number = static_cast<int>(c.downloaded.size()) + 1;
    client_command(c, "download " + path);
    return;
  }
  if (!c.begun) {
    c.begun = true;
    client_command(c, "begin " + std::to_string(c.spawncount));
  }
}

void start_skins(ClassicClientEnd& c) {
  c.wanted_skins.clear();
  for (const std::string& u : c.userinfos) {
    const std::string skin = info_get(u, "skin");
    if (u.empty() || skin.empty()) continue;
    const std::string path = "skins/" + skin + ".pcx";
    bool dup = false;
    for (const std::string& w : c.wanted_skins) dup = dup || w == path;
    if (!dup && !has_file(c, path)) c.wanted_skins.push_back(path);
  }
  next_skin_or_begin(c);
}

void finish_download(ClassicClientEnd& c, bool ok) {
  if (ok) {
    c.dl.bytes.resize(static_cast<size_t>(c.dl.size));
    c.downloaded[c.dl.name] = c.dl.bytes;
    if (c.dl.chunked) client_command(c, "nextdl -1 100 " + std::to_string(c.dl.number));
  }
  c.dl = ClientDownload{};
  next_skin_or_begin(c);
}

void download_chunk(ClassicClientEnd& c, int chunk, const uint8_t* data) {
  if (!c.dl.active || !c.dl.chunked || c.dl.size < 0) return;
  const size_t n = c.dl.have.size();
  if (chunk < 0 || static_cast<size_t>(chunk) >= n || c.dl.have[static_cast<size_t>(chunk)]) return;
  std::memcpy(c.dl.bytes.data() + static_cast<size_t>(chunk) * kChunkBytes, data, kChunkBytes);
  c.dl.have[static_cast<size_t>(chunk)] = true;
  for (bool h : c.dl.have)
    if (!h) return;
  finish_download(c, true);
}

void on_download(ClassicClientEnd& c, const SvcMsg& m) {
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
  if (m.ints[1] >= 100) {
    c.dl.size = static_cast<int>(c.dl.bytes.size());
    finish_download(c, true);
  } else {
    client_command(c, "nextdl");
  }
}

void on_stufftext(ClassicClientEnd& c, const std::string& text) {
  c.stufftexts++;
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    const std::string line = text.substr(at, end - at);
    at = end + 1;
    pg::Words w;
    pg::tokenize_words(line.c_str(), w);
    const std::string cmd = pg::word(w, 0);
    if (cmd == "cmd") {
      const std::string rest = line.substr(line.find("cmd") + 4);
      if (rest == "pext") {
        std::string ans = "pext";
        if (c.cfg.mode == ClientExtMode::pext)
          for (ExtFamily f : {ExtFamily::x, ExtFamily::x2, ExtFamily::mvd})
            if (c.cfg.ask[f]) ans += " " + hex32(family_magic(f)) + " " + hex32(c.cfg.ask[f]);
        client_command(c, ans);
      } else {
        client_command(c, rest);
      }
    } else if (cmd == "fullserverinfo") {
      c.serverinfo = pg::word(w, 1);
      const std::string z = info_get(c.serverinfo, "*z_ext");
      c.ext[ExtFamily::z] = c.cfg.ask[ExtFamily::z] & static_cast<uint32_t>(std::atoi(z.c_str()));
    } else if (cmd == "skins") {
      start_skins(c);
    } else if (cmd == "reconnect" || cmd == "changing") {
      if (cmd == "reconnect") client_command(c, "new");
    }
  }
}

void on_list(ClassicClientEnd& c, const SvcMsg& m, bool models) {
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
    client_command(c, "prespawn " + sc + " 0 0");
  }
}

void on_entities(ClassicClientEnd& c, gn::Reader& r, bool delta) {
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

void apply(ClassicClientEnd& c, const SvcMsg& m) {
  switch (m.id) {
    case svc::serverdata:
      for (const auto& [magic, bits] : m.ext) {
        ExtFamily f{};
        if (family_of_magic(magic, f)) c.ext[f] = bits;
      }
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
      if (m.ints[0] >= 0 && m.ints[0] < 32) c.userinfos[m.ints[0]] = m.strings[0];
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
    case svc::centerprint:
      c.centerprints.push_back(m.strings[0]);
      break;
    case svc::playerinfo:
      if (m.player.slot >= 0 && m.player.slot < 32) {
        c.players[m.player.slot] = m.player;
        c.player_seen[m.player.slot] = true;
      }
      break;
    case svc::download:
      on_download(c, m);
      break;
    case svc::disconnect:
      c.stage = ClientStage::dropped;
      c.refused = "disconnected by the server";
      break;
    default:
      break;
  }
}

void read_payload(ClassicClientEnd& c, const uint8_t* data, size_t size, size_t at) {
  gn::Reader r = gn::make_reader(data, size);
  r.at = at;
  WireCtx ctx;
  std::vector<std::pair<size_t, size_t>> messages;  // each message's bytes, for a recording
  const auto recorded = [&] {
    if (c.record) record_packet(*c.record, c.now, {data, at}, {data, size}, messages);
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
void speak(ClassicClientEnd& c, int samples) {
  const bool talking = c.cfg.talk != nullptr && c.cfg.talk(c.tick, c.cfg.seed);
  for (int i = 0; i < samples; i++) {
    const bool say = talking && !c.speech.empty();
    c.mic.push_back(say ? c.speech[c.speech_at] : 0.0f);
    if (say) c.speech_at = (c.speech_at + 1) % c.speech.size();
  }
  ClcMsg m;
  m.id = clc::voice;
  m.gen = cl::kVoiceOpus << 4;
  m.seq = c.voice_seq;
  int frames = 0;
  while (c.mic.size() >= static_cast<size_t>(gn::kVoiceFrame)) {
    if (talking && c.encoder) {
      uint8_t frame[gn::kVoiceMaxBytes];
      const int n = c.encoder->encode(c.mic.data(), frame, sizeof frame);
      if (n > 0) {
        m.data.push_back(static_cast<uint8_t>(n & 0xFF));
        m.data.push_back(static_cast<uint8_t>(n >> 8));
        m.data.insert(m.data.end(), frame, frame + n);
        frames++;
      }
    }
    c.mic.erase(c.mic.begin(), c.mic.begin() + gn::kVoiceFrame);
  }
  if (frames == 0) return;
  c.voice_seq = (c.voice_seq + frames) & 0xFF;
  c.voice_sent += frames;
  uint8_t buf[kMaxMsgLen];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  write_clc(w, m);
  if (w.overflow) return;
  // Unreliable, like a move: a frame late is a frame lost.
  c.voice_out.insert(c.voice_out.end(), buf, buf + w.size);
  if (c.record && c.playernum >= 0) record_own_voice(*c.record, c.playernum, m);
}

void read_oob(ClassicClientEnd& c, const uint8_t* data, size_t size) {
  if (size == 1 && data[0] == 'l') return;
  if (size < 5) return;
  const char type = static_cast<char>(data[4]);
  if (size > 10 && std::memcmp(data + 4, "\\chunk", 6) == 0) {
    gn::Reader r = gn::make_reader(data, size);
    r.at = 10;
    read_long(r);  // the download's number
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
    c.stage = ClientStage::signon;
    c.connected_at = c.now;
    channel_setup(c.ch, true, c.cfg.qport, c.now);
    channel_set_rate(c.ch, 0);
    client_command(c, "new");
  } else if (type == 'n' && c.stage == ClientStage::connect) {
    c.refused = std::string(reinterpret_cast<const char*>(data) + 5,
                            strnlen(reinterpret_cast<const char*>(data) + 5, size - 5));
    c.stage = ClientStage::dropped;
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

void client_start(ClassicClientEnd& c, gn::Transport& t, const gn::Address& server,
                  const ClientConfig& cfg, double now) {
  c = ClassicClientEnd{};
  c.cfg = cfg;
  c.transport = &t;
  c.server = server;
  c.now = now;
  if (cfg.voice) {
    c.cfg.ask[ExtFamily::x2] |= kX2Voice;
    c.encoder = std::make_unique<gn::VoiceEncoder>();
    std::string error;
    if (!c.encoder->open(gn::VoiceCodecConfig{}, error)) c.encoder.reset();
    c.speech = cfg.speech ? *cfg.speech
                          : gn::voice_trial_speech(static_cast<uint32_t>(cfg.seed) * 7919u + 17u,
                                                   6.0, 100.0 + 25.0 * (cfg.seed % 5));
  }
}

void client_command(ClassicClientEnd& c, const std::string& line) {
  uint8_t buf[1100];
  gn::Writer w = gn::make_writer(buf, sizeof buf);
  write_byte(w, clc::stringcmd);
  write_string(w, line);
  channel_reliable(c.ch, buf, w.size);
}

void client_tick(ClassicClientEnd& c, int64_t now_us) {
  c.now = static_cast<double>(now_us) / 1e6;
  c.tick++;
  gn::transport_pump(*c.transport, now_us);
  uint8_t buf[gn::kMaxDatagram];
  gn::Address from;
  while (true) {
    const int n = gn::transport_receive(*c.transport, buf, sizeof buf, from, now_us);
    if (n <= 0) break;
    const auto size = static_cast<size_t>(n);
    c.datagrams_in++;
    c.bytes_in += n;
    if (from != c.server) continue;
    if (is_out_of_band(buf, size) || (size == 1 && buf[0] == 'l')) {
      read_oob(c, buf, size);
      continue;
    }
    if (c.stage != ClientStage::signon && c.stage != ClientStage::active) continue;
    size_t at = 0;
    if (!channel_process(c.ch, buf, size, c.now, at)) continue;
    read_payload(c, buf, size, at);
    if (!c.error.empty()) {
      c.stage = ClientStage::dropped;
      c.refused = "could not read the server: " + c.error;
    }
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
    sh::MoveCmd cmd = usercmd_grid(c.cfg.script(c.tick, c.cfg.seed));
    cmd.msec = 13;
    c.cmds[2] = cmd;
    const int loss =
        c.ch.accepted ? static_cast<int>(100 * c.ch.dropped / (c.ch.accepted + c.ch.dropped)) : 0;
    write_move(w, c.cmds, static_cast<uint8_t>(loss), c.ch.outgoing_sequence);
    if (c.record) record_command(*c.record, c.now, cmd);
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
    for (size_t i = 0; i < c.dl.have.size() && asked < kChunksPerTick; i++) {
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
