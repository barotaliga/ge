// engine/web/viewer_args.hpp: the words the hub's page hands a web viewer, read and refused.
//
// ge:summary Test: the web viewer's arguments (--demo, --tv, --stream, --play, --bots, --pack,
// ge:summary --speed)
// ge:keywords test web viewer arguments parse_viewer_args demo tv stream pack speed play bots
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
  CHECK(!parse({}, a, error) &&
        error.find("none of --demo, --tv, --play and --join") != std::string::npos);
  CHECK(!parse({"--demo=/d/x", "--tv=ws://h:1/", "--stream=1"}, a, error));
  CHECK(error.find("more than one") != std::string::npos);
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

TEST(a_play_page_s_words_are_read) {
  ViewerArgs a;
  std::string error;
  CHECK(parse({"--play=dm3", "--bots=5", "--pack=/content/ab.pak.zst"}, a, error));
  CHECK(a.play == "dm3" && a.bots == 5 && a.demo.empty() && a.tv.empty() && a.packs.size() == 1);
  CHECK(parse({"--play=e1m1"}, a, error) && a.bots == 3);
  CHECK(!parse({"--play=../x"}, a, error) && error.find("not a map") != std::string::npos);
  CHECK(!parse({"--play=dm3", "--bots=16"}, a, error) &&
        error.find("0 to 15") != std::string::npos);
  CHECK(!parse({"--demo=/d/x", "--bots=2"}, a, error) &&
        error.find("goes with --play") != std::string::npos);
  CHECK(!parse({"--demo=/d/x", "--play=dm3"}, a, error));
}

TEST(a_join_page_s_words_are_read) {
  ViewerArgs a;
  std::string error;
  const std::string hash(64, 'a');
  CHECK(parse({"--join=https://10.0.0.5:4433/play?to=10.0.0.9:27500", "--cert=" + hash,
               "--pack=/content/ab.pak.zst"},
              a, error));
  CHECK(a.join == "https://10.0.0.5:4433/play?to=10.0.0.9:27500" && a.join_to == "10.0.0.9:27500");
  CHECK(a.cert == hash && a.play.empty() && a.packs.size() == 1);
  CHECK(parse({"--join=https://h:1/play?name=x&to=example.org:27500"}, a, error));
  CHECK(a.join_to == "example.org:27500" && a.cert.empty());
  CHECK(!parse({"--join=http://h:1/play?to=x:1"}, a, error) &&
        error.find("not an endpoint") != std::string::npos);
  CHECK(!parse({"--join=https://h:1/play"}, a, error));
  CHECK(!parse({"--join=https://h:1/play?to=x:1", "--cert=abc"}, a, error) &&
        error.find("not a certificate") != std::string::npos);
  CHECK(!parse({"--play=dm3", "--cert=" + hash}, a, error) &&
        error.find("goes with --join") != std::string::npos);
  CHECK(!parse({"--play=dm3", "--join=https://h:1/play?to=x:1"}, a, error) &&
        error.find("more than one") != std::string::npos);
}
