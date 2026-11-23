// The disc image reader (engine/vfs/disc.hpp): fixture images (the primary tree and the Joliet
// one, cooked 2048-byte and raw 2352-byte sectors) read back file by file; a cue sheet with a raw
// data track and two audio tracks in one .bin, and with a track per file; damage refused (a cut
// image, a sheet naming a missing file, no file system).
//
// ge:summary Tests the disc reader: ISO 9660 and Joliet, raw sectors, cue sheets, audio, damage
// ge:keywords test vfs disc iso iso9660 joliet cue bin raw-sector audio-track refuse
// ge:skill vfs testing
#include <algorithm>
#include <cstddef>
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

TEST(disc_reads_raw_sectors_with_their_subchannel) {
  // A disc copier's .mdf: each 2352-byte sector followed by its 96 bytes of subchannel.
  const Files files = game_files();
  const std::vector<uint8_t> raw = vfs::raw_sectors(vfs::write_iso(files, true));
  std::vector<uint8_t> mdf;
  for (size_t s = 0; s < raw.size(); s += 2352) {
    mdf.insert(mdf.end(), raw.begin() + std::ptrdiff_t(s), raw.begin() + std::ptrdiff_t(s + 2352));
    mdf.insert(mdf.end(), 96, uint8_t(0xff));
  }
  const fs::path p = scratch("copy.mdf");
  save(p, mdf);
  CHECK(vfs::is_disc_image(p.string()));
  vfs::Disc d;
  std::string error;
  CHECK_MSG(vfs::open_disc(p.string(), &d, &error), error);
  CHECK(d.tracks[0].sector == 2448 && d.tracks[0].user == 16);
  check_files(d, files, false);
}

TEST(disc_reads_an_mdf_by_its_mds_sheet_data_and_audio) {
  // A disc copier's pair: the .mdf holds every sector with its 96 bytes of subchannel, the data
  // track then two audio tracks, each after a 150-sector pregap; the .mds lists them.
  const Files files = game_files();
  const std::vector<uint8_t> raw = vfs::raw_sectors(vfs::write_iso(files, true));
  std::vector<uint8_t> mdf;
  const auto sector = [&](const uint8_t* p) {
    mdf.insert(mdf.end(), p, p + 2352);
    mdf.insert(mdf.end(), 96, uint8_t(0x5a));
  };
  for (size_t s = 0; s < raw.size(); s += 2352) sector(raw.data() + s);
  const std::vector<int16_t> a2 = tone(30, 7), a3 = tone(20, 13);
  std::vector<vfs::DiscTrack> tracks(3);
  tracks[0].number = 1;
  tracks[0].sector = 2448;
  tracks[0].sectors = uint32_t(raw.size() / 2352);
  int n = 2;
  for (const auto* a : {&a2, &a3}) {
    const std::vector<uint8_t> gap(2352, 0), b = le(*a);
    for (int g = 0; g < 150; g++) sector(gap.data());
    vfs::DiscTrack& t = tracks[size_t(n - 1)];
    t.number = n++;
    t.kind = vfs::TrackKind::audio;
    t.sector = 2448;
    t.start = mdf.size();
    for (size_t s = 0; s < b.size(); s += 2352) sector(b.data() + s);
  }
  // A track's length runs to the next one's first sector, its pregap included.
  tracks[0].sectors += 150;
  tracks[1].sectors = 30 + 150;
  tracks[2].sectors = 20;
  save(scratch("PACK.MDF"), mdf);
  save(scratch("Pack.mds"), vfs::write_mds(tracks));
  for (const char* open : {"PACK.MDF", "Pack.mds"}) {
    vfs::Disc d;
    std::string error;
    CHECK_MSG(vfs::open_disc(scratch(open).string(), &d, &error), error);
    CHECK(d.tracks.size() == 3 && d.tracks[0].user == 16 && d.tracks[0].sector == 2448);
    check_files(d, files, false);
    std::vector<int16_t> got;
    CHECK(d.read_audio(d.tracks[2], &got, &error) && got == a3);
    CHECK(d.read_audio(d.tracks[1], &got, &error) && got.size() == size_t(180) * 588 * 2);
    CHECK(std::equal(a2.begin(), a2.end(), got.begin()));
  }
  // A sheet that names an image that is not there says so.
  save(scratch("lost.mds"), vfs::write_mds(tracks, "lost.mdf"));
  vfs::Disc d;
  std::string error;
  CHECK(!vfs::open_disc(scratch("lost.mds").string(), &d, &error));
  CHECK_MSG(error.find("lost.mdf") != std::string::npos, error);
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

TEST(disc_reads_a_sheet_renamed_with_its_image) {
  // A download that names the sheet and its image after the release, the sheet still naming the
  // disc's own file: the image with the sheet's name is read.
  save(scratch("The Pack.BIN"), vfs::raw_sectors(vfs::write_iso(game_files(), true)));
  std::ofstream(scratch("The Pack.CUE")) << "FILE \"Q2MP1.BIN\" BINARY\n  TRACK 01 MODE1/2352\n"
                                         << "    INDEX 01 00:00:00\n";
  vfs::Disc d;
  std::string error;
  CHECK_MSG(vfs::open_disc(scratch("The Pack.CUE").string(), &d, &error), error);
  check_files(d, game_files(), false);
}

TEST(disc_refuses_damage) {
  std::string error;
  vfs::Disc d;
  std::ofstream(scratch("missing.cue")) << "FILE \"nothere.bin\" BINARY\nTRACK 01 AUDIO\n";
  CHECK(!vfs::open_disc(scratch("missing.cue").string(), &d, &error));
  CHECK(error.find("nothere.bin") != std::string::npos);
  save(scratch("blank.iso"), std::vector<uint8_t>(size_t{40} * 2048));
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
