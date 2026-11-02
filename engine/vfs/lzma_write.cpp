// The writers behind lzma.hpp: an xz file by the SDK's xz encoder, and a 7z archive of one
// LZMA-coded file whose header is written here (the signature header, the pack and unpack info,
// the CRC, the name). Kept apart from lzma.cpp so a reader links no encoder.
//
// ge:summary The xz and 7z writers: the SDK's encoders and a one-file 7z header
// ge:keywords xz-encode lzma-encode 7z-header 7z-number signature-header write_7z xz_pack
// ge:skill vfs
// ge:test tests/vfs_unpack_test.cpp
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <mutex>

#include "vfs/lzma.hpp"

extern "C" {
#include <7zCrc.h>
#include <LzmaEnc.h>
#include <XzCrc64.h>
#include <XzEnc.h>
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

struct SpanIn {
  ISeqInStream vt;
  std::span<const uint8_t> bytes;
  size_t pos = 0;
};
SRes span_in(ISeqInStreamPtr p, void* buf, size_t* size) {
  auto* s = reinterpret_cast<SpanIn*>(const_cast<ISeqInStream*>(p));
  const size_t n = std::min(*size, s->bytes.size() - s->pos);
  if (n) std::memcpy(buf, s->bytes.data() + s->pos, n);
  s->pos += n;
  *size = n;
  return SZ_OK;
}
struct VecOut {
  ISeqOutStream vt;
  std::vector<uint8_t>* out;
};
size_t vec_out(ISeqOutStreamPtr p, const void* buf, size_t size) {
  auto* s = reinterpret_cast<VecOut*>(const_cast<ISeqOutStream*>(p));
  const auto* b = static_cast<const uint8_t*>(buf);
  s->out->insert(s->out->end(), b, b + size);
  return size;
}

void put_u32(std::vector<uint8_t>& o, uint32_t v) {
  for (int i = 0; i < 4; i++) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void put_u64(std::vector<uint8_t>& o, uint64_t v) {
  for (int i = 0; i < 8; i++) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
// A 7z NUMBER: the first byte's leading ones count the bytes that follow (low bytes first), its
// remaining bits the value's top.
void put_number(std::vector<uint8_t>& o, uint64_t v) {
  for (int n = 0; n < 8; n++) {
    const int bits = 8 * n + (7 - n);
    if (v < (uint64_t{1} << bits)) {
      o.push_back(static_cast<uint8_t>((0xFF00u >> n) & 0xFF) | static_cast<uint8_t>(v >> (8 * n)));
      for (int i = 0; i < n; i++) o.push_back(static_cast<uint8_t>(v >> (8 * i)));
      return;
    }
  }
  o.push_back(0xFF);
  put_u64(o, v);
}

}  // namespace

std::vector<uint8_t> xz_pack(std::span<const uint8_t> bytes, int level) {
  tables();
  CXzProps props;
  XzProps_Init(&props);
  props.lzma2Props.lzmaProps.level = level;
  props.checkId = XZ_CHECK_CRC64;
  props.reduceSize = bytes.size();
  std::vector<uint8_t> out;
  SpanIn in{{span_in}, bytes, 0};
  VecOut o{{vec_out}, &out};
  CXzEncHandle x = XzEnc_Create(&kAlloc, &kAlloc);
  XzEnc_SetProps(x, &props);
  XzEnc_SetDataSize(x, bytes.size());
  XzEnc_Encode(x, &o.vt, &in.vt, nullptr);
  XzEnc_Destroy(x);
  return out;
}

std::vector<uint8_t> write_7z(const std::string& name, std::span<const uint8_t> bytes, int level) {
  tables();
  CLzmaEncProps props;
  LzmaEncProps_Init(&props);
  props.level = level;
  props.reduceSize = bytes.size();
  std::vector<uint8_t> packed(bytes.size() + bytes.size() / 3 + 128);
  SizeT packed_size = packed.size();
  Byte coder_props[LZMA_PROPS_SIZE];
  SizeT props_size = LZMA_PROPS_SIZE;
  LzmaEncode(packed.data(), &packed_size, bytes.data(), bytes.size(), &props, coder_props,
             &props_size, 0, nullptr, &kAlloc, &kAlloc);
  packed.resize(packed_size);

  std::vector<uint8_t> h;  // the header
  h.push_back(0x01);       // kHeader
  h.push_back(0x04);       // kMainStreamsInfo
  h.push_back(0x06);       // kPackInfo: at 0, one stream of packed_size bytes
  put_number(h, 0);
  put_number(h, 1);
  h.push_back(0x09);
  put_number(h, packed.size());
  h.push_back(0x00);
  h.push_back(0x07);  // kUnPackInfo: one folder of one LZMA coder
  h.push_back(0x0B);
  put_number(h, 1);
  h.push_back(0x00);  // not external
  put_number(h, 1);   // one coder
  h.push_back(0x23);  // a 3-byte id, with properties
  h.push_back(0x03);
  h.push_back(0x01);
  h.push_back(0x01);
  put_number(h, props_size);
  h.insert(h.end(), coder_props, coder_props + props_size);
  h.push_back(0x0C);  // kCodersUnPackSize
  put_number(h, bytes.size());
  h.push_back(0x00);
  h.push_back(0x08);  // kSubStreamsInfo: the one file's CRC
  h.push_back(0x0A);
  h.push_back(0x01);
  put_u32(h, CrcCalc(bytes.data(), bytes.size()));
  h.push_back(0x00);
  h.push_back(0x00);  // the streams' end
  h.push_back(0x05);  // kFilesInfo: one file and its name
  put_number(h, 1);
  std::vector<uint8_t> wide;
  for (const char c : name) {
    wide.push_back(static_cast<uint8_t>(c));
    wide.push_back(0);
  }
  wide.push_back(0);
  wide.push_back(0);
  h.push_back(0x11);
  put_number(h, wide.size() + 1);
  h.push_back(0x00);  // not external
  h.insert(h.end(), wide.begin(), wide.end());
  h.push_back(0x00);  // the files' end
  h.push_back(0x00);  // the header's end

  std::vector<uint8_t> start;  // the start header the signature header's CRC covers
  put_u64(start, packed.size());
  put_u64(start, h.size());
  put_u32(start, CrcCalc(h.data(), h.size()));
  std::vector<uint8_t> out(kSevenMagic, kSevenMagic + 6);
  out.push_back(0);
  out.push_back(4);
  put_u32(out, CrcCalc(start.data(), start.size()));
  out.insert(out.end(), start.begin(), start.end());
  out.insert(out.end(), packed.begin(), packed.end());
  out.insert(out.end(), h.begin(), h.end());
  return out;
}

}  // namespace ge::vfs
