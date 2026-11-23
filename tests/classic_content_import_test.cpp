// Taking a classic game in (plugins/classic/content/import.hpp) from fixtures only, never a game's
// data: a cue sheet's image (a raw data track holding the base folder in upper case beside
// programs and configs, and two audio tracks) copied into data/q1/id1 with its music as
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
#include <iterator>
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
#include "vfs/lzma.hpp"
#include "vfs/pak.hpp"
#include "vfs/zip.hpp"

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

// A Quake II mission pack's first pack, and an identifier that knows it by its digest as the
// profiles know the releases' own.
std::vector<uint8_t> reckoning_pak() { return pak("xswamp"); }

content::PackIdentifier knows_the_fixture() {
  return [](uint64_t size, std::string_view sha, const ge::classicenhance::Profile** game) {
    const std::vector<uint8_t> want = reckoning_pak();
    const auto* q2 = ge::classicenhance::find_profile("q2");
    if (size != want.size() || sha != ge::assets::sha256_hex(want))
      return static_cast<const ge::classicenhance::MissionPack*>(nullptr);
    *game = q2;
    return ge::classicenhance::find_pack(*q2, "xatrix");
  };
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
  CHECK(r.game != nullptr && r.game->id == "q1");
  CHECK(r.game_dir == (data / "q1").string());
  CHECK(r.copied.size() == 2);
  CHECK(fs::exists(data / "q1/id1/pak0.pak") && fs::exists(data / "q1/id1/pak1.pak"));
  CHECK(!fs::exists(data / "q1/id1/config.cfg") && !fs::exists(data / "q1/game.exe"));
  CHECK(r.tracks.size() == 2);
  CHECK(r.tracks[0].track == 2 && r.tracks[0].from == "disc" && r.tracks[0].kept);
  CHECK(r.tracks[0].pcm_sha256 == ge::assets::sha256_hex(b2));  // the image's own bytes
  CHECK(r.tracks[1].pcm_sha256 == ge::assets::sha256_hex(b3));
  CHECK(r.tracks[0].length_check == -1);  // a second of tone is not the disc's track 2
  const std::vector<int32_t> got = flac_samples(data / "q1/music/track02.flac");
  CHECK(std::equal(got.begin(), got.end(), t2.begin(), t2.end()));
  const std::vector<content::StoredTrack> m = content::read_manifest(r.game_dir);
  CHECK(m.size() == 2 && m[1].file == "track03.flac" && m[1].pcm_sha256 == r.tracks[1].pcm_sha256);

  // Taken in once: the same image again reads nothing (taken.txt holds its fingerprint).
  CHECK(fs::exists(data / "q1/taken.txt"));
  content::ImportReport again;
  CHECK(content::import_game((src / "disc.cue").string(), data.string(), &again, &error));
  CHECK(again.unchanged && again.copied.empty() && again.same.empty() && again.tracks.empty());
  CHECK(again.game_dir == r.game_dir);
  // A copy that lost a file is taken in again: the lost one copied, the rest checked and kept.
  fs::remove(data / "q1/id1/pak1.pak");
  content::ImportReport mended;
  CHECK(content::import_game((src / "disc.cue").string(), data.string(), &mended, &error));
  CHECK(!mended.unchanged && mended.copied.size() == 1 && mended.same.size() == 1);
  CHECK(mended.tracks.size() == 2 && !mended.tracks[0].kept);
}

TEST(import_takes_a_changed_source_in_again) {
  const fs::path src = fresh("changed"), data = fresh("changed_data");
  save(src / "id1/pak0.pak", pak("start"));
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  CHECK(r.copied.size() == 1);
  CHECK(content::import_game(src.string(), data.string(), &r, &error) && r.unchanged);
  save(src / "id1/pak1.pak", pak("end"));  // the player added a pack
  CHECK(content::import_game(src.string(), data.string(), &r, &error));
  CHECK(!r.unchanged && r.copied.size() == 1 && r.same.size() == 1);
  CHECK(fs::exists(data / "q1/id1/pak1.pak"));
  CHECK(content::import_game(src.string(), data.string(), &r, &error) && r.unchanged);
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
  CHECK(fs::exists(data / "q1/id1/pak0.pak") && fs::exists(data / "q1/id1/maps/extra.bsp"));
  CHECK(!fs::exists(data / "q1/id1/autoexec.cfg"));
  CHECK(r.tracks.size() == 2 && r.tracks[0].track == 2 && r.tracks[1].track == 3);
  CHECK(r.tracks[0].from == "wav" && r.tracks[0].channels == 2 && r.tracks[0].rate == 44100);
  CHECK(fs::exists(data / "q1/music/track03.flac"));
}

