// THE VOICE CODEC: one talker's 20 ms frames of 48 kHz mono to Opus packets and back, for voice
// chat (net/voice.hpp). Opus in its voice mode at a CONSTANT bitrate, so every frame is the same
// size whatever is said and a link's packets are a function of the session alone; with in-band
// forward error correction, so a frame carries a coarse copy of the one before it and a single
// lost frame is RECOVERED from the next one; and with the decoder's loss concealment for the rest.
//
// A build without the encoders (GE_ENCODERS=OFF, cmake/Codecs.cmake) compiles this to a stand-in:
// the encoder writes frames of the same size whose bytes say nothing, the decoder plays silence,
// and `voice_codec_available` says so. The session, the relay and the jitter buffer run the same
// either way, which is what keeps the `voice` lab's digest the same with or without the codec.
//
// Opus's own arithmetic is floating point with the platform's vector code: what a frame decodes to
// may differ in its last bits between platforms. Nothing deterministic reads it (the lab's meters
// are measured, never hashed); what IS deterministic is every frame's size and when it is sent.
//
// ge:summary The voice codec: Opus mono at a constant bitrate, in-band FEC, concealment, or a stub
// ge:keywords net voice codec opus libopus encoder decoder cbr constant-bitrate fec inband
// ge:keywords forward-error-correction plc packet-loss-concealment voip 20ms frame stub
// ge:skill net audio
// ge:test tests/net_voice_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace ge::net {

inline constexpr int kVoiceRate = 48000;    // the synth's rate (audio::kRate)
inline constexpr int kVoiceFrame = 960;     // samples a frame: 20 ms
inline constexpr int kVoiceMaxBytes = 250;  // the largest frame (80 kbit/s)

struct VoiceCodecConfig {
  int bitrate = 24000;    // bit/s: 8000 to 80000; 24 000 is wideband speech, 60 bytes a frame
  bool fec = true;        // in-band forward error correction
  int loss_percent = 10;  // the loss the FEC is sized for, 0 to 50
  int complexity = 5;     // 0 to 10: the encoder's effort
};

// Whether this build carries Opus (GE_ENCODERS).
bool voice_codec_available();
// The bytes every frame is at `bitrate` (constant): bitrate x 20 ms / 8, within kVoiceMaxBytes.
int voice_frame_bytes(int bitrate);

class VoiceEncoder {
 public:
  VoiceEncoder();
  ~VoiceEncoder();
  VoiceEncoder(VoiceEncoder&&) noexcept;
  VoiceEncoder& operator=(VoiceEncoder&&) noexcept;

  // Start a talker's stream. false with `error` set when Opus refuses the settings.
  bool open(const VoiceCodecConfig& config, std::string& error);
  bool is_open() const { return bytes_ > 0; }
  // One frame of kVoiceFrame samples into `out`: the bytes written (voice_frame_bytes), or 0 when
  // the encoder is not open or `capacity` is short.
  int encode(const float* pcm, uint8_t* out, size_t capacity);

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
  int bytes_ = 0;
  uint32_t frames_ = 0;  // the stand-in's frame count, written into its bytes
};

class VoiceDecoder {
 public:
  VoiceDecoder();
  ~VoiceDecoder();
  VoiceDecoder(VoiceDecoder&&) noexcept;
  VoiceDecoder& operator=(VoiceDecoder&&) noexcept;

  bool open(std::string& error);
  bool is_open() const { return open_; }
  // A frame received: kVoiceFrame samples into `pcm`. false when it does not decode (`pcm` is
  // then concealed instead).
  bool decode(const uint8_t* data, size_t size, float* pcm);
  // A frame lost, the NEXT one in hand: the lost frame rebuilt from the next frame's FEC copy.
  // false when it carries none (`pcm` is then concealed instead).
  bool recover(const uint8_t* next, size_t size, float* pcm);
  // A frame lost and nothing to rebuild it from: the decoder's concealment, fading as it goes on.
  void conceal(float* pcm);

  struct Impl;

 private:
  std::unique_ptr<Impl> impl_;
  bool open_ = false;
};

}  // namespace ge::net
