// The manifest's parser, the stamp's lines and the check of a committed model.
//
// ge:summary The asset manifest parsed, the stamp hashed and written, a model checked
// ge:keywords assets manifest stamp hash budget stale check_asset parse_manifest
// ge:skill assets
// ge:test tests/assets_manifest_test.cpp
#include "assets/manifest.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace ge::assets {

namespace {

std::string trim(std::string_view s) {
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string_view::npos) return "";
  const size_t b = s.find_last_not_of(" \t");
  return std::string(s.substr(a, b - a + 1));
}

bool read_file(const fs::path& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

bool skipped(const std::string& name) {
  return name == ".git" || name.starts_with("build") || name == "_deps" || name == "previews" ||
         name == ".cache" || name == "node_modules" || name == "porting";
}

void find_in(const fs::path& dir, std::vector<fs::path>& out) {
  std::error_code ec;
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
    const std::string name = e.path().filename().string();
    if (e.is_directory(ec)) {
      if (!skipped(name)) find_in(e.path(), out);
    } else if (name == kManifestFile) {
      out.push_back(e.path());
    }
  }
}

std::string where(const AssetManifest& m, const fs::path& file) {
  return (m.dir / file.filename()).lexically_normal().generic_string();
}

}  // namespace

std::string hash_text(std::string_view text) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (const char c : text) {
    if (c == '\r') continue;
    h ^= static_cast<unsigned char>(c);
    h *= 0x100000001b3ull;
  }
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(h));
  return buf;
}

ManifestRead parse_manifest(std::string_view text, const fs::path& dir) {
  ManifestRead r;
  AssetManifest& m = r.manifest;
  m.dir = dir;
  m.name = dir.filename().string();
  const std::string file = (dir.filename() / std::string(kManifestFile)).generic_string();
  std::istringstream in{std::string(text)};
  std::string raw;
  int line = 0;
  bool budget = false;
  while (std::getline(in, raw)) {
    line++;
    std::string_view s = raw;
    if (const size_t hash = s.find('#'); hash != std::string_view::npos) s = s.substr(0, hash);
    const std::string body = trim(s);
    if (body.empty() || body == "\r") continue;
    const size_t colon = body.find(':');
    const std::string key = colon == std::string::npos ? body : trim(body.substr(0, colon));
    const std::string value = colon == std::string::npos ? "" : trim(body.substr(colon + 1));
    const std::string at = file + ":" + std::to_string(line);
    if (colon == std::string::npos || value.empty()) {
      r.error = at;
      r.error += ": \"" + body + "\" is not `key: value`; write e.g. `builder: build.py`";
      return r;
    }
    if (key == "builder") {
      m.builder = value;
    } else if (key == "source") {
      m.sources.push_back(value);
    } else if (key == "data") {
      m.data = value;
    } else if (key == "budget-kib") {
      if (value.find_first_not_of("0123456789") != std::string::npos || value.size() > 9) {
        r.error = at;
        r.error += ": budget-kib takes a whole number of KiB, got \"" + value + "\"";
        return r;
      }
      m.budget_bytes = std::stoull(value) * 1024;
      budget = true;
    } else {
      r.error = at;
      r.error +=
          ": unknown key \"" + key +
          "\"; the keys are builder, data, source and budget-kib (engine/assets/manifest.hpp)";
      return r;
    }
  }
  if (m.builder.empty() || !budget || m.budget_bytes == 0) {
    r.error = file +
              ": needs a `builder:` and a non-zero `budget-kib:` line (engine/assets/manifest.hpp)";
    return r;
  }
  r.ok = true;
  return r;
}

ManifestRead read_manifest(const fs::path& file) {
  std::string text;
  if (!read_file(file, text)) {
    ManifestRead r;
    r.error = file.generic_string() + ": cannot be read; check the path";
    return r;
  }
  return parse_manifest(text, file.parent_path());
}

std::vector<fs::path> find_manifests(const fs::path& root) {
  std::vector<fs::path> out;
  find_in(root, out);
  std::sort(out.begin(), out.end());
  return out;
}

