// `ge save`'s body (save.hpp): a save file opened through engine/save and printed as a table,
// findings and one JSON result.
//
// ge:summary ge save's body: inspect a save file through the engine's own reader
// ge:keywords cli save inspect run_save run_save_inspect chunk table
// ge:skill agent-interface save
// ge:test tests/cli_save_test.cpp
#include "cli/save.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <vector>

#include "cli/json.hpp"
#include "cli/play.hpp"
#include "core/num_text.hpp"
#include "save/file.hpp"

namespace ge::cli {

namespace {

const char* kNext =
    "a damaged save: load another (a slot's .bak, an older autosave); a newer one: a newer "
    "build of the game (skill: save)";

std::string hex(uint64_t v) {
  static const char* k = "0123456789abcdef";
  std::string s(16, '0');
  for (int i = 15; i >= 0; i--, v >>= 4) s[static_cast<size_t>(i)] = k[v & 15];
  return s;
}

std::string table(const std::vector<std::vector<std::string>>& rows) {
  std::vector<size_t> width;
  for (const auto& r : rows) {
    if (width.size() < r.size()) width.resize(r.size(), 0);
    for (size_t i = 0; i < r.size(); i++) width[i] = std::max(width[i], r[i].size());
  }
  std::string out;
  for (const auto& r : rows) {
    std::string line = " ";
    for (size_t i = 0; i < r.size(); i++) {
      line += " " + r[i];
      if (i + 1 < r.size()) line += std::string(width[i] - r[i].size() + 1, ' ');
    }
    out += line + "\n";
  }
  return out;
}

const char* kind_name(save::Kind k) {
  switch (k) {
    case save::Kind::run:
      return "run";
    case save::Kind::character:
      return "character";
  }
  return "the game's own";
}

}  // namespace

Outcome run_save_inspect(const std::string& file) {
  Outcome o;
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    o.findings.push_back({Severity::error, "cannot read " + file, file,
                          "check the path; a store keeps slots as <dir>/<slot> (skill: save)"});
    return o;
  }
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  save::FileReader r;
  if (!r.open(bytes, file)) {
    o.findings.push_back({Severity::error, r.problem(), file, kNext});
    return o;
  }
  const save::Header& h = r.header();
  const save::Meta& m = r.meta();
  o.text += "  " + file + ": a " + kind_name(m.kind) + " save of \"" + m.game +
            "\" (save version " + std::to_string(m.game_version) + ", format " +
            std::to_string(h.format) + "), " + std::to_string(bytes.size()) + " bytes, " +
            std::to_string(h.chunks) + " chunks\n";
  o.text += "  title \"" + m.title + "\", place \"" + m.place + "\", played " +
            core::num::to_fixed(m.play_s, 1) + " s, saved at " + std::to_string(m.saved_unix) +
            "\n";
  std::vector<std::vector<std::string>> rows = {
      {"tag", "instance", "version", "offset", "bytes", "checksum"}};
  std::vector<std::string> chunks;
  for (const save::ChunkInfo& c : r.chunks()) {
    rows.push_back({save::tag_name(c.tag), std::to_string(c.instance), std::to_string(c.version),
                    std::to_string(c.offset), std::to_string(c.stored), hex(c.checksum)});
    chunks.push_back(json_obj({{"tag", json_str(save::tag_name(c.tag))},
                               {"instance", json_num(c.instance)},
                               {"version", json_num(c.version)},
                               {"offset", json_num(static_cast<long long>(c.offset))},
                               {"bytes", json_num(static_cast<long long>(c.stored))},
                               {"checksum", json_str(hex(c.checksum))}}));
  }
  o.text += table(rows);
  o.result = json_obj({{"file", json_str(file)},
                       {"bytes", json_num(static_cast<long long>(bytes.size()))},
                       {"format", json_num(h.format)},
                       {"kind", json_str(kind_name(m.kind))},
                       {"game", json_str(m.game)},
                       {"game_version", json_num(m.game_version)},
                       {"title", json_str(m.title)},
                       {"place", json_str(m.place)},
                       {"play_s", core::num::to_string(m.play_s)},
                       {"saved_unix", json_num(static_cast<long long>(m.saved_unix))},
                       {"setup", json_str(m.setup)},
                       {"chunks", json_arr(chunks)}});
  o.summary = "every checksum matches: " + std::to_string(h.chunks) + " chunks";
  return o;
}