TEST(import_takes_the_words_by_key_out_of_the_releases_archive) {
  // A release whose program prints keys ($qc_got_item) keeps the words in an archive beside the
  // base folder: only that file is taken, loose under the base folder.
  const fs::path src = fresh("kpf"), data = fresh("kpf_data");
  save(src / "rerelease/id1/pak0.pak", pak("start"));
  const std::string words = "qc_got_item = \"You got the {}\"\n";
  save(src / "rerelease/Game.kpf",
       vfs::write_stored_zip(
           {{"localization/loc_english.txt", std::vector<uint8_t>(words.begin(), words.end())},
            {"gfx/big.lmp", std::vector<uint8_t>(64, 3)}}));
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  const fs::path loc = data / "q1/id1/localization/loc_english.txt";
  CHECK(fs::exists(loc) && fs::file_size(loc) == words.size());
  CHECK(!fs::exists(data / "q1/Game.kpf") && !fs::exists(data / "q1/id1/gfx/big.lmp"));
}

TEST(import_opens_an_installers_archive) {
  const fs::path src = fresh("installer"), data = fresh("installer_data");
  save(src / "RESOURCE.1",
       vfs::write_lha({{"id1\\pak0.pak", pak("start")}, {"id1\\readme.txt", {'r'}}}, '5', 1));
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  CHECK(fs::exists(data / "q1/id1/pak0.pak"));
  CHECK(!content::import_game(fresh("empty").string(), data.string(), &r, &error));
  CHECK(error.find("no game") != std::string::npos);
}

