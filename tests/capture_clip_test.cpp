// A row of pictures written as a video (engine/capture/clip.hpp): read back as a WebM whose one
// track is AV1 at the pictures' size, one block a picture in time order; and the refusals said.
//
// ge:summary Test: write_clip's WebM reads back with a block a picture; empty and tiny refused
// ge:keywords test capture clip write_clip webm av1 blocks refused
// ge:skill testing replay
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "capture/clip.hpp"
#include "capture/codecs.hpp"
#include "capture/mkv_read.hpp"
#include "check.hpp"

namespace gc = ge::capture;
using ge::lab::Picture;

TEST(a_clip_reads_back_as_a_webm_with_a_block_a_picture) {
  if (!gc::encoders_built()) {
    ge::test::skip("this build has no encoders (GE_ENCODERS off)");
    return;
  }
  std::vector<Picture> frames;
  for (int k = 0; k < 12; ++k) {
    // An odd width: the clip drops the last column.
    Picture p = ge::lab::create_picture(97, 64, {30, 30, 40});
    ge::lab::fill_rect(p, 4 + k * 6, 20, 16, 16, {250, 200, 60});
    frames.push_back(p);
  }
  const std::string path = (std::filesystem::temp_directory_path() / "ge_clip_test.webm").string();
  std::string error;
  CHECK(gc::write_clip(path, frames, 24, error));
  std::vector<uint8_t> bytes;
  {
    // Closed before the file is removed below: Windows refuses to delete an open file.
    std::ifstream f(path, std::ios::binary);
    bytes.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }
  const std::optional<gc::ReadFile> read = gc::read_mkv(bytes, error);
  CHECK(read.has_value());
  if (read.has_value()) {
    const gc::ReadFile& got = *read;
    const gc::ReadFile* file = &got;
    CHECK(file->doc_type == "webm");
    CHECK(file->tracks.size() == 1 && file->tracks[0].codec == "V_AV1");
    CHECK(file->tracks[0].width == 96 && file->tracks[0].height == 64);
    CHECK(file->blocks.size() == frames.size());
    bool ordered = true;
    for (size_t i = 1; i < file->blocks.size(); ++i)
      ordered = ordered && file->blocks[i].ms > file->blocks[i - 1].ms;
    CHECK(ordered);
  }
  std::error_code ec;
  std::filesystem::remove(path, ec);
}

TEST(no_pictures_or_tiny_ones_are_refused_with_the_reason) {
  std::string error;
  CHECK(!gc::write_clip("unused.webm", {}, 30, error));
  CHECK(error.find("no pictures") != std::string::npos);
  const std::vector<Picture> tiny = {ge::lab::create_picture(1, 1, {0, 0, 0})};
  CHECK(!gc::write_clip("unused.webm", tiny, 30, error));
  CHECK(error.find("2x2") != std::string::npos);
}
