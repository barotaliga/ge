// Where the classic content comes from (plugins/classicenhance/source.hpp): the pack found
// through a release zip's installer archive (an LHA archive behind a self-extractor's program),
// in a zip directly, as a file and in an installed folder in any case; an unknown pack taken as the
// player's own copy, unverified; a damaged one refused, with its digest, writing nothing; the full
// game's later packs layered over the first; and a release known by its files whatever packs hold
// them (split, repacked into one with extras, a content zip), with the nearest set and what of it
// differs named when none matches; a game folder laid out from a source for --game, and a download
// named by its URL.
//
// ge:summary Tests the source search: every route, unverified and later packs
// ge:keywords test classicenhance lha source zip folder pack verify refuse sha256
// ge:keywords known-set repack pk3 content-zip remaster lay-out setup basedir url
// ge:skill classicenhance testing
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include "assets/sha256.hpp"
#include "check.hpp"
#include "classicenhance/cache.hpp"
#include "classicenhance/source.hpp"
#include "vfs/lha.hpp"
#include "vfs/pak.hpp"
#include "vfs/zip.hpp"

namespace ce = ge::classicenhance;
namespace fs = std::filesystem;

namespace {

// A self-extracting LHA archive: a program's bytes, then level-0 headers, then the end marker.
std::vector<uint8_t> sfx(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files) {
  static const std::vector<uint8_t> program = {'M', 'Z', 1, 2,   3,   4,   5,
                                               6,   7,   8, '-', 'l', 'h', 'x'};
  return ge::vfs::write_lha(files, '5', 0, program);
}

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

// A small pack standing in for the real one (whose bytes are never in the tree).
std::vector<uint8_t> fixture_pack() {
  std::vector<uint8_t> pal(768);
  for (size_t i = 0; i < pal.size(); i++) pal[i] = uint8_t(i * 7);
  std::vector<uint8_t> out;
  std::string error;
  ge::vfs::write_pak({{"gfx/palette.lmp", pal}, {"readme.txt", text("a fixture pack")}}, &out,
                     &error);
  return out;
}

fs::path scratch(const std::string& name) {
  const fs::path p = fs::temp_directory_path() / ("ge_classicenhance_" + name);
  fs::remove_all(p);
  fs::create_directories(p);
  return p;
}

void write(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
}

}  // namespace

TEST(source_finds_the_pack_by_every_route) {
  const std::vector<uint8_t> pack = fixture_pack();
  const std::string sha = ge::assets::sha256_hex(pack);
  const ce::KnownFile known[] = {{sha.c_str(), uint32_t(pack.size()), "the fixture pack"}};
  const fs::path dir = scratch("routes");
  std::string error;
  ce::Source s;

  // The release zip: the pack inside the installer's archive inside the zip.
  const auto release = ge::vfs::write_stored_zip(
      {{"resource.1", sfx({{"HELP.TXT", text("help")}, {"ID1\\PAK0.PAK", pack}})},
       {"install.bat", text("@echo off")}});
  write(dir / "release.zip", release);
  CHECK_MSG(ce::open_source((dir / "release.zip").string(), &s, &error, known), error);
  CHECK(s.route == ce::Route::release_zip && s.pack == pack && s.known == &known[0]);
  CHECK(s.zip == nullptr && s.zip_sha256 == ge::assets::sha256_hex(release));

  // The installer's archive on its own.
  write(dir / "resource.1", sfx({{"ID1\\PAK0.PAK", pack}}));
  CHECK_MSG(ce::open_source((dir / "resource.1").string(), &s, &error, known), error);
  CHECK(s.route == ce::Route::installer && s.pack == pack);

  // A zip holding the pack itself.
  write(dir / "plain.zip", ge::vfs::write_stored_zip({{"GAME/ID1/PAK0.PAK", pack}}));
  CHECK_MSG(ce::open_source((dir / "plain.zip").string(), &s, &error, known), error);
  CHECK(s.pack == pack);

  // The pack as a file, and an installed folder with its names in capitals.
  write(dir / "game" / "ID1" / "PAK0.PAK", pack);
  CHECK_MSG(ce::open_source((dir / "game" / "ID1" / "PAK0.PAK").string(), &s, &error, known),
            error);
  CHECK(s.route == ce::Route::pack_file);
  CHECK_MSG(ce::open_source((dir / "game").string(), &s, &error, known), error);
  CHECK(s.route == ce::Route::folder && s.pack == pack);

  // A folder with only the release zip in it.
  fs::create_directories(dir / "downloads");
  fs::copy_file(dir / "release.zip", dir / "downloads" / "shareware.zip");
  CHECK_MSG(ce::open_source((dir / "downloads").string(), &s, &error, known), error);
  CHECK(s.pack == pack);
  fs::remove_all(dir);
}

