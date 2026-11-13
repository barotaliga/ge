// Tests the map from one figure's frames to another's by their names' kinds.
//
// ge:summary Tests figure_frames: kinds by stem, runs by kind in turn, the pace kept, the rest
// ge:keywords test figure_frames frame_kind player-figures
// ge:skill shooter
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/app/figure_frames.hpp"

namespace md = shooters_edge::models;

namespace {

std::vector<std::string> names(std::initializer_list<std::pair<const char*, int>> runs) {
  std::vector<std::string> out;
  for (const auto& [stem, n] : runs)
    for (int i = 1; i <= n; i++)
      out.push_back(std::string(stem) + (i < 10 ? "0" : "") + std::to_string(i));
  return out;
}

}  // namespace

TEST(a_frames_kind_is_its_stems) {
  CHECK(md::frame_kind("axrun3") == md::FrameKind::run);
  CHECK(md::frame_kind("stand01") == md::FrameKind::stand);
  CHECK(md::frame_kind("axstnd12") == md::FrameKind::stand);
  CHECK(md::frame_kind("axdeth4") == md::FrameKind::death);
  CHECK(md::frame_kind("death203") == md::FrameKind::death);
  CHECK(md::frame_kind("pain301") == md::FrameKind::pain);
  CHECK(md::frame_kind("nailatt1") == md::FrameKind::attack);
  CHECK(md::frame_kind("light2") == md::FrameKind::attack);
  CHECK(md::frame_kind("crattak3") == md::FrameKind::none);  // crouched
  CHECK(md::frame_kind("salute05") == md::FrameKind::none);
}

TEST(each_run_plays_the_other_figures_run_of_its_kind_at_its_pace) {
  const std::vector<std::string> from = names({{"axrun", 6},
                                               {"rockrun", 6},
                                               {"stand", 5},
                                               {"pain", 6},
                                               {"deatha", 11},
                                               {"deathb", 9},
                                               {"shotatt", 6},
                                               {"salute", 2}});
  const std::vector<std::string> to = names({{"stand", 40},
                                             {"run", 6},
                                             {"attack", 8},
                                             {"pain1", 4},
                                             {"pain2", 4},
                                             {"death1", 6},
                                             {"death2", 6}});
  const std::vector<int> map = md::figure_frames(from, to);
  CHECK(map.size() == from.size());
  CHECK(map[0] == 40 && map[5] == 45);    // axrun onto run, frame for frame
  CHECK(map[6] == 40 && map[11] == 45);   // rockrun onto the same run
  CHECK(map[12] == 0 && map[16] == 32);   // 5 stands over 40: k x 40 / 5
  CHECK(map[17] == 54 && map[22] == 57);  // pain onto pain1, 6 over 4
  CHECK(map[23] == 62 && map[33] == 67);  // deatha onto death1
  CHECK(map[34] == 68);                   // deathb onto death2
  CHECK(map[43] == 46 && map[48] == 52);  // shotatt onto attack, 6 over 8
  CHECK(map[49] == 0 && map[50] == 0);    // no kind: the first stand
  CHECK(md::figure_frames(from, {}).empty());
}

TEST(a_kind_the_other_figure_lacks_rests_on_its_stand) {
  const std::vector<std::string> from = names({{"jump", 3}});
  const std::vector<std::string> to = names({{"flip", 2}, {"stand", 4}});
  const std::vector<int> map = md::figure_frames(from, to);
  CHECK(map.size() == 3 && map[0] == 2 && map[2] == 2);
}
