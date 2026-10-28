// The export's AV1 and Opus encoders and decoders (codecs.hpp). The only file that includes
// libaom or libopus; with GE_ENCODERS off every class is a stub that says so.
//
// ge:summary codecs' definitions: libaom and libopus behind the packet interface, or stubs
// ge:keywords av1 opus libaom libopus aom_codec opus_encode av1c obu leb128 sequence-header stub
// ge:skill replay
// ge:test tests/capture_codecs_test.cpp
#include "capture/codecs.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#if GE_ENCODERS
#include <aom/aom_decoder.h>
#include <aom/aom_encoder.h>
#include <aom/aomcx.h>
#include <aom/aomdx.h>
#include <opus/opus.h>
#endif

namespace ge::capture {

namespace {

[[maybe_unused]] constexpr const char* kNoEncoders =
    "this build has no encoders (GE_ENCODERS is OFF); configure with -DGE_ENCODERS=ON, or "
    "record without --codec (skill: replay)";

// Reads bits off a byte string, most significant first; past the end reads zeros.
struct Bits {
  std::span<const uint8_t> b;
  size_t at = 0;  // in bits
  uint32_t read(int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; i++, at++) {
      const size_t byte = at / 8;
      const uint32_t bit = byte < b.size() ? (b[byte] >> (7 - at % 8)) & 1u : 0u;
      v = (v << 1) | bit;
    }
    return v;
  }
};

// The OBU's payload after its header and its size, or the whole tail when it has no size.
std::span<const uint8_t> obu_payload(std::span<const uint8_t> obu) {
  if (obu.empty()) return obu;
  const uint8_t header = obu[0];
  size_t at = (header & 0x04) != 0 ? 2 : 1;  // the extension byte
  if ((header & 0x02) == 0) return obu.subspan(std::min(at, obu.size()));
  uint64_t size = 0;
  for (int i = 0; i < 8 && at < obu.size(); i++) {
    const uint8_t byte = obu[at++];
    size |= static_cast<uint64_t>(byte & 0x7F) << (7 * i);
    if ((byte & 0x80) == 0) break;
  }
  if (at > obu.size()) return {};
  return obu.subspan(at, std::min<size_t>(static_cast<size_t>(size), obu.size() - at));
}

}  // namespace

std::vector<uint8_t> av1_codec_private(std::span<const uint8_t> sequence_header) {
  // The sequence header's opening fields (the AV1 bitstream's sequence_header_obu): the profile,
  // then the first operating point's level when no timing info comes first; otherwise level 31,
  // which a player reads as "the stream says".
  Bits r{obu_payload(sequence_header)};
  const uint32_t profile = r.read(3);
  r.read(1);  // still_picture
  const uint32_t reduced = r.read(1);
  uint32_t level = 31, tier = 0;
  if (reduced != 0) {
    level = r.read(5);
  } else if (r.read(1) == 0) {  // timing_info_present_flag
    r.read(1);                  // initial_display_delay_present_flag
    r.read(5);                  // operating_points_cnt_minus_1
    r.read(12);                 // operating_point_idc[0]
    level = r.read(5);
    if (level > 7) tier = r.read(1);
  }
  std::vector<uint8_t> out = {
      0x81,  // marker and version 1
      static_cast<uint8_t>(profile << 5 | level),
      // tier, 8-bit, not monochrome, 4:2:0 (subsampling x and y), sample position unknown
      static_cast<uint8_t>(tier << 7 | 0x0C),
      0x00,  // no initial presentation delay
  };
  out.insert(out.end(), sequence_header.begin(), sequence_header.end());
  return out;
}

#if GE_ENCODERS

bool encoders_built() { return true; }

struct Av1Encoder::Impl {
  aom_codec_ctx_t ctx{};
  aom_codec_enc_cfg_t cfg{};
  aom_image_t img{};
  bool open = false;
  ~Impl() {
    if (open) aom_codec_destroy(&ctx);
  }
};

Av1Encoder::Av1Encoder() = default;
Av1Encoder::~Av1Encoder() = default;