TEST(source_takes_an_unknown_pack_unverified_and_refuses_damage) {
  std::vector<uint8_t> pack = fixture_pack();
  const fs::path dir = scratch("refuse");
  write(dir / "pak0.pak", pack);
  ce::Source s;
  std::string error;
  // The real pack's list: the fixture is not on it, so it is the player's own copy, unverified.
  CHECK_MSG(ce::open_source((dir / "pak0.pak").string(), &s, &error), error);
  CHECK(s.known == nullptr && s.pack == pack && s.pack_sha256 == ge::assets::sha256_hex(pack));
  // A pack that does not read is refused with its digest.
  std::vector<uint8_t> damaged = pack;
  damaged.resize(damaged.size() / 2);
  write(dir / "pak0.pak", damaged);
  CHECK(!ce::open_source((dir / "pak0.pak").string(), &s, &error));
  CHECK(error.find(ge::assets::sha256_hex(damaged)) != std::string::npos);
  CHECK(error.find("does not read as a pack") != std::string::npos);
  CHECK(!ce::open_source((dir / "nothing-here").string(), &s, &error));
  write(dir / "notes.txt", text("not a pack"));
  CHECK(!ce::open_source((dir / "notes.txt").string(), &s, &error));
  CHECK(error.find("neither a zip, an installer archive nor a pack") != std::string::npos);
  fs::remove_all(dir);
}

TEST(source_layers_the_full_games_later_packs) {
  const std::vector<uint8_t> pack = fixture_pack();
  const std::string sha = ge::assets::sha256_hex(pack);
  const ce::KnownFile known[] = {{sha.c_str(), uint32_t(pack.size()), "the fixture pack"}};
  const fs::path dir = scratch("later");
  // A registered-style install: the first pack, then a second that replaces the readme and adds
  // a file, and a third that replaces the readme again.
  std::vector<uint8_t> second, third;
  std::string error;
  CHECK(ge::vfs::write_pak({{"readme.txt", text("the second")}, {"maps/e2m1.bsp", text("map")}},
                           &second, &error));
  CHECK(ge::vfs::write_pak({{"readme.txt", text("the third")}}, &third, &error));
  write(dir / "id1" / "PAK0.PAK", pack);
  write(dir / "id1" / "Pak1.pak", second);
  write(dir / "id1" / "pak2.pak", third);
  write(dir / "id1" / "pak4.pak", third);  // after a gap: not read, as the game reads them
  ce::Source s;
  CHECK_MSG(ce::open_source(dir.string(), &s, &error, known), error);
  CHECK(s.known == &known[0] && s.later.size() == 2);
  CHECK(s.later[0].name == "Pak1.pak" && s.later[0].known == nullptr &&
        s.later[1].known == nullptr);
  CHECK(s.set_sha256 != s.pack_sha256 && s.content() == s.merged);
  ge::vfs::Pak merged;
  CHECK_MSG(ge::vfs::read_pak(s.content(), &merged, &error), error);
  CHECK(merged.entries.size() == 3);  // the palette, the readme once, the new map
  const ge::vfs::PakEntry* readme = ge::vfs::pak_find(merged, "readme.txt");
  const std::span<const uint8_t> latest = ge::vfs::pak_file(s.content(), *readme);
  CHECK(std::string(latest.begin(), latest.end()) == "the third");
  CHECK(ge::vfs::pak_find(merged, "maps/e2m1.bsp") != nullptr);
  // A later pack known by its digest is reported as that release.
  const std::string sha2 = ge::assets::sha256_hex(second);
  const ce::KnownFile both[] = {known[0], {sha2.c_str(), uint32_t(second.size()), "the second"}};
  CHECK_MSG(ce::open_source(dir.string(), &s, &error, both), error);
  CHECK(s.later[0].known == &both[1] && s.later[1].known == nullptr);
  // The first pack alone keeps its own digest as the cache's key.
  fs::remove(dir / "id1" / "Pak1.pak");
  CHECK_MSG(ce::open_source(dir.string(), &s, &error, known), error);
  CHECK(s.later.empty() && s.set_sha256 == s.pack_sha256 && s.content() == pack);
  // A damaged later pack is refused.
  write(dir / "id1" / "pak1.pak", text("PACK but not one"));
  CHECK(!ce::open_source(dir.string(), &s, &error, known));
  CHECK(error.find("pak1.pak") != std::string::npos &&
        error.find("does not read as a pack") != std::string::npos);
  fs::remove_all(dir);
}

