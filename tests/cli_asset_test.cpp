// engine/cli/asset.hpp's pure parts, held by the engine's own checks:
//
//   * THE LINE: a Python with bpy runs the builder as a script; Blender itself runs it in the
//     background with the system's PYTHONPATH; both pass the output path, and the data when
//     there is a set.
//   * THE OUTPUT READ BACK: the Blender version a builder names, the last traceback frame in the
//     builder rather than in Python's library, and `assets check`'s FAIL and ok lines as findings
//     and fresh names.
//
// ge:summary Tests for ge asset: the Blender command line, the version, tracebacks, check lines
// ge:keywords test cli ge asset blender bpy traceback check findings
// ge:skill agent-interface assets testing
#include <string>
#include <vector>

#include "check.hpp"
#include "cli/asset.hpp"

namespace c = ge::cli;

TEST(the_builder_runs_under_a_python_with_bpy_or_under_blender) {
  const std::string py = c::blender_line("python3.13", "a/build.py", "b/d.json", "b/m.glb");
  CHECK_MSG(py == "python3.13 \"a/build.py\" -- --out \"b/m.glb\" --data \"b/d.json\"", py);
  const std::string bl = c::blender_line("/opt/blender/blender", "a/build.py", "", "m.glb");
  CHECK_MSG(bl.find("--background") != std::string::npos &&
                bl.find("--python-use-system-env") != std::string::npos &&
                bl.find("--python \"a/build.py\" -- --out \"m.glb\"") != std::string::npos &&
                bl.find("--data") == std::string::npos,
            bl);
}

TEST(a_builders_output_says_its_blender_and_where_it_failed) {
  CHECK(c::blender_version("INFO\nge-asset: blender 5.2.2 LTS\nmore") == "5.2.2 LTS");
  CHECK(c::blender_version("no version here").empty());
  const std::string trace =
      "Traceback (most recent call last):\n"
      "  File \"/repo/plugins/skier/assets/skier/build.py\", line 12, in <module>\n"
      "  File \"/usr/lib/python3.13/json/decoder.py\", line 363, in decode\n"
      "json.decoder.JSONDecodeError: Expecting value\n";
  CHECK(c::python_error_where(trace) == "/repo/plugins/skier/assets/skier/build.py:12");
  CHECK(c::python_error_where("all fine").empty());
}

TEST(the_checks_lines_become_findings) {
  std::vector<std::string> fresh;
  const auto f = c::check_findings(
      "ok   skier: plugins/skier/assets/skier/skier.glb, 102 of 384 KiB, built by Blender 5.2.2\n"
      "FAIL sled: sled.glb is older than its builder build.py; run `ge asset build sled`\n"
      "FAIL manifest: x/asset.txt:3: unknown key\n",
      &fresh);
  CHECK(fresh.size() == 1 && fresh[0] == "skier");
  CHECK(f.size() == 2);
  CHECK(f[0].where == "sled" && f[0].next == "ge asset build sled");
  CHECK(f[0].what.starts_with("sled.glb is older"));
  CHECK(f[1].next.find("manifest") != std::string::npos);
}
