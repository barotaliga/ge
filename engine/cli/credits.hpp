// `ge credits check [<dir>]`: a content directory's CREDITS.md held to the files beside it (skill:
// free-assets). The file is one section a source pack:
//
//   ## <the pack's title>
//   - Licence: CC0 1.0 (...)
//   | Our file | Source file |
//   | `sounds/door/stone_stop.wav` | `Audio/doorClose_2.ogg` |
//
// and the check fails:
//   * a sound, model, image or font under the directory that no table row names (an asset with no
//     known source);
//   * a row whose file is not there (a credit for something removed or renamed);
//   * a section with no licence line, or one this repository cannot carry: share-alike,
//     noncommercial, no-derivatives or GPL. CC0, public domain, CC BY, MIT and the repository's own
//     pass; anything else is a warning to read.
//
// ge:summary ge credits check: every asset credited, every credit's file there, licences allowed
// ge:keywords cli ge credits credits-check licence license cc0 attribution free-assets content
// ge:skill agent-interface assets
// ge:test tests/cli_credits_test.cpp
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "cli/finding.hpp"
#include "cli/outcome.hpp"

namespace ge::cli {

Outcome run_credits_check(const fs::path& dir);

// Exposed for the tests.
struct CreditSection {
  std::string title;
  std::string licence;             // the text after "Licence:", or ""
  int line = 0;                    // the heading's line
  std::vector<std::string> files;  // the table rows' first cells, unquoted
  std::vector<int> file_lines;
};
std::vector<CreditSection> parse_credits(std::string_view text);
// error: one this repository cannot carry; warn: one it does not know; note: allowed.
Severity licence_verdict(std::string_view licence);
// The files the check expects a credit for (sounds, models, images, fonts), relative to `dir`.
std::vector<std::string> asset_files(const fs::path& dir);

}  // namespace ge::cli
