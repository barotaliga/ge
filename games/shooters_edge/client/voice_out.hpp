// A PLAYER'S VOICE AS THE PROTOCOL SENDS IT: microphone samples (48 kHz mono, any count a call)
// gathered, cut into 20 ms frames and, while the player talks, each frame Opus-coded
// (engine/net/voice_codec.hpp) into the classic voice message's data: each frame a little-endian
// 16-bit length, then its bytes. The frames cut while he is silent are dropped; the sequence
// counts the frames sent, mod 256, so a listener tells a lost frame from a pause. Our client
// (net/classic_client.hpp) speaks through it, and the window's own player on its listen server
// (listen.hpp's `listen_mic`).
//
// ge:summary A player's voice cut into the protocol's voice message: 20 ms Opus frames
// ge:keywords voice voice-out microphone opus frame voice-message talk sequence push-to-talk
// ge:skill net audio shooter
// ge:test tests/shooters_edge_remote_test.cpp
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "net/voice_codec.hpp"

namespace shooters_edge::client {

struct VoiceOut {
  std::unique_ptr<ge::net::VoiceEncoder> encoder;  // null: the codec did not open (nothing sent)
  std::vector<float> mic;                          // samples not yet a whole frame
  int seq = 0;                                     // frames sent, mod 256
  int64_t sent = 0;                                // frames sent
};

// The encoder opened (false when the codec is missing or refuses: then nothing is ever sent).
bool voice_out_open(VoiceOut& v);
// `samples` added and every whole frame cut: while `talking`, each encoded onto `data` (emptied
// first). Returns the frames encoded; `first_seq` is the first one's sequence, and `v.seq` moves
// on by as many.
int voice_out_cut(VoiceOut& v, std::span<const float> samples, bool talking,
                  std::vector<uint8_t>& data, int& first_seq);

}  // namespace shooters_edge::client
