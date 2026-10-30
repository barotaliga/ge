// The `gpu-bsp` lab's views picked by hand: `--view=<map>:x,y,z,heading,pitch` (m, degrees), or
// `<map>:x,y,z,sun[,lens]` and `<map>:x,y,z,moon[,lens]`, an eye aimed at the body with
// `ge::render::aim_at` once the map's sky is dressed (its heading is not known before).
//
// ge:summary The gpu-bsp lab's hand-picked views: an eye and a heading, or aimed at the sun or moon
// ge:keywords lab bsp view parse_view eye heading pitch aim sun moon lens look-at-sky
// ge:skill brush labs
#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "render/view.hpp"

namespace ge::brush::labs {

struct View {
  std::string id, title, caption;
  size_t map = 0;
  double eye[3] = {0, 0, 0};
  double heading = 0, pitch = 0;
  // Aimed at a sky body once the sky is dressed (`sun` or `moon` for the heading): none, or the
  // body and a lens (rad; 0 keeps the views' own).
  bool aimed = false;
  ge::render::SkyBody body = ge::render::SkyBody::sun;
  double lens = 0;
};

// A `--view` spec read against the maps' names; false when it is neither form or names no map.
bool parse_view(const std::string& spec, const std::vector<std::string>& names, View* out);

}  // namespace ge::brush::labs
