// `ge pak ls <gamedir>` (pak.hpp): a game directory mounted as the game mounts it (engine/vfs's
// search path, with `--mods` over it), every file named with the source that wins it and the
// sources it hides, each answer a table, findings and one JSON result.
//
// ge:summary ge pak ls over a game directory: every file, the source that wins it, what it hides
// ge:keywords cli pak ls gamedir game-directory search-path winner override hides mods pk3dir
// ge:skill agent-interface vfs
// ge:test tests/cli_pak_test.cpp
#include <algorithm>
#include <filesystem>
#include <map>

#include "assets/sha256.hpp"
#include "cli/json.hpp"
#include "cli/pak.hpp"
#include "vfs/search.hpp"

namespace ge::cli {

namespace fs = std::filesystem;

namespace {

// A name folded as the search path compares it: ASCII case folded, '\' as '/'.
std::string folded(std::string_view name) {
  std::string out(name);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    if (c == '\\') c = '/';
  }
  return out;
}

// A source as a table names it: `<game>/<archive>`, or `<game>/` for its loose files.
std::string label(const vfs::Source& s) {
  const std::string name = fs::path(s.path).filename().string();
  if (s.kind == vfs::SourceKind::directory && !name.ends_with(".pk3dir")) return s.game + "/";
  return s.game + "/" + name;
}

std::vector<std::string> split_list(const std::string& list) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < list.size()) {
    size_t end = list.find(',', at);
    if (end == std::string::npos) end = list.size();
    if (end > at) out.push_back(list.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

}  // namespace

Outcome pak_ls_game(const Args& args) {
  Outcome o;
  fs::path dir = fs::absolute(args.words[0]).lexically_normal();
  if (dir.filename().empty()) dir = dir.parent_path();
  const std::string root = dir.parent_path().string();
  const std::string base = dir.filename().string();
  std::vector<std::string> games = {base};
  for (const std::string& m : split_list(flag_value(args, "mods"))) games.push_back(m);
  vfs::SearchPath sp;
  for (const std::string& g : games) {
    std::string error;
    if (!vfs::add_game(&sp, root, g, &error)) {
      std::string what = "cannot mount ";
      what += g;
      what += ": ";
      what += error;
      o.findings.push_back({Severity::error, std::move(what), (fs::path(root) / g).generic_string(),
                            "fix or remove the file it names; --mods names game directories "
                            "beside the base one"});
      return o;
    }
  }
  // Every source holding each folded name, in mount order (the last wins).
  std::map<std::string, std::vector<uint32_t>> holders;
  for (uint32_t i = 0; i < sp.sources.size(); ++i) {
    const vfs::Source& s = sp.sources[i];
    auto hold = [&](std::string_view n) {
      auto& v = holders[folded(n)];
      if (v.empty() || v.back() != i) v.push_back(i);
    };
    for (const vfs::PakEntry& e : s.pak.entries) hold(e.path);
    for (const vfs::ZipEntry& e : s.zip.entries) {
      if (!e.name.empty() && e.name.back() != '/') hold(e.name);
    }
    for (const std::string& f : s.files) hold(f);
  }
  const bool sha = has_flag(args, "sha256");
  const bool only_hidden = has_flag(args, "hidden");
  std::vector<std::vector<std::string>> rows = {{"path", "bytes", "from", "hides"}};
  if (sha) rows[0].emplace_back("sha256");
  std::vector<std::string> json;
  size_t shown = 0, overridden = 0;
  for (const vfs::Listed& f : vfs::list(sp)) {
    const auto it = holders.find(std::string(f.name));
    std::vector<std::string> hidden;
    if (it != holders.end()) {
      for (const uint32_t s : it->second) {
        if (s != f.ref.source) hidden.push_back(label(sp.sources[s]));
      }
    }
    if (!hidden.empty()) overridden++;
    if (only_hidden && hidden.empty()) continue;
    const std::string name(vfs::stored_name(sp, f.ref));
    const std::string from = label(sp.sources[f.ref.source]);
    std::string hides;
    for (const std::string& h : hidden) {
      if (!hides.empty()) hides += ", ";
      hides += h;
    }
    std::vector<std::string> row = {name, std::to_string(f.ref.size), from, hides};
    std::vector<std::pair<std::string, std::string>> fields = {
        {"path", json_str(name)},
        {"bytes", json_num(static_cast<long long>(f.ref.size))},
        {"from", json_str(from)},
        {"hides", json_strs(hidden)}};
    if (sha) {
      std::vector<uint8_t> bytes;
      std::string error, digest;
      if (vfs::read(sp, f.ref, &bytes, &error)) {
        digest = assets::sha256_hex(bytes);
      } else {
        std::string what = name;
        what += ": ";
        what += error;
        o.findings.push_back({Severity::error, std::move(what), from,
                              "the file is damaged in its source; fetch it again"});
      }
      row.push_back(digest);
      fields.emplace_back("sha256", json_str(digest));
    }
    rows.push_back(std::move(row));
    json.push_back(json_obj(fields));
    shown++;
  }
  o.text = pak_table(rows);
  std::vector<std::string> mounted;
  mounted.reserve(sp.sources.size());
  for (const vfs::Source& s : sp.sources) mounted.push_back(label(s));
  o.summary = std::to_string(shown) + " file(s) from " + std::to_string(sp.sources.size()) +
              " source(s); " + std::to_string(overridden) + " hide an earlier one";
  o.next = only_hidden ? "" : "ge pak ls " + args.words[0] + " --hidden shows only what overrides";
  o.result = json_obj({{"gamedir", json_str(args.words[0])},
                       {"sources", json_strs(mounted)},
                       {"overridden", json_num(static_cast<long long>(overridden))},
                       {"files", json_arr(json)}});
  return o;
}

}  // namespace ge::cli
