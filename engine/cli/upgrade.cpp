// `ge upgrade` (upgrade.hpp): the pin rewritten, the skills synced, the repo doctored.
//
// ge:summary ge upgrade's definition: the version compare, the pin rewrite, the sync and doctor
// ge:keywords cli ge upgrade engine-pin compare_versions rewrite regex
// ge:skill agent-interface
// ge:test tests/cli_scaffold_test.cpp
#include "cli/upgrade.hpp"

#include <regex>
#include <sstream>

#include "cli/doctor.hpp"
#include "cli/embedded.hpp"
#include "cli/scaffold.hpp"
#include "cli/skills.hpp"

namespace ge::cli {

int compare_versions(const std::string& a, const std::string& b) {
  std::istringstream sa(a), sb(b);
  for (int part = 0; part < 3; part++) {
    std::string pa, pb;
    std::getline(sa, pa, '.');
    std::getline(sb, pb, '.');
    const long na = std::strtol(pa.c_str(), nullptr, 10);
    const long nb = std::strtol(pb.c_str(), nullptr, 10);
    if (na != nb) return na < nb ? -1 : 1;
  }
  return 0;
}

UpgradeResult upgrade(const fs::path& repo, const std::string& sha256) {
  UpgradeResult r;
  r.to = std::string(engine_version());
  const std::string pin_file(kPinFile);
  const auto pin = read_pin(repo);
  const auto text = read_text(repo / kPinFile);
  if (!pin || !text) {
    r.findings.push_back({Severity::error, "no engine pin (set(GE_ENGINE_VERSION X.Y.Z))", pin_file,
                          "run ge upgrade in a game repo; restore cmake/engine.cmake from a "
                          "fresh `ge new` if it is gone (skill: engine)"});
    return r;
  }
  r.from = pin->version;
  if (!sha256.empty() && !std::regex_match(sha256, std::regex("^[0-9a-f]{64}$"))) {
    r.findings.push_back({Severity::error, "--engine-sha256 is not 64 lower-case hex digits",
                          pin_file,
                          "copy the -src.tar.gz line's checksum from the release's SHA256SUMS"});
    return r;
  }
  if (compare_versions(r.to, r.from) < 0) {
    r.findings.push_back(
        {Severity::error,
         "the repo pins engine " + r.from + ", newer than this ge's " + r.to +
             "; ge upgrade never moves a repo back",
         pin_file,
         "run the ge of release " + r.from + " or later (`make ge` in the engine checkout)"});
    return r;
  }
  std::string next = std::regex_replace(
      *text, std::regex(R"re(set\(GE_ENGINE_VERSION\s+"?[0-9]+\.[0-9]+\.[0-9]+"?\s*\))re"),
      "set(GE_ENGINE_VERSION " + r.to + ")");
  next = std::regex_replace(next, std::regex(R"re(set\(GE_ENGINE_SHA256\s+"[0-9a-f]*"\s*\))re"),
                            "set(GE_ENGINE_SHA256 \"" + sha256 + "\")");
  if (next != *text) {
    std::string error;
    if (!write_text(repo / kPinFile, next, error)) {
      r.findings.push_back(
          {Severity::error, error, pin_file, "check that the file is writable, then rerun"});
      return r;
    }
    r.written.push_back(pin_file);
  }
  SyncResult sync = sync_skills(repo, false);
  r.written.insert(r.written.end(), sync.written.begin(), sync.written.end());
  r.findings.insert(r.findings.end(), sync.findings.begin(), sync.findings.end());
  for (const Finding& f : doctor(repo)) r.findings.push_back(f);
  return r;
}

}  // namespace ge::cli
