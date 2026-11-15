// Taking a classic game in (plugins/classic/content/import.hpp) from fixtures only, never a game's
// data: a cue sheet's image (a raw data track holding the base folder in upper case beside
// programs and configs, and two audio tracks) copied into data/classic/id1 with its music as
// track02.flac and track03.flac, whose samples' hash is the image's track bytes' own; taken in
// again, nothing rewritten; a store-like folder (the install under a game folder, its music as
// WAV at the top, a mod's music beside it left out, programs left out); an installer's LHA
// archive on a disc; the soundtrack store's names, ranks and manifest.
//
// ge:summary Tests taking a game in: a cue/bin image, an install folder, an installer, the music
// ge:keywords test import copy data-folder iso cue bin audio-track soundtrack flac manifest lha
// ge:keywords track-numbers wav
// ge:skill classic testing
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "assets/sha256.hpp"
#include "audio/flac.hpp"
#include "audio/wav.hpp"
#include "check.hpp"
#include "classic/content/import.hpp"
#include "vfs/disc.hpp"
#include "vfs/lha.hpp"
#include "vfs/pak.hpp"

namespace content = ge::classic::content;
namespace vfs = ge::vfs;
namespace fs = std::filesystem;

namespace {

using Files = std::vector<std::pair<std::string, std::vector<uint8_t>>>;

fs::path fresh(const std::string& name) {
  const fs::path d = fs::temp_directory_path() / "ge_import_test" / name;
  fs::remove_all(d);
  fs::create_directories(d);
  return d;
}

void save(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
}

std::vector<uint8_t> pak(const std::string& map) {
  std::vector<uint8_t> out;
  std::string error;
  vfs::write_pak({{"maps/" + map + ".bsp", std::vector<uint8_t>(300, 7)},
                  {"gfx/palette.lmp", std::vector<uint8_t>(768, 1)}},
                 &out, &error);
  return out;
}

std::vector<int16_t> tone(uint32_t sectors, int step) {
  std::vector<int16_t> s(size_t(sectors) * 588 * 2);
  for (size_t i = 0; i < s.size(); i++) s[i] = int16_t((int(i) * step) % 16000 - 8000);
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

std::string at(uint32_t s) {
  char t[16];
  std::snprintf(t, sizeof t, "%02u:%02u:%02u", s / 75 / 60, s / 75 % 60, s % 75);
  return t;
}

std::vector<int32_t> flac_samples(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), {});
  std::vector<int32_t> s;
  ge::audio::FlacInfo info;
  std::string error;
  CHECK_MSG(ge::audio::flac_decode(b, &s, &info, &error), error);
  return s;
}

}  // namespace

TEST(import_copies_a_cue_images_game_and_its_audio_tracks) {
  const fs::path src = fresh("cue"), data = fresh("cue_data");
  const Files files = {{"ID1/PAK0.PAK", pak("start")},
                       {"ID1/PAK1.PAK", pak("end")},
                       {"ID1/CONFIG.CFG", {'b', 'i', 'n', 'd'}},
                       {"GAME.EXE", {'M', 'Z'}}};
  std::vector<uint8_t> bin = vfs::raw_sectors(vfs::write_iso(files, false));
  const uint32_t data_sectors = uint32_t(bin.size() / 2352);
  const std::vector<int16_t> t2 = tone(80, 5), t3 = tone(30, 11);
  const std::vector<uint8_t> b2 = le(t2), b3 = le(t3);
  bin.insert(bin.end(), b2.begin(), b2.end());
  bin.insert(bin.end(), b3.begin(), b3.end());
  save(src / "disc.bin", bin);
  std::ofstream(src / "disc.cue") << "FILE \"disc.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 "
                                     "00:00:00\nTRACK 02 AUDIO\nINDEX 01 "
                                  << at(data_sectors) << "\nTRACK 03 AUDIO\nINDEX 01 "
                                  << at(data_sectors + 80) << "\n";
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game((src / "disc.cue").string(), data.string(), &r, &error), error);
  CHECK(r.game != nullptr && r.game->id == "classic");
  CHECK(r.game_dir == (data / "classic").string());
  CHECK(r.copied.size() == 2);
  CHECK(fs::exists(data / "classic/id1/pak0.pak") && fs::exists(data / "classic/id1/pak1.pak"));
  CHECK(!fs::exists(data / "classic/id1/config.cfg") && !fs::exists(data / "classic/game.exe"));
  CHECK(r.tracks.size() == 2);
  CHECK(r.tracks[0].track == 2 && r.tracks[0].from == "disc" && r.tracks[0].kept);
  CHECK(r.tracks[0].pcm_sha256 == ge::assets::sha256_hex(b2));  // the image's own bytes
  CHECK(r.tracks[1].pcm_sha256 == ge::assets::sha256_hex(b3));
  CHECK(r.tracks[0].length_check == -1);  // a second of tone is not the disc's track 2
  const std::vector<int32_t> got = flac_samples(data / "classic/music/track02.flac");
  CHECK(std::equal(got.begin(), got.end(), t2.begin(), t2.end()));
  const std::vector<content::StoredTrack> m = content::read_manifest(r.game_dir);
  CHECK(m.size() == 2 && m[1].file == "track03.flac" && m[1].pcm_sha256 == r.tracks[1].pcm_sha256);

  content::ImportReport again;
  CHECK(content::import_game((src / "disc.cue").string(), data.string(), &again, &error));
  CHECK(again.copied.empty() && again.same.size() == 2);
  CHECK(again.tracks.size() == 2 && !again.tracks[0].kept);
}

