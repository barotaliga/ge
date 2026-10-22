// The definitions behind index.hpp: reading tag blocks, finding the engine's tree, ranking a
// search, and resolving what `ge explain` was asked about.
//
// ge:summary ge find's ranking and ge explain's resolution over the repo's and engine's tag blocks
// ge:keywords cli ge find explain ranking score tags engine-root declaration header-comment
// ge:skill agent-interface
// ge:test tests/cli_index_test.cpp
#include "cli/index.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <system_error>

#include "cli/json.hpp"
#include "cli/skills.hpp"

#ifndef GE_ENGINE_SOURCE_DIR
#define GE_ENGINE_SOURCE_DIR ""
#endif

namespace ge::cli {

namespace {

constexpr int kTagLines = 60;  // a file's ge: block sits in its first 60 lines

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

std::vector<std::string> split_words(std::string_view s) {
  std::vector<std::string> out;
  std::string cur;
  for (const char c : s) {
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) out.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) out.push_back(cur);
  return out;
}

// A comment line's text without its marker, or nothing when the line is not a comment.
std::optional<std::string> comment_text(std::string_view line) {
  const size_t a = line.find_first_not_of(" \t");
  if (a == std::string_view::npos) return std::nullopt;
  std::string_view rest = line.substr(a);
  if (rest.starts_with("//")) {
    rest.remove_prefix(2);
  } else if (rest.starts_with("#") && !rest.starts_with("#include") &&
             !rest.starts_with("#pragma") && !rest.starts_with("#define") &&
             !rest.starts_with("#if") && !rest.starts_with("#endif")) {
    rest.remove_prefix(1);
  } else {
    return std::nullopt;
  }
  if (rest.starts_with(" ")) rest.remove_prefix(1);
  return std::string(rest);
}

bool is_source(const fs::path& p) {
  const std::string ext = p.extension().string();
  const std::string name = p.filename().string();
  return ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".cmake" ||
         name == "CMakeLists.txt";
}

void walk_into(const fs::path& root, const fs::path& dir, const std::string& origin,
               std::vector<TaggedFile>& out) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return;
  for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::end(it);
       it.increment(ec)) {
    if (it->is_symlink(ec)) {
      if (it->is_directory(ec)) it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory(ec)) {
      if (skipped_dir(it->path().filename().string())) it.disable_recursion_pending();
      continue;
    }
    if (!is_source(it->path())) continue;
    if (auto t = read_tags(root, it->path(), origin)) out.push_back(std::move(*t));
  }
}

bool same_tree(const fs::path& a, const fs::path& b) {
  std::error_code ec;
  return fs::equivalent(a, b, ec);
}

bool is_engine_tree(const fs::path& dir) {
  std::error_code ec;
  return fs::is_regular_file(dir / "engine" / "core" / "prng.hpp", ec);
}

struct Scored {
  const TaggedFile* file;
  int score;
};

// A file's score for the words: a keyword hit counts most, then the file's name, then the
// summary. Every word is matched as a lower-case substring too, so "noise" finds "value-noise".
int score_of(const TaggedFile& f, const std::vector<std::string>& words) {
  const std::string summary = lower(f.summary);
  const std::string name = lower(fs::path(f.path).filename().string());
  int score = 0;
  for (const std::string& w : words) {
    for (const std::string& k : f.keywords) {
      if (k == w) {
        score += 5;
      } else if (k.find(w) != std::string::npos) {
        score += 2;
      }
    }
    if (name.find(w) != std::string::npos) score += 3;
    if (summary.find(w) != std::string::npos) score += 2;
  }
  return score;
}

std::string file_json(const TaggedFile& f, int score) {
  std::vector<std::pair<std::string, std::string>> m = {
      {"path", json_str(f.path)},       {"origin", json_str(f.origin)},
      {"summary", json_str(f.summary)}, {"skills", json_strs(f.skills)},
      {"tests", json_strs(f.tests)},    {"abs", json_str(f.abs.generic_string())}};
  if (score >= 0) m.emplace_back("score", json_num(score));
  return json_obj(m);
}

std::string where_of(const TaggedFile& f) {
  return f.origin == "repo" ? f.path : "engine: " + f.path;
}

