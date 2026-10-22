// `ge asset`: the game's models built in headless Blender by their Python builders, off the
// game's own data, stamped and held to their size budgets (engine/assets/manifest.hpp; skill:
// assets). Three commands:
//
//   ge asset list               every asset in the repo, its budget and whether it is fresh
//   ge asset build [<name>...]  build these (none: every asset whose model is stale or missing;
//                               --force: every asset), each one in four steps: the repo's build,
//                               `assets data <set>` into build/<preset>/assets.d/<name>.json,
//                               the builder under Blender with the engine's ge_asset.py on its
//                               path, then `assets stamp` and the check of the new model
//   ge asset check              every model against its stamp and its budget (the `assets_check`
//                               test's check): each stale, missing or oversized model a finding
//
// THE BLENDER: `--blender=<cmd>`, else the GE_BLENDER environment variable, else `blender` on the
// PATH, else a `python3` that can `import bpy`. A command whose name starts with "python" is
// taken as a Python with the bpy module (`pip install bpy`), run as `<cmd> <builder> -- ...`;
// anything else as Blender itself, run `--background --factory-startup --python-use-system-env
// --python <builder> -- ...`. Either way the builder's PYTHONPATH starts with ge_asset.py's
// directory, written from the copy compiled into this ge. A model is written to a temporary file
// first and moved over the committed one only when the builder succeeds.
//
// ge:summary ge asset list, build and check: headless Blender builders, the stamp, the budget
// ge:keywords cli ge asset asset-build asset-check blender bpy builder ge_asset stamp budget
// ge:keywords model glb ge_blender run_asset_list run_asset_build run_asset_check
// ge:skill agent-interface assets
// ge:test tests/cli_asset_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/build.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

struct AssetOptions {
  std::vector<std::string> names;  // none: every stale one (or every one, with force)
  bool force = false;
  std::string blender;  // --blender
};

Outcome run_asset_list(const BuildOptions& o);
Outcome run_asset_build(const BuildOptions& o, const AssetOptions& a);
Outcome run_asset_check(const BuildOptions& o);

// Exposed for the tests.
// The command line that runs `builder` under `blender` (a Blender, or a Python with bpy).
std::string blender_line(const std::string& blender, const fs::path& builder, const fs::path& data,
                         const fs::path& out);
// The Blender version a builder's output names ("ge-asset: blender 5.2.2 LTS"), or "".
std::string blender_version(std::string_view output);
// A Python traceback's last frame in a builder's output: "<file>:<line>", or "".
std::string python_error_where(std::string_view output);
// `assets check` output: one finding a FAIL line, its asset named; `fresh` gets each ok asset.
std::vector<Finding> check_findings(std::string_view output, std::vector<std::string>* fresh);

}  // namespace ge::cli
