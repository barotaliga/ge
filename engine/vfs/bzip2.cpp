// bzip2.hpp over the bzip2 library (cmake/Archives.cmake, built without its stdio half): the
// stream decoder fed the whole input, output grown a megabyte at a time up to the limit, a new
// stream begun where one ends while bytes remain.
//
// ge:summary bzip2.hpp over the bzip2 library: streams decoded in turn, the bomb limit
// ge:keywords bzip2 bz_stream decompress-init stream-end multi-stream bz-internal-error
// ge:skill vfs
// ge:test tests/vfs_unpack_test.cpp
#include "vfs/bzip2.hpp"

#include <algorithm>
#include <cstdlib>

extern "C" {
#include <bzlib.h>

// The library's own assertion (built without stdio it calls this instead of printing): a fault
// inside the library, never one of the input's, so it stops the program.
void bz_internal_error(int code);
void bz_internal_error(int /*code*/) { std::abort(); }
}

namespace ge::vfs {

bool is_bzip2(std::span<const uint8_t> b) {
  return b.size() >= 4 && b[0] == 'B' && b[1] == 'Z' && b[2] == 'h' && b[3] >= '1' && b[3] <= '9';
}

bool bzip2_unpack(std::span<const uint8_t> bytes, std::vector<uint8_t>& out, std::string& error,
                  size_t limit) {
  out.clear();
  if (!is_bzip2(bytes)) {
    error = "bzip2: the bytes do not open with a bzip2 header (BZh1 to BZh9)";
    return false;
  }
  size_t at = 0;
  while (at < bytes.size()) {
    bz_stream s{};
    if (BZ2_bzDecompressInit(&s, 0, 0) != BZ_OK) {
      error = "bzip2: the decoder could not start (out of memory)";
      return false;
    }
    s.next_in = const_cast<char*>(reinterpret_cast<const char*>(bytes.data() + at));
    s.avail_in = unsigned(std::min<size_t>(bytes.size() - at, 0x7FFFFFFF));
    int rc = BZ_OK;
    while (rc == BZ_OK) {
      if (out.size() >= limit) {
        BZ2_bzDecompressEnd(&s);
        error = "bzip2: the file unpacks to more than " + std::to_string(limit) +
                " bytes, more than any recording; refused";
        return false;
      }
      const size_t have = out.size();
      const size_t room = std::min<size_t>(size_t{1} << 20, limit - have);
      out.resize(have + room);
      s.next_out = reinterpret_cast<char*>(out.data() + have);
      s.avail_out = unsigned(room);
      const unsigned before_in = s.avail_in;
      rc = BZ2_bzDecompress(&s);
      out.resize(have + (room - s.avail_out));
      if (rc == BZ_OK && s.avail_in == before_in && s.avail_out == room) break;  // stalled: cut
    }
    const size_t used = (bytes.size() - at) - s.avail_in;
    BZ2_bzDecompressEnd(&s);
    if (rc != BZ_STREAM_END) {
      error = rc == BZ_OK ? "bzip2: the stream is cut short (the file is incomplete)"
                          : "bzip2: the stream is damaged (code " + std::to_string(rc) + ")";
      return false;
    }
    at += used;
    // Several streams joined: the next one starts here; anything else after the end is padding.
    if (!is_bzip2(bytes.subspan(at))) break;
  }
  return true;
}

std::vector<uint8_t> bzip2_pack(std::span<const uint8_t> bytes) {
  // The format's bound: the input, one percent more and 600 bytes.
  std::vector<uint8_t> out(bytes.size() + bytes.size() / 100 + 600);
  auto len = unsigned(out.size());
  const int rc = BZ2_bzBuffToBuffCompress(
      reinterpret_cast<char*>(out.data()), &len,
      const_cast<char*>(reinterpret_cast<const char*>(bytes.data())), unsigned(bytes.size()), 9,
      0, 0);
  if (rc != BZ_OK) return {};
  out.resize(len);
  return out;
}

}  // namespace ge::vfs
