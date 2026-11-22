// client/render_words.hpp: `sky` and `timerefresh` outside a level and in one, the typed sky box
// and its turn, the usage line, and timerefresh's answer.
//
// ge:summary Test: the sky and timerefresh words: no map, the usage, the typed box, the line
// ge:keywords test sky timerefresh sky_typed refresh_line render-words
// ge:skill render testing
#include "shooters_edge/client/render_words.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"

namespace cl = shooters_edge::client;

TEST(sky_and_timerefresh_need_a_map_and_sky_its_name) {
  ge::console::Console con{false};
  std::vector<std::string> said;
  con.set_sink([&said](std::string_view l) { said.emplace_back(l); });
  ge::classic::SkyTyped sky;
  cl::RenderWords words;
  std::string map;
  cl::add_render_words(con, words, sky, [&map] { return map; });
  con.run_now("sky");
  con.run_now("sky unit1_");
  con.run_now("timerefresh");
  CHECK(said.size() == 3 && said[0] == "Usage: sky <basename> [rotate] [axis x y z]" &&
        said[1] == "No map loaded." && said[2] == "No map loaded.");
  CHECK(!words.sky_asked && words.refresh_asked == 0 && sky.serial == 0);

  map = "base1";
  said.clear();
  con.run_now("sky space1 2");
  CHECK(said.empty() && words.sky_asked && sky.name == "space1" && sky.map == "base1" &&
        sky.rotate == 2 && sky.serial == 1 && sky.for_map("base1") && !sky.for_map("base2"));
  CHECK(sky.axis[0] == 0 && sky.axis[1] == 0 && sky.axis[2] == 1);
  con.run_now("sky unit1_ -4 1 0 0");
  CHECK(sky.name == "unit1_" && sky.rotate == -4 && sky.axis[0] == 1 && sky.axis[2] == 0 &&
        sky.serial == 2);
  con.run_now("timerefresh");
  CHECK(words.refresh_asked == 1);
  con.run_now("timerefresh one");
  CHECK(words.refresh_asked == 2);
}

TEST(timerefresh_says_its_seconds_and_rate) {
  CHECK(cl::refresh_line(0.5, 128) == "0.500000 seconds (256.000000 fps)");
  CHECK(cl::refresh_line(0, 128) == "0.000000 seconds (0.000000 fps)");
}
