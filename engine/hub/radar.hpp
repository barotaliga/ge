// THE HUB'S MAP OF A MATCH (docs/hub.md § The live map): a demo's RADAR track (replay/radar.hpp)
// drawn as an overhead map in SVG, for a phone or a slow machine that will not run the viewer.
// Nothing in it is script: the dots move by SVG's own animation, a minute at a time, and a reader
// steps through the match frame by frame with plain links.
//
// - THE GROUND is the level's outline when the game wrote one, and under it the faint trail of
//   every player over the whole match, which draws the corridors a level has even when no outline
//   was written.
// - THE DOTS are the players, in their team's colour (red, blue, yellow, green by name; any other
//   team or no team a colour of a fixed set by its order), a wedge for the way they face, their
//   name beside; a dead player is drawn faint, a carrier ringed.
// - `/radar/{sha256}` the page: the map animated over one minute (`?from=<s>`, the minute's start)
//   with the links to the minutes before and after; `?at=<s>` the map still at that moment, with
//   links a second and five seconds either way. The Demos page links a demo's map when its file
//   has a radar track.
//
// Live matches get the same map once a broadcast node carries the radar track (M57b): the page
// then reads the node's stream instead of a file.
//
//   radar_svg(radar, from_ms, to_ms, animate)   the SVG alone (the lab draws frames with it)
//   radar_page(spec, row, radar, target)        the whole page
//   radar_answer(archive, spec, target, out)     `/radar/...` answered, or false for another path
//
// ge:summary The hub's map of a match: a demo's radar track drawn as SVG, animated, frame by frame
// ge:keywords hub radar map svg overhead animate frame step trail outline team colour phone
// ge:keywords radar_page radar_svg radar_answer live-map
// ge:skill net
// ge:test tests/hub_radar_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hub/api.hpp"
#include "hub/archive.hpp"
#include "hub/spec.hpp"
#include "replay/radar.hpp"

namespace ge::hub {

constexpr int64_t kRadarWindowMs = 60000;  // the animated page's span

// The map as an SVG element: the ground (outline and trails), then the dots. `animate`: the dots
// move from `from_ms` to `to_ms` and loop; else they stand where they were at `from_ms`.
std::string radar_svg(const replay::Radar& r, int64_t from_ms, int64_t to_ms, bool animate);
// The page for `row`'s radar: `target`'s `from` or `at` (seconds) chooses the view.
std::string radar_page(const HubSpec& spec, const DemoRow& row, const replay::Radar& r,
                       std::string_view target);
// `/radar/{sha256}` answered from the archive (a 404 page when it has no such demo or no radar);
// false for any other path.
bool radar_answer(const Archive* a, const HubSpec& spec, std::string_view target, Answer& out);

}  // namespace ge::hub
