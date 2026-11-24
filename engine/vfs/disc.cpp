// The disc image reader and its fixture writers (disc.hpp).
//
// ge:summary Reads ISO 9660 and cue sheets with their bin files; writes fixture images
// ge:keywords disc iso iso9660 joliet cue bin raw-sector directory-record volume-descriptor
// ge:skill vfs
// ge:test tests/vfs_disc_test.cpp
#include "vfs/disc.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>

namespace ge::vfs {

namespace fs = std::filesystem;

struct Disc::Impl {
  std::vector<std::unique_ptr<std::ifstream>> in;  // one per image file
  int data = -1;                                   // the data track the files are on
  bool read(int file, uint64_t at, size_t n, uint8_t* out) const {
    std::ifstream& f = *in[size_t(file)];
    f.clear();
    f.seekg(std::streamoff(at));
    f.read(reinterpret_cast<char*>(out), std::streamsize(n));
    return size_t(f.gcount()) == n;
  }
};

Disc::Disc() : impl_(std::make_unique<Impl>()) {}
Disc::~Disc() = default;
Disc::Disc(Disc&&) noexcept = default;
Disc& Disc::operator=(Disc&&) noexcept = default;

namespace {

std::string lower(std::string s) {
  for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

uint32_t le32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

const uint8_t kSync[12] = {0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0};

// One logical sector of the data track.
bool data_sector(const Disc& d, const Disc::Impl& im, uint32_t lba, uint8_t* out) {
  const DiscTrack& t = d.tracks[size_t(im.data)];
  if (lba >= t.sectors) return false;
  return im.read(t.file, t.start + uint64_t(lba) * t.sector + t.user, 2048, out);
}

void put_utf8(std::string& s, uint32_t c) {
  if (c < 0x80) {
    s += char(c);
  } else if (c < 0x800) {
    s += char(0xc0 | (c >> 6));
    s += char(0x80 | (c & 0x3f));
  } else {
    s += char(0xe0 | (c >> 12));
    s += char(0x80 | ((c >> 6) & 0x3f));
    s += char(0x80 | (c & 0x3f));
  }
}

std::string record_name(const uint8_t* p, size_t n, bool joliet) {
  std::string s;
  if (joliet) {
    for (size_t i = 0; i + 1 < n; i += 2) put_utf8(s, uint32_t(p[i]) << 8 | p[i + 1]);
  } else {
    s.assign(reinterpret_cast<const char*>(p), n);
  }
  if (const size_t semi = s.find(';'); semi != std::string::npos) s.resize(semi);
  if (!joliet && !s.empty() && s.back() == '.') s.pop_back();
  return s;
}

bool walk(Disc& d, const Disc::Impl& im, uint32_t extent, uint32_t size, const std::string& at,
          int depth, std::set<uint32_t>& seen, std::string* error) {
  if (depth > 32 || !seen.insert(extent).second) return true;  // a loop or a tree too deep
  std::vector<uint8_t> sec(2048);
  for (uint32_t off = 0; off < size; off += 2048) {
    if (!data_sector(d, im, extent + off / 2048, sec.data())) {
      *error = "the directory " + (at.empty() ? std::string("/") : at) +
               " lies outside the image's data track; the image is cut short or damaged";
      return false;
    }
    for (size_t p = 0; p < 2048;) {
      const uint8_t len = sec[p];
      if (len == 0) break;  // the rest of the sector is padding
      if (len < 34 || p + len > 2048) break;
      const uint8_t* r = &sec[p];
      const uint32_t ext = le32(r + 2), sz = le32(r + 10);
      const uint8_t flags = r[25], nlen = r[32];
      p += len;
      if (33u + nlen > len) continue;
      if (nlen == 1 && (r[33] == 0 || r[33] == 1)) continue;  // "." and ".."
      const std::string name = record_name(r + 33, nlen, d.joliet);
      const std::string path = at.empty() ? name : std::string(at).append("/").append(name);
      if (flags & 2) {
        if (!walk(d, im, ext, sz, path, depth + 1, seen, error)) return false;
      } else {
        d.files.push_back({path, ext, sz});
      }
    }
  }
  return true;
}

bool read_filesystem(Disc& d, Disc::Impl& im, std::string* error) {
  for (size_t i = 0; i < d.tracks.size(); i++) {
    if (d.tracks[i].kind == TrackKind::data) {
      im.data = int(i);
      break;
    }
  }
  if (im.data < 0) return true;  // an audio-only disc: no files
  std::vector<uint8_t> sec(2048);
  const uint8_t* root = nullptr;
  std::vector<uint8_t> pvd, svd;
  for (uint32_t lba = 16; lba < 64; lba++) {
    if (!data_sector(d, im, lba, sec.data()) ||
        std::string_view(reinterpret_cast<const char*>(&sec[1]), 5) != "CD001")
      break;
    if (sec[0] == 255) break;
    if (sec[0] == 1 && pvd.empty()) pvd = sec;
    if (sec[0] == 2 && sec[88] == '%' && sec[89] == '/' &&
        (sec[90] == '@' || sec[90] == 'C' || sec[90] == 'E'))
      svd = sec;
  }
  if (pvd.empty()) {
    *error =
        "the image's data track holds no ISO 9660 file system (no volume descriptor at "
        "sector 16); check that it is a game's disc, or give the installed folder instead";
    return false;
  }
  d.joliet = !svd.empty();
  const std::vector<uint8_t>& vd = d.joliet ? svd : pvd;
  root = &vd[156];
  d.volume = record_name(&vd[40], 32, d.joliet);
  while (!d.volume.empty() && (d.volume.back() == ' ' || d.volume.back() == '\0'))
    d.volume.pop_back();
  std::set<uint32_t> seen;
  return walk(d, im, le32(root + 2), le32(root + 10), "", 0, seen, error);
}

// "mm:ss:ff" in sectors (75 a second); -1 when it does not read.
int64_t msf(const std::string& s) {
  int m = 0, sec = 0, f = 0;
  if (std::sscanf(s.c_str(), "%d:%d:%d", &m, &sec, &f) != 3) return -1;
  return (int64_t(m) * 60 + sec) * 75 + f;
}

std::vector<std::string> words(const std::string& line) {
  std::vector<std::string> out;
  for (size_t i = 0; i < line.size();) {
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) i++;
    if (i >= line.size()) break;
    if (line[i] == '"') {
      const size_t end = line.find('"', i + 1);
      out.push_back(line.substr(i + 1, (end == std::string::npos ? line.size() : end) - i - 1));
      i = end == std::string::npos ? line.size() : end + 1;
    } else {
      size_t end = i;
      while (end < line.size() && !std::isspace(static_cast<unsigned char>(line[end]))) end++;
      out.push_back(line.substr(i, end - i));
      i = end;
    }
  }
  return out;
}

// The file a sheet names, beside the sheet, its name matched in any case.
std::string beside(const fs::path& sheet, const std::string& name) {
  fs::path p = sheet.parent_path() / name;
  std::error_code ec;
  if (fs::exists(p, ec)) return p.string();
  const std::string want = lower(fs::path(name).filename().string());
  for (const auto& e : fs::directory_iterator(sheet.parent_path(), ec))
    if (lower(e.path().filename().string()) == want) return e.path().string();
  // A sheet and its image renamed together (a download names both after the release): the file
  // with the sheet's own name and the named file's extension.
  const std::string renamed = lower(sheet.stem().string() + fs::path(name).extension().string());
  for (const auto& e : fs::directory_iterator(sheet.parent_path(), ec))
    if (lower(e.path().filename().string()) == renamed) return e.path().string();
  return "";
}

// A WAVE file's sample data: its offset and size.
bool wave_data(std::ifstream& f, uint64_t* at, uint64_t* size) {
  uint8_t h[12];
  f.seekg(0);
  if (!f.read(reinterpret_cast<char*>(h), 12) || std::string_view((char*)h, 4) != "RIFF")
    return false;
  uint64_t pos = 12;
  for (int i = 0; i < 64; i++) {
    uint8_t c[8];
    f.seekg(std::streamoff(pos));
    if (!f.read(reinterpret_cast<char*>(c), 8)) return false;
    const uint32_t n = le32(c + 4);
    if (std::string_view((char*)c, 4) == "data") {
      *at = pos + 8;
      *size = n;
      return true;
    }
    pos += 8 + n + (n & 1);
  }
  return false;
}

bool open_sheet(const fs::path& path, Disc* d, Disc::Impl* im, std::string* error) {
  std::ifstream in(path);
  if (!in) {
    *error = "could not read the cue sheet " + path.string() + "; check that it is there";
    return false;
  }
  struct T {
    DiscTrack t;
    int64_t index1 = -1;
    bool wave = false;
  };
  std::vector<T> ts;
  std::vector<bool> wave_file;
  std::string line;
  int at_line = 0;
  const auto bad = [&](const std::string& why) {
    *error = "the cue sheet " + path.string() + " line " + std::to_string(at_line) + ": " + why +
             "; check the sheet or give the .bin with no sheet";
    return false;
  };
  while (std::getline(in, line)) {
    at_line++;
    const std::vector<std::string> w = words(line);
    if (w.empty()) continue;
    const std::string key = lower(w[0]);
    if (key == "file") {
      if (w.size() < 2) return bad("FILE names no file");
      const std::string found = beside(path, w[1]);
      if (found.empty()) return bad("the file '" + w[1] + "' is not beside the sheet");
      d->image_files.push_back(found);
      wave_file.push_back(w.size() > 2 && lower(w[2]) == "wave");
    } else if (key == "track") {
      if (d->image_files.empty()) return bad("TRACK before any FILE");
      if (w.size() < 3) return bad("TRACK without a number and a mode");
      T t;
      t.t.number = std::atoi(w[1].c_str());
      t.t.file = int(d->image_files.size()) - 1;
      t.wave = wave_file.back();
      const std::string mode = lower(w[2]);
      if (mode == "audio") {
        t.t.kind = TrackKind::audio;
        t.t.sector = 2352;
      } else if (mode == "mode1/2048" || mode == "mode2/2048") {
        t.t.sector = 2048;
      } else if (mode == "mode1/2352") {
        t.t.sector = 2352;
        t.t.user = 16;
      } else if (mode == "mode2/2352") {
        t.t.sector = 2352;
        t.t.user = 24;
      } else {
        return bad("the track mode '" + w[2] +
                   "' is not read (AUDIO, MODE1/2048, MODE1/2352, "
                   "MODE2/2352 are)");
      }
      ts.push_back(t);
    } else if (key == "index" && w.size() >= 3 && !ts.empty() && std::atoi(w[1].c_str()) == 1) {
      ts.back().index1 = msf(w[2]);
      if (ts.back().index1 < 0) return bad("the INDEX time '" + w[2] + "' does not read");
    }
  }
  if (ts.empty()) return bad("no TRACK");
  for (const std::string& f : d->image_files) {
    im->in.push_back(std::make_unique<std::ifstream>(f, std::ios::binary));
    if (!*im->in.back()) {
      *error = "could not open " + f + ", which the sheet names; check its permissions";
      return false;
    }
  }
  // Byte offsets and lengths, file by file.
  for (size_t i = 0; i < ts.size(); i++) {
    T& t = ts[i];
    if (t.index1 < 0) return bad("track " + std::to_string(t.t.number) + " has no INDEX 01");
    std::ifstream& f = *im->in[size_t(t.t.file)];
    uint64_t base = 0, size = 0;
    if (t.wave) {
      if (!wave_data(f, &base, &size))
        return bad("the WAVE file of track " + std::to_string(t.t.number) + " has no data");
    } else {
      f.clear();
      f.seekg(0, std::ios::end);
      size = uint64_t(f.tellg());
    }
    const bool first_in_file = i == 0 || ts[i - 1].t.file != t.t.file;
    if (first_in_file) {
      t.t.start = base + uint64_t(t.index1) * t.t.sector;
    } else {
      const T& p = ts[i - 1];
      t.t.start = p.t.start + uint64_t(t.index1 - p.index1) * p.t.sector;
    }
    const bool last_in_file = i + 1 == ts.size() || ts[i + 1].t.file != t.t.file;
    const uint64_t end =
        last_in_file ? base + size : t.t.start + uint64_t(ts[i + 1].index1 - t.index1) * t.t.sector;
    if (end < t.t.start)
      return bad("track " + std::to_string(t.t.number) + " ends before it starts");
    t.t.sectors = uint32_t((end - t.t.start) / t.t.sector);
    d->tracks.push_back(t.t);
  }
  return true;
}

// A disc copier's `.mds` sheet beside its `.mdf` (the format as measured on a copy of a game's
// disc, little-endian): the signature "MEDIA DESCRIPTOR" at 0; at 80 the session block's offset.
// The session block: at 10 its count of track blocks (a byte), at 20 their offset. A track block
// is 80 bytes: its mode at 0 (0xA9 audio, any other a data track), its point at 4 (1..99 a track;
// 0xA0 and up the lead-in's entries, skipped), at 12 the offset of its extra block, at 16 its
// sector's size in the image (16 bits), at 40 its byte offset in the image (64 bits), at 52 the
// offset of the footer naming the image. The extra block: the pregap in sectors at 0 (held in the
// image for every track but the image's first, so a track's own sectors start that far after its
// offset), its length in sectors at 4. The footer: at 0 the name's offset, at 4 whether it is
// UTF-16; "*.mdf" names the sheet's own name with `.mdf`.
bool open_mds(const fs::path& path, Disc* d, Disc::Impl* im, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  const auto bad = [&](const std::string& why) {
    *error = "the sheet " + path.string() + ": " + why +
             "; check the sheet, or move it away and give the .mdf alone (its data track is "
             "read without it)";
    return false;
  };
  const auto at32 = [&](size_t o) { return o + 4 <= b.size() ? le32(b.data() + o) : 0u; };
  const auto at16 = [&](size_t o) {
    return o + 2 <= b.size() ? uint32_t(b[o]) | uint32_t(b[o + 1]) << 8 : 0u;
  };
  static const char kSig[] = "MEDIA DESCRIPTOR";
  if (b.size() < 96 || !std::equal(kSig, kSig + 16, b.begin())) return bad("it is not a sheet");
  const size_t session = at32(80);
  const uint32_t blocks = session + 10 < b.size() ? b[session + 10] : 0u;
  const size_t first = at32(session + 20);
  if (session == 0 || first == 0 || first + size_t(blocks) * 80 > b.size())
    return bad("its track list runs past its end");
  std::string image;
  for (uint32_t k = 0; k < blocks; k++) {
    const uint8_t* t = b.data() + first + size_t(k) * 80;
    const int point = t[4];
    if (point < 1 || point > 99) continue;
    const size_t extra = le32(t + 12), footer = le32(t + 52);
    DiscTrack dt;
    dt.number = point;
    dt.kind = t[0] == 0xa9 ? TrackKind::audio : TrackKind::data;
    dt.sector = at16(size_t(t - b.data()) + 16);
    if (dt.sector != 2048 && dt.sector != 2352 && dt.sector != 2448)
      return bad("track " + std::to_string(point) + "'s sectors are " + std::to_string(dt.sector) +
                 " bytes (2048, 2352 and 2448 are read)");
    if (extra == 0 || extra + 8 > b.size())
      return bad("track " + std::to_string(point) + " has no length");
    const uint64_t offset = uint64_t(le32(t + 40)) | uint64_t(le32(t + 44)) << 32;
    const uint32_t pregap = at32(extra);
    dt.sectors = at32(extra + 4);
    dt.start = offset + (offset == 0 ? 0 : uint64_t(pregap) * dt.sector);
    if (image.empty() && footer != 0 && footer + 8 <= b.size()) {
      const size_t name = at32(footer);
      const bool wide = at32(footer + 4) != 0;
      for (size_t i = name; i < b.size(); i += wide ? 2 : 1) {
        const uint32_t c = wide ? at16(i) : b[i];
        if (c == 0) break;
        put_utf8(image, c);
      }
    }
    d->tracks.push_back(dt);
  }
  if (d->tracks.empty()) return bad("it lists no track");
  if (image.empty() || image == "*.mdf") image = path.stem().string() + ".mdf";
  const std::string found = beside(path, image);
  if (found.empty()) return bad("the image '" + image + "' is not beside it");
  d->image_files.push_back(found);
  im->in.push_back(std::make_unique<std::ifstream>(found, std::ios::binary));
  if (!*im->in.back()) {
    *error = "could not open " + found + ", which the sheet " + path.string() +
             " names; check its permissions";
    return false;
  }
  // A data track's user data, placed by its first sector's mode byte.
  for (DiscTrack& t : d->tracks) {
    if (t.kind != TrackKind::data || t.sector == 2048) continue;
    uint8_t head[16] = {};
    if (!im->read(0, t.start, 16, head) || !std::equal(kSync, kSync + 12, head))
      return bad("data track " + std::to_string(t.number) + " has no sync where it starts");
    t.user = head[15] == 2 ? 24 : 16;
  }
  return true;
}

// The `.mds` sheet beside an `.mdf`, its name matched in any case; empty when there is none.
fs::path sheet_beside(const fs::path& mdf) {
  std::error_code ec;
  const std::string want = lower(mdf.stem().string()) + ".mds";
  for (const auto& e :
       fs::directory_iterator(mdf.parent_path().empty() ? "." : mdf.parent_path(), ec))
    if (lower(e.path().filename().string()) == want) return e.path();
  return {};
}

bool open_plain(const fs::path& path, Disc* d, Disc::Impl* im, std::string* error) {
  d->image_files.push_back(path.string());
  im->in.push_back(std::make_unique<std::ifstream>(path, std::ios::binary));
  std::ifstream& f = *im->in.back();
  if (!f) {
    *error = "could not open the image " + path.string() + "; check that it is there";
    return false;
  }
  f.seekg(0, std::ios::end);
  const uint64_t size = uint64_t(f.tellg());
  uint8_t head[16] = {};
  f.seekg(0);
  f.read(reinterpret_cast<char*>(head), 16);
  DiscTrack t;
  t.number = 1;
  if (std::equal(kSync, kSync + 12, head)) {
    // Raw sectors: 2352 bytes, or 2448 when each carries its 96 bytes of subchannel (an `.mdf`
    // as a disc copier writes it), told by where the second sector's sync falls.
    uint8_t next[12] = {};
    f.clear();
    f.seekg(2448);
    f.read(reinterpret_cast<char*>(next), 12);
    t.sector = f && size >= uint64_t{2} * 2448 && std::equal(kSync, kSync + 12, next) ? 2448 : 2352;
    t.user = head[15] == 2 ? 24 : 16;
  }
  t.sectors = uint32_t(size / t.sector);
  d->tracks.push_back(t);
  return true;
}

}  // namespace

const DiscFile* Disc::find(const std::string& path) const {
  const std::string want = lower(path);
  for (const DiscFile& f : files)
    if (lower(f.path) == want) return &f;
  return nullptr;
}

bool Disc::read_file(const DiscFile& f, std::vector<uint8_t>* out, std::string* error) const {
  out->resize(f.size);
  std::vector<uint8_t> sec(2048);
  for (uint32_t off = 0; off < f.size; off += 2048) {
    if (!data_sector(*this, *impl_, f.extent + off / 2048, sec.data())) {
      *error = "the disc's file " + f.path +
               " runs outside the image's data track; the image is "
               "cut short or damaged, make it again from the disc";
      return false;
    }
    std::copy_n(sec.begin(), std::min<uint32_t>(2048, f.size - off), out->begin() + off);
  }
  return true;
}

bool Disc::read_audio(const DiscTrack& t, std::vector<int16_t>* out, std::string* error) const {
  if (t.kind != TrackKind::audio) {
    *error = "track " + std::to_string(t.number) + " is a data track, not audio";
    return false;
  }
  // 2352 bytes of sound a sector; any more a sector holds (its subchannel) is passed over.
  const size_t bytes = size_t(t.sectors) * 2352;
  std::vector<uint8_t> raw(bytes);
  bool whole = true;
  if (t.sector == 2352) {
    whole = impl_->read(t.file, t.start, bytes, raw.data());
  } else {
    for (uint32_t s = 0; s < t.sectors && whole; s++)
      whole = impl_->read(t.file, t.start + uint64_t(s) * t.sector, 2352,
                          raw.data() + size_t(s) * 2352);
  }
  if (!whole) {
    *error = "audio track " + std::to_string(t.number) + " runs past the end of " +
             image_files[size_t(t.file)] + "; the image is cut short, make it again from the disc";
    return false;
  }
  out->resize(bytes / 2);
  for (size_t i = 0; i < out->size(); i++)
    (*out)[i] = int16_t(uint16_t(raw[2 * i]) | uint16_t(raw[2 * i + 1]) << 8);
  return true;
}

bool is_disc_image(const std::string& path) {
  const std::string ext = lower(fs::path(path).extension().string());
  return ext == ".iso" || ext == ".cue" || ext == ".bin" || ext == ".img" || ext == ".mdf";
}

bool open_disc(const std::string& path, Disc* out, std::string* error) {
  *out = Disc();
  const fs::path p(path);
  const std::string ext = lower(p.extension().string());
  const fs::path mds = ext == ".mds" ? p : ext == ".mdf" ? sheet_beside(p) : fs::path();
  const bool ok = ext == ".cue"  ? open_sheet(p, out, out->impl_.get(), error)
                  : !mds.empty() ? open_mds(mds, out, out->impl_.get(), error)
                                 : open_plain(p, out, out->impl_.get(), error);
  return ok && read_filesystem(*out, *out->impl_, error);
}

// ---- Fixtures ----

namespace {

void both32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; i++) {
    p[i] = uint8_t(v >> (8 * i));
    p[7 - i] = uint8_t(v >> (8 * i));
  }
}

