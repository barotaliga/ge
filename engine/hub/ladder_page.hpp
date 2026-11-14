// THE HUB'S LADDER PAGE (docs/accounts.md § The ladders): the ranked ladders a master keeps
// (account/ladder.hpp), served by a hub run with `--accounts` through stats_page.hpp's routing.
//
//   /ladder?game=&mode=   one ladder (the first by default, a link for each): the season's
//                         standings (ranked first: kSeasonMinGames matches or more), when the
//                         season ends, the all-time ratings, and every past season's champions
//   /v1/ladder?game=&mode=   the same as JSON; without `mode`, every ladder's name and size
//
// A duel ladder lists accounts (each linking to their Player page); a pair ladder lists duos by
// name with their two players. Ratings are shown whole (the ladder keeps tenths).
//
// ge:summary The hub's Ladder page: season standings, all-time ratings, past champions, as JSON
// ge:keywords hub ladder-page ranking rating season champions duo league
// ge:skill net
// ge:test tests/hub_ladder_page_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

#include "account/service.hpp"
#include "hub/api.hpp"
#include "hub/spec.hpp"

namespace ge::hub {

constexpr int kAllTimeShown = 20;

// The Ladder page or its JSON (`api`) for `target`; `unix_s` says how long the season has left.
Answer ladder_answer(const HubSpec& spec, const account::Service& svc, std::string_view target,
                     int64_t unix_s, bool api);

}  // namespace ge::hub