bool Av1Encoder::open(const Av1Options& o, std::string& error) {
  if (o.width <= 0 || o.height <= 0 || o.width % 2 != 0 || o.height % 2 != 0 || !(o.fps > 0)) {
    error = "AV1 needs an even size and a frame rate over 0";
    return false;
  }
  if (o.crf < 0 || o.crf > 63) {
    error = "the AV1 crf " + std::to_string(o.crf) + " is not in 0..63";
    return false;
  }
  impl_ = std::make_unique<Impl>();
  fps_ = o.fps;
  aom_codec_iface_t* iface = aom_codec_av1_cx();
  if (aom_codec_enc_config_default(iface, &impl_->cfg, AOM_USAGE_REALTIME) != AOM_CODEC_OK) {
    error = "libaom has no realtime configuration";
    return false;
  }
  aom_codec_enc_cfg_t& c = impl_->cfg;
  c.g_w = static_cast<unsigned>(o.width);
  c.g_h = static_cast<unsigned>(o.height);
  c.g_timebase = {1, 1000};  // a picture's time in milliseconds, as the file keeps it
  c.g_threads = static_cast<unsigned>(std::max(1, o.threads));
  c.g_lag_in_frames = 0;
  c.g_error_resilient = 0;
  c.rc_end_usage = AOM_Q;
  c.kf_mode = AOM_KF_AUTO;
  c.kf_min_dist = 0;
  c.kf_max_dist = static_cast<unsigned>(std::max(1.0, std::round(o.key_seconds * o.fps)));
  if (aom_codec_enc_init(&impl_->ctx, iface, &c, 0) != AOM_CODEC_OK) {
    error = std::string("libaom did not start: ") + aom_codec_error_detail(&impl_->ctx);
    return false;
  }
  impl_->open = true;
  aom_codec_control(&impl_->ctx, AOME_SET_CPUUSED, std::clamp(o.speed, 7, 11));
  aom_codec_control(&impl_->ctx, AOME_SET_CQ_LEVEL, static_cast<unsigned>(o.crf));
  aom_codec_control(&impl_->ctx, AV1E_SET_ROW_MT, 1);
  aom_codec_control(&impl_->ctx, AV1E_SET_TILE_COLUMNS, o.width >= 1280 ? 1 : 0);
  if (aom_fixed_buf_t* h = aom_codec_get_global_headers(&impl_->ctx)) {
    const auto* p = static_cast<const uint8_t*>(h->buf);
    sequence_header_.assign(p, p + h->sz);
    free(h->buf);  // NOLINT(cppcoreguidelines-no-malloc): libaom hands both out with malloc
    free(h);       // NOLINT(cppcoreguidelines-no-malloc)
  }
  if (sequence_header_.empty()) {
    error = "libaom gave no sequence header";
    return false;
  }
  return true;
}

namespace {

bool drain(aom_codec_ctx_t& ctx, std::vector<Packet>& out) {
  aom_codec_iter_t it = nullptr;
  while (const aom_codec_cx_pkt_t* p = aom_codec_get_cx_data(&ctx, &it)) {
    if (p->kind != AOM_CODEC_CX_FRAME_PKT) continue;
    const auto* b = static_cast<const uint8_t*>(p->data.frame.buf);
    size_t n = p->data.frame.sz;
    // Matroska's AV1 blocks carry no temporal delimiter (an OBU of type 2 with no payload).
    if (n >= 2 && (b[0] >> 3 & 0x0F) == 2 && b[1] == 0) {
      b += 2;
      n -= 2;
    }
    Packet k;
    k.data.assign(b, b + n);
    k.ms = p->data.frame.pts;
    k.key = (p->data.frame.flags & AOM_FRAME_IS_KEY) != 0;
    out.push_back(std::move(k));
  }
  return true;
}

}  // namespace

bool Av1Encoder::encode(std::span<const uint8_t> i420, int64_t ms, std::vector<Packet>& out,
                        std::string& error) {
  if (!impl_ || !impl_->open) {
    error = "the AV1 encoder is not open";
    return false;
  }
  const unsigned w = impl_->cfg.g_w, h = impl_->cfg.g_h;
  if (i420.size() != static_cast<size_t>(w) * h * 3 / 2) {
    error = "the picture is not the AV1 encoder's " + std::to_string(w) + "x" + std::to_string(h);
    return false;
  }
  aom_img_wrap(&impl_->img, AOM_IMG_FMT_I420, w, h, 1, const_cast<uint8_t*>(i420.data()));
  const auto duration = static_cast<unsigned long>(std::max(1.0, std::round(1000.0 / fps_)));
  if (aom_codec_encode(&impl_->ctx, &impl_->img, ms, duration, 0) != AOM_CODEC_OK) {
    error = std::string("libaom refused the picture at ") + std::to_string(ms) +
            " ms: " + aom_codec_error_detail(&impl_->ctx);
    return false;
  }
  return drain(impl_->ctx, out);
}

bool Av1Encoder::flush(std::vector<Packet>& out, std::string& error) {
  if (!impl_ || !impl_->open) return true;
  if (aom_codec_encode(&impl_->ctx, nullptr, 0, 0, 0) != AOM_CODEC_OK) {
    error = std::string("libaom did not flush: ") + aom_codec_error_detail(&impl_->ctx);
    return false;
  }
  return drain(impl_->ctx, out);
}

