// The disc image reader (engine/vfs/disc.hpp): fixture images (the primary tree and the Joliet
// one, cooked 2048-byte and raw 2352-byte sectors) read back file by file; a cue sheet with a raw
// data track and two audio tracks in one .bin, and with a track per file; damage refused (a cut
// image, a sheet naming a missing file, no file system).
//
// ge:summary Tests the disc reader: ISO 9660 and Joliet, raw sectors, cue sheets, audio, damage
// ge:keywords test vfs disc iso iso9660 joliet cue bin raw-sector audio-track refuse
// ge:skill vfs testing
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "check.hpp"
#include "vfs/disc.hpp"

namespace vfs = ge::vfs;
namespace fs = std::filesystem;

namespace {

using Files = std::vector<std::pair<std::string, std::vector<uint8_t>>>;

std::vector<uint8_t> bytes(size_t n, uint32_t seed) {
  std::vector<uint8_t> out(n);
  for (size_t i = 0; i < n; i++) {
    seed = seed * 1103515245u + 12345u;
    out[i] = uint8_t(seed >> 24);
  }
  return out;
}

Files game_files() {
  return {{"base/pak0.pak", bytes(5000, 1)},
          {"base/pak1.pak", bytes(2048, 2)},
          {"Readme.txt", bytes(10, 3)},
          {"base/music/deep/notes.txt", bytes(4097, 4)}};
}

fs::path scratch(const std::string& name) {
  const fs::path dir = fs::temp_directory_path() / "ge_vfs_disc_test";
  fs::create_directories(dir);
  return dir / name;
}

void save(const fs::path& p, const std::vector<uint8_t>& b) {
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
}

void check_files(const vfs::Disc& d, const Files& want, bool any_case) {
  CHECK(d.files.size() == want.size());
  for (const auto& [path, data] : want) {
    const vfs::DiscFile* f = d.find(path);
    CHECK_MSG(f != nullptr, "missing " + path);
    if (f == nullptr) continue;
    if (!any_case) CHECK(f->path == path);
    std::vector<uint8_t> got;
    std::string error;
    CHECK(d.read_file(*f, &got, &error));
    CHECK(got == data);
  }
}

// A tone, 16-bit stereo, `sectors` long.
std::vector<int16_t> tone(uint32_t sectors, int step) {
  std::vector<int16_t> s(size_t(sectors) * 588 * 2);
  for (size_t i = 0; i < s.size(); i++) s[i] = int16_t((int(i) * step) % 20000 - 10000);
  return s;
}

std::vector<uint8_t> le(const std::vector<int16_t>& s) {
  std::vector<uint8_t> b;
  for (int16_t v : s) {
    b.push_back(uint8_t(uint16_t(v)));
    b.push_back(uint8_t(uint16_t(v) >> 8));
  }
  return b;
}

}  // namespace

TEST(disc_reads_the_primary_and_the_joliet_tree) {
  const Files files = game_files();
  for (bool joliet : {false, true}) {
    const fs::path p = scratch(joliet ? "j.iso" : "p.iso");
    save(p, vfs::write_iso(files, joliet));
    vfs::Disc d;
    std::string error;
    CHECK_MSG(vfs::open_disc(p.string(), &d, &error), error);
    CHECK(d.joliet == joliet);
    CHECK(d.volume == "GE_DISC");
    check_files(d, files, !joliet);
    CHECK(d.tracks.size() == 1 && d.tracks[0].kind == vfs::TrackKind::data);
  }
}

TEST(disc_reads_raw_sectors_with_no_sheet) {
  const Files files = game_files();
  const fs::path p = scratch("raw.bin");
  save(p, vfs::raw_sectors(vfs::write_iso(files, true)));
  vfs::Disc d;
  std::string error;
  CHECK_MSG(vfs::open_disc(p.string(), &d, &error), error);
  CHECK(d.tracks[0].sector == 2352 && d.tracks[0].user == 16);
  check_files(d, files, false);
}

TEST(disc_reads_a_sheet_with_data_and_audio_in_one_bin) {
  const Files files = game_files();
  std::vector<uint8_t> bin = vfs::raw_sectors(vfs::write_iso(files, true));
  const uint32_t data_sectors = uint32_t(bin.size() / 2352);
  const std::vector<int16_t> a2 = tone(150, 7), a3 = tone(76, 13);
  for (const auto* a : {&a2, &a3}) {
    const std::vector<uint8_t> b = le(*a);
    bin.insert(bin.end(), b.begin(), b.end());
  }
  save(scratch("game.bin"), bin);
  const auto at = [](uint32_t s) {
    char t[16];
    std::snprintf(t, sizeof t, "%02u:%02u:%02u", s / 75 / 60, s / 75 % 60, s % 75);
    return std::string(t);
  };
  std::ofstream(scratch("game.cue"))
      << "FILE \"GAME.BIN\" BINARY\n"
      << "  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n"
      << "  TRACK 02 AUDIO\n    PREGAP 00:02:00\n    INDEX 01 " << at(data_sectors)
      << "\n  TRACK 03 AUDIO\n    INDEX 00 " << at(data_sectors + 100) << "\n    INDEX 01 "
      << at(data_sectors + 150) << "\n";
  vfs::Disc d;
  std::string error;
  CHECK_MSG(vfs::open_disc(scratch("game.cue").string(), &d, &error), error);
  CHECK(d.tracks.size() == 3);
  check_files(d, files, false);
  std::vector<int16_t> got;
  CHECK(d.read_audio(d.tracks[1], &got, &error) && got == a2);
  CHECK(d.read_audio(d.tracks[2], &got, &error) && got == a3);
  CHECK(!d.read_audio(d.tracks[0], &got, &error));
}

TEST(disc_reads_a_sheet_with_a_file_per_track) {
  const std::vector<int16_t> a = tone(40, 3);
  save(scratch("t01.iso"), vfs::write_iso(game_files(), false));
  save(scratch("t02.bin"), le(a));
  std::ofstream(scratch("split.cue"))
      << "FILE \"t01.iso\" BINARY\nTRACK 01 MODE1/2048\nINDEX 01 00:00:00\n"
      << "FILE \"t02.bin\" BINARY\nTRACK 02 AUDIO\nINDEX 01 00:00:00\n";
  vfs::Disc d;
  std::string error;
  CHECK_MSG(vfs::open_disc(scratch("split.cue").string(), &d, &error), error);
  std::vector<int16_t> got;
  CHECK(d.tracks.size() == 2 && d.tracks[1].sectors == 40);
  CHECK(d.read_audio(d.tracks[1], &got, &error) && got == a);
}

TEST(disc_refuses_damage) {
  std::string error;
  vfs::Disc d;
  std::ofstream(scratch("missing.cue")) << "FILE \"nothere.bin\" BINARY\nTRACK 01 AUDIO\n";
  CHECK(!vfs::open_disc(scratch("missing.cue").string(), &d, &error));
  CHECK(error.find("nothere.bin") != std::string::npos);
  save(scratch("blank.iso"), std::vector<uint8_t>(40 * 2048));
  CHECK(!vfs::open_disc(scratch("blank.iso").string(), &d, &error));
  CHECK(error.find("ISO 9660") != std::string::npos);
  std::vector<uint8_t> img = vfs::write_iso(game_files(), true);
  img.resize(img.size() - 4096);  // the last file cut short
  save(scratch("cut.iso"), img);
  CHECK(vfs::open_disc(scratch("cut.iso").string(), &d, &error));
  std::vector<uint8_t> got;
  bool all = true;
  for (const vfs::DiscFile& f : d.files) all = all && d.read_file(f, &got, &error);
  CHECK(!all);
}
