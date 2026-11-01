// The peeling loop behind unpack.hpp: each layer sniffed from its magic, unpacked by its reader,
// and an archive's files filtered by `pick` down to the one taken; the gzip header's name read
// here, since core's inflater skips it.
//
// ge:summary The unpack loop: layer by layer, an archive's one file chosen, the refusals worded
// ge:keywords peel layer sniff gzip-name archive-choice refuse several-files
// ge:skill vfs
// ge:test tests/vfs_unpack_test.cpp
#include "vfs/unpack.hpp"

#include <algorithm>
#include <cstring>

#include "core/inflate.hpp"
#include "vfs/lzma.hpp"
#include "vfs/tar.hpp"
#include "vfs/zip.hpp"
#include "vfs/zstd.hpp"

namespace ge::vfs {

namespace {

constexpr int kMaxLayers = 4;

bool opens_with(std::span<const uint8_t> b, const uint8_t* magic, size_t n) {
  return b.size() >= n && std::memcmp(b.data(), magic, n) == 0;
}

// The name a gzip header carries (FNAME), or "".
std::string gzip_name(std::span<const uint8_t> b) {
  if (b.size() < 10 || (b[3] & 8) == 0) return "";
  size_t at = 10;
  if ((b[3] & 4) != 0) {
    if (at + 2 > b.size()) return "";
    at += 2 + (b[at] | (size_t(b[at + 1]) << 8));
  }
  std::string name;
  while (at < b.size() && b[at] != 0) name += static_cast<char>(b[at++]);
  return name;
}

struct Candidate {
  std::string name;
  size_t index = 0;
};

// The one file of an archive's list, or false with the reason.
bool choose(const std::vector<Candidate>& files, std::string_view kind,
            const std::function<bool(std::string_view)>& pick, size_t& index, std::string& error) {
  std::vector<const Candidate*> taken;
  for (const Candidate& c : files)
    if (!pick || pick(c.name)) taken.push_back(&c);
  if (taken.size() == 1) {
    index = taken[0]->index;
    return true;
  }
  std::string names;
  for (size_t i = 0; i < files.size() && i < 8; i++) names += (i ? ", " : "") + files[i].name;
  if (files.size() > 8) names += ", ...";
  if (taken.empty())
    error = std::string("the ") + std::string(kind) + " archive holds " +
            (files.empty() ? std::string("no file")
                           : std::to_string(files.size()) + " files, none of a kind this reads (" +
                                 names + ")") +
            "; next: name an archive that holds one, or the file itself";
  else
    error = std::string("the ") + std::string(kind) + " archive holds " +
            std::to_string(taken.size()) + " files this could read (" + names +
            "), and only an archive of one is opened; next: unpack it and name the one to open";
  return false;
}

}  // namespace

Wrapping wrapping_of(std::span<const uint8_t> b) {
  static constexpr uint8_t kGzip[2] = {0x1F, 0x8B};
  static constexpr uint8_t kZip[4] = {'P', 'K', 3, 4};
  static constexpr uint8_t kZipEmpty[4] = {'P', 'K', 5, 6};
  if (opens_with(b, kGzip, 2)) return Wrapping::gzip;
  if (is_zstd(b)) return Wrapping::zstd;
  if (opens_with(b, kXzMagic, 6)) return Wrapping::xz;
  if (opens_with(b, kSevenMagic, 6)) return Wrapping::seven;
  if (opens_with(b, kZip, 4) || opens_with(b, kZipEmpty, 4)) return Wrapping::zip;
  if (is_tar(b)) return Wrapping::tar;
  return Wrapping::none;
}

std::string_view wrapping_name(Wrapping w) {
  switch (w) {
    case Wrapping::gzip:
      return "gzip";
    case Wrapping::zstd:
      return "zstd";
    case Wrapping::xz:
      return "xz";
    case Wrapping::tar:
      return "tar";
    case Wrapping::zip:
      return "zip";
    case Wrapping::seven:
      return "7z";
    case Wrapping::none:
      break;
  }
  return "none";
}

bool unpack_one(std::vector<uint8_t> bytes, Unpacked& out, std::string& error,
                const std::function<bool(std::string_view)>& pick) {
  out = Unpacked{};
  for (int depth = 0;; depth++) {
    const Wrapping w = wrapping_of(bytes);
    if (w == Wrapping::none) break;
    if (depth == kMaxLayers) {
      error = "the file is packed more than " + std::to_string(kMaxLayers) +
              " layers deep (the next is " + std::string(wrapping_name(w)) +
              "), which no recording is; next: unpack it by hand";
      return false;
    }
    out.layers.push_back(w);
    std::vector<uint8_t> inner;
    std::string why;
    switch (w) {
      case Wrapping::gzip: {
        const std::string name = gzip_name(bytes);
        auto u = core::inflate_gzip(bytes);
        if (!u) {
          error =
              "the file is a gzip file that does not unpack (torn, or its checksum is off); "
              "next: download it again, or unpack it and name the file inside";
          return false;
        }
        inner = std::move(*u);
        if (!name.empty()) out.name = name;
        break;
      }
      case Wrapping::zstd: {
        bool cut = false;
        if (!zstd_unpack(bytes, inner, cut, why, kUnpackLimit)) {
          error = why;
          return false;
        }
        out.truncated = out.truncated || cut;
        break;
      }
      case Wrapping::xz:
        if (!xz_unpack(bytes, inner, why, kUnpackLimit)) {
          error = why;
          return false;
        }
        break;
      case Wrapping::tar: {
        std::vector<TarEntry> entries;
        if (!read_tar(bytes, entries, error)) return false;
        std::vector<Candidate> files;
        for (size_t i = 0; i < entries.size(); i++) files.push_back({entries[i].name, i});
        size_t k = 0;
        if (!choose(files, "tar", pick, k, error)) return false;
        const TarEntry& e = entries[k];
        inner.assign(bytes.begin() + static_cast<std::ptrdiff_t>(e.data),
                     bytes.begin() + static_cast<std::ptrdiff_t>(e.data + e.size));
        out.name = e.name;
        break;
      }
      case Wrapping::zip: {
        Zip zip;
        if (!read_zip(bytes, &zip, &error)) return false;
        std::vector<Candidate> files;
        for (size_t i = 0; i < zip.entries.size(); i++) {
          const std::string& n = zip.entries[i].name;
          if (!n.empty() && n.back() != '/' && n.back() != '\\') files.push_back({n, i});
        }
        size_t k = 0;
        if (!choose(files, "zip", pick, k, error)) return false;
        if (!zip_unpack(bytes, zip.entries[k], &inner, &error)) return false;
        out.name = zip.entries[k].name;
        break;
      }
      case Wrapping::seven: {
        std::vector<SevenEntry> entries;
        if (!read_7z(bytes, entries, error)) return false;
        std::vector<Candidate> files;
        for (size_t i = 0; i < entries.size(); i++)
          if (!entries[i].dir) files.push_back({entries[i].name, i});
        size_t k = 0;
        if (!choose(files, "7z", pick, k, error)) return false;
        if (!seven_unpack(bytes, k, inner, error)) return false;
        out.name = entries[k].name;
        break;
      }
      case Wrapping::none:
        break;
    }
    bytes = std::move(inner);
  }
  out.bytes = std::move(bytes);
  return true;
}

std::vector<uint8_t> write_gzip(std::span<const uint8_t> bytes, const std::string& name) {
  std::vector<uint8_t> o = {0x1F, 0x8B, 8,  static_cast<uint8_t>(name.empty() ? 0 : 8), 0, 0, 0,
                            0,    0,    255};
  if (!name.empty()) {
    o.insert(o.end(), name.begin(), name.end());
    o.push_back(0);
  }
  size_t at = 0;
  do {  // stored blocks of at most 65535 bytes; the last one's BFINAL bit set
    const size_t n = std::min<size_t>(65535, bytes.size() - at);
    o.push_back(at + n == bytes.size() ? 1 : 0);
    o.push_back(static_cast<uint8_t>(n));
    o.push_back(static_cast<uint8_t>(n >> 8));
    o.push_back(static_cast<uint8_t>(~n));
    o.push_back(static_cast<uint8_t>(~n >> 8));
    o.insert(o.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at),
             bytes.begin() + static_cast<std::ptrdiff_t>(at + n));
    at += n;
  } while (at < bytes.size());
  const uint32_t crc = core::crc32(bytes.data(), bytes.size());
  const auto size = static_cast<uint32_t>(bytes.size());
  for (int i = 0; i < 4; i++) o.push_back(static_cast<uint8_t>(crc >> (8 * i)));
  for (int i = 0; i < 4; i++) o.push_back(static_cast<uint8_t>(size >> (8 * i)));
  return o;
}

}  // namespace ge::vfs