struct Node {
  std::map<std::string, Node> dirs;
  std::vector<std::pair<std::string, size_t>> files;  // name, index into the files given
  uint32_t lba = 0, bytes = 0;
};

std::vector<uint8_t> encode_name(const std::string& name, bool joliet, bool file) {
  std::vector<uint8_t> out;
  if (joliet) {
    for (char c : name) {
      out.push_back(0);
      out.push_back(uint8_t(c));
    }
  } else {
    for (char c : name) out.push_back(uint8_t(std::toupper(static_cast<unsigned char>(c))));
    if (file) {
      out.push_back(';');
      out.push_back('1');
    }
  }
  return out;
}

size_t record_len(size_t nlen) { return 33 + nlen + ((nlen & 1) ? 0 : 1); }

void record(std::vector<uint8_t>& sec, size_t& at, uint32_t lba, uint32_t size, bool dir,
            const std::vector<uint8_t>& name) {
  const size_t len = record_len(name.size());
  uint8_t* r = &sec[at];
  r[0] = uint8_t(len);
  both32(r + 2, lba);
  both32(r + 10, size);
  r[25] = dir ? 2 : 0;
  r[28] = 1;
  r[31] = 1;
  r[32] = uint8_t(name.size());
  std::copy(name.begin(), name.end(), r + 33);
  at += len;
}

