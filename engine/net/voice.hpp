// VOICE CHAT: a player's microphone to the other players' ears, over a session (net/session.hpp).
// Every piece runs on the caller's clock, so a session's voice over the simulated network replays
// to the bit (the `voice` lab); only the codec's samples are the platform's (net/voice_codec.hpp).
//
// THE SENDER (`VoiceSender`, one per local player) cuts the microphone's 48 kHz mono into 20 ms
// frames and lets a frame through its GATE when:
//   push_to_talk   the talk key is held (`voice_sender_key`)
//   activity       the frame's level has risen past `open_db`; the gate then holds through
//                  `hangover_s` of quieter frames (under `close_db`) so a word's tail and the
//                  gaps between syllables are kept
//   open           always
// A run of frames through the gate is a SPURT: its first frame is flagged `start`, its last `end`.
// A frame is encoded (Opus, a constant size at `codec.bitrate`) and queued as an UP message with
// the route the player chose: everyone, their team, one player, or the players near them.
//
// THE RELAY (`VoiceRelay`, on the host) reads every up message on the voice channel, stamps the
// talker's SEAT from the peer it came from (a client never names itself), checks the route by the
// session's rules (a team needs teams, one player must be present, near needs the game's earshot,
// `SessionConfig::hears`), holds each talker to a token bucket (`flood_burst` frames, one more
// every 20 ms) and a server mute, and sends a DOWN message to each seat that hears it: a client is
// never sent a voice it may not hear. Voice rides an UNRELIABLE channel: a frame late is a frame
// lost, and a lost one is rebuilt or concealed rather than sent again.
//
// THE LISTENER (`VoiceListener`, one per client) keeps one `VoiceTalker` a seat: the JITTER BUFFER
// and the decoder. Frames go into slots by sequence; playout starts a spurt once the buffer holds
// its target delay (or the first frame has waited that long), then takes one frame a pull:
//   the frame is there          decoded (played)
//   lost, the next one is there rebuilt from the next frame's in-band FEC copy (recovered)
//   lost, nothing to rebuild    the decoder's concealment (concealed), at most `conceal_max`
//                               frames before the spurt is taken as over
//   the spurt's end played      silence until the next spurt
// A frame that arrives after its turn is LATE and dropped. The target delay is chosen at each
// spurt's start from the spread of the last `jitter_window` frames' transit times (their p95 above
// their least), between `jitter_min` and `jitter_max` frames, and raised by one for the next spurt
// after a frame came late; a buffer that has run past its target by `jitter_slack` frames drops
// the oldest. Delay changes at a spurt's start, never in a word.
// `voice_listener_mix` plays each talker on a synth STREAM (audio/synth.hpp) kept two frames
// ahead, at the talker's volume (`voice_listener_volume`, 0 mutes) times the game's placing of
// them (`voice_listener_place`: a distance's gain, `voice_distance_gain`, and a pan) times the
// listener's `volume`.
//
// THE CVARS (`add_voice_cvars`, net/net_cvars.hpp) are bound to a VoiceConfig the game keeps; a
// sender reads its config when made (`voice_sender_init` again takes a change), a listener and the
// relay read theirs on every call (assign `listener.config` and `relay.config`).
//
// A frame's use, after node_update and the session's update:
//   host:     voice_relay_update(relay, now)
//   client:   voice_listener_receive(listener, now); voice_listener_mix(listener, synth, bus, now)
//             mic samples -> voice_sender_push(sender, ...);
//             while (voice_sender_pop(sender, msg)) node_send(node, host, channel, msg.bytes, ...)
// The game's node_flush sends it all; the game skips the voice channel's events itself.
//
// ge:summary Voice chat: the gate and push-to-talk, the host's relay by route,
// ge:summary the jitter buffer, the mix
// ge:keywords net voice voice-chat microphone push-to-talk ptt voice-activity vad gate hangover
// ge:keywords spurt talk-spurt relay route team whisper proximity nearby earshot jitter-buffer
// ge:keywords playout delay late conceal fec recover mute talker volume distance-gain pan stream
// ge:keywords flood token-bucket server-mute unreliable multiplayer
// ge:skill net audio
// ge:test tests/net_voice_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "audio/synth.hpp"
#include "net/node.hpp"
#include "net/session.hpp"
#include "net/voice_codec.hpp"

