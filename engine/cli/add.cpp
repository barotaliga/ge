// The definitions behind add.hpp: the plugins ge carries, the link added to CMakeLists.txt, and
// a template's files written with their placeholders filled.
//
// ge:summary ge use's CMake edit and skill sync; ge add's templates filled and written
// ge:keywords cli ge use add plugin template placeholders link cmake run_use run_add
// ge:skill agent-interface
// ge:test tests/cli_add_test.cpp
#include "cli/add.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <system_error>

#include "cli/build.hpp"
#include "cli/embedded.hpp"
#include "cli/json.hpp"
#include "cli/scaffold.hpp"
#include "cli/skills.hpp"

namespace ge::cli {

namespace {

constexpr std::string_view kTemplates = "templates/";

void replace_all(std::string& s, std::string_view from, std::string_view to) {
  for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) {
    s.replace(at, from.size(), to);
  }
}

std::string pascal_of(const std::string& snake) {
  std::string out;
  bool up = true;
  for (const char c : snake) {
    if (c == '_') {
      up = true;
      continue;
    }
    out += up ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
    up = false;
  }
  return out;
}

std::string kebab_of(std::string s) {
  std::replace(s.begin(), s.end(), '_', '-');
  return s;
}

std::vector<fs::path> cmake_files(const fs::path& repo) {
  std::vector<fs::path> out{repo / "CMakeLists.txt"};
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(repo / "cmake", ec)) {
    if (e.path().extension() == ".cmake") out.push_back(e.path());
  }
  return out;
}

}  // namespace

std::vector<PluginInfo> plugin_infos() {
  static const std::regex kCall(R"(ge_add_plugin\(\s*([a-z0-9]+)([^)]*)\))");
  std::vector<PluginInfo> out;
  for (const EmbeddedFile& f : embedded_files()) {
    if (!f.path.starts_with("plugins/") || !f.path.ends_with("/CMakeLists.txt")) continue;
    const std::string text(f.text);
    std::smatch m;
    if (!std::regex_search(text, m, kCall)) continue;
    PluginInfo p{m.str(1), {}};
    const std::string rest = m.str(2);
    const size_t at = rest.find("PLUGINS");
    if (at != std::string::npos) {
      std::string list = rest.substr(at + 7);
      if (const size_t app = list.find("APP"); app != std::string::npos) list.resize(app);
      static const std::regex kWord("[a-z0-9]+");
      for (auto it = std::sregex_iterator(list.begin(), list.end(), kWord);
           it != std::sregex_iterator(); ++it) {
        p.plugins.push_back(it->str());
      }
    }
    out.push_back(std::move(p));
  }
  std::sort(out.begin(), out.end(),
            [](const PluginInfo& a, const PluginInfo& b) { return a.name < b.name; });
  return out;
}

std::optional<std::string> game_name(const fs::path& repo) {
  const auto text = read_text(repo / "CMakeLists.txt");
  if (!text) return std::nullopt;
  static const std::regex kProject(R"(project\(\s*([A-Za-z0-9_-]+))");
  std::smatch m;
  if (!std::regex_search(*text, m, kProject)) return std::nullopt;
  return m.str(1);
}

bool links_plugin(const fs::path& repo, const std::string& plugin) {
  const std::regex link("ge::" + plugin + "\\b");
  for (const fs::path& f : cmake_files(repo)) {
    std::string text = read_text(f).value_or("");
    std::string code;
    for (const auto line : split_lines(text)) {
      const std::string l(line);
      code += l.substr(0, l.find('#')) + "\n";
    }
    if (std::regex_search(code, link)) return true;
  }
  return false;
}

Outcome run_use(const fs::path& repo, const std::string& plugin) {
  Outcome o;
  const auto plugins = plugin_infos();
  const auto it = std::find_if(plugins.begin(), plugins.end(),
                               [&](const PluginInfo& p) { return p.name == plugin; });
  if (it == plugins.end()) {
    std::string names;
    for (const PluginInfo& p : plugins) names += (names.empty() ? "" : ", ") + p.name;
    o.findings.push_back({Severity::error, "the engine has no plugin \"" + plugin + "\"", "",
                          "name one of: " + names});
    o.exit = 2;
    return o;
  }
  const auto name = game_name(repo);
  if (!name) {
    o.findings.push_back({Severity::error, "no project() in CMakeLists.txt", "CMakeLists.txt",
                          "run ge in a game repo (`ge new` makes one), or pass --repo=<dir>"});
    return o;
  }
  if (links_plugin(repo, plugin)) {
    o.summary = "ge::" + plugin + " is already linked";
  } else {
    const fs::path cmake = repo / "CMakeLists.txt";
    std::string text = read_text(cmake).value_or("");
    const std::string target = ident_of(*name) + "_sim";
    const std::regex link("(target_link_libraries\\(\\s*" + target + "\\s+PUBLIC\\s+ge::core)");
    if (!std::regex_search(text, link)) {
      o.findings.push_back(
          {Severity::error,
           "no `target_link_libraries(" + target + " PUBLIC ge::core ...)` to add to",
           "CMakeLists.txt",
           "add ge::" + plugin +
               " to the link of the target that holds the simulation, then "
               "`ge skills sync`"});
      return o;
    }
    text =
        std::regex_replace(text, link, "$1 ge::" + plugin, std::regex_constants::format_first_only);
    std::string error;
    if (!write_text(cmake, text, error)) {
      o.findings.push_back(
          {Severity::error, error, "CMakeLists.txt", "check the file's permissions"});
      return o;
    }
    o.written.push_back("CMakeLists.txt");
    o.summary = "linked ge::" + plugin + " into " + target;
  }
  SyncResult sync = sync_skills(repo, false);
  o.written.insert(o.written.end(), sync.written.begin(), sync.written.end());
  o.findings.insert(o.findings.end(), sync.findings.begin(), sync.findings.end());
  const std::string skill = std::string(kSkillsDir) + "/" + plugin + "/SKILL.md";
  const bool has_skill = read_text(repo / skill).has_value();
  o.result = json_obj({{"plugin", json_str(plugin)},
                       {"target", json_str("ge::" + plugin)},
                       {"links", json_strs(it->plugins)},
                       {"skill", has_skill ? json_str(skill) : "null"}});
  o.next = has_skill ? "read " + skill + ", then `ge build`" : "ge build";
  return o;
}

