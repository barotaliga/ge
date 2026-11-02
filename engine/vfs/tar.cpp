// The tar reader: header by header, the octal (or base-256) size, the checksum summed with its
// own field as spaces, the long name of a GNU `L` record or a pax `path=` record carried to the
// next file, and the ustar writer the tests make archives with.
//
// ge:summary The tar reader's headers, checksums and long names, and the ustar writer
// ge:keywords tar-header octal base-256 checksum pax-path gnu-long-name ustar-writer
// ge:skill vfs
// ge:test tests/vfs_unpack_test.cpp
#include "vfs/tar.hpp"

#include <algorithm>
#include <cstring>

namespace ge::vfs {

namespace {

constexpr size_t kBlock = 512;

// A numeric field: octal digits (spaces and NULs around them), or base-256 when its first byte
// has the top bit set (GNU's form for sizes past 8 GiB).
uint64_t number(const uint8_t* f, size_t n) {
  if (n > 0 && (f[0] & 0x80) != 0) {
    uint64_t v = f[0] & 0x7F;
    for (size_t i = 1; i < n; i++) v = (v << 8) | f[i];
    return v;
  }
  uint64_t v = 0;
  for (size_t i = 0; i < n; i++) {
    const bool digit = f[i] >= '0' && f[i] <= '7';
    if (!digit && (v != 0 || (f[i] != ' ' && f[i] != 0))) break;  // past the digits
    if (digit) v = v * 8 + (f[i] - '0');
  }
  return v;
}

std::string text(const uint8_t* f, size_t n) {
  size_t len = 0;
  while (len < n && f[len] != 0) len++;
  return std::string(reinterpret_cast<const char*>(f), len);
}

bool checksum_ok(const uint8_t* h) {
  uint64_t sum = 0;
  for (size_t i = 0; i < kBlock; i++) sum += (i >= 148 && i < 156) ? ' ' : h[i];
  return sum == number(h + 148, 8);
}

bool zero_block(const uint8_t* h) {
  for (size_t i = 0; i < kBlock; i++)
    if (h[i] != 0) return false;
  return true;
}

// The `path` record of a pax header's "<len> key=value\n" records.
std::string pax_path(std::span<const uint8_t> rec) {
  size_t at = 0;
  while (at < rec.size()) {
    size_t len = 0, k = at;
    while (k < rec.size() && rec[k] >= '0' && rec[k] <= '9') len = len * 10 + (rec[k++] - '0');
    if (len == 0 || at + len > rec.size()) break;
    const std::string r(reinterpret_cast<const char*>(rec.data() + k + 1), at + len - k - 2);
    if (r.rfind("path=", 0) == 0) return r.substr(5);
    at += len;
  }
  return "";
}

void put_octal(uint8_t* f, size_t n, uint64_t v) {
  for (size_t i = n - 1; i-- > 0;) {
    f[i] = static_cast<uint8_t>('0' + (v & 7));
    v >>= 3;
  }
  f[n - 1] = 0;
}

}  // namespace

bool is_tar(std::span<const uint8_t> bytes) {
  return bytes.size() >= kBlock && !zero_block(bytes.data()) && checksum_ok(bytes.data());
}

bool read_tar(std::span<const uint8_t> bytes, std::vector<TarEntry>& out, std::string& error) {
  out.clear();
  std::string long_name;
  size_t at = 0;
  while (at + kBlock <= bytes.size()) {
    const uint8_t* h = bytes.data() + at;
    if (zero_block(h)) break;
    if (!checksum_ok(h)) {
      error = "tar: the header at byte " + std::to_string(at) +
              " fails its checksum; the archive is damaged: fetch it again";
      return false;
    }
    const uint64_t size = number(h + 124, 12);
    const size_t data = at + kBlock;
    if (size > bytes.size() - data) {
      error = "tar: the entry at byte " + std::to_string(at) + " ('" + text(h, 100) + "') holds " +
              std::to_string(size) +
              " bytes, past the archive's end; it is cut short: fetch it "
              "again";
      return false;
    }
    const uint8_t type = h[156];
    const std::span<const uint8_t> body = bytes.subspan(data, static_cast<size_t>(size));
    if (type == 'L') {
      long_name = text(body.data(), body.size());
    } else if (type == 'x') {
      long_name = pax_path(body);
    } else if (type == '0' || type == 0 || type == '7') {
      std::string name = long_name;
      if (name.empty()) {
        name = text(h, 100);
        if (std::memcmp(h + 257, "ustar", 5) == 0 && h[345] != 0)
          name = text(h + 345, 155).append("/").append(name);
      }
      out.push_back({name, data, static_cast<size_t>(size)});
      long_name.clear();
    } else {
      long_name.clear();  // a directory, a link, a device or a global pax header: skipped
    }
    at = data + (static_cast<size_t>(size) + kBlock - 1) / kBlock * kBlock;
  }
  return true;
}

std::vector<uint8_t> write_tar(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
  std::vector<uint8_t> o;
  for (const auto& [name, data] : files) {
    uint8_t h[kBlock] = {};
    std::copy_n(name.begin(), std::min<size_t>(name.size(), 100), h);  // NUL-padded, maybe full
    put_octal(h + 100, 8, 0644);
    put_octal(h + 108, 8, 0);
    put_octal(h + 116, 8, 0);
    put_octal(h + 124, 12, data.size());
    put_octal(h + 136, 12, 0);
    h[156] = '0';
    std::memcpy(h + 257, "ustar", 6);
    h[263] = '0';
    h[264] = '0';
    std::memset(h + 148, ' ', 8);
    uint64_t sum = 0;
    for (const uint8_t b : h) sum += b;
    put_octal(h + 148, 7, sum);
    h[155] = ' ';
    o.insert(o.end(), h, h + kBlock);
    o.insert(o.end(), data.begin(), data.end());
    o.resize((o.size() + kBlock - 1) / kBlock * kBlock, 0);
  }
  o.resize(o.size() + 2 * kBlock, 0);
  return o;
}

}  // namespace ge::vfs
