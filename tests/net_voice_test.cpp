// Voice chat (engine/net/voice.hpp, voice_codec.hpp, voice_trial.hpp): the codec's constant frame
// size, its concealment and FEC; the sender's gate by push-to-talk and by activity, the spurt's
// flags; the jitter buffer playing in order, rebuilding a lost frame from the next, concealing,
// dropping a late one and stopping at a spurt's end; the relay's routes and its flood guard; the
// distance gain; the synth's streams; and a whole trial over the simulated network replaying.
//
// ge:summary Test: voice chat: codec, gate, jitter buffer, relay routes, flood, streams, the trial
// ge:keywords test net voice codec opus gate push-to-talk activity jitter-buffer fec conceal late
// ge:keywords relay route flood distance-gain stream synth trial
// ge:skill net audio testing
#include "net/voice.hpp"

#include <memory>
#include <string>
#include <vector>

#include "audio/synth.hpp"
#include "check.hpp"
#include "net/voice_trial.hpp"

using namespace ge::net;

namespace {

VoiceConfig quiet_config() {
  VoiceConfig c;
  c.mode = VoiceMode::open;
  return c;
}

// Up message -> down message from seat `talker`, as the relay would.
std::vector<uint8_t> down_of(const VoiceMessage& m, int talker) {
  std::vector<uint8_t> d(m.bytes, m.bytes + m.size);
  d[0] = 2;
  d[7] = static_cast<uint8_t>(talker);
  return d;
}

std::vector<VoiceMessage> encode_all(const std::vector<float>& pcm, VoiceConfig c) {
  VoiceSender s;
  std::string error;
  CHECK_MSG(voice_sender_init(s, c, error), error);
  std::vector<VoiceMessage> out;
  for (size_t at = 0; at + 480 <= pcm.size(); at += 480) {
    voice_sender_push(s, pcm.data() + at, 480);
    VoiceMessage m;
    while (voice_sender_pop(s, m)) out.push_back(m);
  }
  return out;
}

uint16_t seq_of(const VoiceMessage& m) {
  return static_cast<uint16_t>(m.bytes[2] | (m.bytes[3] << 8));
}

}  // namespace

TEST(every_frame_is_the_bitrates_size) {
  const std::vector<float> pcm = voice_trial_speech(3, 2, 150);
  VoiceConfig c = quiet_config();
  for (int bitrate : {12000, 24000, 40000}) {
    c.codec.bitrate = bitrate;
    const std::vector<VoiceMessage> ms = encode_all(pcm, c);
    CHECK(ms.size() == 100);
    for (const VoiceMessage& m : ms)
      CHECK_MSG(m.size == kVoiceHeader + static_cast<size_t>(voice_frame_bytes(bitrate)),
                "a frame at " + std::to_string(bitrate) + " bit/s was " + std::to_string(m.size) +
                    " bytes: the codec must run at a constant bitrate (skill: net)");
  }
  CHECK(voice_frame_bytes(24000) == 60);
}

TEST(the_codec_decodes_conceals_and_recovers) {
  if (!voice_codec_available()) {
    ge::test::skip("this build has no Opus (GE_ENCODERS is OFF)");
    return;
  }
  const std::vector<float> pcm = voice_trial_speech(5, 2, 180);
  const std::vector<VoiceMessage> ms = encode_all(pcm, quiet_config());
  VoiceDecoder d;
  std::string error;
  CHECK_MSG(d.open(error), error);
  float out[kVoiceFrame];
  double energy = 0;
  for (const VoiceMessage& m : ms) {
    CHECK(d.decode(m.bytes + kVoiceHeader, m.size - kVoiceHeader, out));
    for (float x : out) energy += static_cast<double>(x) * x;
  }
  CHECK_MSG(energy > 1, "a voice decoded to silence");
  // A voiced frame's successor carries an FEC copy of it.
  int rebuilt = 0;
  VoiceDecoder f;
  CHECK(f.open(error));
  for (size_t i = 0; i + 1 < ms.size(); i++)
    rebuilt += f.recover(ms[i + 1].bytes + kVoiceHeader, ms[i + 1].size - kVoiceHeader, out);
  CHECK_MSG(rebuilt > 10, "only " + std::to_string(rebuilt) + " frames carried FEC");
  d.conceal(out);  // concealment never fails
}

