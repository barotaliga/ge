// engine/web/viewer_args.hpp: the words the hub's page hands a web viewer, read and refused.
//
// ge:summary Test: the web viewer's arguments (--demo, --tv, --stream, --pack, --speed)
// ge:keywords test web viewer arguments parse_viewer_args demo tv stream pack speed
// ge:skill net testing
#include "web/viewer_args.hpp"

#include <string>
#include <string_view>
#include <vector>

#include "check.hpp"

using ge::web::parse_viewer_args;
using ge::web::ViewerArgs;

namespace {

bool parse(std::vector<std::string_view> words, ViewerArgs& out, std::string& error) {
  return parse_viewer_args(words, out, error);
}

}  // namespace

TEST(a_demo_page_s_words_are_read) {
  ViewerArgs a;
  std::string error;
  CHECK(parse({"--demo=/d/ab/abcd.ger", "--pack=/viewer/pak0.pak", "--pack=/viewer/pak1.pak"}, a,
              error));
  CHECK(a.demo == "/d/ab/abcd.ger" && a.tv.empty() && a.speed == 1);
  CHECK(a.packs.size() == 2 && a.packs[0] == "/viewer/pak0.pak" &&
        a.packs[1] == "/viewer/pak1.pak");
}

TEST(a_watch_page_s_words_are_read) {
  ViewerArgs a;
  std::string error;
  CHECK(parse({"--tv=ws://10.0.0.5:28080/", "--stream=2", "--speed=0.5"}, a, error));
  CHECK(a.tv == "ws://10.0.0.5:28080/" && a.stream == 2 && a.speed == 0.5 && a.demo.empty());
}

TEST(wrong_words_are_refused_with_what_the_viewer_takes) {
  ViewerArgs a;
  std::string error;
  CHECK(!parse({}, a, error) && error.find("neither --demo nor --tv") != std::string::npos);
  CHECK(!parse({"--demo=/d/x", "--tv=ws://h:1/", "--stream=1"}, a, error));
  CHECK(error.find("both") != std::string::npos);
  CHECK(!parse({"--tv=ws://h:1/"}, a, error) && error.find("--stream") != std::string::npos);
  CHECK(!parse({"--tv=http://h:1/", "--stream=1"}, a, error));
  CHECK(error.find("not a WebSocket") != std::string::npos);
  CHECK(!parse({"--demo=/d/x", "--stream=two"}, a, error));
  CHECK(!parse({"--demo=/d/x", "--speed=21"}, a, error));
  CHECK(!parse({"--demo=/d/x", "--pack="}, a, error));
  CHECK(!parse({"--demo=/d/x", "--fly"}, a, error));
  CHECK(error.find("--fly") != std::string::npos &&
        error.find("--pack=<url>") != std::string::npos);
}
