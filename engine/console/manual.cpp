// The reference manual's source, defined: a console described, the text written and read back.
//
// ge:summary The reference manual's source defined: describe, manual_text, read_manual, the file
// ge:keywords manual describe manual_text read_manual write_manual manual_of manual-source
// ge:skill console
// ge:test tests/console_manual_test.cpp
#include "console/manual.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "console/catalog.hpp"
#include "console/cfg.hpp"
#include "console/console.hpp"
#include "core/num_text.hpp"

namespace ge::console {

namespace {

constexpr int kFields = 14;

// A field kept on its line: a tab or a line break read as a space.
std::string flat(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  return out;
}

std::string bound(double v) {
  constexpr double kInf = std::numeric_limits<double>::infinity();
  return v == kInf || v == -kInf ? "" : core::num::to_string(v);
}

const char* overlap_word(Overlap o) {
  switch (o) {
    case Overlap::same:
      return "same";
    case Overlap::own:
      return "own";
    case Overlap::none:
      break;
  }
  return "";
}

// The words of `line` split at blanks (a header line's).
std::vector<std::string> words(std::string_view line) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && line[i] == ' ') ++i;
    const size_t start = i;
    while (i < line.size() && line[i] != ' ') ++i;
    if (i > start) out.emplace_back(line.substr(start, i - start));
  }
  return out;
}

// A header's id, then its name to the end of the line (a game's ids it takes in after a '|').
bool read_name(std::string_view rest, ManualName& out) {
  while (!rest.empty() && rest.front() == ' ') rest.remove_prefix(1);
  const size_t sp = rest.find(' ');
  if (sp == std::string_view::npos || sp == 0) return false;
  out.id = std::string(rest.substr(0, sp));
  std::string_view name = rest.substr(sp + 1);
  if (const size_t bar = name.find(" | "); bar != std::string_view::npos) {
    out.takes_in = words(name.substr(bar + 3));
    name = name.substr(0, bar);
  }
  out.name = std::string(name);
  return !out.name.empty();
}

std::string name_line(std::string_view what, const ManualName& n) {
  std::string out = "# " + std::string(what) + ' ' + flat(n.id) + ' ' + flat(n.name);
  if (!n.takes_in.empty()) {
    out += " |";
    for (const std::string& id : n.takes_in) out += ' ' + id;
  }
  return out + '\n';
}

}  // namespace

const char* kind_word(ManualKind kind) {
  switch (kind) {
    case ManualKind::setting:
      return "setting";
    case ManualKind::command:
      return "command";
    case ManualKind::flag:
      return "flag";
    case ManualKind::setting_synonym:
      return "setting-synonym";
    case ManualKind::command_synonym:
      return "command-synonym";
  }
  return "setting";
}

bool kind_named(std::string_view word, ManualKind& out) {
  for (const ManualKind k : {ManualKind::setting, ManualKind::command, ManualKind::flag,
                             ManualKind::setting_synonym, ManualKind::command_synonym})
    if (word == kind_word(k)) {
      out = k;
      return true;
    }
  return false;
}

std::vector<ManualEntry> describe_settings(const Cvars& cvars, std::string_view layer) {
  std::vector<ManualEntry> out;
  for (const Cvar* v : cvars.sorted()) {
    if ((v->flags & kCreated) != 0) continue;
    ManualEntry& e = out.emplace_back();
    e.kind = ManualKind::setting;
    e.name = v->name;
    e.layer = layer.empty() ? v->layer : std::string(layer);
    const bool ranged = v->kind == CvarKind::number || v->kind == CvarKind::integer;
    e.type = kind_name(v->kind);
    e.fallback = v->fallback;
    e.min = ranged ? bound(v->min) : "";
    e.max = ranged ? bound(v->max) : "";
    e.flags = flags_text(v->flags);
    e.help = v->help;
    e.values = v->values;
    // A game's layer over an earlier one's name: how they meet, and the layer the name is from.
    if (!v->earlier.empty() && v->earlier.front().was)
      e.overlap =
          std::string(overlap_word(v->earlier.back().how)) + ':' + v->earlier.front().was->layer;
  }
  return out;
}

