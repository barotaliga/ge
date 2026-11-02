// The readers behind lzma.hpp: the xz unpacker run to the end of the input, and the 7z archive
// opened over the bytes in memory (the SDK's seekable stream, answered from a span), its names
// turned from UTF-16 to UTF-8. The CRC tables are built once, on first use.
//
// ge:summary The LZMA SDK's xz unpacker and 7z reader over bytes in memory
// ge:keywords xz-unpacker sz-arex 7z-open look-to-read utf16 crc-table
// ge:skill vfs
// ge:test tests/vfs_unpack_test.cpp
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "vfs/lzma.hpp"

extern "C" {
#include <7z.h>
#include <7zCrc.h>
#include <LzmaDec.h>
#include <Xz.h>
#include <XzCrc64.h>
}

namespace ge::vfs {

namespace {

void* alloc(ISzAllocPtr, size_t size) { return size == 0 ? nullptr : std::malloc(size); }
void release(ISzAllocPtr, void* p) { std::free(p); }
const ISzAlloc kAlloc = {alloc, release};

void tables() {
  static std::once_flag once;
  std::call_once(once, [] {
    CrcGenerateTable();
    Crc64GenerateTable();
  });
}

std::string sres_name(SRes r) {
  switch (r) {
    case SZ_ERROR_DATA:
      return "the packed data is damaged";
    case SZ_ERROR_MEM:
      return "out of memory";
    case SZ_ERROR_CRC:
      return "a checksum does not match";
    case SZ_ERROR_UNSUPPORTED:
      return "it uses a method this reader does not take";
    case SZ_ERROR_INPUT_EOF:
      return "it is cut short";
    case SZ_ERROR_NO_ARCHIVE:
      return "its header is not one";
    case SZ_ERROR_ARCHIVE:
      return "its directory is damaged";
    default:
      return "error " + std::to_string(r);
  }
}

// A seekable stream over a span.
struct SpanStream {
  ISeekInStream vt;
  std::span<const uint8_t> bytes;
  size_t pos = 0;
};

SRes span_read(ISeekInStreamPtr p, void* buf, size_t* size) {
  auto* s = reinterpret_cast<SpanStream*>(const_cast<ISeekInStream*>(p));
  const size_t n = std::min(*size, s->bytes.size() - s->pos);
  if (n) std::memcpy(buf, s->bytes.data() + s->pos, n);
  s->pos += n;
  *size = n;
  return SZ_OK;
}

SRes span_seek(ISeekInStreamPtr p, Int64* pos, ESzSeek origin) {
  auto* s = reinterpret_cast<SpanStream*>(const_cast<ISeekInStream*>(p));
  int64_t base = 0;
  if (origin == SZ_SEEK_CUR) base = static_cast<int64_t>(s->pos);
  if (origin == SZ_SEEK_END) base = static_cast<int64_t>(s->bytes.size());
  const int64_t to = base + *pos;
  if (to < 0 || to > static_cast<int64_t>(s->bytes.size())) return SZ_ERROR_READ;
  s->pos = static_cast<size_t>(to);
  *pos = to;
  return SZ_OK;
}

// The 7z archive opened over a span; closed when it goes.
struct Seven {
  SpanStream stream;
  CLookToRead2 look{};
  CSzArEx db{};
  std::vector<uint8_t> buf = std::vector<uint8_t>(size_t{1} << 16);
  bool open = false;

  explicit Seven(std::span<const uint8_t> bytes) {
    stream.vt.Read = span_read;
    stream.vt.Seek = span_seek;
    stream.bytes = bytes;
    LookToRead2_CreateVTable(&look, False);
    look.buf = buf.data();
    look.bufSize = buf.size();
    look.realStream = &stream.vt;
    LookToRead2_INIT(&look) SzArEx_Init(&db);
  }
  ~Seven() { SzArEx_Free(&db, &kAlloc); }
  Seven(const Seven&) = delete;
  Seven& operator=(const Seven&) = delete;
};

void append_utf8(std::string& out, uint32_t c) {
  if (c < 0x80) {
    out += static_cast<char>(c);
  } else if (c < 0x800) {
    out += static_cast<char>(0xC0 | (c >> 6));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else if (c < 0x10000) {
    out += static_cast<char>(0xE0 | (c >> 12));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (c >> 18));
    out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (c & 0x3F));
  }
}

std::string name_of(const CSzArEx& db, size_t i) {
  const size_t n = SzArEx_GetFileNameUtf16(&db, i, nullptr);
  std::vector<UInt16> w(n);
  SzArEx_GetFileNameUtf16(&db, i, w.data());
  std::string out;
  for (size_t k = 0; k < n && w[k] != 0; k++) {
    uint32_t c = w[k];
    if (c >= 0xD800 && c < 0xDC00 && k + 1 < n && w[k + 1] >= 0xDC00 && w[k + 1] < 0xE000) {
      c = 0x10000 + ((c - 0xD800) << 10) + (w[k + 1] - 0xDC00u);
      k++;
    }
    append_utf8(out, c == '\\' ? '/' : c);
  }
  return out;
}

bool open_seven(Seven& s, std::string& error) {
  tables();
  const SRes r = SzArEx_Open(&s.db, &s.look.vt, &kAlloc, &kAlloc);
  if (r != SZ_OK) {
    error = "the 7z archive does not open: " + sres_name(r) +
            "; next: test it with 7-Zip, or download it again";
    return false;
  }
  s.open = true;
  return true;
}

}  // namespace

