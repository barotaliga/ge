// A TV TRIAL: a made-up match's replay stream broadcast through a source's TV node, a relay under
// it and viewers on both, over the simulated network under one link condition, on a virtual clock
// off one seed. What the `tv` lab measures and the tests hold: every viewer's stream whole from
// the block it started at, chat up and down, the tree's count, each block's lag from the source to
// each viewer, and the bytes a second each viewer took.
//
// ge:summary A TV trial: a source, a relay and viewers on the simulated network, measured
// ge:keywords tv trial relay viewers lag bytes-per-second simulated-network preset run_tv_trial
// ge:skill replay net
// ge:test tests/tv_net_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "net/conditions.hpp"

namespace ge::tv {

struct TrialConfig {
  net::LinkConditions link;
  int blocks = 40;           // the match's one-second blocks
  int keyframe_every = 5;    // a keyframe block every this many
  int block_ms = 100;        // the clock between the source's blocks (100: ten times the speed)
  int viewers_direct = 1;    // viewers on the source's node
  int viewers_relayed = 2;   // viewers under the relay
  size_t chunk_bytes = 200;  // ten chunks a block, this many bytes each
  int64_t delay_ms = 0;      // the source node's delay
  uint32_t seed = 7;
};

struct TrialViewer {
  bool relayed = false;
  bool whole = false;  // ended, and its stream the source's from a keyframe block to the end
  int64_t first = -1;  // the block it started at
  int64_t blocks = 0;
  std::string chat;            // the lines it heard, joined
  int64_t told = 0;            // the tree's count it was last told
  std::vector<double> lag_ms;  // each block's arrival after the source wrote it
  double bytes_per_s = 0;      // what its link carried down a second of the match
  std::string error;
};

struct TrialResult {
  std::vector<TrialViewer> viewers;
  int64_t counted = 0;  // the source node's count of the whole tree
  int64_t source_bytes = 0;
  double seconds = 0;  // the virtual clock's run
  bool all_whole = false;
  std::string why;  // what was not whole
};

TrialResult run_tv_trial(const TrialConfig& config);

}  // namespace ge::tv