// Every skill directory under `root/.agents/skills`, with its SKILL.md text.
std::vector<std::pair<std::string, std::string>> skill_texts(const fs::path& root) {
  std::vector<std::pair<std::string, std::string>> out;
  std::error_code ec;
  for (const auto& e : fs::directory_iterator(root / kSkillsDir, ec)) {
    if (!e.is_directory(ec)) continue;
    if (auto text = read_text(e.path() / "SKILL.md")) {
      out.emplace_back(e.path().filename().string(), *text);
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace

std::optional<TaggedFile> read_tags(const fs::path& root, const fs::path& file,
                                    const std::string& origin) {
  const auto text = read_text(file);
  if (!text) return std::nullopt;
  TaggedFile t;
  t.path = rel_path(root, file);
  t.origin = origin;
  t.abs = file;
  bool tagged = false, in_header = true;
  const auto lines = split_lines(*text);
  for (size_t i = 0; i < lines.size() && i < static_cast<size_t>(kTagLines); i++) {
    const auto c = comment_text(lines[i]);
    if (!c) {
      if (!lines[i].empty()) in_header = false;
      continue;
    }
    static const std::regex kTag(R"(^ge:([a-z-]+)\s+(.*?)\s*$)");
    std::smatch m;
    if (std::regex_match(*c, m, kTag)) {
      tagged = true;
      in_header = false;
      const std::string tag = m[1], value = m[2];
      if (tag == "summary") t.summary += (t.summary.empty() ? "" : " ") + value;
      for (const std::string& w : split_words(value)) {
        if (tag == "keywords") t.keywords.push_back(w);
        if (tag == "skill") t.skills.push_back(w);
        if (tag == "test") t.tests.push_back(w);
      }
    } else if (in_header) {
      t.header += *c + "\n";
    }
  }
  if (!tagged) return std::nullopt;
  while (t.header.ends_with("\n\n")) t.header.pop_back();
  if (t.header == "\n") t.header.clear();
  return t;
}

std::optional<fs::path> find_engine_root(const fs::path& repo, const std::string& flag) {
  std::error_code ec;
  if (!flag.empty()) {
    if (is_engine_tree(flag)) return fs::absolute(flag, ec);
    return std::nullopt;
  }
  if (is_engine_tree(repo)) return fs::absolute(repo, ec);
  std::vector<fs::path> builds;
  for (const auto& e : fs::directory_iterator(repo, ec)) {
    if (e.is_directory(ec) && e.path().filename().string().starts_with("build")) {
      for (const auto& b : fs::directory_iterator(e.path(), ec)) builds.push_back(b.path());
      builds.push_back(e.path());
    }
  }
  std::sort(builds.begin(), builds.end());
  for (const fs::path& b : builds) {
    const auto cache = read_text(b / "CMakeCache.txt");
    if (cache) {
      static const std::regex kLocal(R"(FETCHCONTENT_SOURCE_DIR_GAMEENGINE2:[A-Z]+=(.+))");
      std::smatch m;
      if (std::regex_search(*cache, m, kLocal) && is_engine_tree(m.str(1))) {
        return fs::path(m.str(1));
      }
    }
    if (is_engine_tree(b / "_deps" / "gameengine2-src")) return b / "_deps" / "gameengine2-src";
  }
  fs::path built_from = GE_ENGINE_SOURCE_DIR;
  if (!built_from.empty() && is_engine_tree(built_from)) return built_from;
  return std::nullopt;
}

std::vector<TaggedFile> tagged_files(const fs::path& repo, const std::optional<fs::path>& engine) {
  std::vector<TaggedFile> out;
  const bool repo_is_engine = engine && same_tree(repo, *engine);
  if (!repo_is_engine) walk_into(repo, repo, "repo", out);
  if (engine) {
    for (const char* dir : {"engine", "plugins", "games"}) {
      walk_into(*engine, *engine / dir, repo_is_engine ? "repo" : "engine", out);
    }
  }
  std::sort(out.begin(), out.end(), [](const TaggedFile& a, const TaggedFile& b) {
    return a.origin != b.origin ? a.origin > b.origin : a.path < b.path;
  });
  return out;
}

Outcome run_find(const fs::path& repo, const std::string& engine_flag,
                 const std::vector<std::string>& words_typed, int limit) {
  Outcome o;
  const auto engine = find_engine_root(repo, engine_flag);
  if (!engine) {
    o.findings.push_back(
        {engine_flag.empty() ? Severity::note : Severity::error,
         engine_flag.empty() ? "no engine source tree found, so only the repo was searched"
                             : "--engine=" + engine_flag + " is not the engine's source tree",
         engine_flag.empty() ? "build/" : engine_flag,
         "run `ge build` once (it fetches the pinned engine), or pass --engine=<engine "
         "checkout>"});
  }
  std::vector<std::string> words;
  words.reserve(words_typed.size());
  for (const std::string& w : words_typed) words.push_back(lower(w));
  const std::vector<TaggedFile> files = tagged_files(repo, engine);
  std::vector<Scored> hits;
  for (const TaggedFile& f : files) {
    if (const int s = score_of(f, words); s > 0) hits.push_back({&f, s});
  }
  std::stable_sort(hits.begin(), hits.end(),
                   [](const Scored& a, const Scored& b) { return a.score > b.score; });
  if (static_cast<int>(hits.size()) > limit) hits.resize(static_cast<size_t>(limit));

  std::vector<std::string> file_rows, skill_rows, lesson_rows;
  std::string text;
  for (const Scored& h : hits) {
    file_rows.push_back(file_json(*h.file, h.score));
    text += "  " + where_of(*h.file) + "  (" + std::to_string(h.score) + ")\n      " +
            h.file->summary + "\n";
  }
  for (const auto& [name, body] : skill_texts(repo)) {
    const auto doc = parse_skill(body);
    const std::string hay = lower(name + " " + (doc ? doc->description : ""));
    if (std::none_of(words.begin(), words.end(),
                     [&](const std::string& w) { return hay.find(w) != std::string::npos; })) {
      continue;
    }
    skill_rows.push_back(
        json_obj({{"name", json_str(name)},
                  {"description", json_str(doc ? doc->description : "")},
                  {"path", json_str(std::string(kSkillsDir) + "/" + name + "/SKILL.md")}}));
    text += "  skill " + name + "\n";
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(repo / kSkillsDir / name / ".lessons", ec)) {
      if (e.path().extension() != ".md") continue;
      const Lesson l = parse_lesson(e.path().filename().string(), read_text(e.path()).value_or(""));
      std::string hay_l = lower(l.title);
      for (const std::string& c : l.concepts) hay_l += " " + lower(c);
      if (std::any_of(words.begin(), words.end(),
                      [&](const std::string& w) { return hay_l.find(w) != std::string::npos; })) {
        lesson_rows.push_back(json_obj(
            {{"skill", json_str(name)}, {"title", json_str(l.title)}, {"file", json_str(l.file)}}));
        text += "  lesson " + name + ": " + l.title + "\n";
      }
    }
  }
  o.text = text;
  o.result = json_obj({{"engine", engine ? json_str(engine->generic_string()) : "null"},
                       {"files", json_arr(file_rows)},
                       {"skills", json_arr(skill_rows)},
                       {"lessons", json_arr(lesson_rows)}});
  if (file_rows.empty() && skill_rows.empty()) {
    o.summary = "nothing matched";
    o.next = "try a synonym or a broader word; `ge help` lists what ge does";
  } else {
    o.summary = std::to_string(file_rows.size()) + " file(s), " +
                std::to_string(skill_rows.size()) + " skill(s), " +
                std::to_string(lesson_rows.size()) + " lesson(s)";
    o.next = hits.empty() ? "read the skill's SKILL.md" : "ge explain " + hits.front().file->path;
  }
  return o;
}

namespace {

// A declaration of `name` in `f`: the line and the comment lines right above it.
struct Declaration {
  int line = 0;
  std::string text;
  std::string comment;
};

std::optional<Declaration> declaration_in(const TaggedFile& f, const std::string& name) {
  const auto text = read_text(f.abs);
  if (!text) return std::nullopt;
  const std::regex decl("(^|[^A-Za-z0-9_:.>])(struct|class|enum class|enum|using)\\s+" + name +
                        "\\b|[A-Za-z0-9_>&*]\\s+" + name + "\\s*\\(|constexpr[^=;]*\\b" + name +
                        "\\s*[={]");
  const auto lines = split_lines(*text);
  for (size_t i = 0; i < lines.size(); i++) {
    const std::string line(lines[i]);
    if (comment_text(line)) continue;
    if (!std::regex_search(line, decl)) continue;
    Declaration d;
    d.line = static_cast<int>(i + 1);
    d.text = line;
    for (size_t j = i; j > 0; j--) {
      const auto c = comment_text(lines[j - 1]);
      if (!c) break;
      d.comment = *c + "\n" + d.comment;
    }
    return d;
  }
  return std::nullopt;
}

bool looks_like_path(const std::string& s) {
  return s.find('/') != std::string::npos || s.find(".hpp") != std::string::npos ||
         s.find(".cpp") != std::string::npos || s.find(".cmake") != std::string::npos;
}

Outcome explain_file(const TaggedFile& f, const std::optional<Declaration>& d,
                     const std::string& kind, const std::string& symbol) {
  Outcome o;
  std::string text = where_of(f) + "\n  " + f.summary + "\n";
  if (!f.header.empty()) {
    text += "\n";
    for (const auto line : split_lines(f.header)) text += "  " + std::string(line) + "\n";
  }
  if (d) {
    text += "\n" + f.path + ":" + std::to_string(d->line) + "\n";
    for (const auto line : split_lines(d->comment)) text += "  // " + std::string(line) + "\n";
    text += "  " + d->text + "\n";
  }
  std::string skills, tests;
  for (const std::string& s : f.skills) skills += (skills.empty() ? "" : " ") + s;
  for (const std::string& t : f.tests) tests += (tests.empty() ? "" : " ") + t;
  text += "\n  skill: " + skills + (tests.empty() ? "" : "\n  test: " + tests) + "\n";
  o.text = text;
  std::string decl = "null";
  if (d) {
    decl = json_obj({{"line", json_num(d->line)},
                     {"text", json_str(d->text)},
                     {"comment", json_str(d->comment)}});
  }
  o.result = json_obj({{"kind", json_str(kind)},
                       {"symbol", symbol.empty() ? "null" : json_str(symbol)},
                       {"file", file_json(f, -1)},
                       {"keywords", json_strs(f.keywords)},
                       {"header", json_str(f.header)},
                       {"declaration", decl}});
  o.summary = kind == "word" ? "the best match explained; `ge find` lists the rest"
                             : "explained " + (symbol.empty() ? f.path : symbol);
  o.next = f.skills.empty() ? "" : "read the skill " + f.skills.front();
  return o;
}

}  // namespace

Outcome run_explain(const fs::path& repo, const std::string& engine_flag,
                    const std::string& target) {
  const auto engine = find_engine_root(repo, engine_flag);
  const std::vector<TaggedFile> files = tagged_files(repo, engine);
  const std::string want = fs::path(target).generic_string();

  if (looks_like_path(target)) {
    for (const TaggedFile& f : files) {
      if (f.path == want || f.path == "engine/" + want || f.path == "plugins/" + want ||
          f.path.ends_with("/" + want)) {
        return explain_file(f, std::nullopt, "file", "");
      }
    }
  }
  // A name: a declaration in a header, preferring a file that lists the name as a keyword.
  static const std::regex kIdent("^[A-Za-z_][A-Za-z0-9_]*$");
  if (std::regex_match(target, kIdent)) {
    for (const int pass : {0, 1}) {
      for (const TaggedFile& f : files) {
        if (!f.path.ends_with(".hpp")) continue;
        const bool listed =
            std::find(f.keywords.begin(), f.keywords.end(), lower(target)) != f.keywords.end();
        if ((pass == 0) != listed) continue;
        if (auto d = declaration_in(f, target)) return explain_file(f, d, "symbol", target);
      }
    }
  }
  const std::vector<std::string> word{lower(target)};
  const TaggedFile* best = nullptr;
  int best_score = 0;
  for (const TaggedFile& f : files) {
    if (const int s = score_of(f, word); s > best_score) best = &f, best_score = s;
  }
  if (best) return explain_file(*best, std::nullopt, "word", "");
  Outcome o;
  o.findings.push_back({Severity::error,
                        "nothing in the repo or the engine is called \"" + target + "\"",
                        engine ? "" : "build/",
                        engine ? "try `ge find <word>` with a broader word"
                               : "no engine tree was found to search: run `ge build` once, or "
                                 "pass --engine=<engine checkout>"});
  return o;
}

}  // namespace ge::cli
