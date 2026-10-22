// ge asset list, build and check over the repo's `assets` executable and a headless Blender.
//
// ge:summary ge asset's commands: the manifests, the Blender run, the stamp and the check
// ge:keywords cli ge asset blender bpy builder stamp check list
// ge:skill agent-interface assets
// ge:test tests/cli_asset_test.cpp
#include "cli/asset.hpp"

#include <algorithm>
#include <cstdlib>
#include <regex>

#include "assets/manifest.hpp"
#include "cli/embedded.hpp"
#include "cli/json.hpp"

namespace ge::cli {

namespace {

constexpr std::string_view kHelper = "engine/assets/blender/ge_asset.py";

std::string quote(const fs::path& p) { return "\"" + p.string() + "\""; }

fs::path assets_exe(const BuildOptions& o) {
  fs::path exe = o.repo / "build" / o.preset / "assets";
#ifdef _WIN32
  exe += ".exe";
#endif
  return exe;
}

struct Repo {
  std::vector<assets::AssetManifest> assets;
  std::vector<Finding> findings;  // a manifest that does not parse
};

Repo repo_assets(const fs::path& repo) {
  Repo r;
  for (const fs::path& file : assets::find_manifests(repo)) {
    assets::ManifestRead m = assets::read_manifest(file);
    if (m.ok) {
      r.assets.push_back(std::move(m.manifest));
    } else {
      r.findings.push_back({Severity::error, m.error, rel_path(repo, file),
                            "fix the manifest (engine/assets/manifest.hpp); skill: assets"});
    }
  }
  return r;
}

// The repo's build and its `assets` executable, or false with the findings.
bool ready(const BuildOptions& o, Outcome& out) {
  if (!build_into(o, out)) return false;
  std::error_code ec;
  if (!fs::is_regular_file(assets_exe(o), ec)) {
    out.findings.push_back({Severity::error,
                            "this build made no `assets` executable, which holds the game's "
                            "data sets and checks its models",
                            rel_path(o.repo, assets_exe(o)),
                            "link one over ge::assets (engine/assets/tool.hpp); skill: assets"});
    return false;
  }
  return true;
}

// An environment variable's value, or "".
std::string env_of(const char* name) {
#ifdef _MSC_VER
  char* v = nullptr;
  size_t n = 0;
  std::string out = _dupenv_s(&v, &n, name) == 0 && v ? v : "";
  std::free(v);
  return out;
#else
  const char* v = std::getenv(name);  // NOLINT(concurrency-mt-unsafe)
  return v ? v : "";
#endif
}

// The Blender to build with: the flag, GE_BLENDER, `blender` on the PATH, a python3 with bpy.
std::string find_blender(const std::string& flag) {
  if (!flag.empty()) return flag;
  if (std::string env = env_of("GE_BLENDER"); !env.empty()) return env;
  if (run_quiet("blender --version") == 0) return "blender";
  if (run_quiet("python3 -c \"import bpy\"") == 0) return "python3";
  return "";
}

bool is_python(const std::string& cmd) {
  const std::string name = fs::path(cmd).filename().string();
  return name.rfind("python", 0) == 0;
}

void prepend_python_path(const fs::path& dir) {
#ifdef _WIN32
  const char sep = ';';
#else
  const char sep = ':';
#endif
  std::string value = dir.string();
  if (const std::string old = env_of("PYTHONPATH"); !old.empty()) {
    value += sep;
    value += old;
  }
#ifdef _WIN32
  _putenv_s("PYTHONPATH", value.c_str());
#else
  setenv("PYTHONPATH", value.c_str(), 1);  // NOLINT(concurrency-mt-unsafe)
#endif
}

// A traceback's "<file>:<line>" with the file relative to the repo.
std::string in_repo(const fs::path& repo, const std::string& at) {
  const size_t colon = at.rfind(':');
  return rel_path(repo, fs::path(at.substr(0, colon))) + at.substr(colon);
}

const assets::AssetManifest* named(const Repo& r, const std::string& name) {
  for (const assets::AssetManifest& m : r.assets) {
    if (m.name == name) return &m;
  }
  return nullptr;
}

std::string names_of(const Repo& r) {
  std::string all;
  for (const assets::AssetManifest& m : r.assets) all += (all.empty() ? "" : ", ") + m.name;
  return all.empty() ? "none" : all;
}

// Every asset's check: its findings, and the names of the fresh ones.
struct Checked {
  std::vector<Finding> findings;
  std::vector<std::string> fresh;
  bool ran = false;
};

Checked check_all(const BuildOptions& o, const Repo& r, const std::vector<std::string>& only) {
  Checked c;
  std::string cmd = quote(assets_exe(o)) + " check --root=" + quote(o.repo);
  for (const std::string& n : only) cmd += " " + n;
  const Logged l = run_to_log(cmd, build_log(o, "ge-asset"));
  if (l.code != 0 && l.code != 1) return c;
  c.ran = true;
  c.findings = check_findings(l.output, &c.fresh);
  for (Finding& f : c.findings) {
    if (const assets::AssetManifest* m = named(r, f.where)) f.where = rel_path(o.repo, m->dir);
  }
  return c;
}

}  // namespace

std::string blender_line(const std::string& blender, const fs::path& builder, const fs::path& data,
                         const fs::path& out) {
  std::string args = " -- --out " + quote(out);
  if (!data.empty()) args += " --data " + quote(data);
  const std::string exe = blender.find(' ') == std::string::npos ? blender : "\"" + blender + "\"";
  if (is_python(blender)) return exe + " " + quote(builder) + args;
  return exe + " --background --factory-startup --python-use-system-env --python " +
         quote(builder) + args;
}

std::string blender_version(std::string_view output) {
  static const std::regex kVersion("ge-asset: blender ([0-9][^\\r\\n]*)");
  const std::string text(output);
  std::smatch m;
  if (!std::regex_search(text, m, kVersion)) return "";
  std::string v = m[1].str();
  while (!v.empty() && v.back() == ' ') v.pop_back();
  return v;
}

std::string python_error_where(std::string_view output) {
  static const std::regex kFrame("File \"([^\"]+)\", line ([0-9]+)");
  const std::string text(output);
  std::string where, ours;
  for (auto it = std::sregex_iterator(text.begin(), text.end(), kFrame);
       it != std::sregex_iterator(); ++it) {
    where = (*it)[1].str() + ":" + (*it)[2].str();
    // The last frame in a builder or ge_asset.py, not in Python's own library or bpy.
    const std::string file = (*it)[1].str();
    if (file.find("site-packages") == std::string::npos &&
        file.find("/lib/python") == std::string::npos &&
        file.find("\\Lib\\") == std::string::npos && file.find("/bpy/") == std::string::npos &&
        !file.starts_with("<")) {
      ours = where;
    }
  }
  return ours.empty() ? where : ours;
}

std::vector<Finding> check_findings(std::string_view output, std::vector<std::string>* fresh) {
  std::vector<Finding> out;
  for (const std::string_view view : split_lines(output)) {
    const std::string line(view);
    if (line.rfind("FAIL ", 0) == 0) {
      const size_t colon = line.find(": ", 5);
      const std::string name = colon == std::string::npos ? "" : line.substr(5, colon - 5);
      const std::string what = colon == std::string::npos ? line.substr(5) : line.substr(colon + 2);
      out.push_back({Severity::error, what, name,
                     name.empty() || name == "manifest" ? "fix the manifest; skill: assets"
                                                        : "ge asset build " + name});
    } else if (fresh && line.rfind("ok   ", 0) == 0) {
      const size_t colon = line.find(':', 5);
      if (colon != std::string::npos) fresh->push_back(line.substr(5, colon - 5));
    }
  }
  return out;
}

Outcome run_asset_list(const BuildOptions& o) {
  Outcome out;
  const Repo r = repo_assets(o.repo);
  out.findings = r.findings;
  if (!ready(o, out)) return out;
  const Checked c = check_all(o, r, {});
  std::vector<std::string> rows;
  for (const assets::AssetManifest& m : r.assets) {
    const bool fresh = std::find(c.fresh.begin(), c.fresh.end(), m.name) != c.fresh.end();
    std::error_code ec;
    const auto bytes = fs::exists(m.model_path(), ec) ? fs::file_size(m.model_path(), ec) : 0;
    rows.push_back(json_obj({{"name", json_str(m.name)},
                             {"dir", json_str(rel_path(o.repo, m.dir))},
                             {"builder", json_str(m.builder)},
                             {"data", json_str(m.data)},
                             {"bytes", json_num(static_cast<long long>(bytes))},
                             {"budget_bytes", json_num(static_cast<long long>(m.budget_bytes))},
                             {"fresh", json_bool(fresh)}}));
    out.text += m.name + "  " + rel_path(o.repo, m.dir) + "  " +
                std::to_string((bytes + 1023) / 1024) + " of " +
                std::to_string(m.budget_bytes / 1024) + " KiB  " +
                (fresh ? "fresh" : "needs a build") + "\n";
  }
  out.result = json_obj({{"assets", json_arr(rows)}});
  out.summary =
      std::to_string(r.assets.size()) + " assets, " + std::to_string(c.fresh.size()) + " fresh";
  out.next = c.fresh.size() == r.assets.size() ? "" : "ge asset build";
  return out;
}

Outcome run_asset_check(const BuildOptions& o) {
  Outcome out;
  const Repo r = repo_assets(o.repo);
  out.findings = r.findings;
  if (!ready(o, out)) return out;
  const Checked c = check_all(o, r, {});
  if (!c.ran) {
    out.findings.push_back({Severity::error, "the assets executable's check did not run",
                            rel_path(o.repo, build_log(o, "ge-asset")), "read the log"});
    return out;
  }
  out.findings.insert(out.findings.end(), c.findings.begin(), c.findings.end());
  out.summary = std::to_string(c.fresh.size()) + " of " + std::to_string(r.assets.size()) +
                " models fresh and in budget";
  if (!c.findings.empty()) out.next = "ge asset build";
  return out;
}

Outcome run_asset_build(const BuildOptions& o, const AssetOptions& a) {
  Outcome out;
  const Repo r = repo_assets(o.repo);
  out.findings = r.findings;
  for (const std::string& n : a.names) {
    if (!named(r, n)) {
      out.findings.push_back({Severity::error, "no asset \"" + n + "\"", "",
                              "name one of: " + names_of(r) + " (ge asset list)"});
      out.exit = 2;
      return out;
    }
  }
  if (!ready(o, out)) return out;
  std::vector<const assets::AssetManifest*> todo;
  if (!a.names.empty()) {
    for (const std::string& n : a.names) todo.push_back(named(r, n));
  } else {
    const Checked c = check_all(o, r, {});
    for (const assets::AssetManifest& m : r.assets) {
      const bool fresh = std::find(c.fresh.begin(), c.fresh.end(), m.name) != c.fresh.end();
      if (a.force || !fresh) todo.push_back(&m);
    }
  }
  if (todo.empty()) {
    out.summary = "every model is fresh; nothing to build";
    return out;
  }
  const std::string blender = find_blender(a.blender);
  if (blender.empty()) {
    out.findings.push_back(
        {Severity::error,
         "no Blender to build with: no --blender, no GE_BLENDER, no `blender` "
         "on the PATH and no python3 that can import bpy",
         "",
         "install the bpy module (python3.13 -m pip install --target ~/.cache/bpy bpy==5.2.2) "
         "and rerun with GE_BLENDER=python3.13 PYTHONPATH=~/.cache/bpy, or put Blender 5.2 on "
         "the PATH; skill: assets"});
    return out;
  }
  const fs::path work = o.repo / "build" / o.preset / "assets.d";
  const auto helper = std::find_if(embedded_files().begin(), embedded_files().end(),
                                   [](const EmbeddedFile& f) { return f.path == kHelper; });
  std::string error;
  if (helper == embedded_files().end() ||
      !write_text(work / "lib" / "ge_asset.py", helper->text, error)) {
    out.findings.push_back({Severity::error,
                            "cannot write the builder library ge_asset.py: " +
                                (error.empty() ? std::string("this ge carries none") : error),
                            rel_path(o.repo, work), "check the build directory is writable"});
    return out;
  }
  prepend_python_path(work / "lib");
  const fs::path log = build_log(o, "ge-asset");
  std::error_code ec;
  fs::remove(log, ec);
  std::vector<std::string> built, rows;
  for (const assets::AssetManifest* m : todo) {
    const std::string where = rel_path(o.repo, m->dir);
    fs::path data;
    if (!m->data.empty()) {
      data = work / (m->name + ".json");
      const Logged d = run_to_log(quote(assets_exe(o)) + " data " + m->data, log);
      error.clear();
      if (d.code != 0 || !write_text(data, d.output, error)) {
        out.findings.push_back({Severity::error,
                                "the data set \"" + m->data +
                                    "\" did not print: " + (error.empty() ? d.output : error),
                                where,
                                "register it with GE_ASSET_DATA in a data.cpp beside the "
                                "builder; skill: assets"});
        continue;
      }
    }
    const fs::path temp = work / (m->name + ".glb");
    fs::remove(temp, ec);
    const Logged b = run_to_log(blender_line(blender, m->dir / m->builder, data, temp), log);
    const std::string version = blender_version(b.output);
    if (b.code != 0 || !fs::is_regular_file(temp, ec) || version.empty()) {
      const std::string at = python_error_where(b.output);
      out.findings.push_back(
          {Severity::error,
           m->name + "'s builder failed (exit " + std::to_string(b.code) + ")" +
               (b.code == 0 ? ": it wrote no model, or never called ge_asset.start()" : ""),
           at.empty() ? rel_path(o.repo, m->dir / m->builder) : in_repo(o.repo, at),
           "read " + rel_path(o.repo, log) + " for the builder's own output; skill: assets"});
      continue;
    }
    fs::copy_file(temp, m->model_path(), fs::copy_options::overwrite_existing, ec);
    if (ec) {
      out.findings.push_back({Severity::error, "cannot write the model: " + ec.message(),
                              rel_path(o.repo, m->model_path()), "check the file is writable"});
      continue;
    }
    const Logged s = run_to_log(quote(assets_exe(o)) + " stamp " + m->name + " --blender=\"" +
                                    version + "\" --root=" + quote(o.repo),
                                log);
    if (s.code != 0) {
      out.findings.push_back(
          {Severity::error, "the stamp was not written: " + s.output, where, "read the log"});
      continue;
    }
    out.written.push_back(rel_path(o.repo, m->model_path()));
    out.written.push_back(rel_path(o.repo, m->stamp_path()));
    built.push_back(m->name);
    const auto bytes = fs::file_size(m->model_path(), ec);
    rows.push_back(json_obj({{"name", json_str(m->name)},
                             {"bytes", json_num(static_cast<long long>(bytes))},
                             {"budget_bytes", json_num(static_cast<long long>(m->budget_bytes))},
                             {"blender", json_str(version)}}));
  }
  if (!built.empty()) {
    const Checked c = check_all(o, r, built);
    out.findings.insert(out.findings.end(), c.findings.begin(), c.findings.end());
  }
  out.result = json_obj({{"built", json_arr(rows)}, {"blender", json_str(blender)}});
  out.summary = "built " + std::to_string(built.size()) + " model" +
                (built.size() == 1 ? "" : "s") + " with " + blender;
  out.next = "ge lab models -- --model=" +
             (built.empty() ? std::string("<model.glb>") : out.written.front());
  return out;
}

}  // namespace ge::cli
