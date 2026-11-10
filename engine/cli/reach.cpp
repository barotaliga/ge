// See reach.hpp.
//
// ge:summary What a change reaches: git's changed files, Ninja's deps and the outputs above them
// ge:keywords cli change-reach ninja-deps ninja-query merge-base
// ge:skill agent-interface
// ge:test tests/cli_reach_test.cpp
#include "cli/reach.hpp"

#include <algorithm>
#include <map>
#include <system_error>

namespace ge::cli {

namespace {

std::string in_quotes(const fs::path& p) { return "\"" + p.string() + "\""; }

// `command`'s output lines, or none when it failed.
std::vector<std::string> lines_from(const std::string& command, const fs::path& log) {
  const Logged r = run_to_log(command, log);
  std::vector<std::string> out;
  if (r.code != 0) return out;
  for (const std::string_view l : split_lines(r.output)) {
    if (!l.empty()) out.emplace_back(l);
  }
  return out;
}

std::string relative(const fs::path& repo, const std::string& file) {
  std::error_code ec;
  const fs::path p = fs::weakly_canonical(fs::path(file), ec);
  const fs::path r = fs::weakly_canonical(repo, ec);
  const std::string ps = p.generic_string(), rs = r.generic_string() + "/";
  return ps.starts_with(rs) ? ps.substr(rs.size()) : fs::path(file).generic_string();
}

}  // namespace

void reach_from_deps(const fs::path& repo, const fs::path& build, std::string_view deps,
                     const std::set<std::string>& libraries, Reach& r) {
  struct Object {
    std::string target, source;
    std::vector<std::string> deps;
  };
  std::vector<Object> objects;
  std::error_code ec;
  const std::string prefix = fs::weakly_canonical(repo, ec).generic_string() + "/";
  const fs::path built = fs::weakly_canonical(build, ec);
  // The same few thousand headers recur in every object: each path outside the repository's
  // prefix is made canonical once (a syscall each), never once a line.
  std::map<std::string, std::string> seen;
  for (const std::string_view view : split_lines(deps)) {
    const std::string line(view);
    if (!line.starts_with("    ")) {
      const size_t colon = line.find(": #deps");
      const size_t cm = line.find("CMakeFiles/");
      const size_t dir = line.find(".dir/", cm == std::string::npos ? 0 : cm);
      if (colon == std::string::npos || cm == std::string::npos || dir == std::string::npos) {
        objects.push_back({});  // not an object of a target: its paths are skipped
        continue;
      }
      objects.push_back({line.substr(cm + 11, dir - cm - 11), "", {}});
      continue;
    }
    if (objects.empty() || objects.back().target.empty()) continue;
    Object& o = objects.back();
    // Ninja's paths are absolute, or relative to the build directory when it lies inside the
    // repository ("../../games/g/x.hpp"): either way the repository's prefix is cut off as text,
    // and a path that does not start with it is made canonical.
    const std::string raw = line.substr(4);
    const std::string path =
        fs::path(raw).is_absolute() ? raw : (built / raw).lexically_normal().generic_string();
    std::string dep;
    if (path.starts_with(prefix)) {
      dep = path.substr(prefix.size());
    } else {
      auto it = seen.find(path);
      if (it == seen.end()) it = seen.emplace(path, relative(repo, path)).first;
      dep = it->second;
    }
    if (o.source.empty()) o.source = dep;
    o.deps.push_back(std::move(dep));
  }
  auto dir_of = [](const std::string& p) {
    const size_t slash = p.rfind('/');
    return slash == std::string::npos ? std::string() : p.substr(0, slash);
  };
  auto stem_of = [](const std::string& p) {
    const size_t dot = p.rfind('.');
    return dot == std::string::npos ? p : p.substr(0, dot);
  };
  // What a reached library object passes on to the code that calls it: its own header (x.cpp's
  // x.hpp) when it includes one, else the headers of its own directory that it includes. Never
  // every header of the library's directories: one object of a shared library including a
  // changed header would then reach everything that draws or links through that library.
  auto exposed = [&](const Object& o) {
    std::vector<std::string> out;
    const std::string stem = stem_of(o.source), dir = dir_of(o.source);
    for (size_t k = 1; k < o.deps.size(); k++) {
      const std::string& d = o.deps[k];
      if ((d.ends_with(".hpp") || d.ends_with(".h")) && stem_of(d) == stem) return std::vector{d};
    }
    for (size_t k = 1; k < o.deps.size(); k++) {
      const std::string& d = o.deps[k];
      if ((d.ends_with(".hpp") || d.ends_with(".h")) && dir_of(d) == dir) out.push_back(d);
    }
    return out;
  };
  auto library = [&](const Object& o) {
    return libraries.empty() ? !o.target.ends_with("_test") : libraries.count(o.target) > 0;
  };
  // An object is reached when it compiles a changed file or a header a reached library object
  // passes on, until nothing more is reached.
  std::set<std::string> hit = r.changed;
  std::vector<bool> reached(objects.size(), false);
  for (bool grew = true; grew;) {
    grew = false;
    for (size_t i = 0; i < objects.size(); i++) {
      const Object& o = objects[i];
      if (o.target.empty() || reached[i]) continue;
      if (std::none_of(o.deps.begin(), o.deps.end(),
                       [&](const std::string& d) { return hit.count(d) > 0; })) {
        continue;
      }
      reached[i] = grew = true;
      r.objects.insert(o.source);
      r.targets.insert(o.target);
      if (std::any_of(o.deps.begin(), o.deps.end(),
                      [&](const std::string& d) { return r.changed.count(d) > 0; })) {
        r.sources.insert(o.source);
      }
      if (library(o)) {
        for (const std::string& h : exposed(o)) hit.insert(h);
      }
    }
  }
}

bool change_reach(const fs::path& repo, const fs::path& build, const std::string& base,
                  const fs::path& log, Reach& r) {
  const std::string git = "git -C " + in_quotes(repo) + " ";
  const auto merge_base = lines_from(git + "merge-base " + base + " HEAD", log);
  if (merge_base.empty()) return false;
  for (const std::string& f :
       lines_from(git + "diff --name-only --diff-filter=d " + merge_base[0], log)) {
    r.changed.insert(f);
  }
  for (const std::string& f : lines_from(git + "ls-files --others --exclude-standard", log)) {
    r.changed.insert(f);
  }
  // A module's, a plugin's or a game's own build file reaches its directory's code, not the tree.
  std::set<std::string> scopes;
  for (const std::string& f : r.changed) {
    if (const std::string dir = build_scope(f); !dir.empty()) scopes.insert(dir);
  }
  for (const std::string& dir : scopes) {
    for (const std::string& f : lines_from(git + "ls-files -- " + in_quotes(dir), log)) {
      if (f.ends_with(".cpp") || f.ends_with(".hpp") || f.ends_with(".h")) r.changed.insert(f);
    }
  }
  // A file the build turns into source (a mod's QC embedded as a generated .cpp, a font, a
  // shader) is in no object's deps: Ninja's own graph names what it generates, and that
  // generated source counts as changed too, so its object and the labs over it are reached.
  std::error_code canon;
  const fs::path built = fs::weakly_canonical(build, canon);
  const std::string prefix = fs::weakly_canonical(repo, canon).generic_string() + "/";
  std::vector<std::string> inputs;
  for (const std::string& f : r.changed) {
    if (generator_input(f)) inputs.push_back(f);
  }
  if (inputs.size() <= 200) {
    for (const std::string& f : inputs) {
      const Logged q = run_to_log(
          "ninja -C " + in_quotes(build) + " -t query " + in_quotes(fs::path(prefix + f)), log);
      if (q.code != 0) continue;  // not a file the build knows
      for (const std::string& g : generated_from(q.output, built, prefix)) r.changed.insert(g);
    }
  }
  for (const std::string& f : r.changed) {
    if (f.ends_with(".cpp")) r.sources.insert(f);
  }
  const Logged targets = run_to_log("ninja -C " + in_quotes(build) + " -t targets all", log);
  // Megabytes of deps: a log of their own, fresh each time, never appended to `log`.
  const fs::path dump = log.parent_path() / "ge-reach-deps.log";
  std::error_code ec;
  fs::remove(dump, ec);
  const Logged deps = run_to_log("ninja -C " + in_quotes(build) + " -t deps", dump);
  if (deps.code == 0) {
    reach_from_deps(repo, build, deps.output,
                    targets.code == 0 ? libraries_of(targets.output) : std::set<std::string>{}, r);
  }
  return true;
}

bool generator_input(std::string_view path) {
  static constexpr std::string_view kSource[] = {".cpp", ".hpp", ".h", ".c", ".md", ".txt"};
  for (const std::string_view ext : kSource) {
    if (path.ends_with(ext)) return false;
  }
  return !build_file(path) && !path.starts_with("docs/") && !path.starts_with(".");
}

std::set<std::string> generated_from(std::string_view query, const fs::path& build,
                                     const std::string& prefix) {
  std::set<std::string> out;
  bool outputs = false;
  for (const std::string_view line : split_lines(query)) {
    if (!line.starts_with("  ")) {
      outputs = false;  // the next node
    } else if (!line.starts_with("    ")) {
      outputs = line == "  outputs:";
    } else if (outputs) {
      const std::string raw(line.substr(4));
      const std::string path = fs::path(raw).is_absolute()
                                   ? fs::path(raw).lexically_normal().generic_string()
                                   : (build / raw).lexically_normal().generic_string();
      if (!path.starts_with(prefix)) continue;
      const std::string rel = path.substr(prefix.size());
      if (rel.ends_with(".cpp") || rel.ends_with(".hpp") || rel.ends_with(".h") ||
          rel.ends_with(".c")) {
        out.insert(rel);
      }
    }
  }
  return out;
}

std::set<std::string> libraries_of(std::string_view targets) {
  std::set<std::string> out;
  for (const std::string_view line : split_lines(targets)) {
    const size_t colon = line.find(": ");
    if (colon == std::string_view::npos) continue;
    const fs::path path(std::string(line.substr(0, colon)));
    const std::string ext = path.extension().string();
    if (ext != ".a" && ext != ".lib" && ext != ".so" && ext != ".dylib") continue;
    std::string name = path.stem().string();
    if (ext != ".lib" && name.starts_with("lib")) name = name.substr(3);
    out.insert(name);
  }
  return out;
}

bool build_file(std::string_view path) {
  const std::string_view name =
      path.substr(path.rfind('/') == std::string_view::npos ? 0 : path.rfind('/') + 1);
  return name == "CMakeLists.txt" || name == "CMakePresets.json" || name.ends_with(".cmake");
}

std::string build_scope(std::string_view path) {
  if (!build_file(path)) return "";
  const size_t slash = path.rfind('/');
  if (slash == std::string_view::npos) return "";
  const std::string_view dir = path.substr(0, slash);
  if (dir == "cmake" || dir.starts_with("cmake/") || dir == "tests") return "";
  return std::string(dir);
}

bool global_build_file(std::string_view path) {
  return build_file(path) && build_scope(path).empty();
}

std::vector<std::string> pick_affected(const std::vector<std::string>& tests,
                                       const std::set<std::string>& targets,
                                       const std::set<std::string>& changed) {
  for (const std::string& f : changed) {
    if (global_build_file(f)) return tests;
  }
  const bool labs = targets.count("labs") > 0 ||
                    std::any_of(changed.begin(), changed.end(), [](const std::string& f) {
                      return f.starts_with("tests/digests/");
                    });
  std::vector<std::string> out;
  for (const std::string& t : tests) {
    const bool smoke = t == "labs_smoke" || t.starts_with("labs_smoke_");
    std::string golden = t;
    if (golden.ends_with("_test")) golden.resize(golden.size() - 5);
    golden = std::string("tests/golden/").append(golden).append(".txt");
    if ((smoke && labs) || (!smoke && targets.count(t)) || changed.count(golden)) {
      out.push_back(t);
    }
  }
  return out;
}

}  // namespace ge::cli
