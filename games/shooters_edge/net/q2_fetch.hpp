// WHAT A QUAKE II CLIENT FETCHES ON JOINING, stage by stage: the lists of files a level names,
// in the order a player's client asks for them, each stage built once the one before has come.
//
// 1. The map (models slot 1) and every other model the level names (not a map's own `*` brush
//    models, not the `#` view weapons).
// 2. Once the models are here: the skins each `.md2` names and the frames each `.sp2` names; the
//    sounds (`sound/<name>`, a `#` name as a full path, the `*` figure sounds left to the
//    figures); the pictures (`pics/<name>.pcx`, a `/` name as a full path); for each player's
//    look the figure's model, its view weapons (`weapon.md2` and each `#w_...` the level names),
//    `weapon.pcx`, the skin, its icon (`<skin>_i.pcx`) and its `*` sounds; the sky's six sides
//    (`env/<sky><side>.tga`: rt lf bk ft up dn).
// 3. Once the map is here: its wall textures, `textures/<name>.wal`, in its surfaces' order.
// A path is asked for once a connection, lower-cased, and only when `q2_fetch_path_ok` passes.
//
// ge:summary Quake II's files fetched on joining, stage by stage: models, skins, pictures,
// ge:summary figures, sky, wall textures
// ge:keywords download-order fetch-stages model-skins sky-sides wall-textures figure-files
// ge:keywords player-icon view-weapons precache q2_fetch_models q2_fetch_level q2_fetch_textures
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_net_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "shooters_edge/client/demo.hpp"

namespace shooters_edge::net {

// A file's bytes as this client holds it (empty: lacked or unreadable).
using Q2FetchRead = std::function<std::vector<uint8_t>(const std::string& path)>;

// Whether a path may be asked for at all: under 64 characters, a folder named, no `..`, its first
// and last characters a letter, digit, `_` or `-`, a known kind of file (bsp, dm2, ent, jpg, loc,
// md2, md3, ogg, pcx, png, sp2, tga, txt, wal, wav), and not under `save/`, `music/` or `video/`.
bool q2_fetch_path_ok(std::string_view path);
// The path as asked for: back slashes made forward, `./` dropped, lower-cased.
std::string q2_fetch_path(std::string_view path);

// The look a player's figure is drawn in, its bad names replaced: no `/` → "male/grunt"; an empty
// figure → "male"; a skin or figure not of letters, digits, `_` and `-` → "male/grunt"
// ("female/athena" for a bad skin on the female figure).
std::string q2_fetch_look(std::string_view look);

// Stage 1: the map, then the level's models.
std::vector<std::string> q2_fetch_models(const client::DemoState& s);
// Stage 2: the models' skins and frames (read through `read`), the sounds, the pictures, the
// figures and the sky. `vweps` off (`cl_vwep 0`) asks for no view weapon but `weapon.md2`.
std::vector<std::string> q2_fetch_level(const client::DemoState& s, const Q2FetchRead& read,
                                        bool vweps = true);
// Stage 3: the map's wall textures (none when the map cannot be read).
std::vector<std::string> q2_fetch_textures(std::span<const uint8_t> map);

}  // namespace shooters_edge::net