// Sizes, then sectors of `n`'s directory and below.
void size_tree(Node& n, bool joliet) {
  uint32_t bytes = uint32_t(2 * record_len(1)), used = bytes;
  const auto add = [&](size_t len) {
    if (used + len > 2048) {
      bytes += 2048 - used;
      used = 0;
    }
    bytes += uint32_t(len);
    used += uint32_t(len);
  };
  for (auto& [name, d] : n.dirs) {
    add(record_len(encode_name(name, joliet, false).size()));
    size_tree(d, joliet);
  }
  for (auto& [name, i] : n.files) add(record_len(encode_name(name, joliet, true).size()));
  n.bytes = (bytes + 2047) / 2048 * 2048;
}

void place(Node& n, uint32_t& lba) {
  n.lba = lba;
  lba += n.bytes / 2048;
  for (auto& [name, d] : n.dirs) place(d, lba);
}

void emit(const Node& n, const Node& parent, bool joliet, const std::vector<uint32_t>& file_lba,
          const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files,
          std::vector<uint8_t>& img) {
  std::vector<uint8_t> dir(n.bytes);
  size_t at = 0, sector_end = 2048;
  const auto put = [&](uint32_t lba, uint32_t size, bool is_dir, const std::vector<uint8_t>& nm) {
    if (at + record_len(nm.size()) > sector_end) {
      at = sector_end;
      sector_end += 2048;
    }
    record(dir, at, lba, size, is_dir, nm);
  };
  put(n.lba, n.bytes, true, {0});
  put(parent.lba, parent.bytes, true, {1});
  for (const auto& [name, d] : n.dirs) put(d.lba, d.bytes, true, encode_name(name, joliet, false));
  for (const auto& [name, i] : n.files)
    put(file_lba[i], uint32_t(files[i].second.size()), false, encode_name(name, joliet, true));
  std::copy(dir.begin(), dir.end(), img.begin() + size_t(n.lba) * 2048);
  for (const auto& [name, d] : n.dirs) emit(d, n, joliet, file_lba, files, img);
}

}  // namespace

