// Voice chat (net/voice.hpp): the sender's gate and queue, the host's relay, the talker's jitter
// buffer and the listener's mix onto the synth.
//
// ge:summary Voice chat's sender gate, host relay by route, jitter buffer and synth mix
// ge:keywords net voice gate hangover relay route flood jitter-buffer playout target conceal
// ge:keywords recover fec late mix stream distance-gain
// ge:skill net audio
// ge:test tests/net_voice_test.cpp
#include "net/voice.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/libm/libm.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

namespace libm = core::libm;

constexpr uint8_t kUp = 1;
constexpr uint8_t kDown = 2;
constexpr uint8_t kNoSeat = 255;
constexpr int64_t kFrameUs = 1000000LL * kVoiceFrame / kVoiceRate;  // 20 000

float level_of_db(float db) {
  return static_cast<float>(libm::exp(0.11512925464970229 * static_cast<double>(db)));
}

void queue_message(VoiceSender& s, uint8_t flags, const uint8_t* frame, size_t size) {
  if (s.queue_count == kVoiceQueue) {  // the oldest goes: a voice is better late than never, not
    s.queue_head = (s.queue_head + 1) % kVoiceQueue;  // later than it needs to be
    s.queue_count--;
    s.stats.dropped++;
  }
  VoiceMessage& m = s.queue[(s.queue_head + s.queue_count) % kVoiceQueue];
  m.size = voice_write_up(m.bytes, flags, s.seq, s.clock, s.scope, s.to, frame, size);
  s.queue_count++;
  s.seq++;
  s.stats.sent++;
  s.stats.bytes += m.size;
}

void sender_frame(VoiceSender& s) {
  s.stats.frames++;
  s.clock = static_cast<uint16_t>(s.stats.frames - 1);
  double sum = 0;
  for (float x : s.held) sum += static_cast<double>(x) * x;
  const auto rms = static_cast<float>(std::sqrt(sum / kVoiceFrame));
  s.level_db = rms > 1e-6f ? static_cast<float>(8.68588963806503655 * libm::log(rms)) : -120;
  bool through = false;
  switch (s.config.mode) {
    case VoiceMode::push_to_talk:
      through = s.key;
      break;
    case VoiceMode::open:
      through = true;
      break;
    case VoiceMode::activity:
      if (rms >= s.open_level) {
        through = true;
        s.hang = s.hang_frames;
      } else if (s.gate) {
        if (rms >= s.close_level)
          s.hang = s.hang_frames;
        else
          s.hang--;
        through = s.hang > 0;
      }
      break;
  }
  if (!through && !s.gate) return;
  uint8_t flags = s.config.codec.fec ? kVoiceFec : 0;
  if (through && !s.gate) {
    flags |= kVoiceStart;
    s.stats.spurts++;
  }
  // The frame the gate closes on is sent, flagged the end: the listener stops at it instead of
  // concealing past it.
  if (!through) flags |= kVoiceEnd;
  s.gate = through;
  uint8_t frame[kVoiceMaxBytes];
  const int n = s.encoder.encode(s.held, frame, sizeof frame);
  if (n > 0) queue_message(s, flags, frame, static_cast<size_t>(n));
}

// The seat that sent a message on a host's node, or -1.
int seat_of(const SessionHost& h, int peer) { return session_seat_of_peer(h, peer); }

