// A muxer's output (out_file.hpp).
//
// ge:summary OutFile's definitions: the file or the memory, the write, the 64-bit seek and patch
// ge:keywords muxer output file patch fseeko seek-64
// ge:skill replay
// ge:test tests/capture_matroska_test.cpp
#include "capture/out_file.hpp"

#include <cstring>

namespace ge::capture {
namespace {

// A 64-bit seek (a long is 32 bits on Windows, and an export can pass 2 GB).
bool seek_to(std::FILE* f, int64_t at, int whence) {
#ifdef _WIN32
  return _fseeki64(f, at, whence) == 0;
#else
  return fseeko(f, static_cast<off_t>(at), whence) == 0;
#endif
}

}  // namespace

OutFile::~OutFile() {
  if (file_) std::fclose(file_);
}

bool OutFile::open(const std::string& path, std::string& error) {
  if (file_) std::fclose(file_);
  file_ = nullptr;
  memory_.clear();
  written_ = 0;
  if (path.empty()) return true;
#ifdef _WIN32
  if (fopen_s(&file_, path.c_str(), "wb+") != 0) file_ = nullptr;
#else
  file_ = std::fopen(path.c_str(), "wb+");
#endif
  if (!file_) {
    error = "cannot write " + path;
    return false;
  }
  return true;
}

bool OutFile::emit(std::span<const uint8_t> b, std::string& error) {
  if (file_) {
    if (std::fwrite(b.data(), 1, b.size(), file_) != b.size()) {
      error = "the disk took " + std::to_string(written_) + " bytes and no more";
      return false;
    }
  } else {
    memory_.insert(memory_.end(), b.begin(), b.end());
  }
  written_ += static_cast<int64_t>(b.size());
  return true;
}

bool OutFile::patch(int64_t at, std::span<const uint8_t> b, std::string& error) {
  if (at < 0 || at + static_cast<int64_t>(b.size()) > written_) {
    error = "a patch at byte " + std::to_string(at) + " runs past the " + std::to_string(written_) +
            " bytes written";
    return false;
  }
  if (!file_) {
    std::memcpy(memory_.data() + at, b.data(), b.size());
    return true;
  }
  if (!seek_to(file_, at, SEEK_SET) || std::fwrite(b.data(), 1, b.size(), file_) != b.size() ||
      !seek_to(file_, 0, SEEK_END)) {
    error = "cannot patch the file at byte " + std::to_string(at);
    return false;
  }
  return true;
}

bool OutFile::close(std::string& error) {
  if (!file_) return true;
  const bool ok = std::fclose(file_) == 0;
  file_ = nullptr;
  if (!ok) {
    error = "the file did not close (a full disk?)";
    return false;
  }
  return true;
}

}  // namespace ge::capture
