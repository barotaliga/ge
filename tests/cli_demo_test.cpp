// Tests: `ge demo pack` and `ge demo unpack` (engine/cli/demo.hpp). A recording packed to
// `<name>z` unpacks to the same bytes; a tar.gz, a zip and a gzip holding one are unpacked to the
// recording's own name; the names; and the refusals: a missing file, a packed file packed again, a
// bad level, a plain file unpacked, an archive of two.
//
// ge:summary Tests ge demo pack and unpack: round trips, archive names, refusals
// ge:keywords test cli demo pack unpack zstd mvdz tar gz zip names refuse
// ge:skill agent-interface testing
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "check.hpp"
#include "cli/demo.hpp"
#include "vfs/tar.hpp"
#include "vfs/unpack.hpp"
#include "vfs/zip.hpp"
#include "vfs/zstd.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

namespace {

Outcome demo(const std::vector<std::string>& words) {
  std::vector<const char*> argv = {"ge", "demo"};
  for (const std::string& w : words) argv.push_back(w.c_str());
  const Args a = parse_args(static_cast<int>(argv.size()), argv.data());
  CHECK(a.command != nullptr);
  CHECK(!check_args(a));
  return run_demo(a, std::string(a.command->name).substr(5));
}

std::vector<uint8_t> recording() {
  std::vector<uint8_t> out;
  for (int i = 0; i < 50000; i++) {
    out.push_back(static_cast<uint8_t>(i % 11));
    out.push_back(static_cast<uint8_t>(i >> 5));
  }
  return out;
}

void put(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

std::vector<uint8_t> slurp(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

bool has_error(const Outcome& o, const std::string& part) {
  for (const Finding& f : o.findings)
    if (f.severity == Severity::error && f.what.find(part) != std::string::npos) return true;
  return false;
}

fs::path scratch() {
  const fs::path root = fs::temp_directory_path() / "ge_cli_demo_test";
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

}  // namespace

TEST(demo_names) {
  CHECK(packed_name("a/match.mvd") == "a/match.mvdz");
  CHECK(unpacked_name("a/match.mvdz", "") == "match.mvd");
  CHECK(unpacked_name("match.mvd.gz", "") == "match.mvd");
  CHECK(unpacked_name("match.mvd.zst", "") == "match.mvd");
  CHECK(unpacked_name("m.tgz", "") == "m.tar");
  CHECK(unpacked_name("x.zip", "demos/../../final.mvd") == "final.mvd");
  CHECK(unpacked_name("plain", "") == "plain.plain");
}

TEST(demo_pack_then_unpack_gives_the_recording_back) {
  const fs::path root = scratch();
  put(root / "match.mvd", recording());
  const Outcome p = demo({"pack", (root / "match.mvd").string(), "--level=9"});
  CHECK_MSG(p.findings.empty(), p.findings.empty() ? "" : p.findings[0].what);
  CHECK(fs::exists(root / "match.mvdz"));
  CHECK(fs::file_size(root / "match.mvdz") * 4 < fs::file_size(root / "match.mvd"));
  CHECK(p.result.find("\"level\":9") != std::string::npos);
  fs::remove(root / "match.mvd");
  const Outcome u = demo({"unpack", (root / "match.mvdz").string()});
  CHECK_MSG(u.findings.empty(), u.findings.empty() ? "" : u.findings[0].what);
  CHECK(slurp(root / "match.mvd") == recording());
  CHECK(u.result.find("[\"zstd\"]") != std::string::npos);
  fs::remove_all(root);
}

TEST(demo_unpack_opens_archives_by_the_recording_s_name) {
  const fs::path root = scratch();
  put(root / "m.tar.gz",
      ge::vfs::write_gzip(ge::vfs::write_tar({{"demos/final.mvd", recording()}})));
  Outcome u = demo({"unpack", (root / "m.tar.gz").string()});
  CHECK_MSG(u.findings.empty(), u.findings.empty() ? "" : u.findings[0].what);
  CHECK(slurp(root / "final.mvd") == recording());
  put(root / "z.zip", ge::vfs::write_stored_zip({{"duel.mvd", recording()}}));
  u = demo({"unpack", (root / "z.zip").string(), "--out=" + (root / "o.mvd").string()});
  CHECK(slurp(root / "o.mvd") == recording());
  put(root / "g.mvd.gz", ge::vfs::write_gzip(recording()));
  u = demo({"unpack", (root / "g.mvd.gz").string()});
  CHECK(slurp(root / "g.mvd") == recording());
  fs::remove_all(root);
}

TEST(demo_refusals) {
  const fs::path root = scratch();
  CHECK(has_error(demo({"pack", (root / "none.mvd").string()}), "cannot read"));
  put(root / "p.mvdz", ge::vfs::zstd_pack(recording()));
  CHECK(has_error(demo({"pack", (root / "p.mvdz").string()}), "packed already"));
  put(root / "a.mvd", recording());
  CHECK(has_error(demo({"pack", (root / "a.mvd").string(), "--level=40"}), "zstd level"));
  CHECK(has_error(demo({"unpack", (root / "a.mvd").string()}), "not packed"));
  CHECK(has_error(demo({"pack", (root / "a.mvd").string(), "--out=" + (root / "a.mvd").string()}),
                  "being packed"));
  put(root / "two.zip", ge::vfs::write_stored_zip({{"a.mvd", recording()}, {"b.mvd", {1, 2}}}));
  CHECK(has_error(demo({"unpack", (root / "two.zip").string()}), "a.mvd, b.mvd"));
  fs::remove_all(root);
}
