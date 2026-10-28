// The definitions behind skills.hpp: front matter and fragments parsed, the lock read and
// written, the sync's rules, the AGENTS.md block, and the lesson reader.
//
// ge:summary ge skills sync, the lock and the AGENTS.md block; skill and lesson checks; ge lessons
// ge:keywords cli ge skills sync lock agents-md block lessons fragments front-matter
// ge:keywords sync_skills check_skills check_engine_skills print_lessons parse_lesson
// ge:skill agent-interface
// ge:test tests/cli_skills_test.cpp
#include "cli/skills.hpp"

#include <algorithm>
#include <map>
#include <regex>
#include <system_error>

#include "cli/add.hpp"
#include "cli/embedded.hpp"

namespace ge::cli {

namespace {

constexpr std::string_view kPrefix = "consumer-skills/";
constexpr std::string_view kSuffix = "/SKILL.md";

std::string trim(std::string_view s) {
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string_view::npos) return "";
  const size_t b = s.find_last_not_of(" \t");
  return std::string(s.substr(a, b - a + 1));
}

std::string unquote(std::string_view v) {
  std::string s = trim(v);
  if (s.size() >= 2 && (s.front() == '"' || s.front() == '\'') && s.back() == s.front()) {
    s = s.substr(1, s.size() - 2);
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
      if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '"') continue;
      out.push_back(s[i]);
    }
    return out;
  }
  return s;
}

std::vector<std::string> list_of(std::string_view v) {
  std::string s = trim(v);
  if (!s.empty() && s.front() == '[') s.erase(0, 1);
  if (!s.empty() && s.back() == ']') s.pop_back();
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    const size_t comma = std::min(s.find(',', start), s.size());
    std::string item = unquote(std::string_view(s).substr(start, comma - start));
    if (!item.empty()) out.push_back(item);
    start = comma + 1;
  }
  return out;
}

// The front matter's key/value lines and what follows it; false when there is none.
bool split_front_matter(std::string_view text, std::map<std::string, std::string>& meta,
                        std::string& body) {
  if (!text.starts_with("---\n")) return false;
  const size_t end = text.find("\n---", 3);
  if (end == std::string_view::npos) return false;
  for (const std::string_view line : split_lines(text.substr(4, end - 4))) {
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos) continue;
    meta[trim(line.substr(0, colon))] = trim(line.substr(colon + 1));
  }
  size_t after = end + 4;
  if (after < text.size() && text[after] == '\n') after++;
  body = std::string(text.substr(std::min(after, text.size())));
  return true;
}

std::string skill_path(std::string_view name) {
  return std::string(kSkillsDir) + "/" + std::string(name) + "/SKILL.md";
}

std::string upstream_next(std::string_view name) {
  return "move your change into a lesson (.agents/skills/" + std::string(name) +
         "/.lessons/, skill: skill-reflection) and run `make skills` to restore the engine's "
         "text; a change every game needs goes upstream as an issue on " +
         std::string(kEngineRepo);
}

std::string expected_block(const fs::path& repo) {
  std::string block(kBlockBegin);
  block +=
      "\n<!-- Written by `ge skills sync` from the engine's consumer skills: edits here are "
      "overwritten. -->\n| Skill | Load it when |\n| --- | --- |\n";
  for (const ConsumerSkill& s : skills_for(repo)) {
    const auto doc = parse_skill(s.text);
    std::string when = doc ? doc->description : "";
    std::string escaped;
    for (const char c : when) {
      if (c == '|') escaped += '\\';
      escaped += c;
    }
    block += "| `" + s.name + "` | " + escaped + " |\n";
  }
  block += kBlockEnd;
  return block;
}

// AGENTS.md with its block replaced, or nothing when it has no block.
std::optional<std::string> with_block(const fs::path& repo, std::string_view agents) {
  const size_t a = agents.find(kBlockBegin);
  const size_t b = agents.find(kBlockEnd);
  if (a == std::string_view::npos || b == std::string_view::npos || b < a) return std::nullopt;
  return std::string(agents.substr(0, a)) + expected_block(repo) +
         std::string(agents.substr(b + kBlockEnd.size()));
}

Finding no_block_finding() {
  return {Severity::error,
          "AGENTS.md has no engine skills block (" + std::string(kBlockBegin) + " ... " +
              std::string(kBlockEnd) + ")",
          "AGENTS.md",
          "add the two marker lines, each on its own line, where the router lists the engine's "
          "skills, then run `make skills`"};
}

