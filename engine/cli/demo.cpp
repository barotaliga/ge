// `ge demo`'s body (demo.hpp): the file read, packed or unpacked through engine/vfs, written beside
// it, and the sizes and times as a table, findings and one JSON result.
//
// ge:summary ge demo's body: zstd pack with a round-trip check, unpack through unpack_one
// ge:keywords cli demo pack unpack run_demo zstd_pack unpack_one ratio throughput
// ge:skill agent-interface
// ge:test tests/cli_demo_test.cpp
#include "cli/demo.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "cli/json.hpp"
#include "core/num_text.hpp"
#include "vfs/unpack.hpp"
#include "vfs/zstd.hpp"

namespace ge::cli {

namespace fs = std::filesystem;

namespace {

bool read_all(const std::string& file, std::vector<uint8_t>& out) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

bool write_all(const std::string& file, const std::vector<uint8_t>& bytes) {
  std::ofstream f(file, std::ios::binary | std::ios::trunc);
  f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(f);
}

double seconds_since(std::chrono::steady_clock::time_point t) {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
}

std::string mib(size_t n) {
  return core::num::to_fixed(static_cast<double>(n) / (1024.0 * 1024.0), 2);
}

std::string rate(size_t n, double s) {
  return s > 0 ? core::num::to_fixed(static_cast<double>(n) / (1024.0 * 1024.0) / s, 0) + " MiB/s"
               : "-";
}

bool same_file(const std::string& a, const std::string& b) {
  std::error_code ec;
  return fs::exists(b, ec) && fs::equivalent(a, b, ec);
}

Outcome refuse(const std::string& what, const std::string& where, const std::string& next) {
  Outcome o;
  o.findings.push_back({Severity::error, what, where, next});
  return o;
}

Outcome pack(const Args& a, const std::string& file) {
  std::vector<uint8_t> plain;
  if (!read_all(file, plain))
    return refuse("cannot read " + file, file, "check the path (skill: assets)");
  if (ge::vfs::wrapping_of(plain) != ge::vfs::Wrapping::none)
    return refuse(file + " is packed already (" +
                      std::string(ge::vfs::wrapping_name(ge::vfs::wrapping_of(plain))) + ")",
                  file, "`ge demo unpack " + file + "` first, then pack the plain recording");
  const std::string level_text = flag_value(a, "level", "19");
  int level = 0;
  try {
    level = std::stoi(level_text);
  } catch (...) {
    level = 0;
  }
  if (level < 1 || level > 22)
    return refuse("--level=" + level_text + " is not a zstd level 1..22", file,
                  "pass --level=19 (a game's own) or leave it out");
  const std::string out = flag_value(a, "out", packed_name(file));
  if (same_file(file, out))
    return refuse("--out names the file being packed", out, "name another file with --out");
  const auto t0 = std::chrono::steady_clock::now();
  const std::vector<uint8_t> packed = ge::vfs::zstd_pack(plain, {level, 0, false});
  const double pack_s = seconds_since(t0);
  const auto t1 = std::chrono::steady_clock::now();
  std::vector<uint8_t> back;
  bool truncated = false;
  std::string error;
  const bool ok = ge::vfs::zstd_unpack(packed, back, truncated, error);
  const double unpack_s = seconds_since(t1);
  if (!ok || truncated || back != plain)
    return refuse("the packed bytes do not unpack to the recording (" + error + ")", file,
                  "report it with the file: the packer is broken (skill: vfs)");
  if (!write_all(out, packed))
    return refuse("cannot write " + out, out, "check the folder is writable and has room");
  Outcome o;
  o.written.push_back(out);
  const double ratio =
      packed.empty() ? 0 : static_cast<double>(plain.size()) / static_cast<double>(packed.size());
  o.text = "  " + file + ": " + mib(plain.size()) + " MiB\n  " + out + ": " + mib(packed.size()) +
           " MiB, " + core::num::to_fixed(ratio, 2) + "x smaller at zstd " + std::to_string(level) +
           "\n  packed at " + rate(plain.size(), pack_s) + ", unpacked at " +
           rate(plain.size(), unpack_s) + "; checked: the same bytes\n";
  o.result = json_obj({{"file", json_str(file)},
                       {"out", json_str(out)},
                       {"level", std::to_string(level)},
                       {"bytes", std::to_string(plain.size())},
                       {"packed", std::to_string(packed.size())},
                       {"ratio", core::num::to_fixed(ratio, 3)},
                       {"pack_s", core::num::to_fixed(pack_s, 4)},
                       {"unpack_s", core::num::to_fixed(unpack_s, 4)}});
  o.summary = "packed " + file + " " + core::num::to_fixed(ratio, 2) + "x smaller";
  o.next = "ge demo unpack " + out;
  return o;
}

Outcome unpack(const Args& a, const std::string& file) {
  std::vector<uint8_t> bytes;
  if (!read_all(file, bytes))
    return refuse("cannot read " + file, file, "check the path (skill: assets)");
  const auto t0 = std::chrono::steady_clock::now();
  ge::vfs::Unpacked u;
  std::string error;
  if (!ge::vfs::unpack_one(std::move(bytes), u, error)) return refuse(error, file, "see above");
  const double unpack_s = seconds_since(t0);
  if (u.layers.empty())
    return refuse(file + " is not packed or archived: it is the plain recording already", file,
                  "play it as it is, or `ge demo pack " + file + "`");
  const std::string out =
      flag_value(a, "out", (fs::path(file).parent_path() / unpacked_name(file, u.name)).string());
  if (same_file(file, out))
    return refuse("the recording's name is the file's own", out, "name another file with --out");
  if (!write_all(out, u.bytes))
    return refuse("cannot write " + out, out, "check the folder is writable and has room");
  Outcome o;
  o.written.push_back(out);
  std::string layers, json_layers;
  for (const ge::vfs::Wrapping w : u.layers) {
    layers += (layers.empty() ? "" : " > ") + std::string(ge::vfs::wrapping_name(w));
    json_layers += (json_layers.empty() ? "" : ",") + json_str(ge::vfs::wrapping_name(w));
  }
  o.text = "  " + file + ": " + layers + (u.name.empty() ? "" : ", holding " + u.name) + "\n  " +
           out + ": " + mib(u.bytes.size()) + " MiB, unpacked at " +
           rate(u.bytes.size(), unpack_s) + "\n";
  if (u.truncated)
    o.findings.push_back(
        {Severity::warn, "the packed recording was cut short", file,
         "it plays to its last flush; the end was lost (a crash while recording)"});
  o.result = json_obj({{"file", json_str(file)},
                       {"out", json_str(out)},
                       {"layers", "[" + json_layers + "]"},
                       {"name", json_str(u.name)},
                       {"bytes", std::to_string(u.bytes.size())},
                       {"truncated", u.truncated ? "true" : "false"},
                       {"unpack_s", core::num::to_fixed(unpack_s, 4)}});
  o.summary = "unpacked " + out;
  return o;
}

}  // namespace

std::string packed_name(std::string_view file) { return std::string(file) + "z"; }

std::string unpacked_name(std::string_view file, std::string_view inner) {
  if (!inner.empty()) return fs::path(std::string(inner)).filename().string();
  std::string name = fs::path(std::string(file)).filename().string();
  for (const std::string_view ext : {".gz", ".zst", ".xz"})
    if (name.size() > ext.size() && name.ends_with(ext))
      return name.substr(0, name.size() - ext.size());
  if (name.ends_with(".tgz")) return name.substr(0, name.size() - 4) + ".tar";
  if (name.size() > 1 && name.ends_with('z')) return name.substr(0, name.size() - 1);
  return name + ".plain";
}

Outcome run_demo(const Args& a, std::string_view sub) {
  const std::string& file = a.words[0];
  return sub == "pack" ? pack(a, file) : unpack(a, file);
}

}  // namespace ge::cli
