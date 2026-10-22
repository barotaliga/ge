// `ge skills sync` and the skills ge carries (engine/cli/skills.hpp): every consumer skill is a
// well-formed skill; a sync writes them, the lock and the AGENTS.md block, and a second sync
// writes nothing; a hand-edited engine skill, or a repo skill of the same name, is left alone
// and reported until --force; lessons survive every sync; a skill the engine dropped is removed
// with its lessons kept; lesson fragments are checked to the skill-reflection format.
//
// ge:summary Tests: the consumer skills' shape, ge skills sync's rules, lessons and their format
// ge:keywords test cli ge skills sync lock lessons consumer-skills agents-md force
// ge:skill agent-interface testing
// ge:test tests/cli_skills_test.cpp
#include <filesystem>
#include <sstream>
#include <string>

#include "check.hpp"
#include "cli/embedded.hpp"
#include "cli/skills.hpp"

namespace fs = std::filesystem;
using namespace ge::cli;

namespace {

// A fresh repo with an AGENTS.md that has the block, under the system's temp directory.
fs::path fresh_repo(const std::string& name) {
  const fs::path dir = fs::temp_directory_path() / ("ge_cli_skills_test_" + name);
  fs::remove_all(dir);
  std::string error;
  write_text(dir / "AGENTS.md",
             "# Router\n\n" + std::string(kBlockBegin) + "\n" + std::string(kBlockEnd) +
                 "\n\n| `mine` | the repo's own |\n",
             error);
  return dir;
}

std::string say(const std::vector<Finding>& findings) {
  std::string s;
  for (const Finding& f : findings) s += format_finding("sync", f);
  return s;
}

bool mentions(const std::vector<Finding>& findings, const std::string& what) {
  for (const Finding& f : findings) {
    if (f.what.find(what) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

TEST(every_consumer_skill_is_a_well_formed_skill) {
  const auto skills = consumer_skills();
  CHECK_MSG(skills.size() >= 5, "ge carries " + std::to_string(skills.size()) +
                                    " consumer skills; next: check consumer-skills/ and "
                                    "cmake/EmbedFiles.cmake");
  for (const ConsumerSkill& s : skills) {
    const auto doc = parse_skill(s.text);
    CHECK_MSG(doc && doc->name == s.name,
              "consumer-skills/" + s.name + "/SKILL.md: front-matter name must be its directory");
    CHECK_MSG(doc && doc->description.size() > 40,
              "consumer-skills/" + s.name + "/SKILL.md: needs a description of when to load it");
  }
  CHECK(!engine_version().empty());
}

TEST(a_sync_writes_the_skills_the_lock_and_the_block_then_nothing) {
  const fs::path repo = fresh_repo("sync");
  const SyncResult first = sync_skills(repo, false);
  CHECK_MSG(first.findings.empty(), say(first.findings));
  // Every skill but the plugins' (the repo links none), and AGENTS.md.
  const std::vector<ConsumerSkill> skills = skills_for(repo);
  CHECK(skills.size() < consumer_skills().size());
  CHECK(first.written.size() == skills.size() + 1);
  for (const ConsumerSkill& s : skills) {
    const fs::path dir = repo / kSkillsDir / s.name;
    CHECK(read_text(dir / "SKILL.md").value_or("") == s.text);
    CHECK(fs::exists(dir / ".lessons" / ".gitkeep"));
  }
  const auto lock = read_lock(repo);
  CHECK(lock && lock->engine == engine_version() && lock->skills.size() == skills.size());
  const std::string agents = read_text(repo / "AGENTS.md").value_or("");
  CHECK(agents.find("| `engine` |") != std::string::npos);
  CHECK(agents.find("| `mine` |") != std::string::npos);
  CHECK_MSG(check_engine_skills(repo).empty(), say(check_engine_skills(repo)));
  CHECK_MSG(check_skills(repo).empty(), say(check_skills(repo)));

  const SyncResult second = sync_skills(repo, false);
  CHECK(second.written.empty() && second.findings.empty());
}

TEST(a_hand_edit_is_kept_and_reported_until_force) {
  const fs::path repo = fresh_repo("edit");
  sync_skills(repo, false);
  const fs::path skill = repo / kSkillsDir / "labs" / "SKILL.md";
  std::string error;
  write_text(skill, read_text(skill).value_or("") + "\nA local edit.\n", error);
  write_text(repo / kSkillsDir / "labs" / ".lessons" / "1792000000-kept.md",
             "---\ntitle: A lesson survives a sync\ndate: 2026-10-21\n---\n\nIt does.\n", error);

  CHECK(mentions(check_engine_skills(repo), "engine skill labs was edited by hand"));
  const SyncResult refused = sync_skills(repo, false);
  CHECK(mentions(refused.findings, "edited by hand; sync left it"));
  CHECK(read_text(skill).value_or("").ends_with("A local edit.\n"));

  const SyncResult forced = sync_skills(repo, true);
  CHECK_MSG(forced.findings.empty(), say(forced.findings));
  CHECK(!read_text(skill).value_or("").ends_with("A local edit.\n"));
  CHECK(fs::exists(repo / kSkillsDir / "labs" / ".lessons" / "1792000000-kept.md"));
  CHECK(check_engine_skills(repo).empty());
}

TEST(a_repo_skill_of_the_same_name_is_not_overwritten) {
  const fs::path repo = fresh_repo("own");
  std::string error;
  const std::string mine = "---\nname: labs\ndescription: \"the repo's own labs skill\"\n---\n";
  write_text(repo / kSkillsDir / "labs" / "SKILL.md", mine, error);
  const SyncResult r = sync_skills(repo, false);
  CHECK(mentions(r.findings, "the repo has its own skill named labs"));
  CHECK(read_text(repo / kSkillsDir / "labs" / "SKILL.md").value_or("") == mine);
  CHECK(mentions(check_engine_skills(repo), "engine skill labs is not in the lock"));
}

TEST(a_dropped_skill_goes_and_its_lessons_stay) {
  const fs::path repo = fresh_repo("dropped");
  sync_skills(repo, false);
  // A lock naming a skill this ge does not carry, as an older engine's sync would have left it.
  std::string error;
  const std::string old = "---\nname: retired\ndescription: \"gone upstream\"\n---\n";
  write_text(repo / kSkillsDir / "retired" / "SKILL.md", old, error);
  write_text(repo / kSkillsDir / "retired" / ".lessons" / "1792000000-still-true.md",
             "---\ntitle: Still true\ndate: 2026-10-21\n---\n\nBody.\n", error);
  const std::string lock = read_text(repo / kLockFile).value_or("");
  write_text(repo / kLockFile, lock + "skill retired " + fnv1a64_hex(old) + "\n", error);

  const SyncResult r = sync_skills(repo, false);
  CHECK(!fs::exists(repo / kSkillsDir / "retired" / "SKILL.md"));
  CHECK(fs::exists(repo / kSkillsDir / "retired" / ".lessons" / "1792000000-still-true.md"));
  CHECK(mentions(r.findings, "the engine dropped skill retired; its lessons are kept"));
}

TEST(lesson_fragments_follow_the_format) {
  const Lesson good =
      parse_lesson("1792000000-a-claim.md",
                   "---\ntitle: A claim\ndate: 2026-10-21\nscope: sim/game.cpp, labs/\n"
                   "concepts: [determinism, replay]\n---\n\nThe body.\n");
  CHECK(good.errors.empty());
  CHECK(good.scope.size() == 2 && good.concepts.size() == 2 && good.body == "The body.");

  const Lesson bad = parse_lesson("a-claim.md",
                                  "---\ntitle:\ndate: 21/10/2026\nscope: /etc\n"
                                  "concepts: [Determinism]\nauthor: x\n---\n\n");
  CHECK_MSG(bad.errors.size() == 7, "want 7 errors, got " + std::to_string(bad.errors.size()) +
                                        "; next: read parse_lesson in engine/cli/skills.cpp");
}

TEST(lessons_print_by_skill_and_scope) {
  const fs::path repo = fresh_repo("lessons");
  sync_skills(repo, false);
  std::string error;
  const fs::path lessons = repo / kSkillsDir / "labs" / ".lessons";
  write_text(lessons / "1792000000-sim.md",
             "---\ntitle: About sim\ndate: 2026-10-21\nscope: sim/\n---\n\nSim body.\n", error);
  write_text(lessons / "1792000001-global.md",
             "---\ntitle: About everything\ndate: 2026-10-21\n---\n\nGlobal body.\n", error);
  std::ostringstream all, scoped, missing;
  CHECK(print_lessons(repo, "labs", "", all) == 0);
  CHECK(all.str().find("## About sim") != std::string::npos);
  CHECK(print_lessons(repo, "labs", "labs/run_lab.cpp", scoped) == 0);
  CHECK(scoped.str().find("## About sim") == std::string::npos);
  CHECK(scoped.str().find("## About everything") != std::string::npos);
  CHECK(print_lessons(repo, "nope", "", missing) == 2);
  CHECK(missing.str().find("next: name one of:") != std::string::npos);
}
