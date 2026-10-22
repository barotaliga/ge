// A GAME REPO'S SKILLS, as ge reads and writes them. A game repo keeps its skills where the
// engine does: .agents/skills/<name>/SKILL.md (front matter `name` and `description`) with a
// .lessons/ directory of fragments (the `skill-reflection` skill's format). Two kinds live there:
//
//   - the ENGINE'S skills, copied from consumer-skills/ by `ge skills sync` (and `ge new`),
//     listed with their hashes in .agents/skills/ge-skills.lock, and routed from the block
//     between kBlockBegin and kBlockEnd in AGENTS.md, which sync rewrites;
//   - the repo's OWN skills, which ge never writes.
//
// Sync owns an engine skill's SKILL.md only while it is as sync wrote it (its hash matches the
// lock). A hand-edited one, or a repo skill of the same name, is left alone and reported; a
// skill's .lessons/ belong to the repo and are never touched.
//
// ge:summary A game repo's skills: parsing, lessons, the lock, ge skills sync and ge lessons
// ge:keywords cli ge skills sync consumer-skills lock lessons fragments front-matter
// ge:keywords skill-reflection agents-md routing sync_skills check_skills print_lessons
// ge:skill agent-interface
// ge:test tests/cli_skills_test.cpp
#pragma once

#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "cli/finding.hpp"

namespace ge::cli {

inline constexpr std::string_view kSkillsDir = ".agents/skills";
inline constexpr std::string_view kLockFile = ".agents/skills/ge-skills.lock";
inline constexpr std::string_view kBlockBegin = "<!-- ge:skills:begin -->";
inline constexpr std::string_view kBlockEnd = "<!-- ge:skills:end -->";

struct SkillDoc {
  std::string name;
  std::string description;
};

// A SKILL.md's front matter, or nothing when it has none.
std::optional<SkillDoc> parse_skill(std::string_view text);

// The engine's skills this ge carries, by name, with their SKILL.md text.
struct ConsumerSkill {
  std::string name;
  std::string_view text;
};
std::vector<ConsumerSkill> consumer_skills();

// One lesson fragment, parsed. `errors` are format errors (the fragment fails the check).
struct Lesson {
  std::string file;  // the fragment's file name
  std::string title;
  std::string date;
  std::vector<std::string> scope;
  std::vector<std::string> concepts;
  std::string body;
  std::vector<std::string> errors;
};
Lesson parse_lesson(std::string_view file_name, std::string_view text);

// Every skill directory's shape: SKILL.md with its name and description, a .lessons/ directory,
// well-formed fragments, and a route from AGENTS.md.
std::vector<Finding> check_skills(const fs::path& repo);

// What the lock says: the engine version the skills came from, and each one's hash.
struct LockEntry {
  std::string name;
  std::string hash;
};
struct Lock {
  std::string engine;
  std::vector<LockEntry> skills;
};
std::optional<Lock> read_lock(const fs::path& repo);

// The engine skills' state against this ge: edited by hand, missing, stale or out of the lock.
std::vector<Finding> check_engine_skills(const fs::path& repo);

// `ge skills sync`: writes every engine skill this ge carries that the repo has not changed by
// hand (all of them with `force`), removes the ones the engine dropped, rewrites the lock and the
// AGENTS.md block. `written` lists what changed on disk, for the command's report.
struct SyncResult {
  std::vector<std::string> written;
  std::vector<Finding> findings;
};
SyncResult sync_skills(const fs::path& repo, bool force);

// `ge lessons`: with no skill, the inventory (every skill and its lesson count); with one, its
// lessons in full, oldest first, narrowed to those whose scope overlaps `scope` when given.
// Returns the exit code (2: no such skill).
int print_lessons(const fs::path& repo, std::string_view skill, std::string_view scope,
                  std::ostream& out);

}  // namespace ge::cli
