// `sky`, `r_reload` and `timerefresh` (render_words.hpp): the typed sky box read, the refresh
// asked, its line.
//
// ge:summary sky and timerefresh read: the typed sky box and its turn, the refresh ask, its line
// ge:keywords sky timerefresh sky_typed refresh_line r_reload
// ge:skill render console
// ge:test tests/shooters_edge_render_words_test.cpp
#include "shooters_edge/client/render_words.hpp"

#include <cstdio>
#include <cstdlib>
#include <utility>

namespace shooters_edge::client {

namespace gc = ge::console;

void add_render_words(gc::Console& c, RenderWords& words, ge::classic::SkyTyped& sky,
                      std::function<std::string()> map) {
  c.add_command("sky", "sky <name> [rotate] [axis x y z]: a sky box through the map's sky",
                [&words, &sky, map](gc::Console& k, const gc::Args& a) {
                  if (a.size() < 2) {
                    k.print("Usage: sky <basename> [rotate] [axis x y z]");
                    return;
                  }
                  const std::string on = map ? map() : std::string();
                  if (on.empty()) {
                    k.print("No map loaded.");
                    return;
                  }
                  sky.name = a[1];
                  sky.map = on;
                  sky.rotate = a.size() > 2 ? std::atof(a[2].c_str()) : 0;
                  sky.axis = {0, 0, 1};
                  if (a.size() > 5)
                    for (size_t i = 0; i < 3; ++i) sky.axis[i] = std::atof(a[3 + i].c_str());
                  ++sky.serial;
                  words.sky_asked = true;
                });
  c.add_command("r_reload", "r_reload: the level's textures and models read again from the files",
                [&words, map](gc::Console& k, const gc::Args&) {
                  if (!map || map().empty()) {
                    k.print("No map loaded.");
                    return;
                  }
                  words.reload_asked = true;
                });
  c.add_command("timerefresh",
                "timerefresh [one]: the view drawn 128 times turning a circle, timed",
                [&words, map = std::move(map)](gc::Console& k, const gc::Args& a) {
                  if (!map || map().empty()) {
                    k.print("No map loaded.");
                    return;
                  }
                  words.refresh_asked = a.size() > 1 ? 2 : 1;
                });
}

std::string refresh_line(double seconds, int frames) {
  char line[96];
  std::snprintf(line, sizeof line, "%f seconds (%f fps)", seconds,
                seconds > 0 ? frames / seconds : 0.0);
  return line;
}

}  // namespace shooters_edge::client
