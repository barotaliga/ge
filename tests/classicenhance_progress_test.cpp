// The enhancing watched (plugins/classicenhance/progress.hpp): the board follows the pipeline's
// reports stage by stage (waiting, running with its count and item, done, up to date, failed),
// its whole fraction and the time left on the caller's clock, the last textures kept small and
// at most six; and `shrink` keeps a picture's proportions.
//
// ge:summary Tests the progress board's states, fraction, time left and thumbnails, and shrink
// ge:keywords test classicenhance progress board stage state fraction eta thumbnail shrink
// ge:skill classicenhance testing
#include <string>

#include "check.hpp"
#include "classicenhance/progress.hpp"

namespace ce = ge::classicenhance;
using Kind = ce::Progress::Kind;

namespace {

ge::assets::Image picture(int w, int h, uint8_t v) {
  ge::assets::Image img;
  img.width = w, img.height = h;
  img.rgba.assign(size_t(w) * size_t(h) * 4, v);
  return img;
}

ce::Progress report(Kind k, ce::Stage s, uint32_t done = 0, uint32_t total = 0,
                    std::string item = {}) {
  ce::Progress p;
  p.kind = k, p.stage = s, p.done = done, p.total = total, p.item = std::move(item);
  return p;
}

}  // namespace

TEST(board_follows_every_stage_to_ready) {
  ce::ProgressBoard board;
  ce::BoardView v = board.view();
  CHECK(v.stages.size() == 6 && !v.started && v.fraction() == 0);
  for (const ce::StageView& s : v.stages) CHECK(s.state == ce::StageState::waiting);

  board.on(report(Kind::skipped, ce::Stage::pack, 0, 0, "up to date"), 10);
  board.on(report(Kind::started, ce::Stage::textures, 0, 8), 10);
  const ge::assets::Image before = picture(64, 32, 10), after = picture(256, 128, 200);
  for (uint32_t i = 1; i <= 4; i++) {
    ce::Progress p = report(Kind::item, ce::Stage::textures, i, 0, "wall" + std::to_string(i));
    p.before = &before, p.after = &after;
    board.on(p, 10 + 2.0 * i);
  }
  v = board.view();
  CHECK(v.started && !v.finished && !v.failed);
  CHECK(v.elapsed == 8);
  CHECK(v.stages[0].state == ce::StageState::skipped);
  const ce::StageView& t = v.stages[size_t(ce::Stage::textures)];
  CHECK(t.state == ce::StageState::running && t.done == 4 && t.total == 8 && t.item == "wall4");
  // Two of six stages' worth: the pack's whole one and half the textures.
  CHECK(v.fraction() == 1.5 / 6);
  // Four items left at 2 s each.
  CHECK(v.left == 8);
  CHECK(v.textures_done == 4 && v.recent.size() == 4 && v.recent.front().name == "wall4");
  CHECK(v.recent.front().before.width == 64 && v.recent.front().after.width == 96 &&
        v.recent.front().after.height == 48);

  for (uint32_t i = 5; i <= 8; i++) {
    ce::Progress p = report(Kind::item, ce::Stage::textures, i, 0, "wall" + std::to_string(i));
    p.before = &before, p.after = &after;
    board.on(p, 10 + 2.0 * i);
  }
  board.on(report(Kind::finished, ce::Stage::textures, 8, 0, "8 textures"), 26);
  CHECK(board.view().recent.size() == ce::ProgressBoard::kRecent);
  for (const ce::Stage s : {ce::Stage::lightmaps, ce::Stage::sounds, ce::Stage::models}) {
    board.on(report(Kind::started, s, 0, 1), 26);
    board.on(report(Kind::finished, s, 1, 0, "done"), 27);
  }
  CHECK(!board.view().finished);
  board.on(report(Kind::skipped, ce::Stage::pictures, 0, 0, "up to date"), 27);
  v = board.view();
  CHECK(v.finished && v.fraction() == 1 && v.left == 0);
  CHECK(v.stages[size_t(ce::Stage::textures)].seconds == 16);
}

TEST(board_names_the_stage_that_failed) {
  ce::ProgressBoard board;
  board.on(report(Kind::started, ce::Stage::sounds, 0, 3), 0);
  board.on(report(Kind::item, ce::Stage::sounds, 1, 0, "a.wav"), 1);
  board.on(report(Kind::failed, ce::Stage::sounds, 1, 0, "cancelled"), 2);
  const ce::BoardView v = board.view();
  CHECK(v.failed && !v.finished);
  CHECK(v.stages[size_t(ce::Stage::sounds)].state == ce::StageState::failed);
  CHECK(v.error == "sounds: cancelled");
  CHECK(std::string(ce::state_name(ce::StageState::failed)) == "failed");
}

TEST(shrink_keeps_the_proportions_and_averages) {
  ge::assets::Image img = picture(200, 100, 0);
  for (size_t i = 0; i < img.rgba.size(); i += 8) img.rgba[i] = 255;  // every other column red
  const ge::assets::Image s = ce::shrink(img, 50);
  CHECK(s.width == 50 && s.height == 25);
  CHECK(s.rgba[0] == 128);  // four columns averaged, rounded
  const ge::assets::Image same = ce::shrink(picture(20, 10, 7), 96);
  CHECK(same.width == 20 && same.rgba[0] == 7);
}
