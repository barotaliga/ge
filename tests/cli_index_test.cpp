// `ge find` and `ge explain` (engine/cli/index.hpp) over this tree: a file's tag block and header
// are read, the engine's tree is found, a search ranks the file that lists a word as a keyword
// first, and explain resolves a path, an include-style path, a function and a word.
//
// ge:summary Tests: tag blocks read; ge find ranks keyword hits first; ge explain resolves
// ge:keywords test cli ge find explain tags ranking engine-root declaration
// ge:skill agent-interface testing
// ge:test tests/cli_index_test.cpp
#include <algorithm>
#include <string>

#include "check.hpp"
#include "cli/index.hpp"

using namespace ge::cli;

namespace {

const fs::path kRoot = GE_SOURCE_DIR;

}  // namespace

TEST(a_tag_block_and_its_header_are_read) {
  const auto t = read_tags(kRoot, kRoot / "engine/core/prng.hpp", "engine");
  CHECK_MSG(t.has_value(), "engine/core/prng.hpp has no tag block?");
  if (!t) return;
  CHECK(t->path == "engine/core/prng.hpp");
  CHECK(!t->summary.empty());
  CHECK(std::find(t->keywords.begin(), t->keywords.end(), "prng") != t->keywords.end());
  CHECK(!t->skills.empty() && t->skills[0] == "determinism");
  CHECK(!t->tests.empty());
  CHECK(!t->header.empty() && t->header.find("ge:") == std::string::npos);
}

TEST(the_engine_tree_is_found_by_flag_and_by_being_the_repo) {
  CHECK(find_engine_root(kRoot, "").has_value());
  CHECK(find_engine_root(fs::temp_directory_path(), kRoot.string()).has_value());
  CHECK(!find_engine_root(kRoot, (fs::temp_directory_path() / "no-engine-here").string()));
}

TEST(find_ranks_a_keyword_hit_first) {
  const Outcome o = run_find(kRoot, "", {"mulberry32"}, 5);
  CHECK_MSG(o.result.find("\"path\":\"engine/core/prng.hpp\"") != std::string::npos, o.result);
  const size_t files = o.result.find("\"files\":[{\"path\":\"engine/core/prng.hpp\"");
  CHECK_MSG(files != std::string::npos,
            "prng.hpp lists mulberry32 as a keyword, so it ranks first");
  CHECK(exit_of(o) == 0);
  const Outcome none = run_find(kRoot, "", {"zzzz-not-a-word"}, 5);
  CHECK(none.summary == "nothing matched");
}

TEST(explain_resolves_a_path_an_include_path_a_name_and_a_word) {
  const Outcome path = run_explain(kRoot, "", "engine/core/prng.hpp");
  CHECK(path.result.find("\"kind\":\"file\"") != std::string::npos);
  const Outcome include = run_explain(kRoot, "", "core/prng.hpp");
  CHECK(include.result.find("\"path\":\"engine/core/prng.hpp\"") != std::string::npos);

  const Outcome name = run_explain(kRoot, "", "make_rng");
  CHECK_MSG(name.result.find("\"kind\":\"symbol\"") != std::string::npos, name.result);
  CHECK(name.result.find("\"path\":\"engine/core/prng.hpp\"") != std::string::npos);
  CHECK(name.text.find("make_rng(uint32_t seed)") != std::string::npos);

  const Outcome word = run_explain(kRoot, "", "mulberry32");
  CHECK(word.result.find("\"kind\":\"word\"") != std::string::npos);
  const Outcome none = run_explain(kRoot, "", "zzzz_not_a_name");
  CHECK(exit_of(none) == 1);
}
