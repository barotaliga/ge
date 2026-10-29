// A VOICE TRIAL: a dedicated host's session of talking clients over the simulated network,
// measured. Each client speaks a voice of its own (a buzz through moving vowel resonances, in
// words and pauses, off the trial's seed) on a route of its own, by the client's place in the
// join order: everyone, its team, the next client alone, or the clients near it. Every client hears
// every other through its own jitter buffer and synth. The trial answers what a link does to a
// voice: the frames played, rebuilt from FEC, concealed, late and dropped; the playout delay the
// buffer chose and how it moved; the mouth-to-ear latency; the bytes each way; that every voice
// reached exactly the listeners its route names; and, measured against the same frames decoded
// with nothing lost, how much of the voice survived (segmental SNR) and how loud it came out.
// Everything but the decoded samples is a pure function of the config (`hash`); the samples are
// the codec's and may differ in their last bits between platforms.
//
// ge:summary A voice trial: talking clients on four routes through a host, each heard and measured
// ge:keywords net voice trial talker listener route jitter latency mouth-to-ear fec conceal snr
// ge:keywords segmental-snr level speech-source formant multiplayer measure
// ge:skill net audio
// ge:test tests/net_voice_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "net/conditions.hpp"
#include "net/voice.hpp"

namespace ge::net {

struct VoiceTrialConfig {
  int clients = 4;  // 2 to 32
  double seconds = 20;
  uint32_t seed = 1;
  LinkConditions link;
  VoiceConfig
      voice;  // the mode is the trial's: the first client pushes to talk, the rest by activity
  float near_m = 3, far_m = 40;  // the nearby route's earshot (the clients stand 15 m apart)
  bool keep_audio = false;       // keep the ear's mix and every pair's segmental SNR
};

// One listener hearing one talker.
struct VoicePair {
  int listener = 0, talker = 0;  // seats
  bool should = false;           // the talker's route names this listener
  VoiceTalkerStats stats;
  double snr_db = 0;    // measured: mean segmental SNR of what was heard against the clean decode
  double level_db = 0;  // measured: what was heard, RMS, less the clean decode's
};

struct VoiceTrial {
  int seated = 0;
  std::vector<VoicePair> pairs;  // every listener and talker that should or did hear
  bool routes_ok = false;        // every pair heard exactly when its route says
  uint64_t sent = 0;             // frames the talkers sent
  uint64_t received = 0, played = 0, recovered = 0, concealed = 0, late = 0, dropped = 0;
  uint64_t spurts = 0;
  std::vector<double> latency_ms;      // mouth to ear, every frame played or recovered
  std::vector<double> delay_ms;        // the playout delay chosen at each spurt's start, every pair
  std::vector<double> t_s, target_ms;  // the first heard pair's playout delay, every 100 ms
  double up_kbps = 0;                  // every packet a talker sent, headers too, mean
  double down_kbps = 0;                // every packet the host sent, per listener, mean
  VoiceRelayStats relay;
  int ear = 0;  // the client the most routes name, whose mix is kept
  // Measured (the codec's samples):
  double snr_mean_db = 0, snr_worst_db = 0, level_db = 0;
  std::vector<float> mix;  // keep_audio: the ear's synth output, stereo
  uint64_t hash = 0;
};

VoiceTrial run_voice_trial(const VoiceTrialConfig& config);

// The trial's voice for a talker: `seconds` of 48 kHz mono, words and pauses off `seed`, around
// `f0_hz`. What the lab plays a codec, and a test a sender's gate.
std::vector<float> voice_trial_speech(uint32_t seed, double seconds, double f0_hz);

}  // namespace ge::net
