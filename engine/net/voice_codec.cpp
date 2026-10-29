// The voice codec (net/voice_codec.hpp): libopus behind the encoder and the decoder, or the
// stand-in when GE_ENCODERS is off. With engine/capture/codecs.cpp, the only files that include
// libopus.
//
// ge:summary The voice codec's definitions: libopus in voice mode at a constant bitrate, or a stub
// ge:keywords net voice codec opus opus_encode_float opus_decode_float lbrr fec plc cbr stub
// ge:skill net audio
// ge:test tests/net_voice_test.cpp
#include "net/voice_codec.hpp"

#include <algorithm>
#include <cstring>

#if GE_ENCODERS
#include <opus/opus.h>
#endif

namespace ge::net {

#if GE_ENCODERS

struct VoiceEncoder::Impl {
  OpusEncoder* enc = nullptr;
  ~Impl() {
    if (enc) opus_encoder_destroy(enc);
  }
};

struct VoiceDecoder::Impl {
  OpusDecoder* dec = nullptr;
  ~Impl() {
    if (dec) opus_decoder_destroy(dec);
  }
};

bool voice_codec_available() { return true; }

#else

struct VoiceEncoder::Impl {};
struct VoiceDecoder::Impl {};

bool voice_codec_available() { return false; }

#endif

int voice_frame_bytes(int bitrate) {
  const int b = std::clamp(bitrate, 8000, 80000) * kVoiceFrame / kVoiceRate / 8;
  return std::min(b, kVoiceMaxBytes);
}

VoiceEncoder::VoiceEncoder() = default;
VoiceEncoder::~VoiceEncoder() = default;
VoiceEncoder::VoiceEncoder(VoiceEncoder&&) noexcept = default;
VoiceEncoder& VoiceEncoder::operator=(VoiceEncoder&&) noexcept = default;

bool VoiceEncoder::open(const VoiceCodecConfig& c, std::string& error) {
  if (c.bitrate < 8000 || c.bitrate > 80000 || c.loss_percent < 0 || c.loss_percent > 50 ||
      c.complexity < 0 || c.complexity > 10) {
    error = "voice: a bitrate of " + std::to_string(c.bitrate) + " bit/s, " +
            std::to_string(c.loss_percent) + "% loss or complexity " +
            std::to_string(c.complexity) +
            " is out of range; pass 8000 to 80000 bit/s, 0 to 50% and 0 to 10 (skill: net)";
    return false;
  }
  impl_ = std::make_unique<Impl>();
  frames_ = 0;
#if GE_ENCODERS
  int err = OPUS_OK;
  impl_->enc = opus_encoder_create(kVoiceRate, 1, OPUS_APPLICATION_VOIP, &err);
  if (err != OPUS_OK || !impl_->enc) {
    impl_.reset();
    error = std::string("voice: libopus did not start an encoder: ") + opus_strerror(err) +
            " (skill: net)";
    return false;
  }
  OpusEncoder* e = impl_->enc;
  // A constant bitrate: every frame the same size (the frame's padding fills what speech leaves).
  if (opus_encoder_ctl(e, OPUS_SET_BITRATE(c.bitrate)) != OPUS_OK ||
      opus_encoder_ctl(e, OPUS_SET_VBR(0)) != OPUS_OK ||
      opus_encoder_ctl(e, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)) != OPUS_OK ||
      opus_encoder_ctl(e, OPUS_SET_INBAND_FEC(c.fec ? 1 : 0)) != OPUS_OK ||
      opus_encoder_ctl(e, OPUS_SET_PACKET_LOSS_PERC(c.loss_percent)) != OPUS_OK ||
      opus_encoder_ctl(e, OPUS_SET_COMPLEXITY(c.complexity)) != OPUS_OK ||
      opus_encoder_ctl(e, OPUS_SET_DTX(0)) != OPUS_OK) {
    impl_.reset();
    error = "voice: libopus refused the encoder's settings (bitrate " + std::to_string(c.bitrate) +
            "); skill: net";
    return false;
  }
#endif
  bytes_ = voice_frame_bytes(c.bitrate);
  return true;
}

int VoiceEncoder::encode(const float* pcm, uint8_t* out, size_t capacity) {
  if (!impl_ || capacity < static_cast<size_t>(bytes_)) return 0;
  ++frames_;
#if GE_ENCODERS
  const opus_int32 n = opus_encode_float(impl_->enc, pcm, kVoiceFrame, out, bytes_);
  if (n <= 0) return 0;
  // CBR pads every frame to the bitrate's size; a shorter one is padded here so the size on the
  // wire is the bitrate's alone (opus_packet_pad keeps it a valid packet).
  if (n < bytes_ && opus_packet_pad(out, n, bytes_) != OPUS_OK) return 0;
  return bytes_;
#else
  (void)pcm;
  std::memset(out, 0, static_cast<size_t>(bytes_));
  std::memcpy(out, &frames_, std::min<size_t>(sizeof frames_, static_cast<size_t>(bytes_)));
  return bytes_;
#endif
}

VoiceDecoder::VoiceDecoder() = default;
VoiceDecoder::~VoiceDecoder() = default;
VoiceDecoder::VoiceDecoder(VoiceDecoder&&) noexcept = default;
VoiceDecoder& VoiceDecoder::operator=(VoiceDecoder&&) noexcept = default;

bool VoiceDecoder::open(std::string& error) {
  impl_ = std::make_unique<Impl>();
#if GE_ENCODERS
  int err = OPUS_OK;
  impl_->dec = opus_decoder_create(kVoiceRate, 1, &err);
  if (err != OPUS_OK || !impl_->dec) {
    impl_.reset();
    error = std::string("voice: libopus did not start a decoder: ") + opus_strerror(err) +
            " (skill: net)";
    return false;
  }
#else
  (void)error;
#endif
  open_ = true;
  return true;
}

bool VoiceDecoder::decode(const uint8_t* data, size_t size, float* pcm) {
  if (!open_) {
    std::fill(pcm, pcm + kVoiceFrame, 0.0F);
    return false;
  }
#if GE_ENCODERS
  const int n =
      opus_decode_float(impl_->dec, data, static_cast<opus_int32>(size), pcm, kVoiceFrame, 0);
  if (n == kVoiceFrame) return true;
  conceal(pcm);
  return false;
#else
  (void)data;
  (void)size;
  std::fill(pcm, pcm + kVoiceFrame, 0.0F);
  return true;
#endif
}

bool VoiceDecoder::recover(const uint8_t* next, size_t size, float* pcm) {
  if (!open_) {
    std::fill(pcm, pcm + kVoiceFrame, 0.0F);
    return false;
  }
#if GE_ENCODERS
  if (opus_packet_has_lbrr(next, static_cast<opus_int32>(size)) != 1) {
    conceal(pcm);
    return false;
  }
  const int n =
      opus_decode_float(impl_->dec, next, static_cast<opus_int32>(size), pcm, kVoiceFrame, 1);
  if (n == kVoiceFrame) return true;
  conceal(pcm);
  return false;
#else
  (void)next;
  (void)size;
  std::fill(pcm, pcm + kVoiceFrame, 0.0F);
  return true;
#endif
}

void VoiceDecoder::conceal(float* pcm) {
#if GE_ENCODERS
  if (open_ && opus_decode_float(impl_->dec, nullptr, 0, pcm, kVoiceFrame, 0) == kVoiceFrame)
    return;
#endif
  std::fill(pcm, pcm + kVoiceFrame, 0.0F);
}

}  // namespace ge::net