namespace {

// A fixture release by its files: the palette, a readme and a map, and a startup script known by
// its contents alone.
struct FixtureSet {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  std::vector<std::string> digests;  // kept alive for the rows
  std::vector<ce::KnownEntry> rows;
  ce::KnownSet set;
};

void fixture_set(FixtureSet* f, const char* id, bool remade) {
  std::vector<uint8_t> pal(768);
  for (size_t i = 0; i < pal.size(); i++) pal[i] = uint8_t(i * 7);
  f->files = {{"gfx/palette.lmp", pal},
              {"maps/e2m1.bsp", text(std::string("map of ") + id)},
              {"readme.txt", text("a fixture release")},
              {"start.rc", text("exec default.cfg")}};
  f->digests.clear();
  for (const auto& [path, bytes] : f->files) f->digests.push_back(ce::sha128_hex(bytes));
  f->rows.clear();
  for (size_t i = 0; i < f->files.size(); i++) {
    const bool by_contents = f->files[i].first == "start.rc";
    f->rows.push_back({by_contents ? nullptr : f->files[i].first.c_str(),
                       uint32_t(f->files[i].second.size()), f->digests[i].c_str()});
  }
  f->set = {id, id, f->rows, remade};
}

std::vector<uint8_t> pack_of(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& f) {
  std::vector<uint8_t> out;
  std::string error;
  CHECK_MSG(ge::vfs::write_pak(f, &out, &error), error);
  return out;
}

}  // namespace

