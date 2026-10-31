// HIGHLIGHTS: the moments of a recorded run worth watching again, found by the game's rules over
// the game's own events, and laid on the run's timeline for a play bar's markers, a jump to the
// next one, a clip's window and the director's marks (docs/replays.md § Highlights).
//
// THE GAME'S HALF is two things. Its EVENTS: whatever its scan of a run reads off it (a kill with
// its killer, victim, weapon and distance; a jump with its height), as a struct of its own. Its
// RULES: a `HighlightRule<Event>` a kind, a function that reads the whole event list and adds a
// `Highlight` for every moment it finds (its kind, its time, its weight, whose moment it is and a
// line saying what happened). A rule sees every event at once, so a rule about several events (two
// kills from one shot, three in ten seconds) is as easy to write as one about one. Another game
// adds its own events and rules; nothing here knows a weapon or a kill.
//
// THE ENGINE'S HALF is what every game would otherwise write again (`finish_highlights`):
//   - a highlight of a kind with no `HighlightKind` is refused and counted (`unruled`);
//   - each is scored `weight * scale` and given its window, `lead_s` before and `hold_s` after its
//     time, held inside the run (0..length);
//   - with `subject` set, only that subject's highlights are kept (-1: everyone's);
//   - highlights of one subject whose times lie within `merge_s` of each other are ONE moment: a
//     two-kill shot that was also a mid-air kill is one marker reading "double kill + air kill",
//     its kinds best first, its score their sum, its window theirs together;
//   - with `most` set, only the best `most` are kept (ties: the earlier);
//   - the list is in time order.
// The same events and rules always give the same list.
//
// FOR PLAYBACK: `next_highlight` (the next window's opening after a time, or the last before it) is
// the jump key; `highlight_marks` hands the list to the director (director.hpp) as marks at `hz`
// steps a second.
//
// ge:summary Highlights: a game's rules over its events, scored, windowed, merged, in time order
// ge:keywords replay highlight highlights moment moments rule rules event events finder find
// ge:keywords highlight-kind highlight-rule finish_highlights run_highlight_rules next_highlight
// ge:keywords highlight_marks highlight_at merge window lead hold score subject markers
// ge:keywords play-bar demo
// ge:skill replay
// ge:test tests/replay_highlights_test.cpp
#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "replay/file.hpp"

namespace ge::replay {

// How the game shows and scores one kind of highlight.
struct HighlightKind {
  std::string kind;     // the rule's name, `mark_kind_ok`
  std::string label;    // what a marker says ("double kill")
  uint32_t rgba = ~0u;  // the marker's colour, 0xRRGGBBAA
  double lead_s = 3;    // the window opens this long before the moment
  double hold_s = 2;    // and closes this long after it
  double scale = 1;     // a highlight's score is its weight times this
};

struct Highlight {
  std::string kind;  // the best kind first once finished; `kinds` holds every one merged
  double at_s = 0;   // the moment, s of the run's timeline
  double weight = 1;
  int32_t subject = -1;  // whose moment (a player's slot); -1 nobody's in particular
  std::string text;      // what happened, for a list or a tooltip ("rocket, 1240 units")
  // Set by `finish_highlights`:
  std::vector<std::string> kinds;
  double from_s = 0, to_s = 0;  // the window
  double score = 0;
};

// What a rule adds for a moment (its kind is set from the rule's).
inline Highlight highlight_at(double at_s, double weight, int32_t subject, std::string text) {
  Highlight h;
  h.at_s = at_s;
  h.weight = weight;
  h.subject = subject;
  h.text = std::move(text);
  return h;
}

// One rule: the kind it finds, and the finder that adds a highlight for each moment.
template <class Event>
struct HighlightRule {
  std::string kind;
  std::function<void(std::span<const Event> events, std::vector<Highlight>& out)> find;
};

// Every rule over `events`, in the rules' order; each highlight's kind is its rule's.
template <class Event>
std::vector<Highlight> run_highlight_rules(std::span<const Event> events,
                                           std::span<const HighlightRule<Event>> rules) {
  std::vector<Highlight> out;
  for (const HighlightRule<Event>& r : rules) {
    const size_t first = out.size();
    if (r.find) r.find(events, out);
    for (size_t i = first; i < out.size(); i++) out[i].kind = r.kind;
  }
  return out;
}

struct HighlightPolicy {
  int32_t subject = -1;  // keep only this subject's (-1: everyone's)
  double merge_s = 1;    // one subject's highlights this close are one moment
  int32_t most = 0;      // the best this many (0: all)
};

struct Highlights {
  std::vector<Highlight> list;  // in time order
  int64_t unruled = 0;          // found with a kind no HighlightKind names
};

// Why `kinds` cannot score ("" when they can): a kind named twice or not `mark_kind_ok`, a
// negative lead or hold, a scale not above 0.
std::string kinds_problem(std::span<const HighlightKind> kinds);

// The found highlights scored, windowed, filtered, merged, cut to the best and put in time order
// on a run of `length_s` (see above).
Highlights finish_highlights(std::vector<Highlight> found, std::span<const HighlightKind> kinds,
                             double length_s, const HighlightPolicy& policy);

// The kind's row in `kinds`, or null.
const HighlightKind* kind_of(std::span<const HighlightKind> kinds, const std::string& kind);

// The jump: the index of the highlight whose window opens first after `t_s` (dir > 0) or last
// before it (dir < 0), one opening within `slop_s` of `t_s` skipped so that a press after a jump
// to a window's opening goes on to the next; -1 when none. A player seeks to its `from_s`.
int next_highlight(std::span<const Highlight> list, double t_s, int dir, double slop_s = 0.5);

// The highlights as the director's marks at `hz` steps a second: each at its moment, its kind the
// best one, its weight its score.
std::vector<Mark> highlight_marks(std::span<const Highlight> list, double hz);

}  // namespace ge::replay
