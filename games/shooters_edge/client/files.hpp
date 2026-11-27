// THE GAME'S FILES, as the client names them: the search path every shooter game on the classic
// files shares (plugins/classic/content/files.hpp: the base folder and a mod folder over it, packs,
// zips and loose files in the game's own order, saves, the palette, a map's light, sky boxes, the
// release's words by key), its names brought into the client's namespace so the client's code
// says `SearchPath` and `find_file` as it did. New code outside the client includes the plugin's
// header and names `ge::classic::content`.
//
// ge:summary The client's names for the shared search path in plugins/classic/content
// ge:keywords search-path files content-layer using-declarations game-folder
// ge:skill shooter vfs
// ge:test tests/classic_content_files_test.cpp
#pragma once

#include "classic/content/files.hpp"
#include "shooters_edge/client/load_order.hpp"

namespace shooters_edge::client {

using ge::classic::content::add_archive;
using ge::classic::content::archive_paths;
using ge::classic::content::classic_save_name;
using ge::classic::content::client_folder;
using ge::classic::content::content_cache;
using ge::classic::content::content_packs;
using ge::classic::content::content_packs_with_mission;
using ge::classic::content::find_file;
using ge::classic::content::game_palette;
using ge::classic::content::localized_text;
using ge::classic::content::map_lit;
using ge::classic::content::map_names;
using ge::classic::content::on_disk_any_case;
using ge::classic::content::open_game_dir;
using ge::classic::content::own_folder;
using ge::classic::content::read_save;
using ge::classic::content::save_label;
using ge::classic::content::SearchPath;
using ge::classic::content::sky_box_files;
using ge::classic::content::SkyBoxFiles;
using ge::classic::content::write_save;

}  // namespace shooters_edge::client
