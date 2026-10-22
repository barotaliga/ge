// The `assets` command line: the manifests found, the data printed, the models checked and
// stamped.
//
// ge:summary The assets command line's commands
// ge:keywords assets tool list data check stamp
// ge:skill assets
// ge:test tests/assets_manifest_test.cpp
#include "assets/tool.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "assets/data.hpp"
#include "assets/gltf.hpp"
#include "assets/manifest.hpp"

namespace ge::assets {

namespace {

struct Found {
  std::vector<AssetManifest> assets;
  std::vector<std::string> errors;
};

Found find_all(const fs::path& root) {
  Found f;
  for (const fs::path& file : find_manifests(root)) {
    ManifestRead r = read_manifest(file);
    if (r.ok) {
      f.assets.push_back(std::move(r.manifest));
    } else {
      f.errors.push_back(r.error);
    }
  }
  return f;
}

// The data's JSON for a manifest, or false with the reason.
bool data_of(const AssetManifest& m, std::string& json, std::string& error) {
  json.clear();
  if (m.data.empty()) return true;
  const DataSpec* spec = find_data(m.data);
  if (!spec) {
    error = m.name + "'s manifest names data \"" + m.data +
            "\", which no GE_ASSET_DATA registers in this build; add it in a data.cpp beside "
            "the builder (engine/assets/data.hpp)";
    return false;
  }
  json = spec->make();
  return true;
}

std::string json_str(const std::string& s) {
  std::string o = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o + "\"";
}

std::string rel(const fs::path& root, const fs::path& p) {
  return p.lexically_relative(root).lexically_normal().generic_string();
}

// Every problem with one model: the stamp and the budget, then whether it loads.
std::vector<std::string> problems_of(const AssetManifest& m, AssetCheck& check) {
  std::string json, error;
  if (!data_of(m, json, error)) return {error};
  check = check_asset(m, json);
  std::vector<std::string> out = check.problems;
  if (check.bytes > 0) {
    const LoadResult loaded = load_gltf_file(m.model_path().string());
    if (!loaded.ok) out.push_back(loaded.error);
  }
  return out;
}

int usage(const char* why) {
  std::fprintf(stderr,
               "assets: %s\n"
               "  assets list [--root=D] [--json]\n"
               "  assets data <name>\n"
               "  assets check [--root=D] [<name>...]\n"
               "  assets stamp <name> --blender=<version> [--root=D]\n",
               why);
  return 2;
}

}  // namespace

int run_assets(int argc, const char* const* argv) {
  std::vector<std::string> words;
  fs::path root = fs::current_path();
  std::string blender;
  bool json = false;
  for (int i = 1; i < argc; i++) {
    const std::string w = argv[i];
    if (w.rfind("--root=", 0) == 0) {
      root = w.substr(7);
    } else if (w.rfind("--blender=", 0) == 0) {
      blender = w.substr(10);
    } else if (w == "--json") {
      json = true;
    } else if (w == "--help" || w == "-h") {
      usage("the built assets (engine/assets/manifest.hpp)");
      return 0;
    } else if (w.rfind("--", 0) == 0) {
      return usage(("unknown flag " + w).c_str());
    } else {
      words.push_back(w);
    }
  }
  // From the root, so every path a problem names is the repository's own.
  std::error_code ec;
  fs::current_path(root, ec);
  if (ec) return usage(("cannot enter --root=" + root.string()).c_str());
  root = ".";
  const std::string command = words.empty() ? "list" : words[0];
  const std::vector<std::string> names(words.empty() ? words.end() : words.begin() + 1,
                                       words.end());

  if (command == "data") {
    if (names.size() != 1) return usage("`assets data` takes one data set's name");
    const DataSpec* spec = find_data(names[0]);
    if (!spec) {
      std::string all;
      for (const DataSpec* s : registered_data())
        all += std::string(all.empty() ? "" : ", ") + s->name;
      std::fprintf(stderr, "assets: no data set \"%s\"; the registered ones: %s\n",
                   names[0].c_str(), all.empty() ? "none" : all.c_str());
      return 1;
    }
    std::printf("%s\n", spec->make().c_str());
    return 0;
  }

  const Found found = find_all(root);
  const auto named = [&](const std::string& name) -> const AssetManifest* {
    for (const AssetManifest& m : found.assets) {
      if (m.name == name) return &m;
    }
    return nullptr;
  };
  for (const std::string& n : names) {
    if (!named(n)) {
      std::string all;
      for (const AssetManifest& m : found.assets) all += (all.empty() ? "" : ", ") + m.name;
      std::fprintf(stderr, "assets: no asset \"%s\" under %s; the assets: %s\n", n.c_str(),
                   root.generic_string().c_str(), all.empty() ? "none" : all.c_str());
      return 1;
    }
  }

  if (command == "list") {
    std::string out = "[";
    for (const AssetManifest& m : found.assets) {
      AssetCheck check;
      const std::vector<std::string> problems = problems_of(m, check);
      if (json) {
        std::string p = "[";
        for (const std::string& s : problems) p += (p.size() > 1 ? "," : "") + json_str(s);
        out += std::string(out.size() > 1 ? "," : "") + "{\"name\":" + json_str(m.name) +
               ",\"dir\":" + json_str(rel(root, m.dir)) + ",\"builder\":" + json_str(m.builder) +
               ",\"data\":" + json_str(m.data) +
               ",\"budget_bytes\":" + std::to_string(m.budget_bytes) +
               ",\"bytes\":" + std::to_string(check.bytes) +
               ",\"blender\":" + json_str(check.blender) + ",\"problems\":" + p + "]}";
      } else {
        std::printf("%-12s %-40s %6llu/%llu KiB  %s\n", m.name.c_str(), rel(root, m.dir).c_str(),
                    static_cast<unsigned long long>((check.bytes + 1023) / 1024),
                    static_cast<unsigned long long>(m.budget_bytes / 1024),
                    problems.empty() ? "fresh" : "NEEDS WORK");
      }
    }
    if (json) std::printf("%s]\n", out.c_str());
    for (const std::string& e : found.errors) std::fprintf(stderr, "assets: %s\n", e.c_str());
    return found.errors.empty() ? 0 : 1;
  }

  if (command == "check") {
    int failed = 0;
    for (const std::string& e : found.errors) {
      std::printf("FAIL manifest: %s\n", e.c_str());
      failed++;
    }
    for (const AssetManifest& m : found.assets) {
      if (!names.empty() && std::find(names.begin(), names.end(), m.name) == names.end()) continue;
      AssetCheck check;
      const std::vector<std::string> problems = problems_of(m, check);
      for (const std::string& p : problems) std::printf("FAIL %s: %s\n", m.name.c_str(), p.c_str());
      if (problems.empty()) {
        std::printf("ok   %s: %s, %llu of %llu KiB, built by Blender %s\n", m.name.c_str(),
                    rel(root, m.model_path()).c_str(),
                    static_cast<unsigned long long>((check.bytes + 1023) / 1024),
                    static_cast<unsigned long long>(m.budget_bytes / 1024), check.blender.c_str());
      }
      failed += problems.empty() ? 0 : 1;
    }
    if (found.assets.empty() && found.errors.empty()) {
      std::printf("ok   no assets under %s\n", root.generic_string().c_str());
    }
    return failed ? 1 : 0;
  }

  if (command == "stamp") {
    if (names.size() != 1 || blender.empty()) {
      return usage("`assets stamp` takes one asset's name and --blender=<version>");
    }
    const AssetManifest& m = *named(names[0]);
    std::string data, error;
    std::vector<StampLine> lines;
    if (!data_of(m, data, error) || !stamp_lines(m, data, lines, error)) {
      std::fprintf(stderr, "assets: %s\n", error.c_str());
      return 1;
    }
    std::ofstream out(m.stamp_path(), std::ios::binary);
    out << stamp_text(m, lines, blender);
    if (!out) {
      std::fprintf(stderr, "assets: cannot write %s; check the directory is writable\n",
                   m.stamp_path().generic_string().c_str());
      return 1;
    }
    std::printf("%s\n", rel(root, m.stamp_path()).c_str());
    return 0;
  }
  return usage(("unknown command \"" + command + "\"").c_str());
}

}  // namespace ge::assets
