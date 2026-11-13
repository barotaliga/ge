// Tests the skins and player figures a server lists for `listskins`.
//
// ge:summary Tests looks_offered: skins, figures with their skins, icons left out, downloads off
// ge:keywords test listskins looks_offered
// ge:skill net
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/net/classic_looks.hpp"

namespace sn = shooters_edge::net;

TEST(a_server_lists_its_skins_and_figures_with_their_skins) {
  const std::vector<std::string> names = {"skins/base.pcx",
                                          "skins/rust.png",
                                          "skins/deep/x.pcx",
                                          "players/cyborg/tris.md2",
                                          "players/cyborg/oni911.pcx",
                                          "players/cyborg/oni911_i.pcx",
                                          "players/cyborg/weapon.pcx",
                                          "players/female/tris.md2",
                                          "players/female/athena.tga",
                                          "players/empty/notes.txt",
                                          "progs/player.mdl"};
  const std::string t = sn::looks_offered(names, true);
  CHECK(t ==
        "Skins on this server: base, rust.\n"
        "Player models on this server: cyborg (oni911), female (athena).\n");
}

TEST(a_server_with_none_says_so_and_names_downloads_off) {
  const std::string t = sn::looks_offered({}, false);
  CHECK(t.find("no skins") != std::string::npos);
  CHECK(t.find("no player models") != std::string::npos);
  CHECK(t.find("Downloads are off") != std::string::npos);
}