TEST(source_knows_a_release_by_its_files_in_any_packs) {
  FixtureSet full, remaster;
  fixture_set(&full, "fixture-full", false);
  fixture_set(&remaster, "fixture-remaster", true);
  const ce::KnownSet sets[] = {full.set, remaster.set};
  const fs::path dir = scratch("sets");
  ce::Source s;
  std::string error;

  // As shipped: the first pack and a second, which together hold every file.
  write(dir / "split" / "id1" / "pak0.pak", pack_of({full.files[0], full.files[2], full.files[3]}));
  write(dir / "split" / "id1" / "pak1.pak", pack_of({full.files[1]}));
  CHECK_MSG(ce::open_source((dir / "split").string(), &s, &error, {}, sets), error);
  CHECK(s.set != nullptr && std::string(s.set->id) == "fixture-full" && s.set_note.empty());

  // Repackaged into one pack, the startup script under another name and files of the player's own
  // added beside: still the release.
  auto repack = full.files;
  repack[3].first = "boot.rc";
  repack.push_back({"maps/mine.bsp", text("the player's own map")});
  repack.push_back({"gfx/extra.lmp", text("more")});
  write(dir / "one" / "id1" / "pak0.pak", pack_of(repack));
  CHECK_MSG(ce::open_source((dir / "one").string(), &s, &error, {}, sets), error);
  CHECK(s.set != nullptr && std::string(s.set->id) == "fixture-full" && !s.content_zip);
  CHECK(s.later.empty() && s.set_sha256 == s.pack_sha256);

  // The same files in a content zip standing in for the first pack, its names in capitals and
  // '\\' separated, a folder's own entry and a path too long for a pack among them.
  std::vector<std::pair<std::string, std::vector<uint8_t>>> zipped = {
      {"GFX\\PALETTE.LMP", full.files[0].second},
      {"maps/", {}},
      {"Maps/E2M1.bsp", full.files[1].second},
      {"readme.txt", full.files[2].second},
      {"start.rc", full.files[3].second},
      {"textures/" + std::string(60, 'x') + ".png", text("too long for a pack")}};
  const std::vector<uint8_t> pk3 = ge::vfs::write_stored_zip(zipped);
  write(dir / "zip" / "id1" / "PAK0.PK3", pk3);
  CHECK_MSG(ce::open_source((dir / "zip").string(), &s, &error, {}, sets), error);
  CHECK(s.route == ce::Route::folder && s.content_zip &&
        s.zip_sha256 == ge::assets::sha256_hex(pk3));
  CHECK(s.set != nullptr && std::string(s.set->id) == "fixture-full");
  CHECK(s.set_sha256 == s.zip_sha256);  // keyed by the zip the game mounts
  ge::vfs::Pak pak;
  CHECK_MSG(ge::vfs::read_pak(s.content(), &pak, &error), error);
  CHECK(pak.entries.size() == 4 && ge::vfs::pak_find(pak, "maps/e2m1.bsp") != nullptr);
  // The zip as a file, and a later pack beside it layered over it.
  CHECK_MSG(ce::open_source((dir / "zip" / "id1" / "PAK0.PK3").string(), &s, &error, {}, sets),
            error);
  CHECK(s.route == ce::Route::pack_file && s.content_zip && s.set != nullptr);
  write(dir / "zip" / "id1" / "pak1.pak", pack_of({{"readme.txt", text("changed by pak1")}}));
  CHECK_MSG(ce::open_source((dir / "zip").string(), &s, &error, {}, sets), error);
  CHECK(s.later.size() == 1 && s.set == nullptr);
  CHECK(s.set_sha256 == ce::set_key(std::vector<std::string>{s.zip_sha256, s.later[0].sha256}));
  CHECK_MSG(s.set_note.find("3 of the 4 files of fixture-full") != std::string::npos &&
                s.set_note.find("1 changed (readme.txt)") != std::string::npos,
            s.set_note);
  // A pack and a content zip both: the pack is the first.
  write(dir / "zip" / "id1" / "pak0.pak", pack_of(remaster.files));
  fs::remove(dir / "zip" / "id1" / "pak1.pak");
  CHECK_MSG(ce::open_source((dir / "zip").string(), &s, &error, {}, sets), error);
  CHECK(!s.content_zip && s.set != nullptr && std::string(s.set->id) == "fixture-remaster" &&
        s.set->remade_sounds);

  // A file missing and one changed: no set, the nearest named with what differs.
  auto broken = full.files;
  broken[1].second = text("a map changed");
  broken.erase(broken.begin() + 3);
  write(dir / "broken" / "pak0.pak", pack_of(broken));
  CHECK_MSG(ce::open_source((dir / "broken" / "pak0.pak").string(), &s, &error, {}, sets), error);
  CHECK(s.set == nullptr);
  CHECK_MSG(s.set_note.find("1 missing (a file of 16 bytes)") != std::string::npos &&
                s.set_note.find("1 changed (maps/e2m1.bsp)") != std::string::npos,
            s.set_note);
  fs::remove_all(dir);
}

