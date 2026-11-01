// The zstd library's streaming calls behind ZstdStream, the one-shot pack, and the unpacker that
// reads frame after frame to a limit (cmake/Archives.cmake builds the library).
//
// ge:summary The zstd codec: the library's compression and decompression streams behind zstd.hpp
// ge:keywords zstd cctx dctx compress-stream decompress-stream window-log-max ldm
// ge:skill vfs
// ge:test tests/vfs_zstd_test.cpp
#include "vfs/zstd.hpp"

#include <algorithm>
#include <cstring>

#define ZSTD_STATIC_LINKING_ONLY  // ZSTD_sizeof_CCtx
#include <zstd.h>

namespace ge::vfs {

struct ZstdStream::Context {
  ZSTD_CCtx* c = nullptr;
};

namespace {

void free_context(ZstdStream::Context* c) {
  if (c) ZSTD_freeCCtx(c->c);
  delete c;
}

void set_level(ZSTD_CCtx* c, const ZstdLevel& level) {
  ZSTD_CCtx_setParameter(c, ZSTD_c_compressionLevel, level.level);
  if (level.long_match) ZSTD_CCtx_setParameter(c, ZSTD_c_enableLongDistanceMatching, 1);
  if (level.window_log > 0) ZSTD_CCtx_setParameter(c, ZSTD_c_windowLog, level.window_log);
  ZSTD_CCtx_setParameter(c, ZSTD_c_checksumFlag, 1);
}

// Runs the packer over `in` with `op` until it has taken it all (and, for a flush or an end,
// until it says nothing is left), appending to `out`.
void run(ZSTD_CCtx* c, std::span<const uint8_t> in, ZSTD_EndDirective op,
         std::vector<uint8_t>& out) {
  ZSTD_inBuffer ib{in.data(), in.size(), 0};
  const size_t chunk = ZSTD_CStreamOutSize();
  for (;;) {
    const size_t at = out.size();
    out.resize(at + chunk);
    ZSTD_outBuffer ob{out.data() + at, chunk, 0};
    const size_t left = ZSTD_compressStream2(c, &ob, &ib, op);
    out.resize(at + ob.pos);
    if (ZSTD_isError(left)) return;  // only a bad parameter: the levels above are all valid
    if (op == ZSTD_e_continue ? ib.pos == ib.size : left == 0) return;
  }
}

}  // namespace

bool is_zstd(std::span<const uint8_t> bytes) {
  return bytes.size() >= 4 && std::memcmp(bytes.data(), kZstdMagic, 4) == 0;
}

size_t zstd_pack_memory(const ZstdLevel& level) {
  // Measured, not estimated: a stream of unknown length allocates for its whole window at its
  // first byte, and the library's estimate divides by zero for the long matcher's defaults.
  ZSTD_CCtx* c = ZSTD_createCCtx();
  set_level(c, level);
  const uint8_t one = 0;
  std::vector<uint8_t> out;
  run(c, {&one, 1}, ZSTD_e_flush, out);
  const size_t n = ZSTD_sizeof_CCtx(c);
  ZSTD_freeCCtx(c);
  return n;
}

std::vector<uint8_t> zstd_pack(std::span<const uint8_t> bytes, const ZstdLevel& level) {
  ZSTD_CCtx* c = ZSTD_createCCtx();
  set_level(c, level);
  ZSTD_CCtx_setPledgedSrcSize(c, bytes.size());
  std::vector<uint8_t> out;
  out.reserve(ZSTD_compressBound(bytes.size()));
  run(c, bytes, ZSTD_e_end, out);
  ZSTD_freeCCtx(c);
  return out;
}

bool zstd_unpack(std::span<const uint8_t> bytes, std::vector<uint8_t>& out, bool& truncated,
                 std::string& error, size_t limit) {
  out.clear();
  truncated = false;
  if (!is_zstd(bytes)) {
    error = "the bytes do not open with a zstd frame's magic (28 B5 2F FD); next: check the file";
    return false;
  }
  if (const unsigned long long size = ZSTD_getFrameContentSize(bytes.data(), bytes.size());
      size != ZSTD_CONTENTSIZE_UNKNOWN && size != ZSTD_CONTENTSIZE_ERROR)
    out.reserve(static_cast<size_t>(std::min<unsigned long long>(size, limit)));
  ZSTD_DCtx* d = ZSTD_createDCtx();
  ZSTD_DCtx_setParameter(d, ZSTD_d_windowLogMax, 31);
  ZSTD_inBuffer ib{bytes.data(), bytes.size(), 0};
  const size_t chunk = ZSTD_DStreamOutSize();
  size_t want = 1;  // the last call's hint: 0 when a frame has just ended
  bool ok = true;
  while (ib.pos < ib.size) {
    const size_t at = out.size();
    out.resize(at + chunk);
    ZSTD_outBuffer ob{out.data() + at, chunk, 0};
    const size_t pos_in = ib.pos;
    want = ZSTD_decompressStream(d, &ob, &ib);
    out.resize(at + ob.pos);
    if (ZSTD_isError(want)) {
      error = std::string("the zstd frame is damaged at packed byte ") + std::to_string(ib.pos) +
              " (" + ZSTD_getErrorName(want) + "); next: get the file again";
      ok = false;
      break;
    }
    if (out.size() > limit) {
      error = "the zstd file unpacks to more than " + std::to_string(limit) +
              " bytes, which no recording is; next: check the file";
      ok = false;
      break;
    }
    if (ob.pos == 0 && ib.pos == pos_in) break;  // no progress: the input is exhausted
  }
  // Output the decoder still holds after the input ran out.
  while (ok && want != 0) {
    const size_t at = out.size();
    out.resize(at + chunk);
    ZSTD_outBuffer ob{out.data() + at, chunk, 0};
    ZSTD_inBuffer none{nullptr, 0, 0};
    want = ZSTD_decompressStream(d, &ob, &none);
    out.resize(at + ob.pos);
    if (ZSTD_isError(want) || ob.pos == 0) break;
  }
  ZSTD_freeDCtx(d);
  if (ok && want != 0) truncated = true;
  return ok;
}

void zstd_begin(ZstdStream& s, const ZstdLevel& level) {
  s.ctx = {new ZstdStream::Context{ZSTD_createCCtx()}, free_context};
  set_level(s.ctx->c, level);
  s.out.clear();
  s.fed = 0;
  s.ended = false;
}

void zstd_write(ZstdStream& s, std::span<const uint8_t> bytes) {
  if (!s.ctx || bytes.empty()) return;
  run(s.ctx->c, bytes, ZSTD_e_continue, s.out);
  s.fed += static_cast<int64_t>(bytes.size());
}

void zstd_flush(ZstdStream& s) {
  if (s.ctx) run(s.ctx->c, {}, ZSTD_e_flush, s.out);
}

void zstd_end(ZstdStream& s) {
  if (!s.ctx) return;
  run(s.ctx->c, {}, ZSTD_e_end, s.out);
  s.ctx.reset();
  s.ended = true;
}

}  // namespace ge::vfs