TEST(push_to_talk_sends_only_while_the_key_is_held) {
  VoiceConfig c;
  c.mode = VoiceMode::push_to_talk;
  VoiceSender s;
  std::string error;
  CHECK(voice_sender_init(s, c, error));
  std::vector<float> loud(kVoiceFrame, 0.3F);
  voice_sender_push(s, loud.data(), kVoiceFrame);
  CHECK(s.stats.sent == 0);
  voice_sender_key(s, true);
  for (int i = 0; i < 5; i++) voice_sender_push(s, loud.data(), kVoiceFrame);
  voice_sender_key(s, false);
  voice_sender_push(s, loud.data(), kVoiceFrame);  // the frame the gate closes on, flagged the end
  voice_sender_push(s, loud.data(), kVoiceFrame);
  CHECK(s.stats.sent == 6);
  CHECK(s.stats.spurts == 1);
  VoiceMessage m;
  std::vector<uint8_t> flags;
  while (voice_sender_pop(s, m)) flags.push_back(m.bytes[1]);
  CHECK(flags.size() == 6);
  CHECK((flags.front() & kVoiceStart) != 0);
  CHECK((flags.back() & kVoiceEnd) != 0);
  for (size_t i = 1; i + 1 < flags.size(); i++) CHECK((flags[i] & (kVoiceStart | kVoiceEnd)) == 0);
}

TEST(the_activity_gate_opens_holds_and_closes) {
  VoiceConfig c;
  c.mode = VoiceMode::activity;
  c.hangover_s = 0.1f;  // five frames
  VoiceSender s;
  std::string error;
  CHECK(voice_sender_init(s, c, error));
  std::vector<float> loud(kVoiceFrame, 0.1F), hush(kVoiceFrame, 0.0001F);
  for (int i = 0; i < 3; i++) voice_sender_push(s, hush.data(), kVoiceFrame);
  CHECK(s.stats.sent == 0);
  for (int i = 0; i < 4; i++) voice_sender_push(s, loud.data(), kVoiceFrame);
  CHECK(s.stats.sent == 4);
  for (int i = 0; i < 20; i++) voice_sender_push(s, hush.data(), kVoiceFrame);
  // Four loud, four of hangover, and the frame it closes on.
  CHECK_MSG(s.stats.sent == 9, "sent " + std::to_string(s.stats.sent));
  CHECK(s.stats.spurts == 1);
  CHECK(!s.gate);
}

TEST(the_jitter_buffer_plays_in_order_and_mends_losses) {
  const std::vector<float> pcm = voice_trial_speech(9, 1, 150);
  const std::vector<VoiceMessage> ms = encode_all(pcm, quiet_config());
  CHECK(ms.size() == 50);
  VoiceConfig c = quiet_config();
  c.jitter_start = 3;
  c.jitter_slack = 20;  // all ten frames held at once, none dropped for depth
  auto l = std::make_unique<VoiceListener>();
  Node dummy;
  voice_listener_init(*l, c, dummy, 1);
  float out[kVoiceFrame];
  int64_t now = 0;
  // Frames 0..9 arrive in a jumble; 4 never arrives (5 carries its FEC), 7 and 8 never arrive.
  for (int i : {1, 0, 3, 2, 5, 6, 9}) {
    const std::vector<uint8_t> d = down_of(ms[static_cast<size_t>(i)], 3);
    voice_listener_take(*l, d.data(), d.size(), now);
  }
  VoiceTalker& t = l->talkers[3];
  CHECK(t.heard);
  std::vector<VoicePlay> plays;
  std::vector<int> seqs;
  for (int i = 0; i < 10; i++) {
    now += 20000;
    const VoicePulled p = voice_talker_pull(t, c, out, now);
    plays.push_back(p.play);
    seqs.push_back(p.seq);
  }
  for (int i = 0; i < 10; i++) CHECK_MSG(seqs[static_cast<size_t>(i)] == i, "out of order");
  CHECK(plays[0] == VoicePlay::played && plays[3] == VoicePlay::played);
  CHECK(plays[4] == VoicePlay::recovered);
  CHECK(plays[7] == VoicePlay::concealed);  // 8 is missing too: no FEC copy to rebuild 7 from
  CHECK(plays[8] == VoicePlay::recovered);  // 9 carries 8's
  CHECK(plays[9] == VoicePlay::played);
  // Frame 2 again (a duplicate) and 4 at last (late) are dropped.
  for (int i : {2, 4}) {
    const std::vector<uint8_t> d = down_of(ms[static_cast<size_t>(i)], 3);
    voice_listener_take(*l, d.data(), d.size(), now);
  }
  CHECK(t.stats.late == 2);
  CHECK(t.stats.played == 7);
  CHECK(t.stats.recovered == 2);
  CHECK(t.stats.concealed == 1);
}

