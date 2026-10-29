// The voice trial's talkers, its session and what it measures (net/voice_trial.hpp).
//
// ge:summary The voice trial: the speech source, the session of talkers and listeners, the measures
// ge:keywords net voice trial speech formant vowel syllable word session relay tap snr latency
// ge:skill net audio
// ge:test tests/net_voice_test.cpp
#include "net/voice_trial.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include "audio/synth.hpp"
#include "core/libm/libm.hpp"
#include "core/prng.hpp"
#include "net/sim_network.hpp"
#include "net/transport.hpp"

namespace ge::net {

namespace {

namespace libm = core::libm;

constexpr double kPi = 3.14159265358979323846;
constexpr int64_t kTickUs = 10000;
constexpr int kTickSamples = kVoiceRate / 100;
constexpr float kSpacingM = 15;  // the clients stand along a line, this far apart by seat

double db10(double ratio) { return 4.3429448190325182 * libm::log(ratio); }  // 10 log10

uint64_t fnv(uint64_t h, uint64_t v) {
  for (int i = 0; i < 8; i++) h = (h ^ ((v >> (8 * i)) & 0xff)) * 0x100000001b3ull;
  return h;
}

struct Window {
  size_t from = 0, to = 0;
};

// A voice: a sawtooth buzz at a gliding pitch through two resonances that move from vowel to vowel,
// a syllable at a time, in words with pauses between. Not speech, but speech's shape for a codec
// and a gate: a pitched source, formants, onsets, and silence.
std::vector<float> speech(uint32_t seed, double seconds, double f0, std::vector<Window>* words) {
  static constexpr double kVowels[][2] = {{730, 1090}, {270, 2290}, {300, 870}, {530, 1840},
                                          {570, 840},  {440, 1020}, {660, 1720}};
  const auto n = static_cast<size_t>(seconds * kVoiceRate);
  std::vector<float> out(n, 0.0F);
  core::Rng r = core::make_rng(seed);
  double phase = 0, y1[2] = {0, 0}, y2[2] = {0, 0};
  auto at = static_cast<size_t>(core::range(r, 0.2, 0.8) * kVoiceRate);
  double voiced = 0;
  size_t voiced_n = 0;
  while (at < n) {
    const auto len = static_cast<size_t>(core::range(r, 0.6, 2.2) * kVoiceRate);
    const Window w{at, std::min(n, at + len)};
    if (words) words->push_back(w);
    size_t t = w.from;
    while (t < w.to) {
      const auto syl = static_cast<size_t>(core::range(r, 0.12, 0.26) * kVoiceRate);
      const size_t end = std::min(w.to, t + syl);
      const double* v = kVowels[core::pick_index(r, 7)];
      const double fa = f0 * core::range(r, 0.9, 1.15), fb = f0 * core::range(r, 0.85, 1.1);
      double c1[2], c2[2];
      for (int k = 0; k < 2; k++) {  // a two-pole resonance at the formant, 90 Hz wide
        const double rad = libm::exp(-kPi * 90 / kVoiceRate);
        c1[k] = 2 * rad * libm::cos(2 * kPi * v[k] / kVoiceRate);
        c2[k] = -rad * rad;
      }
      const double span = static_cast<double>(end - t);
      for (size_t i = t; i < end; i++) {
        const double u = static_cast<double>(i - t) / span;
        phase += (fa + (fb - fa) * u) / kVoiceRate;
        if (phase >= 1) phase -= 1;
        const double x = 2 * phase - 1 + 0.05 * (core::next(r) - 0.5);
        const double env = std::min({1.0, static_cast<double>(i - t) / (0.015 * kVoiceRate),
                                     static_cast<double>(end - i) / (0.03 * kVoiceRate)});
        double sum = 0;
        for (int k = 0; k < 2; k++) {
          const double y = x + c1[k] * y1[k] + c2[k] * y2[k];
          y2[k] = y1[k];
          y1[k] = y;
          sum += y * (k == 0 ? 1.0 : 0.5);
        }
        out[i] = static_cast<float>(sum * env);
        voiced += sum * env * sum * env;
        voiced_n++;
      }
      t = end;
    }
    at = w.to + static_cast<size_t>(core::range(r, 0.4, 1.4) * kVoiceRate);
  }
  // Spoken at -20 dBFS, RMS over the words.
  if (voiced_n) {
    const double scale = 0.1 / std::sqrt(voiced / static_cast<double>(voiced_n));
    for (float& x : out) x = static_cast<float>(x * scale);
  }
  return out;
}

NodeConfig node_config(bool host, uint64_t seed) {
  NodeConfig c;
  c.protocol = protocol_id("voice-trial", 1);
  c.accept = host;
  c.max_peers = host ? kMaxPeers : 1;
  c.channels.count = 2;
  c.channels.delivery[0] = Delivery::reliable;    // the session
  c.channels.delivery[1] = Delivery::unreliable;  // the voice
  c.timeout_us = 5000000;
  c.seed = seed;
  return c;
}

struct Client {
  Transport t;
  Node n;
  SessionClient s;
  VoiceSender send;
  std::unique_ptr<VoiceListener> hear = std::make_unique<VoiceListener>();
  std::unique_ptr<audio::Synth> synth;
  std::vector<float> speech;
  std::vector<Window> words;
  size_t at = 0;
  ChatScope scope = ChatScope::all;
  int to = -1;                                // direct: the client heard
  std::vector<int64_t> frame_end;             // by seq: when the frame's last sample was spoken
  VoiceDecoder clean;                         // keep_audio: every frame sent, decoded in order
  std::vector<std::vector<float>> clean_pcm;  // by seq
  // Per talker seat, as this client hears them.
  uint64_t spurts_seen[kMaxPlayers] = {};
  double snr_sum[kMaxPlayers] = {}, heard_sq[kMaxPlayers] = {}, clean_sq[kMaxPlayers] = {};
  uint64_t snr_n[kMaxPlayers] = {}, level_n[kMaxPlayers] = {};
};

struct Run {
  const VoiceTrialConfig* k = nullptr;
  std::vector<std::unique_ptr<Client>> clients;
  int client_of_seat[kMaxPlayers];
  VoiceTrial* o = nullptr;
  int64_t now = 0;
  bool talking = false;
  int ear = 0;  // the client whose mix is kept
};

struct PullTap {
  Run* run;
  int client;
};

bool hears(void* ctx, int speaker, int listener) {
  const float far = *static_cast<const float*>(ctx);
  const float d =
      kSpacingM * static_cast<float>(speaker > listener ? speaker - listener : listener - speaker);
  return d <= far;
}

// Whether talker `t`'s route names listener `l` (the client at `li`).
bool should_hear(const Client& l, const Client& t, int li, const SessionHost& host, float far) {
  if (l.s.me < 0 || t.s.me < 0) return false;
  switch (t.scope) {
    case ChatScope::all:
      return true;
    case ChatScope::team:
      return host.players[l.s.me].team == host.players[t.s.me].team;
    case ChatScope::direct:
      return t.to == li;
    case ChatScope::nearby:
      return hears(&far, t.s.me, l.s.me);
  }
  return false;
}

void on_pull(void* ctx, int seat, const VoicePulled& p, const float* pcm) {
  const PullTap& tap = *static_cast<const PullTap*>(ctx);
  Run& run = *tap.run;
  Client& me = *run.clients[static_cast<size_t>(tap.client)];
  const VoiceTalker& t = me.hear->talkers[seat];
  if (t.stats.spurts != me.spurts_seen[seat]) {
    me.spurts_seen[seat] = t.stats.spurts;
    run.o->delay_ms.push_back(t.target * 20.0);
  }
  if (p.play == VoicePlay::silent) return;
  const int from = run.client_of_seat[seat];
  if (from < 0) return;
  const Client& talker = *run.clients[static_cast<size_t>(from)];
  if (p.seq >= talker.frame_end.size()) return;
  if (p.play != VoicePlay::concealed) {
    // The frame's first sample was spoken 20 ms before its end, and is heard once the samples
    // already queued on the stream ahead of it have played.
    const int queued = me.synth->queued(me.hear->streams[seat]);
    const double heard_us = static_cast<double>(run.now) + queued * 1e6 / kVoiceRate;
    const double spoken_us = static_cast<double>(talker.frame_end[p.seq]) - 20000;
    run.o->latency_ms.push_back((heard_us - spoken_us) / 1000);
  }
  if (!run.k->keep_audio || p.seq >= talker.clean_pcm.size()) return;
  const std::vector<float>& c = talker.clean_pcm[p.seq];
  double sig = 0, err = 0, mine = 0;
  for (int i = 0; i < kVoiceFrame; i++) {
    const double a = c[static_cast<size_t>(i)], b = pcm[i];
    sig += a * a;
    err += (b - a) * (b - a);
    mine += b * b;
  }
  me.heard_sq[seat] += mine;
  me.clean_sq[seat] += sig;
  me.level_n[seat]++;
  if (sig / kVoiceFrame < 3.2e-5) return;  // under -45 dBFS: not a voiced frame
  const double snr = db10(sig / std::max(err, 1e-12));
  me.snr_sum[seat] += std::clamp(snr, -10.0, 35.0);
  me.snr_n[seat]++;
}

void tick(Run& run, Transport& /*ht*/, Node& hn, SessionHost& host, VoiceRelay* relay) {
  node_update(hn, run.now);
  session_host_update(host, run.now);
  if (relay) voice_relay_update(*relay, run.now);
  for (size_t i = 0; i < run.clients.size(); i++) {
    Client& c = *run.clients[i];
    node_update(c.n, run.now);
    session_client_update(c.s, run.now);
    if (!run.talking) continue;
    voice_listener_receive(*c.hear, run.now);
    if (i == 0) {  // push to talk: the key held through every word
      bool held = false;
      for (const Window& w : c.words) held = held || (c.at >= w.from && c.at < w.to);
      voice_sender_key(c.send, held);
    }
    const size_t take =
        std::min<size_t>(kTickSamples, c.speech.size() - std::min(c.at, c.speech.size()));
    float silence[kTickSamples] = {};
    voice_sender_push(c.send, take ? c.speech.data() + c.at : silence, kTickSamples);
    c.at += kTickSamples;
    VoiceMessage m;
    while (voice_sender_pop(c.send, m)) {
      const auto seq = static_cast<uint16_t>(m.bytes[2] | (m.bytes[3] << 8));
      if (c.frame_end.size() <= seq) c.frame_end.resize(seq + 1u, 0);
      c.frame_end[seq] = run.now;
      if (run.k->keep_audio) {
        if (c.clean_pcm.size() <= seq) c.clean_pcm.resize(seq + 1u);
        c.clean_pcm[seq].assign(kVoiceFrame, 0.0F);
        c.clean.decode(m.bytes + kVoiceHeader, m.size - kVoiceHeader, c.clean_pcm[seq].data());
      }
      node_send(c.n, c.s.peer, 1, m.bytes, m.size);
      run.o->sent++;
    }
    voice_listener_mix(*c.hear, *c.synth, 0, run.now);
    float block[2 * kTickSamples];
    c.synth->render(block, kTickSamples);
    if (static_cast<int>(i) == run.ear && run.k->keep_audio)
      run.o->mix.insert(run.o->mix.end(), block, block + 2 * kTickSamples);
  }
  node_flush(hn, run.now);
  for (auto& c : run.clients) node_flush(c->n, run.now);
}

}  // namespace

std::vector<float> voice_trial_speech(uint32_t seed, double seconds, double f0_hz) {
  return speech(seed, seconds, f0_hz, nullptr);
}

VoiceTrial run_voice_trial(const VoiceTrialConfig& k) {
  VoiceTrial o;
  const int n = std::clamp(k.clients, 2, kMaxPlayers);
  SimNetwork net = make_sim_network(k.seed, k.link);
  Transport ht;
  Node hn;
  SessionHost host;
  open_sim_transport(ht, net, sim_address(0, 1));
  node_init(hn, node_config(true, uint64_t{k.seed} * 1000), ht);
  float far = k.far_m;
  SessionConfig sc;
  sc.channel = 0;
  sc.team_count = 2;
  sc.need_ready = false;
  sc.hears = hears;
  sc.hears_ctx = &far;
  sc.seed = k.seed;
  session_host_init(host, sc, hn, 0);
  Run run;
  run.k = &k;
  run.o = &o;
  std::vector<PullTap> taps(static_cast<size_t>(n));
  static constexpr double kPitch[] = {110, 205, 140, 240, 125, 180, 160, 220};
  for (int i = 0; i < n; i++) {
    auto c = std::make_unique<Client>();
    const auto id = static_cast<uint32_t>(i + 1);
    open_sim_transport(c->t, net, sim_address(id, 1));
    node_init(c->n, node_config(false, uint64_t{k.seed} * 1000 + id), c->t);
    session_client_init(c->s, c->n, 0);
    VoiceConfig vc = k.voice;
    vc.mode = i == 0 ? VoiceMode::push_to_talk : VoiceMode::activity;
    std::string error;
    voice_sender_init(c->send, vc, error);
    voice_listener_init(*c->hear, vc, c->n, 1);
    c->synth = std::make_unique<audio::Synth>(k.seed + id);
    c->speech = speech(k.seed * 7919 + id, k.seconds, kPitch[i % 8], &c->words);
    if (k.keep_audio) c->clean.open(error);
    taps[static_cast<size_t>(i)] = PullTap{&run, i};
    c->hear->tap = on_pull;
    c->hear->tap_ctx = &taps[static_cast<size_t>(i)];
    run.clients.push_back(std::move(c));
  }
  // The lobby: everyone seated before the first word, one at a time, so the seats (and with them
  // the teams and the places) are the join order's whatever the link.
  for (int t = 0; t < 100 * n + 1000; t++) {
    for (int i = 0; i < n; i++) {
      Client& c = *run.clients[static_cast<size_t>(i)];
      if (c.s.state != ClientState::idle) continue;
      if (i == 0 || run.clients[static_cast<size_t>(i - 1)]->s.state == ClientState::seated)
        session_client_join(c.s, sim_address(0, 1), ("talker" + std::to_string(i)).c_str(), 0,
                            run.now);
      break;
    }
    run.now += kTickUs;
    sim_advance(net, run.now);
    tick(run, ht, hn, host, nullptr);
    bool all = true;
    for (auto& c : run.clients) all = all && c->s.state == ClientState::seated;
    if (all) break;
  }
  std::fill(std::begin(run.client_of_seat), std::end(run.client_of_seat), -1);
  for (int i = 0; i < n; i++) {
    Client& c = *run.clients[static_cast<size_t>(i)];
    if (c.s.state != ClientState::seated) continue;
    o.seated++;
    run.client_of_seat[c.s.me] = i;
  }
  // The routes, by join order: everyone, the team, the next client alone, the near.
  for (int i = 0; i < n; i++) {
    Client& c = *run.clients[static_cast<size_t>(i)];
    c.scope = static_cast<ChatScope>(i % 4);
    c.to = c.scope == ChatScope::direct ? (i + 1) % n : -1;
    const int to_seat = c.to >= 0 ? run.clients[static_cast<size_t>(c.to)]->s.me : kNoPlayer;
    voice_sender_route(c.send, c.scope, to_seat);
  }
  // The ear: the client the most routes name.
  int most = -1;
  for (int li = 0; li < n; li++) {
    int count = 0;
    for (int ti = 0; ti < n; ti++)
      count += ti != li && should_hear(*run.clients[static_cast<size_t>(li)],
                                       *run.clients[static_cast<size_t>(ti)], li, host, far);
    if (count > most) {
      most = count;
      run.ear = li;
    }
  }
  o.ear = run.ear;
  // The near talkers are placed by distance on every listener.
  for (auto& lp : run.clients) {
    for (auto& tp : run.clients) {
      if (lp == tp || tp->scope != ChatScope::nearby || tp->s.me < 0 || lp->s.me < 0) continue;
      const float d = kSpacingM * static_cast<float>(std::abs(tp->s.me - lp->s.me));
      voice_listener_place(*lp->hear, tp->s.me, voice_distance_gain(d, k.near_m, k.far_m),
                           tp->s.me < lp->s.me ? -0.5F : 0.5F);
    }
  }
  VoiceRelay relay;
  voice_relay_init(relay, k.voice, host, 1);
  run.talking = true;
  const auto ticks = static_cast<int>(k.seconds * 100);
  int sample_pair_l = -1, sample_pair_t = -1;
  for (int t = 1; t <= ticks; t++) {
    run.now += kTickUs;
    sim_advance(net, run.now);
    tick(run, ht, hn, host, &relay);
    if (sample_pair_l < 0) {
      for (int a = 0; a < n && sample_pair_l < 0; a++)
        for (int b = 0; b < kMaxPlayers && sample_pair_l < 0; b++)
          if (run.clients[static_cast<size_t>(a)]->hear->talkers[b].stats.spurts) {
            sample_pair_l = a;
            sample_pair_t = b;
          }
    }
    if (t % 10 == 0 && sample_pair_l >= 0) {
      const VoiceTalker& vt =
          run.clients[static_cast<size_t>(sample_pair_l)]->hear->talkers[sample_pair_t];
      o.t_s.push_back(t / 100.0);
      o.target_ms.push_back(vt.playing ? vt.target * 20.0 : 0.0);
    }
  }

  // Who heard whom, against what each route names.
  o.routes_ok = o.seated == n;
  double snr_sum = 0, heard_sq = 0, clean_sq = 0;
  uint64_t snr_n = 0;
  o.snr_worst_db = 35;
  uint64_t h = 0xcbf29ce484222325ull;
  for (int li = 0; li < n; li++) {
    Client& l = *run.clients[static_cast<size_t>(li)];
    for (int ti = 0; ti < n; ti++) {
      if (ti == li) continue;
      const Client& t = *run.clients[static_cast<size_t>(ti)];
      if (l.s.me < 0 || t.s.me < 0) continue;
      VoicePair p;
      p.listener = l.s.me;
      p.talker = t.s.me;
      p.should = should_hear(l, t, li, host, far);
      p.stats = l.hear->talkers[t.s.me].stats;
      const bool heard = p.stats.received > 0;
      if (heard != p.should) o.routes_ok = false;
      const VoiceTalkerStats& s = p.stats;
      o.received += s.received;
      o.played += s.played;
      o.recovered += s.recovered;
      o.concealed += s.concealed;
      o.late += s.late;
      o.dropped += s.dropped;
      o.spurts += s.spurts;
      for (uint64_t v : {s.received, s.played, s.recovered, s.concealed, s.late, s.dropped,
                         s.duplicate, s.spurts})
        h = fnv(h, v);
      const int ts = t.s.me;
      if (l.snr_n[ts]) {
        p.snr_db = l.snr_sum[ts] / static_cast<double>(l.snr_n[ts]);
        o.snr_worst_db = std::min(o.snr_worst_db, p.snr_db);
        snr_sum += l.snr_sum[ts];
        snr_n += l.snr_n[ts];
      }
      if (l.level_n[ts] && l.clean_sq[ts] > 0 && l.heard_sq[ts] > 0)
        p.level_db = db10(l.heard_sq[ts] / l.clean_sq[ts]);
      heard_sq += l.heard_sq[ts];
      clean_sq += l.clean_sq[ts];
      if (heard || p.should) o.pairs.push_back(p);
    }
  }
  if (snr_n) o.snr_mean_db = snr_sum / static_cast<double>(snr_n);
  if (clean_sq > 0 && heard_sq > 0) o.level_db = db10(heard_sq / clean_sq);
  const double seconds = static_cast<double>(ticks) / 100;
  for (auto& c : run.clients)
    for (const auto& peer : c->n.peers)
      o.up_kbps += static_cast<double>(peer.conn.stats.bytes_sent);
  o.up_kbps = o.up_kbps * 8 / 1000 / seconds / n;
  for (const auto& peer : hn.peers) o.down_kbps += static_cast<double>(peer.conn.stats.bytes_sent);
  o.down_kbps = o.down_kbps * 8 / 1000 / seconds / n;
  o.relay = relay.stats;
  for (uint64_t v : {relay.stats.frames, relay.stats.copies, relay.stats.flooded,
                     relay.stats.refused, o.sent, static_cast<uint64_t>(o.latency_ms.size())})
    h = fnv(h, v);
  double lat = 0;
  for (double v : o.latency_ms) lat += v;
  o.hash = fnv(fnv(h, static_cast<uint64_t>(static_cast<int64_t>(lat * 1000))), o.routes_ok);
  for (auto& c : run.clients) voice_listener_close(*c->hear, *c->synth);
  return o;
}

}  // namespace ge::net
