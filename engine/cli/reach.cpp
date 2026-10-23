// See reach.hpp.
//
// ge:summary What a change reaches: git's changed files, Ninja's deps and the outputs above them
// ge:keywords cli change-reach ninja-deps ninja-query merge-base
// ge:skill agent-interface
// ge:test tests/cli_reach_test.cpp
#include "cli/reach.hpp"

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

void reach_from_deps(const fs::path& repo, std::string_view deps,
                     const std::set<std::string>& libraries, Reach& r) {
  struct Object {
    std::string target, source;
    std::vector<std::string> deps;
  };
  std::vector<Object> objects;
  std::error_code ec;
  const std::string prefix = fs::weakly_canonical(repo, ec).generic_string() + "/";
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
    // Ninja's paths are absolute: the repository's prefix cut off as text, canonical only when not.
    const std::string path = line.substr(4);
    std::string dep = path.starts_with(prefix) ? path.substr(prefix.size()) : relative(repo, path);
    if (o.source.empty()) o.source = dep;
    o.deps.push_back(std::move(dep));
  }
  auto dir_of = [](const std::string& p) {
    const size_t slash = p.rfind('/');
    return slash == std::string::npos ? std::string() : p.substr(0, slash);
  };
  // Which directories each target compiles sources in.
  std::map<std::string, std::set<std::string>> owners;  // directory -> targets
  for (const Object& o : objects) {
    const bool owns = libraries.empty() ? !o.target.ends_with("_test") : libraries.count(o.target);
    if (!o.target.empty() && owns) owners[dir_of(o.source)].insert(o.target);
  }
  for (const Object& o : objects) {
    for (const std::string& d : o.deps) {
      if (!r.changed.count(d)) continue;
      r.targets.insert(o.target);
      r.sources.insert(o.source);
      break;
    }
  }
  r.targets.erase("");
  // Through the headers of reached targets' directories, until nothing more is reached.
  for (bool grew = true; grew;) {
    grew = false;
    std::set<std::string> dirs;
    for (const auto& [dir, targets] : owners) {
      for (const std::string& t : targets) {
        if (r.targets.count(t)) dirs.insert(dir);
      }
    }
    for (const Object& o : objects) {
      if (o.target.empty() || r.targets.count(o.target)) continue;
      for (size_t k = 1; k < o.deps.size(); k++) {
        const std::string& d = o.deps[k];
        if (!d.ends_with(".hpp") && !d.ends_with(".h")) continue;
        if (!dirs.count(dir_of(d))) continue;
        r.targets.insert(o.target);
        grew = true;
        break;
      }
    }
  }
  // Each object on its own: the same two ways in, now that the reached libraries are known.
  std::set<std::string> dirs;
  for (const auto& [dir, targets] : owners) {
    for (const std::string& t : targets) {
      if (r.targets.count(t)) dirs.insert(dir);
    }
  }
  for (const Object& o : objects) {
    if (o.target.empty()) continue;
    for (size_t k = 0; k < o.deps.size(); k++) {
      const std::string& d = o.deps[k];
      const bool header = k > 0 && (d.ends_with(".hpp") || d.ends_with(".h"));
      if (r.changed.count(d) || (header && dirs.count(dir_of(d)))) {
        r.objects.insert(o.source);
        break;
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
    reach_from_deps(repo, deps.output,
                    targets.code == 0 ? libraries_of(targets.output) : std::set<std::string>{}, r);
  }
  return true;
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

std::vector<std::string> pick_affected(const std::vector<std::string>& tests,
                                       const std::set<std::string>& targets,
                                       const std::set<std::string>& changed) {
  for (const std::string& f : changed) {
    if (build_file(f)) return tests;
  }
  const bool labs = targets.count("labs") > 0 || changed.count("tests/labs_digests.txt") > 0;
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