TEST(a_spurt_ends_at_its_last_frame_or_after_a_few_concealed) {
  VoiceConfig c;
  c.mode = VoiceMode::push_to_talk;
  VoiceSender s;
  std::string error;
  CHECK(voice_sender_init(s, c, error));
  std::vector<float> loud(kVoiceFrame, 0.2F);
  voice_sender_key(s, true);
  for (int i = 0; i < 4; i++) voice_sender_push(s, loud.data(), kVoiceFrame);
  voice_sender_key(s, false);
  voice_sender_push(s, loud.data(), kVoiceFrame);
  std::vector<VoiceMessage> ms;
  VoiceMessage m;
  while (voice_sender_pop(s, m)) ms.push_back(m);
  CHECK(ms.size() == 5);
  auto l = std::make_unique<VoiceListener>();
  Node dummy;
  voice_listener_init(*l, c, dummy, 1);
  int64_t now = 0;
  for (const VoiceMessage& x : ms) {
    const std::vector<uint8_t> d = down_of(x, 1);
    voice_listener_take(*l, d.data(), d.size(), now);
  }
  float out[kVoiceFrame];
  int played = 0, silent = 0;
  for (int i = 0; i < 12; i++) {
    now += 20000;
    const VoicePulled p = voice_talker_pull(l->talkers[1], c, out, now);
    played += p.play == VoicePlay::played;
    silent += p.play == VoicePlay::silent;
  }
  CHECK(played == 5);
  CHECK(silent == 7);  // the end flag stops it: nothing concealed past the spurt
  CHECK(l->talkers[1].stats.concealed == 0);
  CHECK(seq_of(ms.back()) == 4);
}

TEST(the_buffer_waits_longer_after_jitter) {
  VoiceConfig c = quiet_config();
  c.jitter_start = 1;
  const std::vector<float> pcm = voice_trial_speech(11, 4, 150);
  const std::vector<VoiceMessage> ms = encode_all(pcm, c);
  auto l = std::make_unique<VoiceListener>();
  Node dummy;
  voice_listener_init(*l, c, dummy, 1);
  // A first spurt whose frames arrive up to 100 ms late in a sawtooth, then an end.
  for (size_t i = 0; i < 60; i++) {
    std::vector<uint8_t> d = down_of(ms[i], 2);
    if (i == 59) d[1] |= kVoiceEnd;
    voice_listener_take(*l, d.data(), d.size(), static_cast<int64_t>(i) * 20000 + (i % 6) * 20000);
  }
  float out[kVoiceFrame];
  for (int i = 0; i < 80; i++) voice_talker_pull(l->talkers[2], c, out, 1300000 + i * 20000LL);
  // The next spurt starts with a target that covers the spread.
  std::vector<uint8_t> d = down_of(ms[60], 2);
  voice_listener_take(*l, d.data(), d.size(), 3000000);
  voice_talker_pull(l->talkers[2], c, out, 3200000);
  CHECK_MSG(l->talkers[2].target >= 6,
            "target " + std::to_string(l->talkers[2].target) + " frames after 100 ms of jitter");
}

TEST(the_relay_routes_by_the_sessions_rules) {
  SessionHost h;
  h.config.max_players = 8;
  for (int s = 0; s < 5; s++) {
    h.players[s].used = h.players[s].present = true;
    h.players[s].team = s % 2;
  }
  h.players[4].present = false;  // away: hears nothing
  int out[kMaxPlayers];
  SessionWhy why;
  CHECK(voice_route(h, 0, ChatScope::all, kNoPlayer, out, why) == 3);
  CHECK(voice_route(h, 0, ChatScope::team, kNoPlayer, out, why) == 1 && out[0] == 2);
  CHECK(voice_route(h, 1, ChatScope::direct, 3, out, why) == 1 && out[0] == 3);
  CHECK(voice_route(h, 1, ChatScope::direct, 4, out, why) == -1 && why == SessionWhy::no_player);
  CHECK(voice_route(h, 1, ChatScope::nearby, kNoPlayer, out, why) == -1 &&
        why == SessionWhy::no_nearby);
  h.config.hears = [](void*, int a, int b) { return b == a + 1; };
  CHECK(voice_route(h, 1, ChatScope::nearby, kNoPlayer, out, why) == 1 && out[0] == 2);
  h.players[0].team = kNoTeam;
  CHECK(voice_route(h, 0, ChatScope::team, kNoPlayer, out, why) == -1 &&
        why == SessionWhy::no_team);
}

