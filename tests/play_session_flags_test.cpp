// The flags every game takes (engine/play/session_flags.hpp): -config and -logfile taken off a
// command line in both spellings, the rest left in order for the game, a flag with no file refused.
//
// ge:summary Test: the session flags (-config, -logfile) taken off a command line, the rest kept
// ge:keywords test session-flags config logfile command-line
// ge:skill agent-interface testing
#include <cstdlib>
#include <string>
#include <vector>

#include "check.hpp"
#include "play/session_flags.hpp"

namespace {

// A mutable argv over `words` (argv[0] the program).
struct Argv {
  std::vector<std::string> words;
  std::vector<char*> ptrs;
  explicit Argv(std::vector<std::string> w) : words(std::move(w)) {
    for (std::string& s : words) ptrs.push_back(s.data());
  }
  int argc() const { return static_cast<int>(ptrs.size()); }
};

}  // namespace

TEST(the_session_flags_come_off_and_the_rest_stay_in_order) {
  Argv a(
      {"game", "-basedir", "/g", "-config", "/tmp/s.cfg", "--logfile=/tmp/s.log", "+map", "e1m2"});
  int argc = a.argc();
  ge::play::SessionFlags f;
  std::string error;
  CHECK_MSG(ge::play::take_session_flags(argc, a.ptrs.data(), f, error), error);
  CHECK(f.config == "/tmp/s.cfg" && f.logfile == "/tmp/s.log");
  CHECK(argc == 5);
  const std::vector<std::string> rest = {"game", "-basedir", "/g", "+map", "e1m2"};
  for (int i = 0; i < argc && i < 5; i++)
    CHECK(std::string(a.ptrs[static_cast<size_t>(i)]) == rest[static_cast<size_t>(i)]);
}

TEST(a_session_flag_with_no_file_is_refused) {
  Argv a({"game", "-logfile"});
  int argc = a.argc();
  ge::play::SessionFlags f;
  std::string error;
  CHECK(!ge::play::take_session_flags(argc, a.ptrs.data(), f, error));
  CHECK_MSG(error.find("-logfile") != std::string::npos, error);
}

TEST(a_session_file_under_home_is_expanded) {
  const char* home = std::getenv("HOME");
  if (home == nullptr) return;
  Argv a({"game", "--config", "~/x.cfg"});
  int argc = a.argc();
  ge::play::SessionFlags f;
  std::string error;
  CHECK(ge::play::take_session_flags(argc, a.ptrs.data(), f, error));
  CHECK(f.config == std::string(home) + "/x.cfg");
  CHECK(argc == 1);
}