TEST(soundtrack_numbers_names_and_ranks) {
  const std::vector<std::string> named = {"Track02.ogg", "track_11.flac", "music.txt"};
  CHECK((content::track_numbers(named) == std::vector<int>{2, 11, 0}));
  const std::vector<std::string> album = {"01 - one.flac", "02 - two.flac", "10 - ten.flac"};
  CHECK((content::track_numbers(album) == std::vector<int>{2, 3, 11}));
  CHECK(content::track_file(2) == "track02.flac");
  CHECK(content::soundtrack_of("q1")->seconds.size() == 10);
  const fs::path dir = fresh("rank");
  content::Samples lossy, disc;
  lossy.data.assign(size_t{2} * 1000, 5);
  disc.data.assign(size_t{2} * 1000, 6);
  content::StoredTrack t;
  std::string error;
  CHECK(content::store_track(dir.string(), "q1", 4, lossy, content::MusicSource::ogg, "ab", &t,
                             &error));
  CHECK(t.kept);
  CHECK(content::store_track(dir.string(), "q1", 4, disc, content::MusicSource::disc, "", &t,
                             &error));
  CHECK(t.kept && t.from == "disc");  // a disc's track replaces a decoded lossy one
  CHECK(content::store_track(dir.string(), "q1", 4, lossy, content::MusicSource::ogg, "ab", &t,
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

TEST(quake_iis_mission_packs_ask_the_digital_releases_track_numbers) {
  // ogg_remap_tracks: rogue's 2..11 are 12..21, xatrix's a table of its own; anything else stays.
  CHECK(content::q2_remap_track("rogue", 2) == 12 && content::q2_remap_track("rogue", 11) == 21);
  CHECK(content::q2_remap_track("xatrix", 2) == 9 && content::q2_remap_track("xatrix", 7) == 2);
  CHECK(content::q2_remap_track("xatrix", 11) == 18);
  CHECK(content::q2_remap_track("baseq2", 5) == 5 && content::q2_remap_track("rogue", 12) == 12);
  CHECK(content::q2_remap_track("rogue", 1) == 1);
}

TEST(import_takes_a_games_mission_packs_in_beside_it) {
  // An install with both packs: each copied into its own folder beside the base folder.
  const fs::path src = fresh("packs"), data = fresh("packs_data");
  save(src / "Quake/id1/pak0.pak", pak("start"));
  save(src / "Quake/HIPNOTIC/PAK0.PAK", pak("hip1m1"));
  save(src / "Quake/rogue/pak0.pak", pak("r1m1"));
  save(src / "Quake/rogue/progs.dat", {'p'});
  save(src / "Quake/mymod/pak0.pak", pak("mine"));  // a mod, not a pack: left
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  CHECK(r.base && r.game->id == "q1" && r.packs.size() == 2);
  CHECK(fs::exists(data / "q1/hipnotic/pak0.pak") && fs::exists(data / "q1/rogue/pak0.pak"));
  CHECK(!fs::exists(data / "q1/rogue/progs.dat") && !fs::exists(data / "q1/mymod"));
  content::ImportReport again;
  CHECK(content::import_game(src.string(), data.string(), &again, &error));
  CHECK(again.unchanged && again.packs.size() == 2 && again.packs[0]->name == "Scourge of Armagon");
}

TEST(import_tells_a_pack_two_games_name_alike_by_the_base_folder_beside_it) {
  const fs::path src = fresh("q2packs"), data = fresh("q2packs_data");
  save(src / "Quake II/baseq2/pak0.pak", pak("base1"));
  save(src / "Quake II/rogue/pak0.pak", pak("rmine1"));
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  CHECK(r.game->id == "q2" && r.packs.size() == 1 && r.packs[0]->name == "Ground Zero");
  CHECK(fs::exists(data / "q2/rogue/pak0.pak"));
}

TEST(import_takes_a_pack_alone_and_says_the_game_is_needed) {
  // The pack's own folder pointed at: the pack copied, the base game's claim left alone.
  const fs::path src = fresh("xatrix") / "xatrix", data = fresh("xatrix_data");
  save(src / "pak0.pak", reckoning_pak());
  save(src / "video/xin.cin", std::vector<uint8_t>(32, 4));
  save(data / "q2/taken.txt", {'x'});
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  CHECK(!r.base && r.game->id == "q2" && r.packs.size() == 1);
  CHECK(fs::exists(data / "q2/xatrix/pak0.pak") && fs::exists(data / "q2/xatrix/video/xin.cin"));
  CHECK(r.notes.size() == 1 && r.notes[0].find("Quake II itself") != std::string::npos);
  CHECK(fs::exists(data / "q2/taken.txt") && fs::exists(data / "q2/taken-xatrix.txt"));
  content::ImportReport again;
  CHECK(content::import_game(src.string(), data.string(), &again, &error));
  CHECK(again.unchanged && !again.base && again.packs.size() == 1);
}

TEST(import_reads_a_packs_disc_by_its_digest) {
  // A pack's disc: the pack under another folder's name, its movies under its own, a patch of the
  // base game's player models, and its music on audio tracks kept as the pack's own.
  const fs::path src = fresh("pack_disc"), data = fresh("pack_disc_data");
  const Files files = {{"Data/all/pak0.pak", reckoning_pak()},
                       {"Data/max/Xatrix/video/xin.cin", std::vector<uint8_t>(40, 5)},
                       {"Data/max/Xatrix/gamex86.dll", {'M', 'Z'}},
                       {"Data/patch/baseq2/players/male/w_ripper.md2", {'I', 'D', 'P', '2'}},
                       {"Setup.exe", {'M', 'Z'}}};
  std::vector<uint8_t> bin = vfs::raw_sectors(vfs::write_iso(files, true));
  const uint32_t data_sectors = uint32_t(bin.size() / 2352);
  const std::vector<uint8_t> b2 = le(tone(40, 7));
  bin.insert(bin.end(), b2.begin(), b2.end());
  save(src / "x.bin", bin);
  std::ofstream(src / "x.cue") << "FILE \"x.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 "
                                  "00:00:00\nTRACK 02 AUDIO\nINDEX 01 "
                               << at(data_sectors) << "\n";
  content::ImportReport r;
  std::string error;
  CHECK(!content::import_game((src / "x.cue").string(), data.string(), &r, &error));
  content::ImportOptions opts;
  opts.identify = knows_the_fixture();
  CHECK_MSG(content::import_game((src / "x.cue").string(), data.string(), &r, &error, opts), error);
  CHECK(!r.base && r.packs.size() == 1 && r.packs[0]->folder == "xatrix");
  CHECK(fs::exists(data / "q2/xatrix/pak0.pak") && fs::exists(data / "q2/xatrix/video/xin.cin"));
  CHECK(fs::exists(data / "q2/baseq2/players/male/w_ripper.md2"));
  CHECK(!fs::exists(data / "q2/xatrix/gamex86.dll"));
  CHECK(r.music_folder == "xatrix" && r.tracks.size() == 1);
  CHECK(fs::exists(data / "q2/xatrix/music/track02.flac") && !fs::exists(data / "q2/music"));
}

TEST(import_gives_a_discs_tracks_to_each_pack_on_it) {
  // One disc carrying both of Quake's packs and one set of audio tracks, which both packs' maps
  // ask for: each pack keeps the tracks.
  const fs::path src = fresh("qpacks_disc"), data = fresh("qpacks_disc_data");
  const Files files = {{"Armagon/HIPNOTIC/PAK0.PAK", pak("hip1m1")},
                       {"Eternity/ROGUE/PAK0.PAK", pak("r1m1")}};
  std::vector<uint8_t> bin = vfs::raw_sectors(vfs::write_iso(files, true));
  const uint32_t data_sectors = uint32_t(bin.size() / 2352);
  const std::vector<uint8_t> b2 = le(tone(30, 5));
  bin.insert(bin.end(), b2.begin(), b2.end());
  save(src / "QPacks.BIN", bin);
  std::ofstream(src / "QPacks.cue") << "FILE \"QPacks.BIN\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 "
                                       "00:00:00\nTRACK 02 AUDIO\nINDEX 01 "
                                    << at(data_sectors) << "\n";
  content::ImportOptions opts;
  opts.identify = [](uint64_t size, std::string_view sha,
                     const ge::classicenhance::Profile** game) {
    const auto* q1 = ge::classicenhance::find_profile("q1");
    *game = q1;
    for (const auto& [map, folder] :
         {std::pair{"hip1m1", "hipnotic"}, std::pair{"r1m1", "rogue"}}) {
      const std::vector<uint8_t> want = pak(map);
      if (size == want.size() && sha == ge::assets::sha256_hex(want))
        return ge::classicenhance::find_pack(*q1, folder);
    }
    return static_cast<const ge::classicenhance::MissionPack*>(nullptr);
  };
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game((src / "QPacks.cue").string(), data.string(), &r, &error, opts),
            error);
  CHECK(!r.base && r.packs.size() == 2 && r.tracks.size() == 1);
  for (const char* folder : {"hipnotic", "rogue"}) {
    CHECK(fs::exists(data / "q1" / folder / "pak0.pak"));
    CHECK(fs::exists(data / "q1" / folder / "music/track02.flac"));
    CHECK(fs::exists(data / "q1" / folder / "music/tracks.txt"));
  }
  CHECK(flac_samples(data / "q1/rogue/music/track02.flac") ==
        flac_samples(data / "q1/hipnotic/music/track02.flac"));
  CHECK(!fs::exists(data / "q1/music"));
}

TEST(import_reads_a_release_zip_and_a_7z_of_its_disc) {
  const fs::path src = fresh("archives"), data = fresh("archives_data");
  save(src / "release.zip",
       vfs::write_stored_zip({{"QuakeM1/Quake/ID1/PAK0.PAK", pak("start")},
                              {"QuakeM1/Quake/HIPNOTIC/PAK0.PAK", pak("hip1m1")},
                              {"QuakeM1/Quake/HIPNOTIC/README.TXT", {'r'}}}));
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game((src / "release.zip").string(), data.string(), &r, &error), error);
  CHECK(r.base && r.packs.size() == 1 && fs::exists(data / "q1/hipnotic/pak0.pak"));
  CHECK(!fs::exists(data / "q1/hipnotic/readme.txt"));
  // A 7z of a disc copier's image: unpacked beside the data, read, and the unpacked image removed.
  const std::vector<uint8_t> iso = vfs::write_iso({{"Data/all/pak0.pak", reckoning_pak()}}, true);
  save(src / "disc.7z", vfs::write_7z("disc.iso", iso, 1));
  content::ImportOptions opts;
  opts.identify = knows_the_fixture();
  CHECK_MSG(content::import_game((src / "disc.7z").string(), data.string(), &r, &error, opts),
            error);
  CHECK(r.packs.size() == 1 && fs::exists(data / "q2/xatrix/pak0.pak"));
  CHECK(!fs::exists(data / ".unpacking/unpacked.iso"));
}

TEST(packs_on_this_machine_are_offered_until_taken_in) {
  // A store's library with an install holding a pack, a pack's folder alone, another game's base
  // beside a folder of a shared name, and a disc holding a pack under another name (its size).
  const fs::path lib = fresh("offers"), data = fresh("offers_data");
  save(lib / "Quake/id1/pak0.pak", pak("start"));
  save(lib / "Quake/hipnotic/pak0.pak", pak("hip1m1"));
  save(lib / "Packs/xatrix/pak0.pak", reckoning_pak());
  save(lib / "Quake II/baseq2/pak0.pak", pak("base1"));
  save(lib / "Quake II/mods/rogue/readme.txt", {'r'});  // named, holds no pack
  std::vector<content::Offer> offers = content::find_offers({lib.string()}, data.string());
  CHECK_MSG(offers.size() == 2, std::to_string(offers.size()));
  if (offers.size() != 2) return;
  bool hip = false, xat = false;
  for (const content::Offer& o : offers) {
    if (o.pack->folder == "hipnotic")
      hip = o.game->id == "q1" && fs::path(o.source).filename() == "Quake";
    if (o.pack->folder == "xatrix")
      xat = o.game->id == "q2" && fs::path(o.source).filename() == "xatrix";
  }
  CHECK(hip && xat);
  // Taken in: no longer offered.
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game((lib / "Quake").string(), data.string(), &r, &error), error);
  offers = content::find_offers({lib.string()}, data.string());
  CHECK(offers.size() == 1 && offers[0].pack->folder == "xatrix");
}