void relay_one(VoiceRelay& r, int seat, const uint8_t* up, size_t size, int64_t now_us) {
  if (seat < 0 || seat >= kMaxPlayers) {
    r.stats.strangers++;
    return;
  }
  if (size <= kVoiceHeader || size > kVoiceMessage || up[0] != kUp || up[6] > 3) {
    r.stats.malformed++;
    return;
  }
  r.stats.frames++;
  if (r.server_mute[seat]) {
    r.stats.muted++;
    return;
  }
  // The flood guard: a bucket of `flood_burst` frames, one more each 20 ms.
  const double burst = std::max(1, r.config.flood_burst);
  double& tokens = r.tokens[seat];
  tokens = std::min(burst, tokens + static_cast<double>(now_us - r.at[seat]) / kFrameUs);
  r.at[seat] = now_us;
  if (tokens < 1) {
    r.stats.flooded++;
    return;
  }
  tokens -= 1;
  const auto scope = static_cast<ChatScope>(up[6]);
  const int to = up[7] == kNoSeat ? kNoPlayer : up[7];
  int seats[kMaxPlayers];
  SessionWhy why = SessionWhy::none;
  const int n = voice_route(*r.session, seat, scope, to, seats, why);
  if (n < 0) {
    r.stats.refused++;
    return;
  }
  uint8_t down[kVoiceMessage];
  std::memcpy(down, up, size);
  down[0] = kDown;
  down[7] = static_cast<uint8_t>(seat);  // the talker, as the host knows them
  const SessionHost& h = *r.session;
  for (int i = 0; i < n; i++) {
    const int peer = h.seat_peer[seats[i]];
    if (peer >= 0) {
      if (node_send(*h.node, peer, r.channel, down, size)) r.stats.copies++;
    } else if (r.local) {  // the host's own player
      r.local(r.local_ctx, down, size);
      r.stats.copies++;
    }
  }
}

int oldest_slot(const VoiceTalker& t) {
  int best = -1;
  int best_d = 0;
  for (int i = 0; i < kVoiceSlots; i++) {
    if (!t.slots[i].full) continue;
    const int d = seq_diff(t.slots[i].seq, t.last_seq);
    if (best < 0 || d < best_d) {
      best = i;
      best_d = d;
    }
  }
  return best;
}

bool has_newer(const VoiceTalker& t, uint16_t seq) {
  for (const VoiceSlot& s : t.slots)
    if (s.full && seq_diff(s.seq, seq) > 0) return true;
  return false;
}

// The playout delay for a spurt starting now: the spread of the last frames' transit times.
int choose_target(VoiceTalker& t, const VoiceConfig& c) {
  int target = c.jitter_start;
  const int window = std::clamp(c.jitter_window, 1, 64);
  const int n = std::min(t.transit_count, window);
  if (n >= 8) {
    int64_t v[64];
    for (int i = 0; i < n; i++) v[i] = t.transit[(t.transit_at - 1 - i + 64) % 64];
    std::sort(v, v + n);
    const int64_t spread = v[(95 * n + 99) / 100 - 1] - v[0];
    target = static_cast<int>((spread + kFrameUs - 1) / kFrameUs) + 1;
  }
  if (t.raise) target++;
  t.raise = false;
  return std::clamp(target, std::max(1, c.jitter_min), std::max(1, c.jitter_max));
}

void take_down(VoiceListener& l, const uint8_t* down, size_t size, int64_t now_us) {
  l.stats.messages++;
  if (size <= kVoiceHeader || size > kVoiceMessage || down[0] != kDown || down[7] >= kMaxPlayers) {
    l.stats.malformed++;
    return;
  }
  const int seat = down[7];
  VoiceTalker& t = l.talkers[seat];
  if (!t.heard) {
    std::string error;
    t.decoder.open(error);  // a decoder that does not open plays silence; counted by its pulls
    t.heard = true;
  }
  const auto seq = static_cast<uint16_t>(down[2] | (down[3] << 8));
  const auto clock = static_cast<uint16_t>(down[4] | (down[5] << 8));
  voice_talker_receive(t, seq, clock, down[1], down + kVoiceHeader, size - kVoiceHeader, now_us);
}

}  // namespace

const char* voice_mode_name(VoiceMode m) {
  switch (m) {
    case VoiceMode::push_to_talk:
      return "push-to-talk";
    case VoiceMode::activity:
      return "activity";
    case VoiceMode::open:
      return "open";
  }
  return "?";
}

const char* voice_play_name(VoicePlay p) {
  switch (p) {
    case VoicePlay::silent:
      return "silent";
    case VoicePlay::played:
      return "played";
    case VoicePlay::recovered:
      return "recovered";
    case VoicePlay::concealed:
      return "concealed";
  }
  return "?";
}