std::vector<uint8_t> write_iso(
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files, bool joliet,
    const std::string& volume) {
  Node root;
  for (size_t i = 0; i < files.size(); i++) {
    Node* n = &root;
    std::string rest = files[i].first;
    for (size_t slash; (slash = rest.find('/')) != std::string::npos;) {
      n = &n->dirs[rest.substr(0, slash)];
      rest = rest.substr(slash + 1);
    }
    n->files.emplace_back(rest, i);
  }
  Node jroot = root;
  size_tree(root, false);
  if (joliet) size_tree(jroot, true);
  uint32_t lba = joliet ? 19 : 18;  // the descriptors, then the trees
  place(root, lba);
  if (joliet) place(jroot, lba);
  std::vector<uint32_t> file_lba(files.size());
  for (size_t i = 0; i < files.size(); i++) {
    file_lba[i] = lba;
    lba += uint32_t((files[i].second.size() + 2047) / 2048);
  }
  std::vector<uint8_t> img(size_t(lba) * 2048);
  const auto descriptor = [&](uint32_t at, uint8_t type, const Node& r, bool j) {
    uint8_t* v = &img[size_t(at) * 2048];
    v[0] = type;
    std::copy_n("CD001", 5, v + 1);
    v[6] = 1;
    for (int i = 0; i < 32; i++) v[40 + i] = j && i % 2 == 0 ? 0 : ' ';  // UCS-2 spaces in Joliet
    const std::vector<uint8_t> vn = encode_name(volume, j, false);
    std::copy_n(vn.begin(), std::min<size_t>(vn.size(), 32), v + 40);
    both32(v + 80, lba);
    if (j) {
      v[88] = '%';
      v[89] = '/';
      v[90] = 'E';
    }
    size_t a = 0;
    std::vector<uint8_t> rec(64);
    record(rec, a, r.lba, r.bytes, true, {0});
    std::copy_n(rec.begin(), 34, v + 156);
    v[881] = 1;
  };
  descriptor(16, 1, root, false);
  if (joliet) descriptor(17, 2, jroot, true);
  uint8_t* term = &img[size_t(joliet ? 18 : 17) * 2048];
  term[0] = 255;
  std::copy_n("CD001", 5, term + 1);
  term[6] = 1;
  emit(root, root, false, file_lba, files, img);
  if (joliet) emit(jroot, jroot, true, file_lba, files, img);
  for (size_t i = 0; i < files.size(); i++)
    std::copy(files[i].second.begin(), files[i].second.end(),
              img.begin() + size_t(file_lba[i]) * 2048);
  return img;
}

