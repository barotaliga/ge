// engine/hub/content.hpp: a demo's content pack: only the named files out of the packs beside the
// viewer (a later pack wins, a folder name takes the folder, a missing name counted), zstd-packed
// and named by its hash, made once and read back, served by its path, and the Demo page handing
// it to the viewer in place of every pack; what a file found names in turn taken once each.
//
// ge:summary Test: a demo's content pack made from the viewer's packs, cached, served, handed over
// ge:keywords test hub content pack demo files zstd pak cache demo-page
// ge:skill net testing
#include "hub/content.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "hub/archive.hpp"
#include "hub/watch.hpp"
#include "mod/sha256.hpp"
#include "replay/stream.hpp"
#include "vfs/pak.hpp"
#include "vfs/unpack.hpp"

using namespace ge::hub;
namespace gr = ge::replay;
namespace fs = std::filesystem;

namespace {

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

std::vector<uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

// The demo names its map in its meta; the game's list reads it from there.
std::vector<uint8_t> demo_file(const std::string& map) {
  gr::StreamHeader h;
  h.game = "test_game";
  h.meta = {{"map", map}, {"mode", "duel"}};
  h.tracks = {{1, gr::TrackKind::meta, gr::TrackCodec::stored, "meta"}};
  std::vector<uint8_t> out;
  gr::StreamWriter w;
  gr::stream_begin(w, h, out);
  const uint8_t c[] = {1};
  gr::stream_add(w, 1, 0, c);
  gr::stream_end_block(w, true);
  gr::stream_finish(w, {{"duration_ms", "1000"}});
  return out;
}

HubSpec spec() {
  HubSpec s;
  s.game = "Test Game";
  s.id = "test_game";
  s.demo_files = [](std::span<const uint8_t> demo, std::vector<std::string>& names,
                    std::string& error) {
    gr::StreamReader r;
    if (!gr::read_stream_header(demo, r, error)) return false;
    std::string map;
    for (const auto& [k, v] : r.header.meta)
      if (k == "map") map = v;
    names = {"maps/" + map + ".bsp", "sound/", "gfx/missing.lmp"};
    return true;
  };
  return s;
}

struct Fixture {
  fs::path dir;
  Archive archive;
  Site site;
  DemoRow tower, yard;

  Fixture() {
    dir = fs::temp_directory_path() / "ge-hub-content-test";
    fs::remove_all(dir);
    const fs::path viewer = dir / "viewer";
    fs::create_directories(viewer);
    std::ofstream(viewer / "test_game_viewer.js") << "// the loader\n";
    std::ofstream(viewer / "test_game_viewer.wasm", std::ios::binary) << std::string("\0asm", 4);
    std::vector<uint8_t> pak;
    std::string error;
    // pak0 holds both maps and a sound; pak1 replaces the tower and adds a sound.
    CHECK(ge::vfs::write_pak({{"maps/tower.bsp", bytes("old tower")},
                              {"maps/yard.bsp", bytes(std::string(4000, 'y'))},
                              {"sound/a.wav", bytes("aaaa")},
                              {"progs/big.mdl", bytes(std::string(9000, 'm'))}},
                             &pak, &error));
    std::ofstream(viewer / "pak0.pak", std::ios::binary)
        .write(reinterpret_cast<const char*>(pak.data()), static_cast<std::streamsize>(pak.size()));
    CHECK(ge::vfs::write_pak(
        {{"maps/tower.bsp", bytes(std::string(3000, 't'))}, {"sound/b/b.wav", bytes("bbbb")}}, &pak,
        &error));
    std::ofstream(viewer / "pak1.pak", std::ios::binary)
        .write(reinterpret_cast<const char*>(pak.data()), static_cast<std::streamsize>(pak.size()));
    CHECK(archive_open(archive, (dir / "archive").string(), error));
    CHECK(archive_add(archive, demo_file("tower"), 1, {}, error, &tower) == AddResult::added);
    CHECK(archive_add(archive, demo_file("yard"), 2, {}, error, &yard) == AddResult::added);
    site.archive = &archive;
    site.viewer_dir = viewer.string();
  }
  ~Fixture() { fs::remove_all(dir); }
};

// The files inside a pack's packed bytes.
std::vector<ge::vfs::PakEntry> entries(const std::vector<uint8_t>& packed,
                                       std::vector<uint8_t>& raw) {
  ge::vfs::Unpacked u;
  std::string error;
  CHECK(ge::vfs::unpack_one(std::vector<uint8_t>(packed), u, error));
  raw = u.bytes;
  ge::vfs::Pak pak;
  CHECK(ge::vfs::read_pak(raw, &pak, &error));
  return pak.entries;
}

}  // namespace