size_t voice_write_up(uint8_t* out, uint8_t flags, uint16_t seq, uint16_t clock, ChatScope scope,
                      int to, const uint8_t* frame, size_t size) {
  size = std::min(size, static_cast<size_t>(kVoiceMaxBytes));
  out[0] = kUp;
  out[1] = flags;
  out[2] = static_cast<uint8_t>(seq & 0xff);
  out[3] = static_cast<uint8_t>(seq >> 8);
  out[4] = static_cast<uint8_t>(clock & 0xff);
  out[5] = static_cast<uint8_t>(clock >> 8);
  out[6] = static_cast<uint8_t>(scope);
  out[7] = to >= 0 && to < kMaxPlayers ? static_cast<uint8_t>(to) : kNoSeat;
  std::memcpy(out + kVoiceHeader, frame, size);
  return kVoiceHeader + size;
}

// ---- the sender ----

bool voice_sender_init(VoiceSender& s, const VoiceConfig& config, std::string& error) {
  s.config = config;
  if (!s.encoder.open(config.codec, error)) return false;
  s.open_level = level_of_db(config.open_db);
  s.close_level = level_of_db(std::min(config.close_db, config.open_db));
  s.hang_frames = std::max(1, static_cast<int>(std::lround(config.hangover_s * 50)));
  s.filled = 0;
  s.gate = s.key = false;
  s.hang = 0;
  s.queue_head = s.queue_count = 0;
  s.stats = {};
  return true;
}

void voice_sender_key(VoiceSender& s, bool held) { s.key = held; }

void voice_sender_route(VoiceSender& s, ChatScope scope, int to) {
  s.scope = scope;
  s.to = to;
}

void voice_sender_push(VoiceSender& s, const float* mono, int n) {
  while (n > 0) {
    const int take = std::min(n, kVoiceFrame - s.filled);
    std::memcpy(s.held + s.filled, mono, sizeof(float) * static_cast<size_t>(take));
    s.filled += take;
    mono += take;
    n -= take;
    if (s.filled == kVoiceFrame) {
      sender_frame(s);
      s.filled = 0;
    }
  }
}

bool voice_sender_pop(VoiceSender& s, VoiceMessage& out) {
  if (s.queue_count == 0) return false;
  const VoiceMessage& m = s.queue[s.queue_head];
  std::memcpy(out.bytes, m.bytes, m.size);
  out.size = m.size;
  s.queue_head = (s.queue_head + 1) % kVoiceQueue;
  s.queue_count--;
  return true;
}

// ---- the relay ----

void voice_relay_init(VoiceRelay& r, const VoiceConfig& config, SessionHost& session, int channel) {
  r.config = config;
  r.session = &session;
  r.channel = channel;
  for (int i = 0; i < kMaxPlayers; i++) {
    r.tokens[i] = std::max(1, config.flood_burst);
    r.at[i] = 0;
    r.server_mute[i] = false;
  }
  r.stats = {};
}

void voice_relay_update(VoiceRelay& r, int64_t now_us) {
  Node& node = *r.session->node;
  for (const Event& e : node.events) {
    if (e.type != EventType::message || e.channel != r.channel) continue;
    relay_one(r, seat_of(*r.session, e.peer), event_data(node, e), e.size, now_us);
  }
}

void voice_relay_local(VoiceRelay& r, const uint8_t* up, size_t size, int64_t now_us) {
  const SessionHost& h = *r.session;
  const int seat = h.config.host_plays && h.players[0].used ? 0 : -1;
  relay_one(r, seat, up, size, now_us);
}