TEST(a_game_taken_in_before_packs_were_known_gets_them_offered_and_taken) {
  // A base game taken in by an older build (its taken file of the first form), its packs left
  // behind in the source: offered from the source the taken file names, and taken in again.
  const fs::path src = fresh("old_take") / "Quake", data = fresh("old_take_data");
  save(src / "id1/pak0.pak", pak("start"));
  save(src / "hipnotic/pak0.pak", pak("hip1m1"));
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  fs::remove_all(data / "q1/hipnotic");
  {
    std::ifstream in(data / "q1/taken.txt");
    std::string text((std::istreambuf_iterator<char>(in)), {});
    text.replace(0, 7, "taken 1");
    std::ofstream(data / "q1/taken.txt") << text;
  }
  const std::vector<content::Offer> offers = content::find_offers({}, data.string());
  CHECK(offers.size() == 1 && offers[0].pack->folder == "hipnotic");
  content::ImportReport again;
  CHECK_MSG(content::import_game(src.string(), data.string(), &again, &error), error);
  CHECK(!again.unchanged && again.packs.size() == 1 && fs::exists(data / "q1/hipnotic/pak0.pak"));
  CHECK(content::find_offers({}, data.string()).empty());
}

TEST(another_add_on_beside_the_base_folder_is_named_and_left) {
  const fs::path src = fresh("addon"), data = fresh("addon_data");
  save(src / "Quake 2/baseq2/pak0.pak", pak("base1"));
  save(src / "Quake 2/xatrix/pak0.pak", pak("xswamp"));
  save(src / "Quake 2/zaero/pak0.pak", pak("zbase1"));
  content::ImportReport r;
  std::string error;
  CHECK_MSG(content::import_game(src.string(), data.string(), &r, &error), error);
  CHECK(r.packs.size() == 1 && !fs::exists(data / "q2/zaero"));
  CHECK_MSG(r.notes.size() == 1 && r.notes[0].starts_with("zaero beside baseq2 is not one of"),
            r.notes.empty() ? "" : r.notes[0]);
}