TEST(import_copies_a_store_install_and_its_music_files) {
  const fs::path src = fresh("store"), data = fresh("store_data");
  save(src / "Game/id1/pak0.pak", pak("start"));
  save(src / "Game/id1/maps/extra.bsp", std::vector<uint8_t>(40, 2));
  save(src / "Game/id1/autoexec.cfg", {'x'});
  save(src / "Game/game.exe", {'M', 'Z'});
  std::vector<float> f(size_t{2} * 4410);
  for (size_t i = 0; i < f.size(); i++) f[i] = float(int(i % 200) - 100) / 400.0f;
  save(src / "Game/music/Track02.wav", ge::audio::wav_bytes(f, 2, 44100));
  save(src / "Game/music/track03.wav", ge::audio::wav_bytes(f, 2, 44100));
  save(src / "Game/addon/music/track02.wav", ge::audio::wav_bytes(f, 1, 22050));  // a mod's
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  CHECK(fs::exists(data / "classic/id1/pak0.pak") &&
        fs::exists(data / "classic/id1/maps/extra.bsp"));
  CHECK(!fs::exists(data / "classic/id1/autoexec.cfg"));
  CHECK(r.tracks.size() == 2 && r.tracks[0].track == 2 && r.tracks[1].track == 3);
  CHECK(r.tracks[0].from == "wav" && r.tracks[0].channels == 2 && r.tracks[0].rate == 44100);
  CHECK(fs::exists(data / "classic/music/track03.flac"));
}

TEST(import_opens_an_installers_archive) {
  const fs::path src = fresh("installer"), data = fresh("installer_data");
  save(src / "RESOURCE.1",
       vfs::write_lha({{"id1\\pak0.pak", pak("start")}, {"id1\\readme.txt", {'r'}}}, '5', 1));
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  CHECK(fs::exists(data / "classic/id1/pak0.pak"));
  CHECK(!content::import_game(fresh("empty").string(), data.string(), &r, &error));
  CHECK(error.find("no game") != std::string::npos);
}

TEST(soundtrack_numbers_names_and_ranks) {
  const std::vector<std::string> named = {"Track02.ogg", "track_11.flac", "music.txt"};
  CHECK((content::track_numbers(named) == std::vector<int>{2, 11, 0}));
  const std::vector<std::string> album = {"01 - one.flac", "02 - two.flac", "10 - ten.flac"};
  CHECK((content::track_numbers(album) == std::vector<int>{2, 3, 11}));
  CHECK(content::track_file(2) == "track02.flac");
  CHECK(content::soundtrack_of("classic")->seconds.size() == 10);
  const fs::path dir = fresh("rank");
  content::Samples lossy, disc;
  lossy.data.assign(size_t{2} * 1000, 5);
  disc.data.assign(size_t{2} * 1000, 6);
  content::StoredTrack t;
  std::string error;
  CHECK(content::store_track(dir.string(), "classic", 4, lossy, content::MusicSource::ogg, "ab", &t,
                             &error));
  CHECK(t.kept);
  CHECK(content::store_track(dir.string(), "classic", 4, disc, content::MusicSource::disc, "", &t,
                             &error));
  CHECK(t.kept && t.from == "disc");  // a disc's track replaces a decoded lossy one
  CHECK(content::store_track(dir.string(), "classic", 4, lossy, content::MusicSource::ogg, "ab", &t,
                             &error));
  CHECK(!t.kept && t.from == "disc");  // and is not replaced by one
}

TEST(a_levels_track_and_its_kept_file) {
  CHECK(content::level_track("{\n\"classname\" \"worldspawn\"\n\"sounds\" \"4\"\n}\n{\n}") == 4);
  CHECK(content::level_track("{\n\"classname\" \"worldspawn\"\n}\n{\n\"sounds\" \"2\"\n}") == 0);
  CHECK(content::level_track("") == 0);
  const fs::path dir = fresh("kept");
  save(dir / "music/track04.flac", {'f', 'L', 'a', 'C'});
  CHECK(content::track_path(dir.string(), 4) == (dir / "music/track04.flac").string());
  CHECK(content::track_path(dir.string(), 5).empty() && content::track_path("", 4).empty());
}
