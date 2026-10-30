// The brush tool (plugins/brush/tool.hpp): `info` counts a fixture map, `ents` lists its entity
// blocks (or one class's), `check` reads every map on a search path and names a damaged one, and a
// usage mistake exits 2 with what to do.
//
// ge:summary Test: the brush tool's info, ents and check answers, a refusal named, usage errors
// ge:keywords test brush tool info ents check search-path json usage
// ge:skill brush testing
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "brush/tool.hpp"
#include "brush/write.hpp"
#include "check.hpp"
#include "vfs/pak.hpp"

namespace br = ge::brush;
namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> fixture_map() {
  std::vector<uint8_t> bytes;
  std::string error;
  CHECK(br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error));
  return bytes;
}

void put(const fs::path& p, const std::vector<uint8_t>& b) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary)
      .write(reinterpret_cast<const char*>(b.data()), std::streamsize(b.size()));
}

int run(const std::vector<std::string>& words, std::string* out) {
  std::vector<const char*> argv = {"brush"};
  for (const std::string& w : words) argv.push_back(w.c_str());
  std::ostringstream o;
  const int code = br::bsp_main(int(argv.size()), argv.data(), o);
  *out = o.str();
  return code;
}

bool has(const std::string& s, const std::string& part) {
  return s.find(part) != std::string::npos;
}

}  // namespace

TEST(info_and_ents_answer_for_a_file_and_a_search_path) {
  const fs::path root = fs::temp_directory_path() / "ge_brush_tool_test";
  fs::remove_all(root);
  const std::vector<uint8_t> map = fixture_map();
  put(root / "one.bsp", map);
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(ge::vfs::write_pak({{"maps/one.bsp", map}}, &pak, &error));
  put(root / "base" / "pak0.pak", pak);

  std::string out;
  CHECK(run({"info", (root / "one.bsp").string()}, &out) == 0);
  CHECK(has(out, "\"ok\":true") && has(out, "\"family\":\"29\"") && has(out, "\"clip_nodes\":"));
  CHECK(run({"info", "MAPS\\One.bsp", "--root=" + root.string(), "--game=base"}, &out) == 0);
  CHECK(has(out, "\"ok\":true"));
  CHECK(run({"ents", (root / "one.bsp").string()}, &out) == 0);
  CHECK(has(out, "\"classname\":\"worldspawn\""));
  CHECK(run({"ents", (root / "one.bsp").string(), "--class=none"}, &out) == 0);
  CHECK(has(out, "\"entities\":[]"));
  CHECK(run({"check", "--root=" + root.string(), "--game=base"}, &out) == 0);
  CHECK(has(out, "\"read\":1") && has(out, "\"refused\":0"));
}

TEST(check_names_a_damaged_map_and_usage_mistakes_exit_2) {
  const fs::path root = fs::temp_directory_path() / "ge_brush_tool_test_bad";
  fs::remove_all(root);
  std::vector<uint8_t> bad = fixture_map();
  bad.resize(bad.size() / 2);
  std::vector<uint8_t> pak;
  std::string error;
  CHECK(
      ge::vfs::write_pak({{"maps/good.bsp", fixture_map()}, {"maps/bad.bsp", bad}}, &pak, &error));
  put(root / "base" / "pak0.pak", pak);
  std::string out;
  CHECK(run({"check", "--root=" + root.string(), "--game=base"}, &out) == 1);
  CHECK(has(out, "\"read\":1") && has(out, "\"refused\":1") && has(out, "maps/bad.bsp"));
  CHECK(run({"help"}, &out) == 0 && has(out, "check"));
  CHECK(run({}, &out) == 2);
  CHECK(run({"spin"}, &out) == 2 && has(out, "unknown command"));
  CHECK(run({"check"}, &out) == 2);
  CHECK(run({"info", "x.bsp", "--bogus"}, &out) == 2);
  CHECK(run({"info", (root / "missing.bsp").string()}, &out) == 1 && has(out, "cannot open"));
}