std::vector<uint8_t> raw_sectors(const std::vector<uint8_t>& iso) {
  const size_t n = iso.size() / 2048;
  std::vector<uint8_t> out(n * 2352);
  const auto bcd = [](uint32_t v) { return uint8_t((v / 10) << 4 | (v % 10)); };
  for (size_t s = 0; s < n; s++) {
    uint8_t* o = &out[s * 2352];
    std::copy_n(kSync, 12, o);
    const uint32_t a = uint32_t(s) + 150;
    o[12] = bcd(a / 75 / 60);
    o[13] = bcd(a / 75 % 60);
    o[14] = bcd(a % 75);
    o[15] = 1;
    std::copy_n(iso.begin() + s * 2048, 2048, o + 16);
  }
  return out;
}

std::vector<uint8_t> write_mds(const std::vector<DiscTrack>& tracks, const std::string& image) {
  // The header, one session, the lead-in's three entries, the tracks, their extra blocks, the
  // footer and the name, laid out as a disc copier lays them.
  const size_t session = 96, first = 120, count = 3 + tracks.size();
  const size_t extras = first + count * 80, footer = extras + tracks.size() * 8;
  const size_t name = footer + 16;
  std::vector<uint8_t> b(name + (image.size() + 1) * 2, 0);
  const auto put32 = [&](size_t o, uint32_t v) {
    for (int i = 0; i < 4; i++) b[o + size_t(i)] = uint8_t(v >> (8 * i));
  };
  const char* sig = "MEDIA DESCRIPTOR";
  std::copy(sig, sig + 16, b.begin());
  b[16] = 1;
  b[17] = 5;
  put32(80, uint32_t(session));
  b[session + 10] = uint8_t(count);
  b[session + 12] = 1;
  b[session + 14] = uint8_t(tracks.empty() ? 0 : tracks.back().number);
  put32(session + 20, uint32_t(first));
  for (size_t k = 0; k < 3; k++) b[first + k * 80 + 4] = uint8_t(0xa0 + k);
  for (size_t k = 0; k < tracks.size(); k++) {
    const DiscTrack& t = tracks[k];
    const size_t o = first + (3 + k) * 80;
    const uint32_t pregap = 150;
    const uint64_t offset = k == 0 ? t.start : t.start - uint64_t(pregap) * t.sector;
    b[o] = t.kind == TrackKind::audio ? 0xa9 : 0xaa;
    b[o + 1] = 8;
    b[o + 4] = uint8_t(t.number);
    put32(o + 12, uint32_t(extras + k * 8));
    b[o + 16] = uint8_t(t.sector);
    b[o + 17] = uint8_t(t.sector >> 8);
    put32(o + 40, uint32_t(offset));
    put32(o + 44, uint32_t(offset >> 32));
    put32(o + 48, 1);
    put32(o + 52, uint32_t(footer));
    put32(extras + k * 8, pregap);
    put32(extras + k * 8 + 4, t.sectors);
  }
  put32(footer, uint32_t(name));
  put32(footer + 4, 1);
  for (size_t i = 0; i < image.size(); i++) b[name + 2 * i] = uint8_t(image[i]);
  return b;
}

}  // namespace ge::vfs
