// Highlights (engine/replay/highlights.hpp): a game's rules run over its events; the finished list
// scored, windowed inside the run, filtered by subject, one subject's near moments merged best
// first, cut to the best, in time order; the jump to the next and the last window; the marks for
// the director; bad kinds named.
//
// ge:summary Tests: highlight rules, scoring, windows, merging, best-of, the jump, the marks
// ge:keywords test replay highlights rules merge window score subject next_highlight marks
// ge:skill replay testing
// ge:test tests/replay_highlights_test.cpp
#include <span>
#include <string>
#include <vector>

#include "check.hpp"
#include "replay/highlights.hpp"

namespace gr = ge::replay;

namespace {

// A toy game's event: someone scored `points` at `t`.
struct Goal {
  double t;
  int who;
  int points;
};

const std::vector<gr::HighlightKind> kKinds{
    {"big", "big goal", 0xff0000ffu, 2, 1, 2},
    {"double", "two quick goals", 0x00ff00ffu, 3, 2, 1},
};

std::vector<gr::HighlightRule<Goal>> rules() {
  std::vector<gr::HighlightRule<Goal>> r;
  r.push_back({"big", [](std::span<const Goal> gs, std::vector<gr::Highlight>& out) {
                 for (const Goal& g : gs)
                   if (g.points >= 3)
                     out.push_back(gr::highlight_at(g.t, double(g.points), g.who, "big"));
               }});
  r.push_back({"double", [](std::span<const Goal> gs, std::vector<gr::Highlight>& out) {
                 for (size_t i = 1; i < gs.size(); i++)
                   if (gs[i].who == gs[i - 1].who && gs[i].t - gs[i - 1].t < 5)
                     out.push_back(gr::highlight_at(gs[i].t, 1, gs[i].who, "double"));
               }});
  r.push_back({"unknown", [](std::span<const Goal>, std::vector<gr::Highlight>& out) {
                 out.push_back(gr::highlight_at(1, 1, 0, "no kind"));
               }});
  return r;
}

const std::vector<Goal> kGoals{{1, 0, 3}, {10, 1, 1}, {13, 1, 3}, {30, 0, 1}, {59, 1, 4}};

}  // namespace

TEST(rules_find_and_the_list_is_scored_windowed_merged_and_in_time_order) {
  const auto rs = rules();
  std::vector<gr::Highlight> found = gr::run_highlight_rules<Goal>(kGoals, rs);
  CHECK(found.size() == 5);  // big at 1, 13, 59; double at 13; the unknown one
  CHECK(found[0].kind == "big" && found[3].kind == "double" && found[4].kind == "unknown");
  const gr::Highlights h = gr::finish_highlights(found, kKinds, 60, {});
  CHECK(h.unruled == 1);
  CHECK(h.list.size() == 3);
  // At 1: the big goal, its window held at the run's start.
  CHECK(h.list[0].at_s == 1 && h.list[0].from_s == 0 && h.list[0].to_s == 2);
  CHECK(h.list[0].score == 6);
  // At 13: the big goal and the double merged, the big one (score 6) first.
  CHECK(h.list[1].kinds == std::vector<std::string>({"big", "double"}));
  CHECK(h.list[1].kind == "big" && h.list[1].score == 7);
  CHECK(h.list[1].from_s == 10 && h.list[1].to_s == 15);
  CHECK(h.list[1].text == "big + double");
  // At 59: held at the run's end.
  CHECK(h.list[2].at_s == 59 && h.list[2].to_s == 60 && h.list[2].score == 8);
}

TEST(a_subject_and_a_best_of_cut) {
  const auto rs = rules();
  gr::HighlightPolicy one;
  one.subject = 1;
  const gr::Highlights h =
      gr::finish_highlights(gr::run_highlight_rules<Goal>(kGoals, rs), kKinds, 60, one);
  CHECK(h.list.size() == 2 && h.list[0].subject == 1 && h.list[1].subject == 1);
  gr::HighlightPolicy best;
  best.most = 2;
  const gr::Highlights b =
      gr::finish_highlights(gr::run_highlight_rules<Goal>(kGoals, rs), kKinds, 60, best);
  CHECK(b.list.size() == 2);
  CHECK(b.list[0].at_s == 13 && b.list[1].at_s == 59);  // scores 7 and 8, in time order
}

TEST(the_jump_goes_to_window_openings_and_on_past_one_just_reached) {
  const auto rs = rules();
  const gr::Highlights h =
      gr::finish_highlights(gr::run_highlight_rules<Goal>(kGoals, rs), kKinds, 60, {});
  CHECK(gr::next_highlight(h.list, 0, 1) == 1);  // the window at 0 is not after 0 (+slop)
  CHECK(gr::next_highlight(h.list, 2, 1) == 1);
  CHECK(gr::next_highlight(h.list, 10, 1) == 2);  // on past the one just jumped to
  CHECK(gr::next_highlight(h.list, 58, 1) == -1);
  CHECK(gr::next_highlight(h.list, 20, -1) == 1);
  CHECK(gr::next_highlight(h.list, 10, -1) == 0);  // back past the one just jumped to
  CHECK(gr::next_highlight(h.list, 0, -1) == -1);
  const std::vector<gr::Mark> marks = gr::highlight_marks(h.list, 10);
  CHECK(marks.size() == 3 && marks[1].step == 130 && marks[1].kind == "big" &&
        marks[1].weight == 7);
}

TEST(bad_kinds_are_named) {
  CHECK(gr::kinds_problem(kKinds).empty());
  CHECK(gr::kinds_problem(std::vector<gr::HighlightKind>{{"Big", "", 0, 1, 1, 1}})
            .find("not a mark kind") != std::string::npos);
  CHECK(gr::kinds_problem(
            std::vector<gr::HighlightKind>{{"a", "", 0, 1, 1, 1}, {"a", "", 0, 1, 1, 1}})
            .find("twice") != std::string::npos);
  CHECK(
      gr::kinds_problem(std::vector<gr::HighlightKind>{{"a", "", 0, -1, 1, 1}}).find("negative") !=
      std::string::npos);
  CHECK(gr::kinds_problem(std::vector<gr::HighlightKind>{{"a", "", 0, 1, 1, 0}}).find("scale") !=
        std::string::npos);
}