void write_lock(const fs::path& repo, const std::vector<LockEntry>& entries,
                std::vector<Finding>& findings) {
  std::string text =
      "# Written by `ge skills sync`: the engine's skills in this repo, the engine version they\n"
      "# came from and each SKILL.md's FNV-1a 64 as written. Do not edit; run `make skills`.\n";
  text += "engine " + std::string(engine_version()) + "\n";
  for (const LockEntry& e : entries) text += "skill " + e.name + " " + e.hash + "\n";
  std::string error;
  if (!write_text(repo / kLockFile, text, error)) {
    findings.push_back({Severity::error, error, std::string(kLockFile),
                        "check the directory's permissions and rerun `make skills`"});
  }
}

void ensure_lessons_dir(const fs::path& dir) {
  std::error_code ec;
  if (fs::exists(dir / ".lessons", ec)) return;
  std::string error;
  write_text(dir / ".lessons" / ".gitkeep", "", error);
}

bool overlaps(std::string_view a, std::string_view b) {
  auto norm = [](std::string_view p) {
    std::string s(p);
    if (s.starts_with("./")) s.erase(0, 2);
    while (!s.empty() && s.back() == '/') s.pop_back();
    return s;
  };
  const std::string x = norm(a), y = norm(b);
  return x == y || x.starts_with(y + "/") || y.starts_with(x + "/");
}

