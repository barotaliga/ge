// plugins/classic/content/web_serve.hpp: what a classic server's web files answer: the game folder
// played or its base folder, a file in a folder, a kind the game's servers send, the server's own
// download switches; a pack, a file of another kind, another game folder or a switched-off folder
// never.
//
// ge:summary Test: a classic server's web files: folder, kind and download switches checked
// ge:keywords test web-serve web-files allow-list
// ge:skill classic net testing
#include "classic/content/web_serve.hpp"

#include <string>

#include "check.hpp"
#include "classic/content/web_downloads.hpp"

using namespace ge::classic::content;

TEST(web_serve_answers_the_games_files_alone) {
  WebServe w;
  w.game_folder = "ktx";
  w.base_folder = "qw";
  w.kinds = quake_web_kinds();
  w.allowed = [](const std::string& path) { return path.rfind("progs/", 0) != 0; };
  std::string file;
  CHECK(web_serve_allows(w, "qw/maps/dm3.bsp", file) && file == "maps/dm3.bsp");
  CHECK(web_serve_allows(w, "KTX/Skins/Base.PCX", file) && file == "skins/base.pcx");
  CHECK(web_serve_allows(w, "qw/locs/dm3.loc", file));
  CHECK(!web_serve_allows(w, "id1/maps/dm3.bsp", file));     // another game folder
  CHECK(!web_serve_allows(w, "qw/pak0.pak", file));          // a pack, and not in a folder
  CHECK(!web_serve_allows(w, "qw/maps/pak1.pak", file));     // a pack's kind is never served
  CHECK(!web_serve_allows(w, "qw/maps/server.cfg", file));   // not a kind the game sends
  CHECK(!web_serve_allows(w, "qw/progs/player.mdl", file));  // its switch is off
  CHECK(!web_serve_allows(w, "qw/maps/dm3", file));
  CHECK(!web_serve_allows(w, "qw", file));
}