TEST(the_known_sets_list_every_file_once_sorted) {
  CHECK(ce::kKnownSets.size() == 2);
  for (const ce::KnownSet& set : ce::kKnownSets) {
    CHECK(set.files.size() > 400);
    std::string last;
    size_t nameless = 0;
    for (const ce::KnownEntry& e : set.files) {
      CHECK_MSG(std::string(e.sha128).size() == 32, set.id);
      if (e.path == nullptr) {
        nameless++;
        continue;
      }
      CHECK_MSG(nameless == 0 && std::string(e.path) > last, std::string(set.id) + ": " + e.path);
      last = e.path;
    }
    CHECK(nameless == 1);
  }
  // Only the remaster's sounds are remade; neither set matches the fixture pack.
  CHECK(!ce::kKnownSets[0].remade_sounds && ce::kKnownSets[1].remade_sounds);
  const ce::SetMatch m = ce::match_sets(fixture_pack(), ce::kKnownSets);
  CHECK(!m.whole() && m.held <= 1);
}

TEST(source_lays_out_a_game_folder_from_any_route) {
  const std::vector<uint8_t> pack = fixture_pack();
  const std::string sha = ge::assets::sha256_hex(pack);
  const ce::KnownFile known[] = {{sha.c_str(), uint32_t(pack.size()), "the fixture pack"}};
  const fs::path dir = scratch("layout");
  std::string error;
  ce::Source s;
  // The release zip: the pack out of the installer's archive, written as the base folder's first.
  write(dir / "release.zip",
        ge::vfs::write_stored_zip({{"resource.1", sfx({{"ID1\\PAK0.PAK", pack}})}}));
  CHECK_MSG(ce::open_source((dir / "release.zip").string(), &s, &error, known), error);
  std::vector<std::string> written;
  CHECK_MSG(ce::lay_out_game(s, (dir / "out").string(), &written, &error), error);
  const fs::path first = dir / "out" / std::string(s.profile->base_dir) / "pak0.pak";
  CHECK(written.size() == 1 && fs::path(written[0]) == first);
  ce::Source back;
  CHECK_MSG(ce::open_source((dir / "out").string(), &back, &error, known), error);
  CHECK(back.route == ce::Route::folder && back.pack == pack && back.known == &known[0]);
  CHECK(!fs::exists(first.string() + ".part"));
  // An install with a later pack: both written, the later one under its own spelling.
  std::vector<uint8_t> second;
  CHECK(ge::vfs::write_pak({{"maps/e2m1.bsp", text("map")}}, &second, &error));
  write(dir / "install" / "ID1" / "PAK0.PAK", pack);
  write(dir / "install" / "ID1" / "Pak1.pak", second);
  CHECK_MSG(ce::open_source((dir / "install").string(), &s, &error, known), error);
  CHECK_MSG(ce::lay_out_game(s, (dir / "out2").string(), &written, &error), error);
  CHECK(written.size() == 2 && fs::path(written[1]).filename() == "Pak1.pak");
  CHECK_MSG(ce::open_source((dir / "out2").string(), &back, &error, known), error);
  CHECK(back.later.size() == 1 && back.later[0].bytes == second);
  // A base folder that cannot be made is refused by name.
  write(dir / "blocked", text("a file, not a folder"));
  CHECK(!ce::lay_out_game(s, (dir / "blocked").string(), &written, &error));
  CHECK(error.find("--basedir") != std::string::npos);
  fs::remove_all(dir);
}

TEST(source_names_a_download_by_its_urls_last_segment) {
  CHECK(ce::url_file_name("https://example.org/pub/release106.zip") == "release106.zip");
  CHECK(ce::url_file_name("https://example.org/get/r.zip?mirror=2#top") == "r.zip");
  CHECK(ce::url_file_name("https://example.org/") == "");
  CHECK(ce::url_file_name("https://example.org/a/..") == "");
  CHECK(ce::url_file_name("https://example.org/a$b.zip") == "");
  CHECK(ce::url_file_name("no-slash") == "");
}