int voice_route(const SessionHost& h, int speaker, ChatScope scope, int to, int* out,
                SessionWhy& why) {
  why = SessionWhy::none;
  if (speaker < 0 || speaker >= kMaxPlayers || !h.players[speaker].used) {
    why = SessionWhy::no_player;
    return -1;
  }
  const int team = h.players[speaker].team;
  if (scope == ChatScope::team && team == kNoTeam) {
    why = SessionWhy::no_team;
    return -1;
  }
  if (scope == ChatScope::direct && (to < 0 || to >= kMaxPlayers || to == speaker ||
                                     !h.players[to].used || !h.players[to].present)) {
    why = SessionWhy::no_player;
    return -1;
  }
  if (scope == ChatScope::nearby && !h.config.hears) {
    why = SessionWhy::no_nearby;
    return -1;
  }
  int n = 0;
  for (int s = 0; s < kMaxPlayers; s++) {
    const Player& p = h.players[s];
    if (s == speaker || !p.used || !p.present) continue;
    bool hears = false;
    switch (scope) {
      case ChatScope::all:
        hears = true;
        break;
      case ChatScope::team:
        hears = p.team == team;
        break;
      case ChatScope::direct:
        hears = s == to;
        break;
      case ChatScope::nearby:
        hears = h.config.hears(h.config.hears_ctx, speaker, s);
        break;
    }
    if (hears) out[n++] = s;
  }
  return n;
}

// ---- the talker ----

void voice_talker_receive(VoiceTalker& t, uint16_t seq, uint16_t clock, uint8_t flags,
                          const uint8_t* frame, size_t size, int64_t now_us) {
  t.stats.received++;
  t.stats.bytes += size;
  if (t.stats.received == 1) {
    t.last_seq = seq;
    t.last_clock = clock;
    t.first_us = now_us;
    t.newest_clock = 0;
  }
  if (seq_newer(seq, t.last_seq)) t.last_seq = seq;
  // When the frame was spoken, by its clock, and its transit: arrival less that time.
  const int64_t spoken = t.newest_clock + seq_diff(clock, t.last_clock);
  if (spoken > t.newest_clock) {
    t.newest_clock = spoken;
    t.last_clock = clock;
  }
  t.transit[t.transit_at] = now_us - t.first_us - spoken * kFrameUs;
  t.transit_at = (t.transit_at + 1) % 64;
  t.transit_count = std::min(t.transit_count + 1, 64);
  if (t.stats.spurts > 0 && seq_diff(seq, t.next) < 0) {
    t.stats.late++;
    t.raise = true;
    return;
  }
  VoiceSlot& s = t.slots[seq % kVoiceSlots];
  if (s.full) {
    if (s.seq == seq) {
      t.stats.duplicate++;
      return;
    }
    t.stats.dropped++;  // a frame a whole buffer older, never played
  }
  s.full = true;
  s.seq = seq;
  s.flags = flags;
  s.size = static_cast<uint8_t>(std::min(size, static_cast<size_t>(kVoiceMaxBytes)));
  s.arrived_us = now_us;
  std::memcpy(s.bytes, frame, s.size);
}

int voice_talker_buffered(const VoiceTalker& t) {
  int n = 0;
  for (const VoiceSlot& s : t.slots) n += s.full;
  return n;
}

VoicePulled voice_talker_pull(VoiceTalker& t, const VoiceConfig& c, float* pcm, int64_t now_us) {
  VoicePulled out;
  if (!t.playing) {
    const int o = oldest_slot(t);
    if (o < 0) {
      std::fill(pcm, pcm + kVoiceFrame, 0.0F);
      return out;
    }
    const VoiceSlot& first = t.slots[o];
    const int target = choose_target(t, c);
    const int span = seq_diff(t.last_seq, first.seq) + 1;
    if (span < target && now_us - first.arrived_us < target * kFrameUs &&
        !(first.flags & kVoiceEnd)) {
      std::fill(pcm, pcm + kVoiceFrame, 0.0F);
      return out;
    }
    t.target = target;
    t.playing = true;
    t.next = first.seq;
    t.missing = 0;
    t.stats.spurts++;
  }
  // A buffer run deep (a delay spike that passed) comes back to its target, oldest first.
  while (seq_diff(t.last_seq, t.next) + 1 > t.target + c.jitter_slack) {
    VoiceSlot& s = t.slots[t.next % kVoiceSlots];
    if (s.full && s.seq == t.next) {
      s.full = false;
      t.stats.dropped++;
    }
    t.next++;
  }
  VoiceSlot& now = t.slots[t.next % kVoiceSlots];
  const VoiceSlot& after = t.slots[static_cast<uint16_t>(t.next + 1) % kVoiceSlots];
  out.seq = t.next;
  if (now.full && now.seq == t.next) {
    t.decoder.decode(now.bytes, now.size, pcm);
    now.full = false;
    out.play = VoicePlay::played;
    t.stats.played++;
    t.missing = 0;
    if (now.flags & kVoiceEnd) t.playing = false;
  } else if (after.full && after.seq == static_cast<uint16_t>(t.next + 1) &&
             (after.flags & kVoiceFec)) {
    if (!t.decoder.recover(after.bytes, after.size, pcm)) t.stats.fec_empty++;
    out.play = VoicePlay::recovered;
    t.stats.recovered++;
    t.missing = 0;
  } else if (t.missing >= c.conceal_max && !has_newer(t, t.next)) {
    t.playing = false;  // the spurt's end was lost: it is over
    std::fill(pcm, pcm + kVoiceFrame, 0.0F);
    return VoicePulled{};
  } else {
    t.decoder.conceal(pcm);
    out.play = VoicePlay::concealed;
    t.stats.concealed++;
    t.missing++;
  }
  t.next++;
  return out;
}

