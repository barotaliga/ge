// Tests: a demo file picked in the main menu's DEMOS checked before it plays
// (games/shooters_edge/client/demo_check.hpp). A recording of each format passes with its facts,
// plain and packed (gzip, a zip with a readme beside it, a tar in gzip); a text file named as a
// demo, a damaged recording, an archive of two recordings and a file that is gone are each refused
// with the reason, and the facts read as one line.
//
// ge:summary Tests the DEMOS page's check: each format plain and packed passes, the rest refused
// ge:keywords test demo-check check_demo_file check_demo_bytes facts_line valid-demo packed
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo_check.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "vfs/tar.hpp"
#include "vfs/unpack.hpp"
#include "vfs/zip.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace fs = std::filesystem;
using W = ge::vfs::Wrapping;

cl::DemoScript script() { return cl::loop_script("dm4", {128, 192, 24}, 64, 90); }

std::vector<uint8_t> text(const std::string& s) { return {s.begin(), s.end()}; }

}  // namespace

TEST(every_format_passes_with_its_facts) {
  const cl::GameAdapter& game = cl::classic_adapter();
  const cl::DemoScript s = script();
  struct One {
    std::vector<uint8_t> bytes;
    cl::DemoFormat format;
  };
  const One all[] = {{cl::write_single_demo(s), cl::DemoFormat::single},
                     {cl::write_client_demo(s), cl::DemoFormat::client},
                     {cl::write_multiview_demo(s), cl::DemoFormat::multiview}};
  for (const One& o : all) {
    cl::DemoFacts f;
    std::string error;
    CHECK_MSG(cl::check_demo_bytes(o.bytes, game, f, error), error);
    CHECK(f.format == o.format);
    CHECK(f.map == "dm4");
    CHECK(f.layers.empty());
  }
}

TEST(a_packed_demo_passes_as_the_recording_inside) {
  const cl::GameAdapter& game = cl::classic_adapter();
  const std::vector<uint8_t> mvd = cl::write_multiview_demo(script());
  cl::DemoFacts f;
  std::string error;
  CHECK_MSG(cl::check_demo_bytes(ge::vfs::write_gzip(mvd, "match.mvd"), game, f, error), error);
  CHECK(f.layers == std::vector<W>{W::gzip});
  CHECK(cl::facts_line(f).find("MULTIVIEW") == 0);
  CHECK(cl::facts_line(f).find("DM4") != std::string::npos);
  CHECK(cl::facts_line(f).find("GZIP") != std::string::npos);

  // A zip with the notes beside the recording: the notes are passed over.
  const std::vector<uint8_t> zip =
      ge::vfs::write_stored_zip({{"readme.txt", text("a final")}, {"final.mvd", mvd}});
  CHECK_MSG(cl::check_demo_bytes(zip, game, f, error), error);
  CHECK(f.layers == std::vector<W>{W::zip} && f.inner_name == "final.mvd");

  // A tar in gzip: two layers, said outermost first.
  const std::vector<uint8_t> tgz = ge::vfs::write_gzip(ge::vfs::write_tar({{"team.mvd", mvd}}));
  CHECK_MSG(cl::check_demo_bytes(tgz, game, f, error), error);
  CHECK((f.layers == std::vector<W>{W::gzip, W::tar}));
  CHECK(cl::facts_line(f).find("GZIP + TAR") != std::string::npos);
}

TEST(a_file_that_is_no_demo_is_refused_with_the_reason) {
  const cl::GameAdapter& game = cl::classic_adapter();
  cl::DemoFacts f;
  std::string error;
  // Text named as a recording.
  CHECK(!cl::check_demo_bytes(text("<html>not found</html>\n"), game, f, error));
  CHECK(!error.empty());
  // A recording cut off before its level starts.
  std::vector<uint8_t> mvd = cl::write_multiview_demo(script());
  mvd.resize(12);
  error.clear();
  CHECK(!cl::check_demo_bytes(mvd, game, f, error));
  CHECK(!error.empty());
  // An archive of two recordings: which one is not the player's to guess here.
  const std::vector<uint8_t> two = cl::write_multiview_demo(script());
  error.clear();
  CHECK(!cl::check_demo_bytes(ge::vfs::write_stored_zip({{"a.mvd", two}, {"b.mvd", two}}), game, f,
                              error));
  CHECK(error.find("a.mvd") != std::string::npos);
}

TEST(a_file_on_disk_is_read_and_checked) {
  const cl::GameAdapter& game = cl::classic_adapter();
  const fs::path dir = fs::temp_directory_path() / "ge_demo_check_test";
  fs::remove_all(dir);
  fs::create_directories(dir);
  {
    const std::vector<uint8_t> gz = ge::vfs::write_gzip(cl::write_client_demo(script()));
    std::ofstream(dir / "duel.rec.gz", std::ios::binary)
        .write(reinterpret_cast<const char*>(gz.data()), static_cast<std::streamsize>(gz.size()));
  }
  cl::DemoFacts f;
  std::string error;
  CHECK_MSG(cl::check_demo_file((dir / "duel.rec.gz").string(), game, f, error), error);
  CHECK(f.format == cl::DemoFormat::client && f.map == "dm4");
  error.clear();
  CHECK(!cl::check_demo_file((dir / "gone.mvd").string(), game, f, error));
  CHECK(error.find("gone.mvd") != std::string::npos);
  std::error_code ec;
  fs::remove_all(dir, ec);
}
