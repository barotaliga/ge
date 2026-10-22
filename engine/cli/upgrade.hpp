// `ge upgrade`: A GAME REPO MOVED ONTO THIS ge's ENGINE RELEASE. Run with the ge of the release to
// move to (a release's `ge`, or `make ge` in an engine checkout), it rewrites the pin
// (cmake/engine.cmake: the version, and the checksum given or cleared) and syncs the engine's
// skills to the release (`ge skills sync`), then holds the repo to `ge doctor`. It refuses to move
// a repo onto an older release than it pins. The build, the tests and the labs' before-and-after
// are the agent's next steps (the `engine` consumer skill, § Upgrade); a ge run in a repo that
// pins another release hands every other command to the pinned ge, but not this one.
//
// ge:summary ge upgrade: the repo's engine pin moved to this ge's release, its skills synced
// ge:keywords cli ge upgrade engine-pin release version skills sync downgrade checksum
// ge:skill agent-interface
// ge:test tests/cli_scaffold_test.cpp
#pragma once

#include <string>
#include <vector>

#include "cli/finding.hpp"

namespace ge::cli {

struct UpgradeResult {
  std::string from;                  // the release the repo pinned ("" when it pinned none)
  std::string to;                    // this ge's release
  std::vector<std::string> written;  // repository-relative paths
  std::vector<Finding> findings;     // any error means the pin did not move
};

// `sha256`: the new release's -src.tar.gz checksum, or "" to pin none (ge doctor notes it).
UpgradeResult upgrade(const fs::path& repo, const std::string& sha256);

// "1.10.0" against "1.9.2": -1, 0 or 1; a part that is not a number reads as 0.
int compare_versions(const std::string& a, const std::string& b);

}  // namespace ge::cli
