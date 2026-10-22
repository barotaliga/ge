// The definitions behind doctor.hpp: each check, and the walk over a repo's files.
//
// ge:summary ge doctor's checks: guidance links, the pin, build flags, sources, diagnostics
// ge:keywords cli ge doctor read_pin check_diagnostics_in walk ge-tags libm clock fast-math
// ge:skill agent-interface
// ge:test tests/cli_doctor_test.cpp
#include "cli/doctor.hpp"

#include <algorithm>
#include <regex>
#include <sstream>
#include <system_error>

#include "cli/diagnostic.hpp"
#include "cli/embedded.hpp"
#include "cli/scaffold.hpp"
#include "cli/skills.hpp"

namespace ge::cli {

namespace {

constexpr int kLineCap = 1000;
constexpr int kTagLines = 60;  // a file's ge: block sits in its first 60 lines

// Build output, dependencies and pictures: never the repo's own text.
bool skipped_dir(const std::string& name) {
  return name == ".git" || name.starts_with("build") || name == "_deps" || name == "previews" ||
         name == ".cache" || name == "stage" || name == "node_modules";
}

struct RepoFiles {
  std::vector<fs::path> sources;  // .cpp .hpp .h
  std::vector<fs::path> cmake;    // CMakeLists.txt, *.cmake
};

RepoFiles walk(const fs::path& repo) {
  RepoFiles files;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(repo, ec); !ec && it != fs::end(it);
       it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (it->is_symlink(ec)) {
      if (it->is_directory(ec)) it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory(ec)) {
      if (skipped_dir(name)) it.disable_recursion_pending();
      continue;
    }
    const std::string ext = it->path().extension().string();
    if (ext == ".cpp" || ext == ".hpp" || ext == ".h") files.sources.push_back(it->path());
    if (name == "CMakeLists.txt" || ext == ".cmake") files.cmake.push_back(it->path());
  }
  std::sort(files.sources.begin(), files.sources.end());
  std::sort(files.cmake.begin(), files.cmake.end());
  return files;
}

// One of the guidance's links: a symlink, onto its target as written.
void check_link(const fs::path& repo, const Link& link, std::vector<Finding>& out) {
  std::error_code ec;
  const fs::path path = repo / link.path;
  const std::string name(link.path);
  const std::string want(link.target);
  if (!fs::is_symlink(fs::symlink_status(path, ec))) {
    out.push_back({Severity::error, name + " is not a symlink to " + want, name,
                   "replace it: rm -rf " + name + " && ln -s " + want + " " + name});
    return;
  }
  const std::string got = fs::read_symlink(path, ec).generic_string();
  if (got != want) {
    out.push_back({Severity::error, name + " points at " + got + ", not " + want, name,
                   "relink it: ln -sfn " + want + " " + name});
  }
}

void check_guidance(const fs::path& repo, std::vector<Finding>& out) {
  std::error_code ec;
  if (!fs::is_regular_file(fs::symlink_status(repo / "AGENTS.md", ec))) {
    out.push_back({Severity::error, "AGENTS.md is missing or is not a regular file", "AGENTS.md",
                   "restore the router: it is the one guidance file, and the tools' files link "
                   "to it (`ge new` writes one to copy from)"});
  }
  for (const Link& link : kLinks) check_link(repo, link, out);
}

void check_pin(const fs::path& repo, std::vector<Finding>& out) {
  const auto pin = read_pin(repo);
  if (!pin) {
    out.push_back({Severity::error, "no engine pin (set(GE_ENGINE_VERSION X.Y.Z))",
                   std::string(kPinFile),
                   "restore cmake/engine.cmake from a fresh `ge new` (skill: engine)"});
    return;
  }
  if (pin->version != engine_version()) {
    out.push_back(
        {Severity::error,
         "the repo pins engine " + pin->version + "; this ge is " + std::string(engine_version()),
         std::string(kPinFile),
         "run ge through `make ge ARGS=...` (or `make agents-check`), which builds the "
         "ge of the pinned engine; to move the pin, skill: engine, § Upgrade"});
  }
  if (pin->sha256.empty()) {
    out.push_back({Severity::note,
                   "the engine pin carries no checksum, so the download is not verified",
                   std::string(kPinFile),
                   "set GE_ENGINE_SHA256 to the -src.tar.gz line of " + std::string(kEngineRepo) +
                       "/releases/download/v" + pin->version + "/SHA256SUMS"});
  }
}

void check_build(const fs::path& repo, const std::vector<fs::path>& cmake,
                 std::vector<Finding>& out) {
  static const std::regex kFast(
      R"((-ffast-math|-Ofast|-ffp-contract=(fast|on)|-funsafe-math-optimizations|)"
      R"(-fassociative-math|-ffinite-math-only|/fp:fast))");
  bool links_core = false;
  for (const fs::path& file : cmake) {
    const std::string text = read_text(file).value_or("");
    links_core = links_core || text.find("ge::core") != std::string::npos;
    const auto lines = split_lines(text);
    for (size_t i = 0; i < lines.size(); i++) {
      std::string line(lines[i]);
      if (const size_t hash = line.find('#'); hash != std::string::npos) line.resize(hash);
      std::smatch m;
      if (std::regex_search(line, m, kFast)) {
        out.push_back({Severity::error,
                       "the build flag " + m.str() +
                           " lets the compiler change a simulation's "
                           "bits",
                       rel_path(repo, file) + ":" + std::to_string(i + 1),
                       "remove it: ge::core carries the engine's FP flags (ge::fp) to every "
                       "target that links it; skill: engine, § Determinism"});
      }
    }
  }
  if (!links_core) {
    out.push_back({Severity::error, "no CMake file links ge::core", "CMakeLists.txt",
                   "link ge::core into the game's simulation: it carries the FP flags every "
                   "deterministic target needs; skill: engine"});
  }
}

// The text with every // comment cut, line by line (string literals with '//' in them are rare
// in simulation code, and only make the check stricter).
std::string strip_line_comment(std::string_view line) {
  const size_t at = line.find("//");
  return std::string(at == std::string_view::npos ? line : line.substr(0, at));
}

void check_source(const fs::path& repo, const fs::path& file, std::vector<Finding>& out) {
  static const std::regex kTag(R"(^\s*(//|#)\s*ge:([a-z]+)\s?(.*)$)");
  static const std::regex kKeyword(R"(^[a-z0-9][a-z0-9_-]*$)");
  static const std::regex kLibm(
      R"((^|[^A-Za-z0-9_:.>])(std::|::)?(sin|cos|tan|asin|acos|atan|atan2|sinh|cosh|tanh|asinh|)"
      R"(acosh|atanh|exp|exp2|expm1|log|log2|log10|log1p|pow|cbrt|erf|erfc|tgamma|lgamma)[fl]?)"
      R"(\s*\(|std::hypot[fl]?\s*\()");
  static const std::regex kClock(
      R"(std::chrono|\bstd::time\s*\(|(^|[^A-Za-z0-9_:.>])time\s*\(|\brand\s*\(|<random>|)"
      R"(random_device)");
  const std::string rel = rel_path(repo, file);
  const std::string text = read_text(file).value_or("");
  const auto lines = split_lines(text);

  if (static_cast<int>(lines.size()) > kLineCap &&
      text.find("agents:allow-large-file") == std::string::npos) {
    out.push_back(
        {Severity::error,
         std::to_string(lines.size()) + " lines, over the cap of " + std::to_string(kLineCap), rel,
         "split it by subject (skill: write-code); an honest exception carries an "
         "`agents:allow-large-file: <reason>` comment"});
  }

  std::string summary, keywords, skills, tests;
  for (size_t i = 0; i < lines.size() && i < kTagLines; i++) {
    std::smatch m;
    const std::string line(lines[i]);
    if (!std::regex_match(line, m, kTag)) continue;
    const std::string tag = m[2], value = m[3];
    std::string* into = tag == "summary"    ? &summary
                        : tag == "keywords" ? &keywords
                        : tag == "skill"    ? &skills
                        : tag == "test"     ? &tests
                                            : nullptr;
    if (!into) {
      out.push_back({Severity::error, "unknown tag ge:" + tag, rel + ":" + std::to_string(i + 1),
                     "use ge:summary, ge:keywords, ge:skill and ge:test; skill: write-code"});
      continue;
    }
    *into += (into->empty() ? "" : " ") + value;
  }
  const std::string tag_next =
      "end the file's leading comment with its ge: block (ge:summary, ge:keywords, ge:skill, "
      "ge:test); skill: write-code, § Discovery tags";
  for (const auto& [tag, value] : {std::pair{"summary", &summary}, std::pair{"keywords", &keywords},
                                   std::pair{"skill", &skills}}) {
    if (value->empty()) {
      out.push_back({Severity::error, std::string("no ge:") + tag + " tag", rel, tag_next});
    }
  }
  auto words = [](const std::string& s) {
    std::vector<std::string> w;
    std::istringstream in(s);
    for (std::string x; in >> x;) w.push_back(x);
    return w;
  };
  for (const std::string& k : words(keywords)) {
    if (!std::regex_match(k, kKeyword)) {
      out.push_back({Severity::error, "ge:keywords \"" + k + "\" is not lower case", rel,
                     "spell keywords as lower-case kebab or snake case concepts"});
    }
  }
  std::error_code ec;
  for (const std::string& s : words(skills)) {
    if (!fs::is_directory(repo / kSkillsDir / s, ec)) {
      out.push_back({Severity::error, "ge:skill names \"" + s + "\", which is not a skill", rel,
                     "name a directory of .agents/skills/, or write that skill"});
    }
  }
  for (const std::string& t : words(tests)) {
    if (!fs::exists(repo / t, ec)) {
      out.push_back({Severity::error, "ge:test names " + t + ", which does not exist", rel,
                     "point it at the test that holds this file, or drop the tag"});
    }
  }

  const bool sim = rel.starts_with("sim/");
  if (sim || rel.starts_with("labs/")) {
    for (size_t i = 0; i < lines.size(); i++) {
      const std::string code = strip_line_comment(lines[i]);
      std::smatch m;
      if (std::regex_search(code, m, kLibm)) {
        out.push_back({Severity::error,
                       "a platform transcendental in simulation code, whose bits differ between "
                       "platforms",
                       rel + ":" + std::to_string(i + 1),
                       "call the same function from core/libm/libm.hpp (ge::core::libm); "
                       "skill: engine, § Determinism"});
      }
      if (sim && std::regex_search(code, m, kClock)) {
        out.push_back(
            {Severity::error,
             "the simulation reads a clock or an unseeded random source (" + m.str() + ")",
             rel + ":" + std::to_string(i + 1),
             "time is the step count and every draw comes from the state's "
             "ge::core::Rng; skill: sim-system"});
      }
    }
  }
}

}  // namespace