std::vector<std::string> skill_dirs(const fs::path& repo) {
  std::vector<std::string> names;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(repo / kSkillsDir, ec)) {
    if (entry.is_directory()) names.push_back(entry.path().filename().string());
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::vector<std::pair<std::string, fs::path>> lesson_files(const fs::path& skill_dir) {
  std::vector<std::pair<std::string, fs::path>> files;
  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(skill_dir / ".lessons", ec)) {
    const std::string name = entry.path().filename().string();
    if (entry.is_regular_file() && name.ends_with(".md")) files.emplace_back(name, entry.path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

}  // namespace

std::optional<SkillDoc> parse_skill(std::string_view text) {
  std::map<std::string, std::string> meta;
  std::string body;
  if (!split_front_matter(text, meta, body)) return std::nullopt;
  return SkillDoc{unquote(meta["name"]), unquote(meta["description"]), unquote(meta["plugin"])};
}

std::vector<ConsumerSkill> consumer_skills() {
  std::vector<ConsumerSkill> skills;
  for (const EmbeddedFile& f : embedded_files()) {
    if (!f.path.starts_with(kPrefix) || !f.path.ends_with(kSuffix)) continue;
    const std::string_view name =
        f.path.substr(kPrefix.size(), f.path.size() - kPrefix.size() - kSuffix.size());
    if (name.find('/') == std::string_view::npos) skills.push_back({std::string(name), f.text});
  }
  return skills;
}

std::vector<ConsumerSkill> skills_for(const fs::path& repo) {
  // The plugins the repo links, and every plugin those link in turn (skier brings mountain).
  const auto infos = plugin_infos();
  std::vector<std::string> linked;
  for (const PluginInfo& p : infos) {
    if (links_plugin(repo, p.name)) linked.push_back(p.name);
  }
  for (size_t i = 0; i < linked.size(); i++) {
    for (const PluginInfo& p : infos) {
      if (p.name != linked[i]) continue;
      for (const std::string& dep : p.plugins) {
        if (std::find(linked.begin(), linked.end(), dep) == linked.end()) linked.push_back(dep);
      }
    }
  }
  std::vector<ConsumerSkill> out;
  for (ConsumerSkill& s : consumer_skills()) {
    const auto doc = parse_skill(s.text);
    // `plugin:` names a plugin, or an engine module a game links on its own (ge::water).
    if (doc && !doc->plugin.empty() &&
        std::find(linked.begin(), linked.end(), doc->plugin) == linked.end() &&
        !links_plugin(repo, doc->plugin)) {
      continue;
    }
    out.push_back(std::move(s));
  }
  return out;
}

Lesson parse_lesson(std::string_view file_name, std::string_view text) {
  static const std::regex kFileRe(R"(^\d{9,}-[a-z0-9][a-z0-9-]*\.md$)");
  static const std::regex kDateRe(R"(^\d{4}-\d{2}-\d{2}$)");
  static const std::regex kConceptRe(R"(^[a-z0-9][a-z0-9-]*$)");
  Lesson l;
  l.file = file_name;
  if (!std::regex_match(l.file, kFileRe)) {
    l.errors.push_back("file name must be <unix-timestamp>-<kebab-slug>.md (date +%s)");
  }
  std::map<std::string, std::string> meta;
  if (!split_front_matter(text, meta, l.body)) {
    l.errors.push_back("no front matter (--- title, date[, scope, concepts] ---)");
    l.body = trim(text);
    return l;
  }
  for (const auto& [key, value] : meta) {
    if (key != "title" && key != "date" && key != "scope" && key != "concepts") {
      l.errors.push_back("unknown front-matter key \"" + key + "\"");
    }
  }
  l.title = unquote(meta["title"]);
  l.date = unquote(meta["date"]);
  if (l.title.empty()) l.errors.push_back("no title (one line, written as a claim)");
  if (!std::regex_match(l.date, kDateRe)) {
    l.errors.push_back("date \"" + l.date + "\" is not YYYY-MM-DD");
  }
  l.scope = list_of(meta["scope"]);
  for (const std::string& p : l.scope) {
    if (p.starts_with("/") || p.find("..") != std::string::npos) {
      l.errors.push_back("scope \"" + p + "\" must be a repo-relative path");
    }
  }
  l.concepts = list_of(meta["concepts"]);
  for (const std::string& c : l.concepts) {
    if (!std::regex_match(c, kConceptRe)) {
      l.errors.push_back("concept \"" + c + "\" is not lower-case kebab");
    }
  }
  while (!l.body.empty() && (l.body.back() == '\n' || l.body.back() == ' ')) l.body.pop_back();
  while (!l.body.empty() && l.body.front() == '\n') l.body.erase(0, 1);
  if (l.body.empty()) l.errors.push_back("empty body");
  return l;
}

std::vector<Finding> check_skills(const fs::path& repo) {
  std::vector<Finding> out;
  std::error_code ec;
  if (!fs::is_directory(repo / kSkillsDir, ec)) {
    out.push_back({Severity::error, std::string(kSkillsDir) + " does not exist", "",
                   "run `make skills` to write the engine's skills (`ge new` scaffolds a new "
                   "repo with them)"});
    return out;
  }
  const std::string agents = read_text(repo / "AGENTS.md").value_or("");
  for (const std::string& name : skill_dirs(repo)) {
    const fs::path dir = repo / kSkillsDir / name;
    const std::string where = skill_path(name);
    const auto text = read_text(dir / "SKILL.md");
    if (!text) {
      out.push_back({Severity::error, "skill directory " + name + "/ has no SKILL.md", where,
                     "write its SKILL.md (front matter: name, description) or delete the "
                     "directory; skill: skill-reflection"});
      continue;
    }
    const auto doc = parse_skill(*text);
    if (!doc || doc->name != name) {
      out.push_back({Severity::error,
                     "SKILL.md's front-matter name is \"" + (doc ? doc->name : "") +
                         "\", not its directory's \"" + name + "\"",
                     where, "set `name: " + name + "` in its front matter"});
    }
    if (!doc || doc->description.empty()) {
      out.push_back({Severity::error, "skill " + name + " has no description", where,
                     "add `description:` to its front matter: when to load it, in the words an "
                     "agent would search for"});
    }
    if (!fs::is_directory(dir / ".lessons", ec)) {
      out.push_back({Severity::error, "skill " + name + " has no .lessons/ directory",
                     std::string(kSkillsDir) + "/" + name,
                     "create .lessons/ with an empty .gitkeep; skill: skill-reflection"});
    }
    if (!agents.empty() && agents.find("`" + name + "`") == std::string::npos) {
      out.push_back({Severity::error, "skill " + name + " is not routed from AGENTS.md",
                     "AGENTS.md",
                     "add a row naming `" + name +
                         "` to AGENTS.md's skills table, saying when "
                         "to load it"});
    }
    for (const auto& [file, path] : lesson_files(dir)) {
      const Lesson l = parse_lesson(file, read_text(path).value_or(""));
      for (const std::string& e : l.errors) {
        out.push_back({Severity::error, "lesson fragment: " + e, rel_path(repo, path),
                       "fix the fragment's format; skill: skill-reflection, § The fragment "
                       "format"});
      }
    }
  }
  return out;
}

std::optional<Lock> read_lock(const fs::path& repo) {
  const auto text = read_text(repo / kLockFile);
  if (!text) return std::nullopt;
  Lock lock;
  for (const std::string_view line : split_lines(*text)) {
    if (line.starts_with("engine ")) lock.engine = trim(line.substr(7));
    if (line.starts_with("skill ")) {
      const std::string rest = trim(line.substr(6));
      const size_t space = rest.find(' ');
      if (space != std::string::npos) {
        lock.skills.push_back({rest.substr(0, space), trim(rest.substr(space + 1))});
      }
    }
  }
  return lock;
}

std::vector<Finding> check_engine_skills(const fs::path& repo) {
  std::vector<Finding> out;
  const auto lock = read_lock(repo);
  if (!lock) {
    out.push_back({Severity::error, "the engine's skills were never synced into this repo",
                   std::string(kLockFile), "run `make skills`"});
    return out;
  }
  if (lock->engine != engine_version()) {
    out.push_back({Severity::error,
                   "the engine skills came from engine " + lock->engine + "; this ge is " +
                       std::string(engine_version()),
                   std::string(kLockFile),
                   "run `make skills`, which runs the ge your engine pin builds"});
  }
  std::map<std::string, std::string> locked;
  for (const LockEntry& e : lock->skills) locked[e.name] = e.hash;
  for (const ConsumerSkill& s : skills_for(repo)) {
    const std::string where = skill_path(s.name);
    const auto it = locked.find(s.name);
    if (it == locked.end()) {
      out.push_back({Severity::error, "engine skill " + s.name + " is not in the lock", where,
                     "run `make skills`; if the repo has its own skill named " + s.name +
                         ", rename that one first"});
      continue;
    }
    const auto disk = read_text(repo / where);
    if (!disk) {
      out.push_back(
          {Severity::error, "engine skill " + s.name + " is missing", where, "run `make skills`"});
    } else if (fnv1a64_hex(*disk) != it->second) {
      out.push_back({Severity::error, "engine skill " + s.name + " was edited by hand", where,
                     upstream_next(s.name)});
    } else if (*disk != s.text && lock->engine == engine_version()) {
      out.push_back({Severity::error, "engine skill " + s.name + " is older than this ge's", where,
                     "run `make skills`"});
    }
  }
  const auto agents = read_text(repo / "AGENTS.md");
  if (agents) {
    const auto updated = with_block(repo, *agents);
    if (!updated) {
      out.push_back(no_block_finding());
    } else if (*updated != *agents) {
      out.push_back({Severity::error, "AGENTS.md's engine skills block is not what sync writes",
                     "AGENTS.md",
                     "run `make skills`; route the repo's own skills outside the block"});
    }
  }
  return out;
}

SyncResult sync_skills(const fs::path& repo, bool force) {
  SyncResult r;
  std::map<std::string, std::string> prior;
  if (const auto lock = read_lock(repo)) {
    for (const LockEntry& e : lock->skills) prior[e.name] = e.hash;
  }
  std::vector<LockEntry> entries;
  std::vector<std::string> carried;
  for (const ConsumerSkill& s : skills_for(repo)) {
    carried.push_back(s.name);
    const std::string where = skill_path(s.name);
    const fs::path path = repo / where;
    const auto disk = read_text(path);
    const auto was = prior.find(s.name);
    const bool owned = disk && was != prior.end() && fnv1a64_hex(*disk) == was->second;
    if (disk && *disk != s.text && !owned && !force) {
      if (was != prior.end()) {
        r.findings.push_back({Severity::error,
                              "engine skill " + s.name + " was edited by hand; sync left it", where,
                              upstream_next(s.name) + " (or `ge skills sync --force`)"});
        entries.push_back({s.name, was->second});
      } else {
        r.findings.push_back(
            {Severity::error,
             "the repo has its own skill named " + s.name + "; sync did not write the engine's",
             where,
             "rename the repo's skill (its directory and its front-matter "
             "name), then run `make skills`; `ge skills sync --force` replaces "
             "it instead"});
      }
      continue;
    }
    if (!disk || *disk != s.text) {
      std::string error;
      if (!write_text(path, s.text, error)) {
        r.findings.push_back(
            {Severity::error, error, where, "check the directory's permissions and rerun"});
        continue;
      }
      r.written.push_back(where);
    }
    ensure_lessons_dir(path.parent_path());
    entries.push_back({s.name, fnv1a64_hex(s.text)});
  }
  // The skills the engine no longer ships: removed while unedited, their lessons kept.
  for (const auto& [name, hash] : prior) {
    if (std::find(carried.begin(), carried.end(), name) != carried.end()) continue;
    const fs::path dir = repo / kSkillsDir / name;
    const auto disk = read_text(dir / "SKILL.md");
    if (disk && fnv1a64_hex(*disk) != hash) {
      r.findings.push_back({Severity::warn,
                            "the engine dropped skill " + name + "; yours was edited, so it stays",
                            skill_path(name),
                            "it is the repo's own skill now: keep it routed from AGENTS.md, or "
                            "delete its directory"});
      continue;
    }
    std::error_code ec;
    fs::remove(dir / "SKILL.md", ec);
    r.written.push_back(skill_path(name) + " (removed)");
    if (lesson_files(dir).empty()) {
      fs::remove_all(dir, ec);
    } else {
      r.findings.push_back({Severity::warn,
                            "the engine dropped skill " + name + "; its lessons are kept",
                            std::string(kSkillsDir) + "/" + name + "/.lessons",
                            "move each lesson that still holds into the skill that owns its "
                            "subject now, then delete the directory; skill: skill-reflection"});
    }
  }
  write_lock(repo, entries, r.findings);
  const auto agents = read_text(repo / "AGENTS.md");
  if (!agents) {
    r.findings.push_back({Severity::error, "AGENTS.md is missing", "AGENTS.md",
                          "restore the router (`ge new` writes one; copy its shape), then "
                          "rerun `make skills`"});
  } else if (const auto updated = with_block(repo, *agents); !updated) {
    r.findings.push_back(no_block_finding());
  } else if (*updated != *agents) {
    std::string error;
    if (write_text(repo / "AGENTS.md", *updated, error)) {
      r.written.push_back("AGENTS.md");
    } else {
      r.findings.push_back(
          {Severity::error, error, "AGENTS.md", "check the file's permissions and rerun"});
    }
  }
  return r;
}

std::vector<Lesson> lessons_of(const fs::path& repo, std::string_view skill,
                               std::string_view scope) {
  std::vector<Lesson> out;
  for (const auto& [file, path] : lesson_files(repo / kSkillsDir / std::string(skill))) {
    Lesson l = parse_lesson(file, read_text(path).value_or(""));
    if (!scope.empty() && !l.scope.empty() &&
        std::none_of(l.scope.begin(), l.scope.end(),
                     [&](const std::string& p) { return overlaps(p, scope); })) {
      continue;
    }
    out.push_back(std::move(l));
  }
  return out;
}

std::vector<std::pair<std::string, int>> lesson_inventory(const fs::path& repo) {
  std::vector<std::pair<std::string, int>> out;
  for (const std::string& name : skill_dirs(repo)) {
    out.emplace_back(name, static_cast<int>(lesson_files(repo / kSkillsDir / name).size()));
  }
  return out;
}

int print_lessons(const fs::path& repo, std::string_view skill, std::string_view scope,
                  std::ostream& out) {
  const std::vector<std::string> names = skill_dirs(repo);
  if (skill.empty()) {
    for (const std::string& name : names) {
      const size_t n = lesson_files(repo / kSkillsDir / name).size();
      out << name << "  " << n << (n == 1 ? " lesson" : " lessons") << "\n";
    }
    return 0;
  }
  if (std::find(names.begin(), names.end(), skill) == names.end()) {
    std::string all;
    for (const std::string& name : names) all += (all.empty() ? "" : ", ") + name;
    out << format_finding("ge lessons", {Severity::error, "no skill \"" + std::string(skill) + "\"",
                                         std::string(kSkillsDir), "name one of: " + all});
    return 2;
  }
  int shown = 0;
  for (const Lesson& l : lessons_of(repo, skill, scope)) {
    const std::string& file = l.file;
    std::string scopes, concepts;
    for (const std::string& p : l.scope) scopes += (scopes.empty() ? "" : ", ") + p;
    for (const std::string& c : l.concepts) concepts += (concepts.empty() ? "" : ", ") + c;
    out << "## " << l.title << "\n" << skill << "/.lessons/" << file << " · " << l.date;
    if (!scopes.empty()) out << " · scope " << scopes;
    if (!concepts.empty()) out << " · " << concepts;
    out << "\n\n" << l.body << "\n\n";
    shown++;
  }
  if (shown == 0) {
    out << "No lessons match. A skill with none is read from its SKILL.md alone; reflect into it "
           "at the end (skill: skill-reflection).\n";
  }
  return 0;
}

}  // namespace ge::cli