Outcome run_add(const fs::path& repo, const std::string& kind, const std::string& name_typed,
                const std::string& lab_kind_typed, bool dry_run) {
  Outcome o;
  const std::string lab_kind = lab_kind_typed.empty() ? "sim" : lab_kind_typed;
  if (kind != "lab" && !lab_kind_typed.empty()) {
    o.findings.push_back(
        {Severity::error, "--kind is for ge add lab only", "", "drop --kind, or run ge add lab"});
    o.exit = 2;
    return o;
  }
  const std::string dir = kind == "lab" ? "lab-" + lab_kind : kind;
  const std::string prefix = std::string(kTemplates) + dir + "/";
  bool known = false;
  for (const EmbeddedFile& f : embedded_files()) known = known || f.path.starts_with(prefix);
  if (!known) {
    o.findings.push_back({Severity::error, "no lab kind \"" + lab_kind + "\"", "",
                          "use --kind=sim, scenario, picture or bench (skill: labs)"});
    o.exit = 2;
    return o;
  }
  std::string snake = name_typed;
  std::replace(snake.begin(), snake.end(), '-', '_');
  if (!std::regex_match(snake, std::regex("^[a-z][a-z0-9]*(_[a-z0-9]+)*$"))) {
    o.findings.push_back({Severity::error,
                          "\"" + name_typed +
                              "\" is not a name: lower case, words joined by _ "
                              "or - (wind_gust)",
                          "", "rerun with a name like wind_gust"});
    o.exit = 2;
    return o;
  }
  const auto game = game_name(repo);
  if (!game) {
    o.findings.push_back({Severity::error, "no project() in CMakeLists.txt", "CMakeLists.txt",
                          "run ge in a game repo (`ge new` makes one), or pass --repo=<dir>"});
    return o;
  }
  auto fill = [&](std::string s) {
    replace_all(s, "{{name}}", *game);
    replace_all(s, "{{ident}}", ident_of(*game));
    replace_all(s, "{{snake}}", snake);
    replace_all(s, "{{kebab}}", kebab_of(snake));
    replace_all(s, "{{Type}}", pascal_of(snake));
    return s;
  };
  std::vector<std::pair<std::string, std::string>> files;
  for (const EmbeddedFile& f : embedded_files()) {
    if (!f.path.starts_with(prefix)) continue;
    std::string rel = fill(std::string(f.path.substr(prefix.size())));
    if (rel.ends_with(".in")) rel.resize(rel.size() - 3);
    files.emplace_back(rel, fill(std::string(f.text)));
  }
  for (const auto& [rel, text] : files) {
    std::error_code ec;
    if (fs::exists(repo / rel, ec)) {
      o.findings.push_back({Severity::error, rel + " exists; ge add never writes over a file", rel,
                            "pick another name, or edit the file you have"});
    }
  }
  if (!o.findings.empty()) return o;
  std::vector<std::string> paths;
  for (const auto& [rel, text] : files) {
    paths.push_back(rel);
    if (dry_run) continue;
    std::string error;
    if (!write_text(repo / rel, text, error)) {
      o.findings.push_back({Severity::error, error, rel, "check the directory's permissions"});
      return o;
    }
    o.written.push_back(rel);
  }
  // A template is formatted for a typical name; the repo's .clang-format settles the real one's
  // line breaks, so the piece passes `ge check` as it lands. Without clang-format, ge check says
  // so.
  if (!dry_run && run_quiet("clang-format --version") == 0) {
    std::string cmd = "clang-format -i";
    for (const std::string& p : paths) cmd += " \"" + (repo / p).string() + "\"";
    run_quiet(cmd);
  }
  o.result = json_obj({{"kind", json_str(kind)},
                       {"lab_kind", kind == "lab" ? json_str(lab_kind) : "null"},
                       {"name", json_str(snake)},
                       {"files", json_strs(paths)},
                       {"dry_run", json_bool(dry_run)}});
  if (dry_run) {
    std::string list;
    for (const std::string& p : paths) list += "  would write " + p + "\n";
    o.text = list;
  }
  o.summary = (dry_run ? "would add " : "added ") + kind + " " + snake;
  if (kind == "system") {
    o.next = "wire it into sim/game.hpp and sim/game.cpp (its header says how), then ge test";
  } else if (kind == "lab") {
    o.next = "make it measure your question, then ge lab " + kebab_of(snake);
  } else {
    o.next = "write its cases, then ge test --filter=" + snake;
  }
  return o;
}

}  // namespace ge::cli