TEST(the_relay_holds_a_talker_to_its_bucket) {
  SessionHost h;
  Node node;
  h.node = &node;
  h.config.host_plays = true;
  h.players[0].used = h.players[0].present = true;
  VoiceConfig c;
  c.flood_burst = 5;
  VoiceRelay r;
  voice_relay_init(r, c, h, 1);
  int heard = 0;
  r.local = [](void* ctx, const uint8_t*, size_t) { ++*static_cast<int*>(ctx); };
  r.local_ctx = &heard;
  h.players[1].used = h.players[1].present = true;  // a listener with no peer: the local seat
  h.seat_peer[1] = -1;
  uint8_t frame[60] = {};
  uint8_t up[kVoiceMessage];
  const size_t n = voice_write_up(up, 0, 0, 0, ChatScope::all, kNoPlayer, frame, sizeof frame);
  for (int i = 0; i < 20; i++) voice_relay_local(r, up, n, 0);  // twenty at once
  CHECK(r.stats.frames == 20);
  CHECK(r.stats.flooded == 15);
  for (int i = 0; i < 10; i++) voice_relay_local(r, up, n, 20000LL * (i + 1));  // one each 20 ms
  CHECK(r.stats.flooded == 15);
  CHECK(heard == 15);
  r.server_mute[0] = true;
  voice_relay_local(r, up, n, 1000000);
  CHECK(r.stats.muted == 1);
  up[6] = 9;  // no such route
  voice_relay_local(r, up, n, 2000000);
  CHECK(r.stats.malformed == 1);
}

TEST(a_voice_fades_with_distance) {
  CHECK(voice_distance_gain(1, 3, 40) == 1);
  CHECK(voice_distance_gain(6, 3, 40) == 0.5f);
  CHECK(voice_distance_gain(40, 3, 40) == 0);
  CHECK(voice_distance_gain(39, 3, 40) < voice_distance_gain(30, 3, 40));
  float last = 1;
  for (float d = 3; d <= 40; d += 0.5f) {
    const float g = voice_distance_gain(d, 3, 40);
    CHECK(g <= last);
    last = g;
  }
}

TEST(a_stream_plays_what_is_fed_and_counts_what_is_not) {
  ge::audio::Synth synth;
  const ge::audio::StreamId id = synth.stream(0);
  CHECK(id.valid());
  std::vector<float> in(256);
  for (size_t i = 0; i < in.size(); i++)
    in[i] = 0.05f * static_cast<float>(static_cast<int>(i % 16) - 8) / 8;
  CHECK(synth.feed(id, in.data(), 256) == 256);
  CHECK(synth.queued(id) == 256);
  std::vector<float> out(2 * 384);
  synth.render(out.data(), 384);
  CHECK(synth.queued(id) == 0);
  for (size_t i = 0; i < 256; i++) {
    CHECK(out[2 * i] == in[i]);  // centred, gain 1, under the limiter: the samples themselves
    CHECK(out[2 * i + 1] == in[i]);
  }
  CHECK(synth.stats().starved == 128);
  synth.stop(id);
  std::vector<float> more(2 * 48000);
  synth.render(more.data(), 48000);
  // Every slot is free again once the fades are done.
  std::vector<ge::audio::StreamId> all;
  for (int i = 0; i < ge::audio::kMaxStreams; i++) all.push_back(synth.stream(0));
  for (const auto& s : all) CHECK(s.valid());
  CHECK(!synth.stream(0).valid());
}

TEST(a_trial_reaches_every_route_and_replays) {
  VoiceTrialConfig k;
  k.clients = 4;
  k.seconds = 6;
  CHECK(conditions_preset("wifi", k.link));
  const VoiceTrial a = run_voice_trial(k);
  const VoiceTrial b = run_voice_trial(k);
  CHECK(a.seated == 4);
  CHECK_MSG(a.routes_ok, "a voice reached a listener its route does not name, or missed one");
  CHECK(a.sent > 100);
  CHECK(a.played > 0);
  CHECK(a.hash == b.hash);
  CHECK(a.relay.flooded == 0);
}
