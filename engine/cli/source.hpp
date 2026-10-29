// `ge source`: the free sources a game's sounds and models come from, searched and fetched
// into a cache outside the repository (skill: free-assets). Three commands:
//
//   ge source list                     the sources ge knows, their licence and what they hold
//   ge source search <source> <word>...  the packs whose names (Kenney), titles (OpenGameArt) or
//                                      names and tags (Poly Haven) hold every word; --kind picks
//                                      sounds or models where a source has both
//   ge source fetch <source>:<pack>    the pack downloaded and unpacked into the cache, with
//                                      SOURCE.txt beside it: title, author, page, licence. A pack
//                                      under a licence this repository cannot carry is refused.
//
// THE SOURCES. kenney: Kenney's kits, all CC0 (the listing pages by category, the zip named on a
// pack's page). oga: OpenGameArt, searched with its CC0 filter, a pack's files those its page
// lists (zips unpacked) and its licences read off the page. polyhaven: Poly Haven's models, all
// CC0, through its API: the glTF at 1k with its textures and buffer. Anything a source holds that
// ge does not read (Quaternius' drive links, a store's bundles) is the skill's to describe.
//
// THE CACHE is --cache, else GE_ASSET_CACHE, else ~/.cache/ge/sources (on Windows
// %LOCALAPPDATA%\ge\sources): never the repository. curl downloads, unzip (or tar) unpacks; the
// scraping is this file's own and tested on saved pages.
//
// ge:summary ge source list, search and fetch: free packs found and unpacked into a cache
// ge:keywords cli ge source search fetch kenney opengameart oga polyhaven cc0 curl unzip
// ge:keywords cache scrape free-assets pack licence
// ge:skill agent-interface assets
// ge:test tests/cli_source_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/commands.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

Outcome run_source(const Args& a);

// Exposed for the tests.

struct PackHit {
  std::string id;     // "kenney:impact-sounds", "oga:door-lock-sounds", "polyhaven:barrel_01"
  std::string title;  // as the source names it
};

// Kenney: the packs a listing page links to (no category, series, tag or page links).
std::vector<std::string> kenney_slugs(std::string_view html);
// Kenney: the zip a pack's page offers, or "".
std::string kenney_zip(std::string_view html, std::string_view slug);
// OpenGameArt: the packs a search page lists, in order.
std::vector<PackHit> oga_results(std::string_view html);
struct OgaPage {
  std::string title, author;
  std::vector<std::string> licences;  // "CC0", "CC-BY 4.0", ...
  std::vector<std::string> files;     // download URLs
};
OgaPage oga_page(std::string_view html);
// Poly Haven: the models whose id, name or tags hold every word (lower case).
std::vector<PackHit> polyhaven_hits(std::string_view index_json,
                                    const std::vector<std::string>& words);
// Poly Haven: the 1k glTF's files as (relative path, URL), the .gltf first; empty when missing.
std::vector<std::pair<std::string, std::string>> polyhaven_files(std::string_view files_json);

}  // namespace ge::cli