struct Av1Decoder::Impl {
  aom_codec_ctx_t ctx{};
  bool open = false;
  ~Impl() {
    if (open) aom_codec_destroy(&ctx);
  }
};

Av1Decoder::Av1Decoder() = default;
Av1Decoder::~Av1Decoder() = default;

bool Av1Decoder::open(std::string& error) {
  impl_ = std::make_unique<Impl>();
  aom_codec_dec_cfg_t cfg{};
  cfg.threads = 1;
  cfg.allow_lowbitdepth = 1;
  if (aom_codec_dec_init(&impl_->ctx, aom_codec_av1_dx(), &cfg, 0) != AOM_CODEC_OK) {
    error = "the AV1 decoder did not start";
    return false;
  }
  impl_->open = true;
  return true;
}

bool Av1Decoder::decode(std::span<const uint8_t> packet, std::vector<uint8_t>& i420, int& width,
                        int& height, bool& got, std::string& error) {
  got = false;
  if (!impl_ || !impl_->open) {
    error = "the AV1 decoder is not open";
    return false;
  }
  if (aom_codec_decode(&impl_->ctx, packet.data(), packet.size(), nullptr) != AOM_CODEC_OK) {
    error = std::string("the AV1 decoder refused a packet: ") + aom_codec_error(&impl_->ctx);
    return false;
  }
  aom_codec_iter_t it = nullptr;
  while (aom_image_t* img = aom_codec_get_frame(&impl_->ctx, &it)) {
    if (img->fmt != AOM_IMG_FMT_I420) {
      error = "the AV1 decoder gave a picture that is not 8-bit I420";
      return false;
    }
    width = static_cast<int>(img->d_w);
    height = static_cast<int>(img->d_h);
    i420.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3 / 2);
    uint8_t* dst = i420.data();
    for (int plane = 0; plane < 3; plane++) {
      const int w = plane == 0 ? width : width / 2, h = plane == 0 ? height : height / 2;
      for (int y = 0; y < h; y++) {
        std::memcpy(dst, img->planes[plane] + static_cast<ptrdiff_t>(y) * img->stride[plane],
                    static_cast<size_t>(w));
        dst += w;
      }
    }
    got = true;
  }
  return true;
}

struct OpusEncoder::Impl {
  ::OpusEncoder* enc = nullptr;
  std::vector<uint8_t> buf = std::vector<uint8_t>(4000);
  ~Impl() {
    if (enc) opus_encoder_destroy(enc);
  }
};

OpusEncoder::OpusEncoder() = default;
OpusEncoder::~OpusEncoder() = default;

bool OpusEncoder::open(int bitrate, std::string& error) {
  impl_ = std::make_unique<Impl>();
  int err = 0;
  impl_->enc = opus_encoder_create(kOpusRate, 2, OPUS_APPLICATION_AUDIO, &err);
  if (err != OPUS_OK || !impl_->enc) {
    impl_->enc = nullptr;
    error = std::string("libopus did not start: ") + opus_strerror(err);
    return false;
  }
  if (opus_encoder_ctl(impl_->enc, OPUS_SET_BITRATE(bitrate)) != OPUS_OK) {
    error = "libopus refused the bitrate " + std::to_string(bitrate);
    return false;
  }
  opus_int32 lookahead = 0;
  opus_encoder_ctl(impl_->enc, OPUS_GET_LOOKAHEAD(&lookahead));
  preskip_ = static_cast<int>(lookahead);
  return true;
}

bool OpusEncoder::encode_one(std::vector<Packet>& out, std::string& error) {
  const opus_int32 n =
      opus_encode_float(impl_->enc, held_.data(), kOpusPacketSamples, impl_->buf.data(),
                        static_cast<opus_int32>(impl_->buf.size()));
  if (n < 0) {
    error =
        std::string("libopus refused packet ") + std::to_string(packets_) + ": " + opus_strerror(n);
    return false;
  }
  Packet p;
  p.data.assign(impl_->buf.begin(), impl_->buf.begin() + n);
  p.ms = next_ms();
  out.push_back(std::move(p));
  packets_++;
  held_.erase(held_.begin(), held_.begin() + std::ptrdiff_t{2} * kOpusPacketSamples);
  return true;
}

bool OpusEncoder::push(const float* stereo, size_t frames, std::vector<Packet>& out,
                       std::string& error) {
  if (!impl_ || !impl_->enc) {
    error = "the Opus encoder is not open";
    return false;
  }
  held_.insert(held_.end(), stereo, stereo + 2 * frames);
  pushed_ += static_cast<int64_t>(frames);
  while (held_.size() >= 2 * static_cast<size_t>(kOpusPacketSamples)) {
    if (!encode_one(out, error)) return false;
  }
  return true;
}

