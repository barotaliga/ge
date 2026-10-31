// write_clip: each picture cropped even, made I420, coded and muxed in time order.
//
// ge:summary write_clip's definition: crop, I420, encode, mux, close
// ge:keywords capture clip write_clip webm av1 mux encode
// ge:skill replay labs
// ge:test tests/capture_clip_test.cpp
#include "capture/clip.hpp"

#include <vector>

#include "capture/av_export.hpp"
#include "capture/mux.hpp"
#include "capture/video.hpp"
#include "capture/yuv.hpp"

namespace ge::capture {

bool write_clip(const std::string& path, std::span<const lab::Picture> frames, double fps,
                std::string& error, std::string_view codec) {
  if (frames.empty()) {
    error = "no pictures to write";
    return false;
  }
  const int w = frames[0].width & ~1, h = frames[0].height & ~1;
  if (w < 2 || h < 2) {
    error = "the pictures are smaller than 2x2";
    return false;
  }
  VideoEncoderOptions vo;
  vo.width = w;
  vo.height = h;
  vo.fps = fps;
  vo.crf = 24;
  vo.speed = 8;
  VideoEncoder enc;
  if (!enc.open(codec, vo, error)) return false;
  MatroskaOptions mo;
  VideoTrack track;
  track.codec = std::string(enc.matroska_codec());
  track.width = w;
  track.height = h;
  track.fps = fps;
  track.codec_private = enc.codec_private();
  mo.video = track;
  Mux mux;
  if (!mux.open(path, container_of(path), mo, error)) return false;
  std::vector<uint8_t> yuv;
  std::vector<Packet> packets;
  lab::Picture even{w, h, std::vector<uint8_t>(size_t(w) * size_t(h) * 4)};
  auto drain = [&]() {
    for (const Packet& p : packets)
      if (!mux.video(p.ms, p.key, p.data, error)) return false;
    packets.clear();
    return true;
  };
  for (size_t k = 0; k < frames.size(); ++k) {
    const lab::Picture& f = frames[k];
    if (f.width < w || f.height < h) {
      error = "picture " + std::to_string(k) + " is smaller than the first";
      return false;
    }
    for (int y = 0; y < h; ++y)
      std::copy_n(&f.rgba[size_t(y) * size_t(f.width) * 4], size_t(w) * 4,
                  &even.rgba[size_t(y) * size_t(w) * 4]);
    to_i420(even, yuv);
    if (!enc.encode(yuv, frame_ms(static_cast<int64_t>(k), fps), packets, error) || !drain())
      return false;
  }
  if (!enc.flush(packets, error) || !drain()) return false;
  return mux.close(frame_ms(static_cast<int64_t>(frames.size()), fps), error);
}

}  // namespace ge::capture