bool stamp_lines(const AssetManifest& m, std::string_view data_json, std::vector<StampLine>& out,
                 std::string& error) {
  out.clear();
  const auto add = [&](const char* kind, const std::string& rel) {
    std::string text;
    if (!read_file(m.dir / rel, text)) {
      error = where(m, kManifestFile) + ": its " + kind + " " + rel +
              " cannot be read; fix the path in the manifest or restore the file";
      return false;
    }
    out.push_back({kind, rel, hash_text(text)});
    return true;
  };
  if (!add("builder", m.builder)) return false;
  for (const std::string& s : m.sources) {
    if (!add("source", s)) return false;
  }
  if (!m.data.empty()) out.push_back({"data", m.data, hash_text(data_json)});
  return true;
}

std::string stamp_text(const AssetManifest& m, const std::vector<StampLine>& lines,
                       std::string_view blender) {
  std::string t = "# What " + m.name +
                  ".glb was built from (engine/assets/manifest.hpp). Written by `ge asset build " +
                  m.name + "`; never edit it by hand.\n";
  for (const StampLine& l : lines) t += l.kind + " " + l.key + " " + l.hash + "\n";
  t += "blender " + std::string(blender) + "\n";
  return t;
}

Stamp parse_stamp(std::string_view text) {
  Stamp s;
  std::istringstream in{std::string(text)};
  std::string raw;
  while (std::getline(in, raw)) {
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    if (raw.empty() || raw[0] == '#') continue;
    std::istringstream words(raw);
    std::string kind, key, hash;
    words >> kind;
    if (kind == "blender") {
      std::getline(words >> std::ws, s.blender);
      continue;
    }
    words >> key >> hash;
    if (!hash.empty()) s.lines.push_back({kind, key, hash});
  }
  return s;
}

AssetCheck check_asset(const AssetManifest& m, std::string_view data_json) {
  AssetCheck c;
  const std::string rebuild = "run `ge asset build " + m.name + "` (skill: assets)";
  std::error_code ec;
  const fs::path model = m.model_path();
  if (!fs::is_regular_file(model, ec)) {
    c.problems.push_back(where(m, model) + " is missing; " + rebuild);
    return c;
  }
  c.bytes = fs::file_size(model, ec);
  if (c.bytes > m.budget_bytes) {
    c.problems.push_back(where(m, model) + " weighs " + std::to_string((c.bytes + 1023) / 1024) +
                         " KiB, over its budget of " + std::to_string(m.budget_bytes / 1024) +
                         " KiB; cut its triangles or its LODs in " + m.builder +
                         " and rebuild, or raise budget-kib with the reason in the PR");
  }
  std::string text;
  if (!read_file(m.stamp_path(), text)) {
    c.problems.push_back(where(m, m.stamp_path()) + " is missing, so nothing says what " + m.name +
                         ".glb was built from; " + rebuild);
    return c;
  }
  const Stamp stamp = parse_stamp(text);
  c.blender = stamp.blender;
  std::vector<StampLine> now;
  std::string error;
  if (!stamp_lines(m, data_json, now, error)) {
    c.problems.push_back(error);
    return c;
  }
  for (const StampLine& l : now) {
    const auto old = std::find_if(stamp.lines.begin(), stamp.lines.end(), [&](const StampLine& s) {
      return s.kind == l.kind && s.key == l.key;
    });
    if (old == stamp.lines.end()) {
      c.problems.push_back(m.name + ".glb was built without its " + l.kind + " " + l.key + "; " +
                           rebuild);
    } else if (old->hash != l.hash) {
      c.problems.push_back(m.name + ".glb is older than its " + l.kind + " " + l.key +
                           " (stamped " + old->hash + ", now " + l.hash + "); " + rebuild);
    }
  }
  for (const StampLine& s : stamp.lines) {
    const bool kept = std::any_of(now.begin(), now.end(), [&](const StampLine& l) {
      return s.kind == l.kind && s.key == l.key;
    });
    if (!kept) {
      c.problems.push_back(m.name + ".stamp names a " + s.kind + " " + s.key +
                           " the manifest no longer has; " + rebuild);
    }
  }
  return c;
}

}  // namespace ge::assets