bool OpusEncoder::finish(std::vector<Packet>& out, std::string& error) {
  if (!impl_ || !impl_->enc) return true;
  // Every pushed sample comes out `preskip` samples late; pad with silence past that.
  const int64_t need = pushed_ + preskip_;
  while (packets_ * kOpusPacketSamples < need) {
    held_.resize(2 * static_cast<size_t>(kOpusPacketSamples), 0.0F);
    if (!encode_one(out, error)) return false;
  }
  return true;
}

struct OpusDecoder::Impl {
  ::OpusDecoder* dec = nullptr;
  std::vector<float> pcm = std::vector<float>(size_t{2} * 5760);
  ~Impl() {
    if (dec) opus_decoder_destroy(dec);
  }
};

OpusDecoder::OpusDecoder() = default;
OpusDecoder::~OpusDecoder() = default;

bool OpusDecoder::open(std::string& error) {
  impl_ = std::make_unique<Impl>();
  int err = 0;
  impl_->dec = opus_decoder_create(kOpusRate, 2, &err);
  if (err != OPUS_OK || !impl_->dec) {
    impl_->dec = nullptr;
    error = std::string("the Opus decoder did not start: ") + opus_strerror(err);
    return false;
  }
  return true;
}

bool OpusDecoder::decode(std::span<const uint8_t> packet, std::vector<float>& out,
                         std::string& error) {
  if (!impl_ || !impl_->dec) {
    error = "the Opus decoder is not open";
    return false;
  }
  const int n = opus_decode_float(impl_->dec, packet.data(), static_cast<opus_int32>(packet.size()),
                                  impl_->pcm.data(), 5760, 0);
  if (n < 0) {
    error = std::string("the Opus decoder refused a packet: ") + opus_strerror(n);
    return false;
  }
  out.insert(out.end(), impl_->pcm.begin(), impl_->pcm.begin() + std::ptrdiff_t{2} * n);
  return true;
}

#else  // no encoders: every open says so

bool encoders_built() { return false; }

struct Av1Encoder::Impl {};
Av1Encoder::Av1Encoder() = default;
Av1Encoder::~Av1Encoder() = default;
bool Av1Encoder::open(const Av1Options&, std::string& error) {
  error = kNoEncoders;
  return false;
}
bool Av1Encoder::encode(std::span<const uint8_t>, int64_t, std::vector<Packet>&,
                        std::string& error) {
  error = kNoEncoders;
  return false;
}
bool Av1Encoder::flush(std::vector<Packet>&, std::string&) { return true; }

struct Av1Decoder::Impl {};
Av1Decoder::Av1Decoder() = default;
Av1Decoder::~Av1Decoder() = default;
bool Av1Decoder::open(std::string& error) {
  error = kNoEncoders;
  return false;
}
bool Av1Decoder::decode(std::span<const uint8_t>, std::vector<uint8_t>&, int&, int&, bool& got,
                        std::string& error) {
  got = false;
  error = kNoEncoders;
  return false;
}

struct OpusEncoder::Impl {};
OpusEncoder::OpusEncoder() = default;
OpusEncoder::~OpusEncoder() = default;
bool OpusEncoder::open(int, std::string& error) {
  error = kNoEncoders;
  return false;
}
bool OpusEncoder::encode_one(std::vector<Packet>&, std::string& error) {
  error = kNoEncoders;
  return false;
}
bool OpusEncoder::push(const float*, size_t, std::vector<Packet>&, std::string& error) {
  error = kNoEncoders;
  return false;
}
bool OpusEncoder::finish(std::vector<Packet>&, std::string&) { return true; }

struct OpusDecoder::Impl {};
OpusDecoder::OpusDecoder() = default;
OpusDecoder::~OpusDecoder() = default;
bool OpusDecoder::open(std::string& error) {
  error = kNoEncoders;
  return false;
}
bool OpusDecoder::decode(std::span<const uint8_t>, std::vector<float>&, std::string& error) {
  error = kNoEncoders;
  return false;
}

#endif

std::vector<uint8_t> OpusEncoder::codec_private() const {
  std::vector<uint8_t> h = {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd', 1, 2};
  const auto le = [&](uint32_t v, int bytes) {
    for (int i = 0; i < bytes; i++) h.push_back(static_cast<uint8_t>(v >> (8 * i)));
  };
  le(static_cast<uint32_t>(preskip_), 2);
  le(kOpusRate, 4);
  le(0, 2);        // output gain
  h.push_back(0);  // channel mapping family 0: mono or stereo
  return h;
}

}  // namespace ge::capture
