// ANOTHER GAME'S FIGURE IN THIS ONE'S POSES: a player figure made for one game (a sequel's
// `tris.md2` dropped into players/<model>/) drawn where the server animates this game's figure. The
// server picks frames of its own figure; each is looked up by its name's stem in the figure it
// stands for and played on the dropped figure's frames of the same kind.
//
// - A frame's KIND comes from its name's stem (the name less its trailing digits, lower case; of a
//   tail of three digits or more the first stays, naming a variant: "pain2" of "pain203"):
//   one with "death" or "deth" a death, "pain" a pain, "att" (or the stem "light") an attack, "run"
//   or "walk" a run, "stand" or "stnd" a stand, "jump" a jump; a crouched one (a stem starting
//   "cr") and anything else is no kind.
// - The n-th run of a kind in the server's figure plays the dropped figure's (n mod its count)-th
//   run of that kind, a frame k of a run of length a landing on frame k x b / a of a run of length
//   b (both counted from 0), so a run's start and its pace across the run are kept.
// - A kind the dropped figure lacks plays its first stand's first frame (else its frame 0).
//
// ge:summary Shooter's Edge's map from one figure's frames to another's by their names' kinds
// ge:keywords player-figures frame-map figure_frames frame-kind stem stand run attack pain death
// ge:keywords player-model another-games-model md2-in-first-game
// ge:skill shooter morph
// ge:test tests/shooters_edge_figure_frames_test.cpp
#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::models {

enum class FrameKind : uint8_t { none, stand, run, attack, pain, death, jump };

// A frame name's kind, by the rule above.
FrameKind frame_kind(std::string_view name);

// For each of `from`'s frames, the frame of `to` it is drawn as (empty when `to` has none).
std::vector<int> figure_frames(std::span<const std::string> from, std::span<const std::string> to);

}  // namespace shooters_edge::models
