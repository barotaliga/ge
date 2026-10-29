// The manifest's reader, checks and writer (manifest.hpp states the format).
//
// ge:summary The mod manifest's reader, field checks and writer, and the platform names
// ge:keywords mod manifest parse check write platform kebab version range sha256-hex
// ge:skill mod
// ge:test tests/mod_manifest_test.cpp
#include "mod/manifest.hpp"

#include <charconv>
#include <initializer_list>

namespace ge::mod {

namespace {

// A message from its parts, in one allocation.
std::string cat(std::initializer_list<std::string_view> parts) {
  size_t n = 0;
  for (std::string_view part : parts) n += part.size();
  std::string out;
  out.reserve(n);
  for (std::string_view part : parts) out += part;
  return out;
}

constexpr std::string_view kPlatforms[] = {"linux-x64",   "linux-arm64", "macos-x64",
                                           "macos-arm64", "windows-x64", "windows-arm64"};

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r'))
    s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
    s.remove_suffix(1);
  return s;
}

bool lower_alnum(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

bool kebab(std::string_view s) {
  if (s.empty() || !(s[0] >= 'a' && s[0] <= 'z') || s.back() == '-') return false;
  for (char c : s)
    if (!lower_alnum(c) && c != '-') return false;
  return true;
}

bool game_id(std::string_view s) {
  if (s.empty() || !(s[0] >= 'a' && s[0] <= 'z')) return false;
  for (char c : s)
    if (!lower_alnum(c) && c != '_') return false;
  return true;
}

bool number(std::string_view s, uint32_t& out) {
  if (s.empty()) return false;
  const auto r = std::from_chars(s.data(), s.data() + s.size(), out);
  return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

// "a.b.c" with each part a number.
bool version3(std::string_view s) {
  int parts = 0;
  while (true) {
    const size_t dot = s.find('.');
    uint32_t v = 0;
    if (!number(s.substr(0, dot), v)) return false;
    parts++;
    if (dot == std::string_view::npos) break;
    s.remove_prefix(dot + 1);
  }
  return parts == 3;
}

bool hex64(std::string_view s) {
  if (s.size() != 64) return false;
  for (char c : s)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

// A path inside the package: relative, '/'-separated, no '..' or '.' part, no drive.
bool package_path(std::string_view p) {
  if (p.empty() || p.front() == '/' || p.find('\\') != std::string_view::npos ||
      p.find(':') != std::string_view::npos)
    return false;
  while (!p.empty()) {
    const size_t slash = p.find('/');
    const std::string_view part = p.substr(0, slash);
    if (part.empty() || part == "." || part == "..") return false;
    if (slash == std::string_view::npos) break;
    p.remove_prefix(slash + 1);
  }
  return true;
}

}  // namespace

bool known_platform(std::string_view p) {
  for (std::string_view k : kPlatforms)
    if (k == p) return true;
  return false;
}

std::string_view this_platform() {
#if defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
  return "windows-arm64";
#elif defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
  return "windows-x64";
#elif defined(__APPLE__) && defined(__aarch64__)
  return "macos-arm64";
#elif defined(__APPLE__) && defined(__x86_64__)
  return "macos-x64";
#elif defined(__linux__) && defined(__aarch64__)
  return "linux-arm64";
#elif defined(__linux__) && defined(__x86_64__)
  return "linux-x64";
#else
  return "";
#endif
}

const ManifestFile* file_for(const Manifest& m, std::string_view platform) {
  for (const ManifestFile& f : m.files)
    if (f.platform == platform) return &f;
  return nullptr;
}

ManifestCheck parse_manifest(std::string_view text, std::string_view source, Manifest& out) {
  out = Manifest{};
  ManifestCheck c;
  const std::string src(source);
  bool seen_name = false, seen_version = false, seen_game = false, seen_api = false,
       seen_abi = false;
  int line_no = 0;
  while (!text.empty()) {
    const size_t nl = text.find('\n');
    const std::string_view raw = text.substr(0, nl);
    text.remove_prefix(nl == std::string_view::npos ? text.size() : nl + 1);
    line_no++;
    const std::string_view line = trim(raw);
    if (line.empty() || line.front() == '#') continue;
    const std::string at = src + ":" + std::to_string(line_no);
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos) {
      c.errors.push_back(
          cat({at, ": `", line,
               "` is not `key = value`; next: fix the line (docs/mods.md § The package)"}));
      continue;
    }
    const std::string_view key = trim(line.substr(0, eq));
    const std::string_view value = trim(line.substr(eq + 1));
    const std::string v(value);
    if (key == "name") {
      seen_name = true;
      out.name = v;
      if (!kebab(value))
        c.errors.push_back(
            cat({at, ": name `", v,
                 "` is not kebab case; next: use lower-case letters, digits and '-'"}));
    } else if (key == "version") {
      seen_version = true;
      out.version = v;
      if (!version3(value))
        c.errors.push_back(
            cat({at, ": version `", v, "` is not major.minor.patch; next: write it as 1.0.0"}));
    } else if (key == "game") {
      seen_game = true;
      out.game = v;
      if (!game_id(value))
        c.errors.push_back(
            cat({at, ": game `", v,
                 "` is not a game id; next: use the game's id, e.g. shooters_edge"}));
    } else if (key == "game_api") {
      seen_api = true;
      const size_t dots = value.find("..");
      const bool range = dots != std::string_view::npos;
      const bool ok = range ? number(value.substr(0, dots), out.game_api_min) &&
                                  number(value.substr(dots + 2), out.game_api_max)
                            : number(value, out.game_api_min);
      if (!range) out.game_api_max = out.game_api_min;
      if (!ok || out.game_api_min < 1 || out.game_api_max < out.game_api_min)
        c.errors.push_back(
            cat({at, ": game_api `", v,
                 "` is not N or N..M with 1 <= N <= M; next: name the game's hook-table "
                 "versions the mod runs on"}));
    } else if (key == "abi") {
      seen_abi = true;
      const size_t dot = value.find('.');
      const bool ok = dot != std::string_view::npos &&
                      number(value.substr(0, dot), out.abi_major) &&
                      number(value.substr(dot + 1), out.abi_minor) && out.abi_major >= 1;
      if (!ok)
        c.errors.push_back(
            cat({at, ": abi `", v,
                 "` is not major.minor (1.0 or later); next: write the engine ABI the "
                 "mod was built against"}));
    } else if (key.starts_with("file.")) {
      ManifestFile f;
      f.platform = std::string(key.substr(5));
      const size_t sp = value.find_last_of(" \t");
      if (sp != std::string_view::npos) {
        f.path = std::string(trim(value.substr(0, sp)));
        f.sha256 = std::string(value.substr(sp + 1));
      }
      if (!known_platform(f.platform))
        c.errors.push_back(cat({at, ": `", f.platform,
                                "` is not a platform; next: use linux-x64, linux-arm64, macos-x64, "
                                "macos-arm64, windows-x64 or windows-arm64"}));
      else if (file_for(out, f.platform))
        c.errors.push_back(cat(
            {at, ": platform ", f.platform, " is listed twice; next: keep one file a platform"}));
      if (!package_path(f.path) || !hex64(f.sha256))
        c.errors.push_back(
            cat({at, ": `", v,
                 "` is not `<path> <sha256>` with a relative path inside the package "
                 "and 64 lower-case hex; next: rebuild with ge mod build"}));
      out.files.push_back(f);
    } else {
      c.warnings.push_back(
          cat({at, ": key `", key,
               "` is not one this engine reads; it is ignored (see docs/mods.md)"}));
    }
  }
  const auto need = [&](bool seen, const char* key) {
    if (!seen)
      c.errors.push_back(cat({src, ": no `", key,
                              "` line; next: add it (docs/mods.md § The package lists the keys)"}));
  };
  need(seen_name, "name");
  need(seen_version, "version");
  need(seen_game, "game");
  need(seen_api, "game_api");
  need(seen_abi, "abi");
  if (out.files.empty())
    c.errors.push_back(cat({src,
                            ": no `file.<platform>` line; next: build the mod (ge mod build), "
                            "which lists its library"}));
  return c;
}

std::string write_manifest(const Manifest& m) {
  std::string s = "# A mod package (docs/mods.md § The package). Written by ge mod build.\n";
  s += "name = " + m.name + "\n";
  s += "version = " + m.version + "\n";
  s += "game = " + m.game + "\n";
  s += "game_api = " + std::to_string(m.game_api_min);
  if (m.game_api_max != m.game_api_min) s += ".." + std::to_string(m.game_api_max);
  s += "\n";
  s += "abi = " + std::to_string(m.abi_major) + "." + std::to_string(m.abi_minor) + "\n";
  for (const ManifestFile& f : m.files)
    s += "file." + f.platform + " = " + f.path + " " + f.sha256 + "\n";
  return s;
}

}  // namespace ge::mod