std::vector<ManualEntry> describe(const Console& console, std::string_view program,
                                  std::string_view games) {
  std::vector<ManualEntry> out;
  const auto entry = [&](ManualKind kind, std::string name, std::string layer) -> ManualEntry& {
    ManualEntry& e = out.emplace_back();
    e.kind = kind;
    e.program = std::string(program);
    e.games = std::string(games);
    e.name = std::move(name);
    e.layer = std::move(layer);
    return e;
  };
  const Cvars& cvars = console.cvars();
  for (ManualEntry& e : describe_settings(cvars)) {
    e.program = std::string(program);
    e.games = std::string(games);
    out.push_back(std::move(e));
  }
  for (const Synonym& s : cvars.synonyms()) {
    if (s.target == nullptr || s.target->retired) continue;
    ManualEntry& e = entry(ManualKind::setting_synonym, s.name, s.layer);
    e.target = s.target->name;
    if (s.part >= 0) e.target += '[' + std::to_string(s.part) + ']';
    e.help = s.target->help;
  }
  for (const auto& [name, c] : console.commands()) {
    ManualEntry& e = entry(ManualKind::command, name, c.layer);
    e.help = c.help;
  }
  for (const auto& [name, target] : console.command_synonyms()) {
    ManualEntry& e = entry(ManualKind::command_synonym, name, "");
    e.target = target;
    if (const auto it = console.commands().find(target); it != console.commands().end()) {
      e.help = it->second.help;
      e.layer = it->second.layer;
    }
  }
  for (const FlagDoc& f : console.manual().flags) {
    ManualEntry& e = entry(ManualKind::flag, f.name, "");
    e.target = f.arg;
    e.help = f.help;
  }
  // The settings outside the console (ManualInfo::more), less a name the console has.
  if (console.manual().more) {
    std::vector<ManualEntry> more;
    console.manual().more(games, more);
    for (ManualEntry& e : more) {
      if (cvars.find(e.name) != nullptr || cvars.synonym(e.name) != nullptr ||
          console.commands().count(e.name) != 0)
        continue;
      e.program = std::string(program);
      e.games = std::string(games);
      out.push_back(std::move(e));
    }
  }
  return out;
}

std::string manual_text(const ManualSource& source) {
  std::string out = "# manual 1\n";
  for (const ManualName& p : source.programs) out += name_line("program", p);
  for (const ManualName& g : source.games) out += name_line("game", g);
  for (const ManualEntry& e : source.entries) {
    const std::string* f[] = {&e.program,  &e.games, &e.name,  &e.layer, &e.type,
                              &e.fallback, &e.min,   &e.max,   &e.flags, &e.target,
                              &e.overlap,  &e.help,  &e.values};
    out += kind_word(e.kind);
    for (const std::string* s : f) out += '\t' + flat(*s);
    out += '\n';
  }
  return out;
}

bool read_manual(std::string_view text, ManualSource& out, std::string& error) {
  int line_no = 0;
  bool versioned = false;
  while (!text.empty()) {
    const size_t nl = text.find('\n');
    std::string_view line = text.substr(0, nl);
    text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
    line_no++;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    const auto bad = [&](const std::string& what) {
      error = "the manual source's line " + std::to_string(line_no) + " " + what +
              ". Write it again with the program's writemanual command.";
      return false;
    };
    if (line.empty()) continue;
    if (line.starts_with("# ")) {
      const std::string_view rest = line.substr(2);
      ManualName n;
      if (rest == "manual 1") {
        versioned = true;
      } else if (rest.starts_with("manual ")) {
        return bad("is a version this ge does not read ('" + std::string(rest) + "')");
      } else if (rest.starts_with("program ")) {
        if (!read_name(rest.substr(8), n)) return bad("names no program");
        out.programs.push_back(std::move(n));
      } else if (rest.starts_with("game ")) {
        if (!read_name(rest.substr(5), n)) return bad("names no game");
        out.games.push_back(std::move(n));
      }
      continue;
    }
    if (!versioned) return bad("comes before the '# manual 1' line");
    std::vector<std::string_view> f;
    size_t at = 0;
    while (true) {
      const size_t tab = line.find('\t', at);
      f.push_back(
          line.substr(at, tab == std::string_view::npos ? std::string_view::npos : tab - at));
      if (tab == std::string_view::npos) break;
      at = tab + 1;
    }
    if (static_cast<int>(f.size()) != kFields)
      return bad("has " + std::to_string(f.size()) + " fields, not " + std::to_string(kFields));
    ManualEntry e;
    if (!kind_named(f[0], e.kind)) return bad("has no kind ('" + std::string(f[0]) + "')");
    std::string* to[] = {&e.program, &e.games, &e.name,   &e.layer,   &e.type, &e.fallback, &e.min,
                         &e.max,     &e.flags, &e.target, &e.overlap, &e.help, &e.values};
    for (int i = 1; i < kFields; ++i) *to[i - 1] = std::string(f[static_cast<size_t>(i)]);
    if (e.name.empty()) return bad("has no name");
    out.entries.push_back(std::move(e));
  }
  return true;
}

ManualSource manual_of(const Console& console) {
  ManualSource s;
  const ManualInfo& m = console.manual();
  if (!m.program.id.empty()) s.programs.push_back(m.program);
  s.games = m.games;
  s.entries = describe(console, m.program.id, m.pass);
  return s;
}

bool write_manual(const std::string& path, const ManualSource& source, std::string& error) {
  std::error_code ec;
  const std::filesystem::path p(path);
  if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary);
  const std::string text = manual_text(source);
  if (!f || !f.write(text.data(), static_cast<std::streamsize>(text.size()))) {
    error = "Could not write the manual to " + path + ".";
    return false;
  }
  return true;
}

}  // namespace ge::console