Outcome run_save(const Args& a) { return run_save_inspect(a.words[0]); }

Outcome run_save_game(const BuildOptions& o, const Args& a, const std::string& sub) {
  std::string args = " save " + sub;
  std::error_code ec;
  for (const std::string& w : a.words)
    args += " \"" + fs::absolute(fs::path(w), ec).string() + "\"";
  for (const auto& [name, value] : a.flags) {
    if (name == "preset" || name == "engine" || name == "repo") continue;
    std::string v = value;
    if (name == "out") v = fs::absolute(fs::path(value), ec).string();
    args += " \"--" + name + (v.empty() ? "" : "=" + v) + "\"";
  }
  Outcome out;
  play::FlatJson m;
  std::string line;
  if (!play_command(o, args, m, line, out)) return out;
  out.result = line;
  if (sub == "write") {
    const std::string file = rel_path(o.repo, fs::path(text_of(m, "file")));
    out.written.push_back(file);
    out.summary = "saved after " + text_of(m, "step") + " steps: " + text_of(m, "bytes") +
                  " bytes in " + text_of(m, "save_ms") + " ms, digest " + text_of(m, "digest");
    out.next = "ge save load " + file + " --steps=1200";
  } else if (sub == "load") {
    out.summary = "loaded at step " + text_of(m, "step") + " in " + text_of(m, "load_ms") +
                  " ms (the map's build included), digest " + text_of(m, "digest_loaded") +
                  "; played on " + text_of(m, "played") + " steps to digest " +
                  text_of(m, "digest");
  } else if (sub == "check") {
    std::vector<std::vector<std::string>> rows = {
        {"step", "bytes", "save ms", "load ms", "parted"}};
    for (int i = 0; m.contains("at." + std::to_string(i) + ".step"); i++) {
      const std::string k = "at." + std::to_string(i) + ".";
      const std::string parted = text_of(m, k + "parted");
      rows.push_back({text_of(m, k + "step"), text_of(m, k + "bytes"), text_of(m, k + "save_ms"),
                      text_of(m, k + "load_ms"), parted == "-1" ? "-" : parted});
      if (parted != "-1") {
        out.findings.push_back(
            {Severity::error,
             "the run saved at step " + text_of(m, k + "step") +
                 " and loaded parts from the run never saved at step " + parted,
             a.words[0],
             "a field the step writes is missing from a describe: ge save write the log at both "
             "steps and ge save diff them against a fresh save (skill: save)"});
      }
    }
    out.text = table(rows);
    out.summary = text_of(m, "saves") +
                  " saves, each loaded and played on to the log's end as "
                  "the run never saved";
  } else {
    std::string text;
    if (text_of(m, "setup_differs") == "true")
      text += "  the saves' setups differ (seed or options): a different run, not a later step\n";
    for (int i = 0; m.contains("chunks." + std::to_string(i) + ".tag"); i++) {
      const std::string k = "chunks." + std::to_string(i) + ".";
      if (text_of(m, k + "same") == "true") continue;
      text += "  chunk " + text_of(m, k + "tag") + ": " + text_of(m, k + "a") + " vs " +
              text_of(m, k + "b") + " bytes, differs\n";
    }
    for (int i = 0; m.contains("fields." + std::to_string(i) + ".key"); i++) {
      const std::string k = "fields." + std::to_string(i) + ".";
      text += "    " + text_of(m, k + "key") + ": " + text_of(m, k + "a") + " vs " +
              text_of(m, k + "b") + "\n";
    }
    if (text_of(m, "more_fields") != "0")
      text += "    and " + text_of(m, "more_fields") + " more\n";
    out.text = text;
    out.summary = text_of(m, "same") == "true"
                      ? "the same run, to the byte"
                      : "saved at steps " + text_of(m, "step_a") + " and " + text_of(m, "step_b") +
                            ": " + text_of(m, "chunks_differ") + " chunks differ";
  }
  return out;
}

}  // namespace ge::cli
