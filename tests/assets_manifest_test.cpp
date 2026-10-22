// engine/assets/manifest.hpp and data.hpp, held by the engine's own checks:
//
//   * THE MANIFEST: the four keys parse, comments and blank lines are skipped, and an unknown
//     key, a line that is not `key: value`, a budget that is not a number and a manifest with
//     no builder or budget each fail with the line and the fix.
//   * THE STAMP: a model stamped from its sources is fresh; a builder, a source or the data set
//     changed makes it stale and the problem names which; a model over its budget, a missing
//     model and a missing stamp are each a problem with `ge asset build` as the next action;
//     the hash ignores carriage returns.
//   * THE DATA: JsonOut writes nested objects and arrays with their commas, numbers in their
//     shortest form and strings escaped; a registered set is found by name.
//
// ge:summary Tests for built assets: the manifest, the stamp and its staleness, the budget, data
// ge:keywords test assets manifest stamp stale budget data jsonout ge-asset
// ge:skill assets testing
#include <fstream>
#include <string>

#include "assets/data.hpp"
#include "assets/manifest.hpp"
#include "check.hpp"

namespace {

namespace a = ge::assets;
namespace fs = std::filesystem;

void write(const fs::path& p, const std::string& text) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << text;
}

bool mentions(const std::vector<std::string>& problems, const std::string& word) {
  for (const std::string& p : problems) {
    if (p.find(word) != std::string::npos) return true;
  }
  return false;
}

std::string one_set() { return "{\"n\":1}"; }
const a::DataSpec kSet{"test-set", "a set for the test", one_set};
GE_ASSET_DATA(kSet);

}  // namespace

TEST(a_manifest_parses_its_keys_and_refuses_what_it_does_not_know) {
  const a::ManifestRead ok = a::parse_manifest(
      "# a model\nbuilder: build.py\n\ndata: skier   # the rig\nsource: ../rig.hpp\n"
      "source: lib.py\nbudget-kib: 64\n",
      fs::path("x") / "skier");
  CHECK_MSG(ok.ok, ok.error);
  CHECK(ok.manifest.name == "skier" && ok.manifest.builder == "build.py");
  CHECK(ok.manifest.data == "skier" && ok.manifest.sources.size() == 2);
  CHECK(ok.manifest.budget_bytes == uint64_t{64} * 1024);
  CHECK(ok.manifest.model_path().filename() == "skier.glb");
  // A CRLF checkout (git on Windows) reads the same.
  const a::ManifestRead crlf =
      a::parse_manifest("builder: build.py\r\ndata: skier\r\nbudget-kib: 64\r\n", "skier");
  CHECK_MSG(crlf.ok, crlf.error);
  CHECK(crlf.manifest.builder == "build.py" && crlf.manifest.budget_bytes == uint64_t{64} * 1024);
  for (const char* bad :
       {"builder: b.py\nbudget-kib: 1\ncolour: red\n", "builder: b.py\nbudget-kib: 1\njust words\n",
        "builder: b.py\nbudget-kib: lots\n", "budget-kib: 4\n", "builder: b.py\n"}) {
    const a::ManifestRead r = a::parse_manifest(bad, "m");
    CHECK_MSG(!r.ok && r.error.find("asset.txt") != std::string::npos,
              std::string("should refuse: ") + bad);
  }
}

TEST(a_stamped_model_is_fresh_until_a_source_moves) {
  const fs::path dir = fs::temp_directory_path() / "ge_assets_manifest_test" / "thing";
  fs::remove_all(dir.parent_path());
  write(dir / "asset.txt", "builder: build.py\nsource: part.txt\ndata: test-set\nbudget-kib: 1\n");
  write(dir / "build.py", "print('model')\n");
  write(dir / "part.txt", "one\n");
  const a::ManifestRead r = a::read_manifest(dir / "asset.txt");
  CHECK_MSG(r.ok, r.error);
  const a::AssetManifest& m = r.manifest;
  CHECK(a::find_manifests(dir.parent_path()).size() == 1);

  CHECK(mentions(a::check_asset(m, "{}").problems, "missing"));
  write(m.model_path(), std::string(100, 'g'));
  CHECK(mentions(a::check_asset(m, "{}").problems, "stamp"));

  std::vector<a::StampLine> lines;
  std::string error;
  CHECK(a::stamp_lines(m, "{\"n\":1}", lines, error));
  CHECK(lines.size() == 3 && lines[0].kind == "builder" && lines[2].kind == "data");
  write(m.stamp_path(), a::stamp_text(m, lines, "5.2.2"));
  const a::AssetCheck fresh = a::check_asset(m, "{\"n\":1}");
  CHECK_MSG(fresh.problems.empty(), fresh.problems.empty() ? "" : fresh.problems[0]);
  CHECK(fresh.blender == "5.2.2" && fresh.bytes == 100);

  CHECK(mentions(a::check_asset(m, "{\"n\":2}").problems, "data test-set"));
  write(dir / "part.txt", "one\r\n");  // a checkout's line endings are not a change
  CHECK(a::check_asset(m, "{\"n\":1}").problems.empty());
  write(dir / "part.txt", "two\n");
  const a::AssetCheck stale = a::check_asset(m, "{\"n\":1}");
  CHECK(stale.problems.size() == 1 && mentions(stale.problems, "source part.txt") &&
        mentions(stale.problems, "ge asset build thing"));
  write(dir / "part.txt", "one\n");
  write(m.model_path(), std::string(2000, 'g'));
  CHECK(mentions(a::check_asset(m, "{\"n\":1}").problems, "over its budget"));
  fs::remove_all(dir.parent_path());
}

TEST(json_out_writes_what_a_builder_reads) {
  a::JsonOut j;
  j.begin_object().field("name", "a \"b\"\n").key("xs").begin_array().num(0.1).num(-2).num(1e21);
  j.end_array().key("in").begin_object().field("k", 1.5).key("on").boolean(true).end_object();
  j.end_object();
  CHECK_MSG(j.text() ==
                "{\"name\":\"a \\\"b\\\"\\u000a\",\"xs\":[0.1,-2,1e+21],"
                "\"in\":{\"k\":1.5,\"on\":true}}",
            j.text());
  const a::DataSpec* set = a::find_data("test-set");
  CHECK(set && set->make() == "{\"n\":1}");
  CHECK(a::find_data("no-such-set") == nullptr);
}