bool xz_unpack(std::span<const uint8_t> bytes, std::vector<uint8_t>& out, std::string& error,
               size_t limit) {
  tables();
  out.clear();
  CXzUnpacker x;
  XzUnpacker_Construct(&x, &kAlloc);
  XzUnpacker_Init(&x);
  size_t in = 0;
  const size_t chunk = size_t{1} << 20;
  ECoderStatus status = CODER_STATUS_NOT_SPECIFIED;
  SRes r = SZ_OK;
  for (;;) {
    const size_t at = out.size();
    out.resize(at + chunk);
    SizeT dest = chunk, src = bytes.size() - in;
    r = XzUnpacker_Code(&x, out.data() + at, &dest, bytes.data() + in, &src, 1, CODER_FINISH_ANY,
                        &status);
    out.resize(at + dest);
    in += src;
    if (r != SZ_OK) break;
    if (out.size() > limit) {
      XzUnpacker_Free(&x);
      error = "the xz file unpacks to more than " + std::to_string(limit) +
              " bytes; next: check the file";
      return false;
    }
    if (dest == 0 && src == 0) break;  // nothing more comes out of what is left
  }
  const bool finished = r == SZ_OK && XzUnpacker_IsStreamWasFinished(&x);
  XzUnpacker_Free(&x);
  if (r != SZ_OK || !finished) {
    error = "the xz file does not unpack: " +
            (r != SZ_OK ? sres_name(r) : std::string("it is cut short")) + " (at packed byte " +
            std::to_string(in) + "); next: download it again";
    return false;
  }
  return true;
}

bool zip_lzma_unpack(std::span<const uint8_t> packed, size_t size, std::vector<uint8_t>& out,
                     std::string& error) {
  if (packed.size() < 4 + LZMA_PROPS_SIZE || (packed[2] | (packed[3] << 8)) != LZMA_PROPS_SIZE) {
    error = "its LZMA header is not the 4 bytes and 5 properties a zip entry carries";
    return false;
  }
  out.resize(size);
  SizeT dest = size, src = packed.size() - 4 - LZMA_PROPS_SIZE;
  ELzmaStatus status;
  const SRes r = LzmaDecode(out.data(), &dest, packed.data() + 4 + LZMA_PROPS_SIZE, &src,
                            packed.data() + 4, LZMA_PROPS_SIZE, LZMA_FINISH_ANY, &status, &kAlloc);
  if (r != SZ_OK || dest != size) {
    error = r != SZ_OK ? sres_name(r) : std::string("it is cut short");
    return false;
  }
  return true;
}

bool read_7z(std::span<const uint8_t> bytes, std::vector<SevenEntry>& out, std::string& error) {
  out.clear();
  Seven s(bytes);
  if (!open_seven(s, error)) return false;
  for (UInt32 i = 0; i < s.db.NumFiles; i++)
    out.push_back({name_of(s.db, i), SzArEx_GetFileSize(&s.db, i), SzArEx_IsDir(&s.db, i) != 0});
  return true;
}

bool seven_unpack(std::span<const uint8_t> bytes, size_t index, std::vector<uint8_t>& out,
                  std::string& error) {
  out.clear();
  Seven s(bytes);
  if (!open_seven(s, error)) return false;
  if (index >= s.db.NumFiles) {
    error = "the 7z archive has " + std::to_string(s.db.NumFiles) + " files, not a file " +
            std::to_string(index) + "; next: pick one read_7z listed";
    return false;
  }
  UInt32 block = 0xFFFFFFFF;
  Byte* buf = nullptr;
  size_t buf_size = 0, offset = 0, size = 0;
  const SRes r = SzArEx_Extract(&s.db, &s.look.vt, static_cast<UInt32>(index), &block, &buf,
                                &buf_size, &offset, &size, &kAlloc, &kAlloc);
  if (r == SZ_OK) out.assign(buf + offset, buf + offset + size);
  std::free(buf);
  if (r != SZ_OK) {
    error = "the 7z archive's file '" + name_of(s.db, index) +
            "' does not unpack: " + sres_name(r) +
            (r == SZ_ERROR_UNSUPPORTED ? " (an encrypted archive is one)" : "") +
            "; next: unpack it with 7-Zip and open the file itself";
    return false;
  }
  return true;
}

}  // namespace ge::vfs