std::optional<EnginePin> read_pin(const fs::path& repo) {
  const auto text = read_text(repo / kPinFile);
  if (!text) return std::nullopt;
  static const std::regex kVersion(
      R"re(set\(GE_ENGINE_VERSION\s+"?([0-9]+\.[0-9]+\.[0-9]+)"?\s*\))re");
  static const std::regex kSha(R"re(set\(GE_ENGINE_SHA256\s+"([0-9a-f]*)"\s*\))re");
  std::smatch m;
  if (!std::regex_search(*text, m, kVersion)) return std::nullopt;
  EnginePin pin{m[1], ""};
  if (std::regex_search(*text, m, kSha)) pin.sha256 = m[1];
  return pin;
}

std::vector<Finding> check_diagnostics_in(const fs::path& repo, const fs::path& file) {
  std::vector<Finding> out;
  std::string name = file.filename().string();
  if (name.ends_with(".in")) name.resize(name.size() - 3);
  const bool cmake = name == "CMakeLists.txt" || name.ends_with(".cmake");
  const auto text = read_text(file);
  if (!text) return out;
  for (const DiagnosticSite& site :
       diagnostic_sites(*text, cmake ? SourceKind::cmake : SourceKind::cpp)) {
    const std::string problem = prompt_problem(site.text);
    if (problem.empty()) continue;
    out.push_back({Severity::error, site.call + "'s message " + problem,
                   rel_path(repo, file) + ":" + std::to_string(site.line),
                   "rewrite it as what failed (with the values), where, and the next action; "
                   "skill: write-code, § Diagnostics"});
  }
  return out;
}

std::vector<Finding> doctor(const fs::path& repo) {
  std::vector<Finding> out;
  check_guidance(repo, out);
  for (Finding& f : check_skills(repo)) out.push_back(std::move(f));
  for (Finding& f : check_engine_skills(repo)) out.push_back(std::move(f));
  check_pin(repo, out);
  const RepoFiles files = walk(repo);
  check_build(repo, files.cmake, out);
  for (const fs::path& file : files.sources) check_source(repo, file, out);
  for (const auto* list : {&files.sources, &files.cmake}) {
    for (const fs::path& file : *list) {
      for (Finding& f : check_diagnostics_in(repo, file)) out.push_back(std::move(f));
    }
  }
  return out;
}

}  // namespace ge::cli
