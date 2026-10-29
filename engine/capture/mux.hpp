// THE CONTAINER A CAPTURE IS WRITTEN IN (docs/replays.md § MP4): Matroska or WebM
// (capture/matroska.hpp) or fragmented MP4 (capture/mp4.hpp), picked by the file's name and
// written through one interface, so the export, the live capture and the interleave (which
// hand it pictures and sound in time order) never know which. Both take the same tracks
// (`MatroskaOptions`, described with Matroska's codec ids); an MP4 refuses the uncompressed ones.
//
// ge:summary The capture's container: Matroska, WebM or fragmented MP4, picked by the file's name
// ge:keywords container mux muxer mp4 fmp4 matroska mkv webm extension pick
// ge:skill replay
// ge:test tests/capture_mp4_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "capture/matroska.hpp"
#include "capture/mp4.hpp"

namespace ge::capture {

enum class Container : uint8_t { kMatroska, kMp4 };

// kMp4 for a name ending .mp4 or .m4v (any case), else kMatroska.
Container container_of(std::string_view path);
// The container a capture's option names: "mp4", "mkv", or "" for the one `path`'s name picks.
// False, with `error`, for any other word.
bool container_named(std::string_view word, std::string_view path, Container& out,
                     std::string& error);

class Mux {
 public:
  // Start a file at `path` (in memory with an empty one) in container `c`.
  bool open(const std::string& path, Container c, const MatroskaOptions& o, std::string& error);
  bool video(int64_t ms, bool key, std::span<const uint8_t> data, std::string& error) {
    return mp4_on_ ? mp4_.video(ms, key, data, error) : mkv_.video(ms, key, data, error);
  }
  bool audio(int64_t ms, std::span<const uint8_t> data, std::string& error) {
    return mp4_on_ ? mp4_.audio(ms, data, error) : mkv_.audio(ms, data, error);
  }
  bool close(int64_t end_ms, std::string& error) {
    return mp4_on_ ? mp4_.close(end_ms, error) : mkv_.close(end_ms, error);
  }

  const std::vector<uint8_t>& bytes() const { return mp4_on_ ? mp4_.bytes() : mkv_.bytes(); }
  int64_t written() const { return mp4_on_ ? mp4_.written() : mkv_.written(); }
  // "mp4", "webm" or "matroska".
  std::string_view doc_type() const { return doc_type_; }

 private:
  bool mp4_on_ = false;
  MatroskaWriter mkv_;
  Mp4Writer mp4_;
  std::string doc_type_;
};

}  // namespace ge::capture
