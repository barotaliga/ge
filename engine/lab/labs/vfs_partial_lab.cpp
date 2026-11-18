// LAB `vfs-partial`: a pack fetched from a server one file at a time, in a shuffled order, kept in
// its partial (engine/vfs/partial.hpp) the way a client keeps downloads. Does every file land in
// `<name>.partial.pak`, does the partial stay a pack the search path reads, and when the last file
// comes is the archive put together byte for byte as the server's and the partial gone? Without
// `--pak` it fetches a fixture pack it writes itself (deterministic); with `--pak=<file>` a pack
// the user points it at, read at run time and never committed. The cost of each keep is measured.
//
// ge:summary Lab: a pack fetched file by file into its partial, put together byte for byte
// ge:keywords lab vfs partial-pack partial download origin assemble byte-identical shuffle
// ge:skill vfs labs
// ge:test tests/vfs_partial_test.cpp
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "vfs/pak.hpp"
#include "vfs/partial.hpp"
#include "vfs/search.hpp"

namespace {

namespace fs = std::filesystem;
namespace lab = ge::lab;
namespace vfs = ge::vfs;
using lab::Det;

std::vector<uint8_t> read_whole(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return {};
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// A fixture pack: forty files of seeded sizes and bytes under the folders a game keeps.
std::vector<uint8_t> fixture_pack(uint32_t seed) {
  ge::core::Rng rng = ge::core::make_rng(seed);
  const char* folders[] = {"skins/", "progs/", "sound/", "maps/", "gfx/"};
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  for (int i = 0; i < 40; ++i) {
    std::vector<uint8_t> b(size_t(ge::core::int_in(rng, 0, 60000)));
    for (uint8_t& x : b) x = uint8_t(ge::core::int_in(rng, 0, 255));
    files.emplace_back(std::string(folders[i % 5]) + "file" + std::to_string(i) + ".dat",
                       std::move(b));
  }
  std::vector<uint8_t> out;
  std::string error;
  vfs::write_pak(files, &out, &error);
  return out;
}

void run(lab::Lab& lab) {
  const std::string pak_arg = lab.arg("pak");
  const bool fixture = pak_arg.empty();
  const auto seed = uint32_t(lab.arg_num("seed"));
  const std::vector<uint8_t> server = fixture ? fixture_pack(seed) : read_whole(pak_arg);
  const std::string name = fixture ? "pak1.pak" : fs::path(pak_arg).filename().string();
  vfs::SearchPath sp;
  std::string error;
  if (!vfs::add_archive(&sp, name, server, "id1", &error)) {
    lab.fail("the pack did not read: " + error, "Pass --pak=<a .pak file>; skill: vfs.");
    return;
  }
  const vfs::Pak& pak = sp.sources[0].pak;
  std::vector<uint32_t> order(pak.entries.size());
  for (uint32_t i = 0; i < order.size(); ++i) order[i] = i;
  ge::core::Rng rng = ge::core::make_rng(seed);
  for (size_t i = order.size(); i > 1; --i)
    std::swap(order[i - 1], order[ge::core::pick_index(rng, i)]);

  const fs::path dir = fs::temp_directory_path() / "ge_lab_vfs_partial" / "id1";
  fs::remove_all(dir.parent_path());
  fs::create_directories(dir);
  lab::Trace held{"files held (% of the pack)", {}, {}},
      size{"partial on disk (% of the pack)", {}, {}};
  double keep_ms = 0, worst_ms = 0;
  bool readable = true, whole = false;
  for (size_t n = 0; n < order.size(); ++n) {
    const vfs::PakEntry& e = pak.entries[order[n]];
    vfs::ArchiveOrigin o;
    const vfs::FileRef* f = vfs::find(sp, e.path);
    // A name stored twice is served once, from the entry that wins it.
    if (f == nullptr || f->entry != order[n] || !vfs::archive_origin(sp, *f, &o)) continue;
    const auto t0 = std::chrono::steady_clock::now();
    vfs::Kept kept;
    if (!vfs::keep_in_partial(dir.string(), o, e.path, vfs::pak_file(server, e), &kept, &error)) {
      lab.fail("file " + e.path + " was not kept: " + error, "Read keep_in_partial; skill: vfs.");
      return;
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    keep_ms += ms;
    worst_ms = std::max(worst_ms, ms);
    whole = kept.whole;
    const double x = double(n + 1);
    held.x.push_back(x);
    held.y.push_back(100.0 * kept.held / std::max(1u, kept.count));
    size.x.push_back(x);
    size.y.push_back(100.0 * double(fs::file_size(kept.on_disk)) / double(server.size()));
    if (!whole && n % 16 == 0) {
      vfs::SearchPath check;
      readable = readable && vfs::add_game(&check, dir.parent_path().string(), "id1", &error) &&
                 vfs::find(check, e.path) != nullptr;
    }
  }
  const std::vector<uint8_t> built = read_whole(dir / name);
  const bool identical = whole && built == server;
  // Every file of the whole pack reads back as the server's; the bytes no file holds may differ.
  bool files_same = whole;
  vfs::Pak back;
  if (!whole || !vfs::read_pak(built, &back, &error) || back.entries.size() != pak.entries.size()) {
    files_same = false;
  } else {
    for (size_t i = 0; i < back.entries.size(); ++i) {
      const std::span<const uint8_t> a = vfs::pak_file(built, back.entries[i]),
                                     b = vfs::pak_file(server, pak.entries[i]);
      files_same = files_same && back.entries[i].path == pak.entries[i].path &&
                   back.entries[i].offset == pak.entries[i].offset &&
                   std::equal(a.begin(), a.end(), b.begin(), b.end());
    }
  }
  size_t differ = 0;
  for (size_t i = 0; i < std::min(built.size(), server.size()); ++i)
    differ += built[i] != server[i];
  differ += std::max(built.size(), server.size()) - std::min(built.size(), server.size());
  const bool partial_gone = !fs::exists(dir / vfs::partial_name(name));
  const Det det = fixture ? Det::deterministic : Det::measured;
  lab.metric("files", double(pak.entries.size()), "count", det);
  lab.metric("pack.bytes", double(server.size()), "B", det);
  lab.metric("whole", whole ? 1 : 0, "bool", det);
  lab.metric("byte_identical", identical ? 1 : 0, "bool", det);
  lab.metric("files_same", files_same ? 1 : 0, "bool", det);
  lab.metric("bytes_differing", double(differ), "B", det);
  lab.metric("partial_removed", partial_gone ? 1 : 0, "bool", det);
  lab.metric("partial_readable", readable ? 1 : 0, "bool", det);
  lab.metric("keep.total_ms", keep_ms, "ms", Det::measured);
  lab.metric("keep.worst_ms", worst_ms, "ms", Det::measured);

  lab::LineChart c;
  c.frame = {"A PACK FETCHED FILE BY FILE INTO ITS PARTIAL",
             name + ", " + std::to_string(pak.entries.size()) + " files, " +
                 std::to_string(server.size() / 1024) + " KB, seed " + lab.arg("seed") +
                 (identical    ? ": put together byte for byte"
                  : files_same ? ": every file in place, " + std::to_string(differ) +
                                     " leftover bytes no file holds are zero"
                               : ": NOT the server's files"),
             "files fetched", "% of the pack"};
  c.traces = {held, size};
  c.spikes = false;
  c.marks = {{double(order.size()), "renamed to " + name}};
  const lab::Chart chart = lab::line_chart(c);
  lab.shot(
      {"partial",
       chart.title,
       "Each file of the pack is fetched once, in a seeded shuffled order, and kept in " +
           vfs::partial_name(name) +
           " beside the game's packs; the lines are the files "
           "held and the partial's size on disk after each. With the last file the pack is "
           "put together and renamed.",
       "both lines climb to 100% together; the subtitle says byte for byte, or names the leftover "
       "bytes between files that no file carries",
       {{"pak", fixture ? "fixture" : name}, {"seed", lab.arg("seed")}}},
      lab::to_picture(chart.plot), det);
  fs::remove_all(dir.parent_path());
  if (!files_same || !partial_gone || !readable)
    lab.fail(
        "the pack was not put back as the server's (whole " + std::to_string(whole) +
            ", files the server's " + std::to_string(files_same) + ", partial removed " +
            std::to_string(partial_gone) + ", readable " + std::to_string(readable) + ")",
        "Run tests/vfs_partial_test; read lay_out_pack in engine/vfs/partial.cpp; skill: vfs.");
}

const lab::LabSpec kSpec{
    "vfs-partial",
    lab::LabKind::sim,
    "engine",
    "engine/vfs/partial.hpp",
    "vfs",
    "Does a pack fetched from a server file by file, in any order, fill its partial and come out "
    "as the server's pack byte for byte?",
    {
        {"pak", "", "a .pak file to fetch (empty: the lab's fixture pack)"},
        {"seed", "1", "the order the files are fetched in"},
    },
    "--seed=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
