// Highlights (highlights.hpp): the finished list's order, merging and cuts.
//
// ge:summary finish_highlights: score, window, subject filter, merge, best-of cut, time order
// ge:keywords highlight finish merge score window next_highlight highlight_marks kinds_problem
// ge:skill replay
// ge:test tests/replay_highlights_test.cpp
#include "replay/highlights.hpp"

#include <algorithm>
#include <cmath>

namespace ge::replay {

std::string kinds_problem(std::span<const HighlightKind> kinds) {
  for (size_t i = 0; i < kinds.size(); i++) {
    const HighlightKind& k = kinds[i];
    if (!mark_kind_ok(k.kind))
      return "the highlight kind '" + k.kind +
             "' is not a mark kind (lower case letters, digits, '-' and '_'); next: rename it";
    for (size_t j = 0; j < i; j++)
      if (kinds[j].kind == k.kind)
        return "the highlight kind '" + k.kind + "' is named twice; next: keep one row";
    if (!(k.lead_s >= 0) || !(k.hold_s >= 0))
      return "the highlight kind '" + k.kind + "' has a negative lead or hold; next: 0 or more";
    if (!(k.scale > 0))
      return "the highlight kind '" + k.kind + "' has a scale not above 0; next: a scale over 0";
  }
  return "";
}

const HighlightKind* kind_of(std::span<const HighlightKind> kinds, const std::string& kind) {
  for (const HighlightKind& k : kinds)
    if (k.kind == kind) return &k;
  return nullptr;
}

Highlights finish_highlights(std::vector<Highlight> found, std::span<const HighlightKind> kinds,
                             double length_s, const HighlightPolicy& policy) {
  Highlights out;
  const double end = std::max(0.0, length_s);
  std::vector<Highlight> kept;
  kept.reserve(found.size());
  for (Highlight& h : found) {
    const HighlightKind* k = kind_of(kinds, h.kind);
    if (k == nullptr) {
      out.unruled++;
      continue;
    }
    if (policy.subject >= 0 && h.subject != policy.subject) continue;
    h.at_s = std::clamp(h.at_s, 0.0, end);
    h.score = h.weight * k->scale;
    h.from_s = std::max(0.0, h.at_s - k->lead_s);
    h.to_s = std::min(end, h.at_s + k->hold_s);
    h.kinds = {h.kind};
    kept.push_back(std::move(h));
  }
  // One subject's highlights in time order; a run of them each within merge_s of the one before
  // is one moment.
  std::stable_sort(kept.begin(), kept.end(), [](const Highlight& a, const Highlight& b) {
    return a.subject != b.subject ? a.subject < b.subject : a.at_s < b.at_s;
  });
  std::vector<Highlight> merged;
  for (size_t i = 0; i < kept.size();) {
    size_t j = i + 1;
    while (j < kept.size() && kept[j].subject == kept[i].subject &&
           kept[j].at_s - kept[j - 1].at_s <= policy.merge_s)
      j++;
    // The group's parts best first (ties: the earlier); the moment is the best part's.
    std::vector<size_t> parts;
    for (size_t k = i; k < j; k++) parts.push_back(k);
    std::stable_sort(parts.begin(), parts.end(),
                     [&](size_t a, size_t b) { return kept[a].score > kept[b].score; });
    Highlight m = kept[parts[0]];
    m.kinds.clear();
    m.text.clear();
    m.score = m.weight = 0;
    for (size_t p : parts) {
      const Highlight& h = kept[p];
      if (std::find(m.kinds.begin(), m.kinds.end(), h.kind) == m.kinds.end())
        m.kinds.push_back(h.kind);
      if (!h.text.empty()) m.text += (m.text.empty() ? "" : " + ") + h.text;
      m.score += h.score;
      m.weight += h.weight;
      m.from_s = std::min(m.from_s, h.from_s);
      m.to_s = std::max(m.to_s, h.to_s);
    }
    m.kind = m.kinds[0];
    merged.push_back(std::move(m));
    i = j;
  }
  if (policy.most > 0 && merged.size() > static_cast<size_t>(policy.most)) {
    std::stable_sort(merged.begin(), merged.end(), [](const Highlight& a, const Highlight& b) {
      return a.score != b.score ? a.score > b.score : a.at_s < b.at_s;
    });
    merged.resize(static_cast<size_t>(policy.most));
  }
  std::stable_sort(merged.begin(), merged.end(), [](const Highlight& a, const Highlight& b) {
    return a.at_s != b.at_s ? a.at_s < b.at_s : a.subject < b.subject;
  });
  out.list = std::move(merged);
  return out;
}

int next_highlight(std::span<const Highlight> list, double t_s, int dir, double slop_s) {
  int best = -1;
  for (size_t i = 0; i < list.size(); i++) {
    const double f = list[i].from_s;
    const bool ok = dir > 0 ? f > t_s + slop_s : f < t_s - slop_s;
    if (!ok) continue;
    if (best < 0 || (dir > 0 ? f < list[static_cast<size_t>(best)].from_s
                             : f > list[static_cast<size_t>(best)].from_s))
      best = static_cast<int>(i);
  }
  return best;
}

std::vector<Mark> highlight_marks(std::span<const Highlight> list, double hz) {
  std::vector<Mark> out;
  out.reserve(list.size());
  for (const Highlight& h : list)
    out.push_back({static_cast<int64_t>(std::llround(h.at_s * hz)), h.kind, h.score});
  return out;
}

}  // namespace ge::replay
