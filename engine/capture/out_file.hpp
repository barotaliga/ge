// WHERE A MUXER'S BYTES GO: a file, or memory when no path is given, written in order and patched
// in place later (a size, a duration or an index known only at the close). Both the Matroska and
// the MP4 muxer write through one. A write the disk refuses says how far it got, so a full disk
// is named as one; the bytes already written stay a file a player can read to its last whole
// cluster or fragment.
//
// ge:summary A muxer's output: a file or memory, appended to and patched, 64-bit offsets
// ge:keywords muxer output file memory patch write full-disk fseek 64-bit
// ge:skill replay
// ge:test tests/capture_matroska_test.cpp
#pragma once

#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

namespace ge::capture {

class OutFile {
 public:
  OutFile() = default;
  OutFile(const OutFile&) = delete;
  OutFile& operator=(const OutFile&) = delete;
  ~OutFile();

  // Start writing `path` (created or emptied), or with an empty path keep every byte in memory.
  bool open(const std::string& path, std::string& error);
  bool emit(std::span<const uint8_t> b, std::string& error);
  // `b` over the bytes at `at`, which were emitted before.
  bool patch(int64_t at, std::span<const uint8_t> b, std::string& error);
  // The file closed (nothing to do in memory).
  bool close(std::string& error);

  const std::vector<uint8_t>& bytes() const { return memory_; }
  int64_t written() const { return written_; }

 private:
  std::FILE* file_ = nullptr;
  std::vector<uint8_t> memory_;
  int64_t written_ = 0;
};

}  // namespace ge::capture