TEST(a_demos_pack_holds_only_its_files_and_the_later_pack_wins) {
  Fixture f;
  ContentPack pack;
  std::string error;
  CHECK(demo_content(f.site, spec(), f.tower.sha, pack, error));
  CHECK(pack.files == 3);    // the tower, and both sounds under the folder
  CHECK(pack.missing == 1);  // gfx/missing.lmp
  const fs::path file = fs::path(f.archive.dir) / content_path(pack.sha);
  std::ifstream in(file, std::ios::binary);
  const std::vector<uint8_t> packed((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
  CHECK(packed.size() == pack.bytes);
  CHECK(ge::mod::sha256_hex(packed.data(), packed.size()) == pack.sha);  // named by its own hash
  std::vector<uint8_t> raw;
  const auto list = entries(packed, raw);
  CHECK(raw.size() == pack.raw);
  bool tower = false, a = false, b = false;
  for (const auto& e : list) {
    if (e.path == "maps/tower.bsp") tower = e.size == 3000;  // pak1's, not pak0's
    if (e.path == "sound/a.wav") a = true;
    if (e.path == "sound/b/b.wav") b = true;
    CHECK(e.path != "maps/yard.bsp" && e.path != "progs/big.mdl");
  }
  CHECK(tower && a && b);
}

TEST(a_pack_is_made_once_and_shared_by_its_hash) {
  Fixture f;
  ContentPack first, again, yard;
  std::string error;
  CHECK(demo_content(f.site, spec(), f.tower.sha, first, error));
  // Made once: the index answers, even with the viewer's packs gone.
  fs::remove(fs::path(f.site.viewer_dir) / "pak0.pak");
  fs::remove(fs::path(f.site.viewer_dir) / "pak1.pak");
  CHECK(demo_content(f.site, spec(), f.tower.sha, again, error));
  CHECK(again.sha == first.sha && again.bytes == first.bytes && again.files == first.files);
  // Another demo with nothing to read it from is refused with the reason.
  CHECK(!demo_content(f.site, spec(), f.yard.sha, yard, error));
  CHECK(!error.empty());
}

TEST(the_pack_is_served_by_its_path_and_other_paths_refused) {
  Fixture f;
  ContentPack pack;
  std::string error;
  CHECK(demo_content(f.site, spec(), f.tower.sha, pack, error));
  Answer a;
  CHECK(content_answer("/" + content_path(pack.sha), f.site, a));
  CHECK(a.code == 200 && a.body.size() == pack.bytes);
  CHECK(a.max_age_s == 365 * 24 * 3600);
  CHECK(content_answer("/" + content_path(std::string(64, 'a')), f.site, a) && a.code == 404);
  CHECK(content_answer("/content/../archive.tsv", f.site, a) && a.code == 404);
  CHECK(!content_answer("/demos", f.site, a));
}

TEST(the_demo_page_hands_the_viewer_its_pack) {
  Fixture f;
  Answer a;
  CHECK(watch_answer(Poller{}, spec(), "/demo/" + f.tower.sha, 0, f.site, a) && a.code == 200);
  CHECK(has(a.body, "\"--pack=/content/") && has(a.body, ".pak.zst\""));
  CHECK(!has(a.body, "--pack=/viewer/pak0.pak"));
  // A game that names no files: every pack beside the viewer, as before.
  HubSpec plain = spec();
  plain.demo_files = nullptr;
  CHECK(watch_answer(Poller{}, plain, "/demo/" + f.tower.sha, 0, f.site, a) && a.code == 200);
  CHECK(has(a.body, "\"--pack=/viewer/pak0.pak\",\"--pack=/viewer/pak1.pak\""));
}

TEST(a_file_found_names_more_and_each_is_taken_once) {
  Fixture f;
  HubSpec s = spec();
  int asked = 0;
  // The tower names the big model, which names the tower again: each read once.
  s.file_needs = [&](std::string_view name, std::span<const uint8_t>,
                     std::vector<std::string>& more) {
    asked++;
    if (name == "maps/tower.bsp") more = {"PROGS/BIG.MDL", "gfx/also-missing.lmp"};
    if (name == "progs/big.mdl") more = {"maps/tower.bsp"};
  };
  ContentPack pack;
  std::string error;
  CHECK_MSG(demo_content(f.site, s, f.tower.sha, pack, error), error);
  CHECK(pack.files == 4);    // the tower, both sounds, and the model it named
  CHECK(pack.missing == 2);  // gfx/missing.lmp and the one the tower named
  CHECK(asked == 4);         // once for each file found
  std::ifstream in(fs::path(f.archive.dir) / content_path(pack.sha), std::ios::binary);
  const std::vector<uint8_t> packed((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
  std::vector<uint8_t> raw;
  bool big = false;
  for (const auto& e : entries(packed, raw)) big = big || e.path == "progs/big.mdl";
  CHECK(big);
}