// ---- the listener ----

void voice_listener_init(VoiceListener& l, const VoiceConfig& config, Node& node, int channel) {
  l.config = config;
  l.node = &node;
  l.channel = channel;
  for (int i = 0; i < kMaxPlayers; i++) {
    l.volume[i] = 1;
    l.gain[i] = 1;
    l.pan[i] = 0;
    l.streams[i] = {};
  }
  l.stats = {};
}

void voice_listener_receive(VoiceListener& l, int64_t now_us) {
  Node& node = *l.node;
  for (const Event& e : node.events) {
    if (e.type != EventType::message || e.channel != l.channel) continue;
    take_down(l, event_data(node, e), e.size, now_us);
  }
}

void voice_listener_take(VoiceListener& l, const uint8_t* down, size_t size, int64_t now_us) {
  take_down(l, down, size, now_us);
}

void voice_listener_mix(VoiceListener& l, audio::Synth& synth, int bus, int64_t now_us) {
  const int lead = std::max(1, l.config.lead_frames) * kVoiceFrame;
  float pcm[kVoiceFrame];
  for (int seat = 0; seat < kMaxPlayers; seat++) {
    VoiceTalker& t = l.talkers[seat];
    if (!t.heard) continue;
    audio::StreamId& id = l.streams[seat];
    if (!id.valid()) {
      id = synth.stream(bus);
      if (!id.valid()) continue;
    } else if (synth.queued(id) == 0) {
      l.stats.starved++;
    }
    synth.steer(id, l.volume[seat] * l.gain[seat] * l.config.volume, l.pan[seat], 0.05f);
    while (synth.queued(id) < lead) {
      const VoicePulled p = voice_talker_pull(t, l.config, pcm, now_us);
      if (l.tap) l.tap(l.tap_ctx, seat, p, pcm);
      if (synth.feed(id, pcm, kVoiceFrame) < kVoiceFrame) break;
    }
  }
}

void voice_listener_volume(VoiceListener& l, int seat, float volume) {
  if (seat >= 0 && seat < kMaxPlayers) l.volume[seat] = std::max(0.0F, volume);
}

void voice_listener_place(VoiceListener& l, int seat, float gain, float pan) {
  if (seat < 0 || seat >= kMaxPlayers) return;
  l.gain[seat] = std::max(0.0F, gain);
  l.pan[seat] = std::clamp(pan, -1.0F, 1.0F);
}

void voice_listener_close(VoiceListener& l, audio::Synth& synth) {
  for (audio::StreamId& id : l.streams) {
    if (id.valid()) synth.stop(id);
    id = {};
  }
}

float voice_distance_gain(float d, float near_m, float far_m) {
  if (d <= near_m) return 1;
  if (d >= far_m) return 0;
  const float g = near_m / d;
  // The last fifth of the range fades the inverse distance to nothing, so the edge is not a step.
  const float edge = far_m - 0.2f * (far_m - near_m);
  return d <= edge ? g : g * (far_m - d) / (far_m - edge);
}

}  // namespace ge::net
