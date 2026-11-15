// The models a player hides (model_filters.hpp).
//
// ge:summary Which models cl_gibfilter and cl_deadbodyfilter hide
// ge:keywords cl_gibfilter cl_deadbodyfilter hidden_by
// ge:skill render shooter
// ge:test tests/shooters_edge_model_filters_test.cpp
#include "shooters_edge/client/model_filters.hpp"

namespace shooters_edge::client {

namespace {
constexpr uint32_t kTrailGib = 4;  // the model file's flag: it bleeds as it flies (app/effects.hpp)
}

bool hidden_by(const ModelFilters& f, uint32_t flags, const std::vector<bool>& resting, int frame) {
  if (f.gibs && (flags & kTrailGib) != 0) return true;
  return f.still_bodies && frame >= 0 && size_t(frame) < resting.size() && resting[size_t(frame)];
}

}  // namespace shooters_edge::client
