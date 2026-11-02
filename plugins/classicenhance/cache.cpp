// The cache's paths, records and whole-file writes (cache.hpp).
//
// ge:summary The local cache's root, records and whole-file writes
// ge:keywords cache root records write rename temporary safe-name
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#include "classicenhance/cache.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

#include "assets/sha256.hpp"

namespace ge::classicenhance {

namespace fs = std::filesystem;

namespace {

std::string env(const char* name) {
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

}  // namespace

fs::path default_cache_root() {
  if (const std::string set = env("GE_CLASSIC_CACHE"); !set.empty()) return set;
  fs::path base;
#if defined(_WIN32)
  base = env("LOCALAPPDATA");
#elif defined(__APPLE__)
  if (const std::string home = env("HOME"); !home.empty()) base = fs::path(home) / "Library/Caches";
#else
  if (const std::string xdg = env("XDG_CACHE_HOME"); !xdg.empty()) {
    base = xdg;
  } else if (const std::string home = env("HOME"); !home.empty()) {
    base = fs::path(home) / ".cache";
  }
#endif
  if (base.empty()) return {};
  return base / "ge" / "classicenhance";
}

fs::path pack_dir(const fs::path& root, const std::string& pack_sha256) {
  return root / pack_sha256.substr(0, 16);
}

std::string set_key(std::span<const std::string> digests) {
  if (digests.empty()) return {};
  if (digests.size() == 1) return digests[0];
  std::string ids = digests[0];
  for (size_t i = 1; i < digests.size(); i++) ids += " " + digests[i];
  return assets::sha256_hex({reinterpret_cast<const uint8_t*>(ids.data()), ids.size()});
}

std::vector<StageRecord> read_records(const fs::path& dir) {
  std::vector<StageRecord> out;
  std::ifstream f(dir / "stages.txt");
  std::string line;
  while (std::getline(f, line)) {
    std::istringstream s(line);
    StageRecord r;
    if (s >> r.stage >> r.key) out.push_back(r);
  }
  return out;
}

std::string recorded_key(const fs::path& dir, const std::string& stage) {
  for (const StageRecord& r : read_records(dir)) {
    if (r.stage == stage) return r.key;
  }
  return "";
}

bool write_record(const fs::path& dir, const std::string& stage, const std::string& key) {
  std::string text;
  for (const StageRecord& r : read_records(dir)) {
    if (r.stage != stage) text += r.stage + " " + r.key + "\n";
  }
  text += stage + " " + key + "\n";
  return write_text(dir / "stages.txt", text);
}

bool write_whole(const fs::path& path, std::span<const uint8_t> bytes) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  const fs::path tmp = path.string() + ".part";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    if (!f) return false;
  }
  fs::rename(tmp, path, ec);
  if (ec) {
    fs::remove(tmp, ec);
    return false;
  }
  return true;
}

bool write_text(const fs::path& path, const std::string& text) {
  return write_whole(path, {reinterpret_cast<const uint8_t*>(text.data()), text.size()});
}

std::string safe_name(const std::string& name) {
  std::string out;
  for (const char raw : name) {
    const char c = raw >= 'A' && raw <= 'Z' ? char(raw - 'A' + 'a') : raw;
    if (c == '*') {
      out += "liq_";
    } else if (c == '+') {
      out += "anim_";
    } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
      out += c;
    } else {
      out += '_';
    }
  }
  return out.empty() ? "_" : out;
}

}  // namespace ge::classicenhance
