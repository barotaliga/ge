// The shooters_edge window's command line: flags read wherever they stand, a `+` console line
// running to the next flag, an unknown flag refused with the line it followed named, -benchmark
// naming -timedemo, and a negative number kept as a console word.
//
// ge:summary Tests: the window's command line, its flags anywhere and its refusals
// ge:keywords test command-line args parse unknown-flag plus-commands timedemo
// ge:skill shooter testing
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/args.hpp"

namespace {

namespace cl = shooters_edge::client;

// The words as a command line (argv[0] the program) parsed into `a`.
bool parse_words(std::vector<std::string> words, cl::Args& a, std::string& why) {
  words.insert(words.begin(), "shooters_edge");
  std::vector<char*> argv;
  argv.reserve(words.size());
  for (std::string& w : words) argv.push_back(w.data());
  return cl::parse(static_cast<int>(argv.size()), argv.data(), a, why);
}

TEST(a_flag_after_a_console_line_is_read_as_a_flag) {
  cl::Args a;
  std::string why;
  CHECK(parse_words({"+r_look", "modern", "-timedemo", "demo1", "+vid_fullscreen", "1"}, a, why));
  CHECK(a.timedemo == "demo1");
  CHECK((a.plus == std::vector<std::string>{"+r_look", "modern", "+vid_fullscreen", "1"}));
}

TEST(an_unknown_flag_is_refused_with_the_console_line_it_followed) {
  cl::Args a;
  std::string why;
  CHECK(!parse_words({"+r_look", "modern", "-frobnicate", "1"}, a, why));
  CHECK(why.find("unknown flag -frobnicate") != std::string::npos);
  CHECK(why.find("+r_look") != std::string::npos);
  CHECK(why.find("next:") != std::string::npos);
  cl::Args b;
  CHECK(!parse_words({"--frobs=2"}, b, why));
  CHECK(why.find("unknown flag --frobs=2") != std::string::npos);
}

TEST(a_clip_is_read_off_the_demos_clock) {
  cl::Args a;
  std::string why;
  CHECK(parse_words({"-demovideo", "m.mvd", "c.gif", "--from=4:31", "--to=279", "--highlight=3",
                     "--gif-width=320"},
                    a, why));
  CHECK(a.from == 271 && a.to == 279 && a.highlight == 3 && a.gif_width == 320);
  cl::Args b;
  CHECK(!parse_words({"--from=soon"}, b, why));
  CHECK(why.find("--from=soon") != std::string::npos && why.find("next:") != std::string::npos);
}

TEST(benchmark_names_its_new_name) {
  cl::Args a;
  std::string why;
  CHECK(!parse_words({"+r_look", "classic", "-benchmark", "demo1"}, a, why));
  CHECK(why.find("renamed to -timedemo") != std::string::npos);
}

TEST(a_negative_number_is_a_console_word) {
  cl::Args a;
  std::string why;
  CHECK(parse_words({"+cl_gunx", "-1", "+cl_guny", "-0.5"}, a, why));
  CHECK((a.plus == std::vector<std::string>{"+cl_gunx", "-1", "+cl_guny", "-0.5"}));
  CHECK(!cl::flag_like("-1") && !cl::flag_like("-") && cl::flag_like("-x") &&
        cl::flag_like("--size=1x1"));
}

TEST(a_bare_word_before_any_console_line_is_refused) {
  cl::Args a;
  std::string why;
  CHECK(!parse_words({"demo1"}, a, why));
  CHECK(why.find("unknown word demo1") != std::string::npos);
}

TEST(sizes_by_name_or_width_and_height) {
  cl::Args a;
  std::string why;
  CHECK(parse_words({"--size=1440p"}, a, why));
  CHECK(a.width == 2560 && a.height == 1440);
  CHECK(parse_words({"--resolution=800x600"}, a, why));
  CHECK(a.width == 800 && a.height == 600);
  int w = 0, h = 0;
  CHECK(!cl::size_named("8x8", w, h) && !cl::size_named("big", w, h));
  cl::Args b;
  CHECK(!parse_words({"--size=big"}, b, why));
  CHECK(why.find("--size=big") != std::string::npos);
}

}  // namespace
