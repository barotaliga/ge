// A CHAIN of one model along a line, as the classic games lay it: Quake's bolts (the lightning
// gun's, the big monster's, the boss's) and Quake II's cables (a parasite's drain, a medic's line,
// the grapple). A link every kChainStep units, as many as the length over it rounded up, each
// turned along the line at a fresh roll every frame (world_fx.hpp's `place_chain` places them).
// Loose (Quake's bolts) each link starts kChainStep after the last, the last one overshooting the
// end; fitted (Quake II's cables) the first starts at the start and the last kChainStep short of
// the end, the rest spread evenly between (one alone at the start). The mission packs' beams take
// their own link length, face back, light and roll (`Chain`'s last fields).
//
// ge:summary A chain of one model along a line: links every 30 units, loose or fitted to the end
// ge:keywords chain bolt cable segment lightning-bolt chain_links chain_share place_chain
// ge:keywords heat-beam tesla-lightning
// ge:skill shooter render
// ge:test tests/shooters_edge_q2_model_fx_test.cpp
#pragma once

#include <cmath>

namespace shooters_edge::client {

inline constexpr double kChainStep = 30;
struct Chain {
  int modelindex = 0;
  bool fit = false;
  bool no_shadow = false;
  // The mission packs' beams (q2_model_fx.hpp): links `step` units long, each turned to face
  // back toward the start (`back`), drawn full bright, on model frame `frame`, all at the one roll
  // `roll` degrees (negative: a fresh roll each link).
  double step = kChainStep;
  bool back = false;
  bool full_bright = false;
  int frame = 0;
  double roll = -1;
};
inline int chain_links(double length, double step = kChainStep) {
  return length > 0 ? static_cast<int>(std::ceil(length / step)) : 0;
}
// Where link `i` of `n` starts, as a share of the way along `length` units.
inline double chain_share(int i, int n, double length, bool fit, double step = kChainStep) {
  if (length <= 0) return 0;
  if (!fit) return i * step / length;
  return n < 2 ? 0 : (length - step) * i / (n - 1) / length;
}

}  // namespace shooters_edge::client