namespace ge::net {

enum class VoiceMode : uint8_t { push_to_talk, activity, open };
const char* voice_mode_name(VoiceMode m);

inline constexpr int kVoiceSlots = 32;     // a talker's jitter buffer: 640 ms of frames
inline constexpr int kVoiceQueue = 8;      // a sender's encoded frames waiting to be sent
inline constexpr size_t kVoiceHeader = 8;  // an up or down message's bytes before the frame
inline constexpr size_t kVoiceMessage = kVoiceHeader + kVoiceMaxBytes;

// A frame's flags on the wire. Each frame carries its SEQUENCE (frames sent, for the order) and its
// CLOCK (frames cut, sent or not, for the time it was spoken: across a pause the sequence steps by
// one and the clock by the pause).
inline constexpr uint8_t kVoiceStart = 1;  // the first frame of a spurt
inline constexpr uint8_t kVoiceEnd = 2;    // the last
inline constexpr uint8_t kVoiceFec = 4;  // encoded with in-band FEC: it can rebuild the one before

struct VoiceConfig {
  VoiceCodecConfig codec;
  VoiceMode mode = VoiceMode::push_to_talk;
  float open_db = -36;      // activity: a frame this loud (RMS, dBFS) opens the gate
  float close_db = -46;     // activity: under this, the hangover runs down
  float hangover_s = 0.3f;  // activity: how long the gate stays open past the voice
  // The listener's jitter buffer, in 20 ms frames.
  int jitter_min = 1;
  int jitter_max = 12;
  int jitter_start = 3;    // the target before any spread is measured
  int jitter_window = 50;  // frames whose transit times choose the target (one second)
  int jitter_slack = 4;    // frames past the target before the oldest is dropped
  int conceal_max = 3;     // frames concealed in a row before a spurt is taken as over
  int lead_frames = 2;     // the synth stream kept this many frames ahead
  // The host's flood guard: a talker may send this many frames at once, one more every 20 ms.
  int flood_burst = 10;
  float volume = 1;  // the listener's: every voice, on top of each talker's own
};

// ---- the sender ----

struct VoiceSenderStats {
  uint64_t frames = 0;  // 20 ms frames of microphone cut
  uint64_t sent = 0;    // frames through the gate, encoded and queued
  uint64_t spurts = 0;
  uint64_t dropped = 0;  // frames lost to a full queue (pop more often)
  uint64_t bytes = 0;    // message bytes queued
};

struct VoiceMessage {
  uint8_t bytes[kVoiceMessage];
  size_t size = 0;
};

struct VoiceSender {
  VoiceConfig config;
  VoiceEncoder encoder;
  float held[kVoiceFrame] = {};
  int filled = 0;
  bool key = false;   // push-to-talk
  bool gate = false;  // a spurt is running
  int hang = 0;       // frames of hangover left
  int hang_frames = 0;
  float open_level = 0, close_level = 0;  // the dB thresholds as RMS
  uint16_t seq = 0;                       // frames sent: their order
  uint16_t clock = 0;                     // frames cut, sent or not: their time (20 ms a step)
  ChatScope scope = ChatScope::all;
  int to = kNoPlayer;
  float level_db = -120;  // the last frame's RMS, for a meter beside the talk key
  VoiceMessage queue[kVoiceQueue];
  int queue_head = 0, queue_count = 0;
  VoiceSenderStats stats;
};

// false, with `error` set, when the codec refuses the config.
bool voice_sender_init(VoiceSender& s, const VoiceConfig& config, std::string& error);
void voice_sender_key(VoiceSender& s, bool held);
// Who hears this player from now on: everyone, their team, one seat (`to`), or the players near.
void voice_sender_route(VoiceSender& s, ChatScope scope, int to = kNoPlayer);
// The microphone's next `n` mono samples at 48 kHz, any count. Each whole frame is gated and,
// through the gate, encoded and queued.
void voice_sender_push(VoiceSender& s, const float* mono, int n);
// The oldest queued up message: false when none.
bool voice_sender_pop(VoiceSender& s, VoiceMessage& out);

// ---- the host's relay ----

struct VoiceRelayStats {
  uint64_t frames = 0;   // up messages taken
  uint64_t copies = 0;   // down messages sent
  uint64_t flooded = 0;  // frames refused past a talker's bucket
  uint64_t refused = 0;  // frames whose route the session refuses (no team, no player, no earshot)
  uint64_t muted = 0;    // frames from a talker the server muted
  uint64_t strangers = 0;  // frames from a peer with no seat yet
  uint64_t malformed = 0;
};

// The host's own player on a listen server hears the relay here (`seat` the talker).
using VoiceLocalFn = void (*)(void* ctx, const uint8_t* down, size_t size);

struct VoiceRelay {
  VoiceConfig config;
  SessionHost* session = nullptr;
  int channel = 0;  // an UNRELIABLE channel of the node's ChannelConfig, voice's alone
  double tokens[kMaxPlayers] = {};
  int64_t at[kMaxPlayers] = {};
  bool server_mute[kMaxPlayers] = {};
  VoiceLocalFn local = nullptr;
  void* local_ctx = nullptr;
  VoiceRelayStats stats;
};

void voice_relay_init(VoiceRelay& r, const VoiceConfig& config, SessionHost& session, int channel);
// Every up message on the voice channel in the node's events this update, relayed.
void voice_relay_update(VoiceRelay& r, int64_t now_us);
// An up message from the host's own player (a listen server), relayed the same way.
void voice_relay_local(VoiceRelay& r, const uint8_t* up, size_t size, int64_t now_us);
// The seats that hear `speaker` by `scope` (and `to`), into `out` (kMaxPlayers): how many, or -1
// when the session refuses the route (`why` says which rule).
int voice_route(const SessionHost& host, int speaker, ChatScope scope, int to, int* out,
                SessionWhy& why);

// ---- the listener ----

enum class VoicePlay : uint8_t { silent, played, recovered, concealed };
const char* voice_play_name(VoicePlay p);

struct VoicePulled {
  VoicePlay play = VoicePlay::silent;
  uint16_t seq = 0;  // the frame played, recovered or concealed
};

struct VoiceTalkerStats {
  uint64_t received = 0;
  uint64_t late = 0;  // arrived after its turn
  uint64_t duplicate = 0;
  uint64_t dropped = 0;  // the oldest, dropped to bring a deep buffer back to its target
  uint64_t played = 0, recovered = 0, concealed = 0;
  uint64_t fec_empty = 0;  // recoveries whose frame carried no FEC copy, concealed (the codec's)
  uint64_t spurts = 0;
  uint64_t bytes = 0;
};

struct VoiceSlot {
  bool full = false;
  uint16_t seq = 0;
  uint8_t flags = 0;
  uint8_t size = 0;
  int64_t arrived_us = 0;
  uint8_t bytes[kVoiceMaxBytes];
};

struct VoiceTalker {
  bool heard = false;  // a frame has come from this seat
  VoiceDecoder decoder;
  VoiceSlot slots[kVoiceSlots];
  bool playing = false;
  uint16_t next = 0;   // the frame whose turn is next
  int target = 0;      // the playout delay, frames
  bool raise = false;  // a frame came late: one more frame of delay at the next spurt
  int missing = 0;     // frames concealed in a row
  // Transit times (arrival less the time the frame was spoken, by its clock) of the last frames,
  // us.
  int64_t transit[64] = {};
  int transit_count = 0, transit_at = 0;
  int64_t first_us = 0;
  int64_t newest_clock = 0;  // the newest frame's clock, unwrapped from the first frame's
  uint16_t last_clock = 0;
  uint16_t last_seq = 0;  // the newest frame's sequence
  VoiceTalkerStats stats;
};

// A frame for `t`, as the relay sent it.
void voice_talker_receive(VoiceTalker& t, uint16_t seq, uint16_t clock, uint8_t flags,
                          const uint8_t* frame, size_t size, int64_t now_us);
// The talker's next 20 ms into `pcm` (kVoiceFrame; silence when it is silent).
VoicePulled voice_talker_pull(VoiceTalker& t, const VoiceConfig& config, float* pcm,
                              int64_t now_us);
// Frames buffered ahead of playout.
int voice_talker_buffered(const VoiceTalker& t);

// Every pull, for a lab or a meter: the talker's seat, what was played and its samples.
using VoiceTapFn = void (*)(void* ctx, int seat, const VoicePulled& pulled, const float* pcm);

struct VoiceListenerStats {
  uint64_t messages = 0, malformed = 0;
  uint64_t starved = 0;  // pulls the synth stream ran dry before
};

struct VoiceListener {
  VoiceConfig config;
  Node* node = nullptr;
  int channel = 0;
  VoiceTalker talkers[kMaxPlayers];
  float volume[kMaxPlayers];  // the player's own, per talker (0 mutes)
  float gain[kMaxPlayers];    // the game's placing: a distance's gain
  float pan[kMaxPlayers];
  audio::StreamId streams[kMaxPlayers];
  VoiceTapFn tap = nullptr;
  void* tap_ctx = nullptr;
  VoiceListenerStats stats;
};

void voice_listener_init(VoiceListener& l, const VoiceConfig& config, Node& node, int channel);
// Every down message on the voice channel in the node's events this update, buffered.
void voice_listener_receive(VoiceListener& l, int64_t now_us);
// A down message by hand (a listen server's own player, a test).
void voice_listener_take(VoiceListener& l, const uint8_t* down, size_t size, int64_t now_us);
// Keep every heard talker's synth stream `lead_frames` ahead, pulling their frames as it drains.
void voice_listener_mix(VoiceListener& l, audio::Synth& synth, int bus, int64_t now_us);
void voice_listener_volume(VoiceListener& l, int seat, float volume);
void voice_listener_place(VoiceListener& l, int seat, float gain, float pan);
// Release every stream (the session is over).
void voice_listener_close(VoiceListener& l, audio::Synth& synth);

// A voice `distance_m` away: 1 within `near_m`, falling as near/distance, faded to 0 at `far_m`.
float voice_distance_gain(float distance_m, float near_m, float far_m);

// The wire, stated once (the relay and the listener; a test reads it).
//   up:   u8 1, u8 flags, u16 seq, u16 clock, u8 scope, u8 to       then the frame
//   down: u8 2, u8 flags, u16 seq, u16 clock, u8 scope, u8 talker   then the frame
size_t voice_write_up(uint8_t* out, uint8_t flags, uint16_t seq, uint16_t clock, ChatScope scope,
                      int to, const uint8_t* frame, size_t size);

}  // namespace ge::net
