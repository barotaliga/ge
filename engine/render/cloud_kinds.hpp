// THE KINDS OF CLOUD: the genera a sky is built from, each a row of bands a world deals one
// layer from (render/view.hpp's `CloudLayer`): the heap of a fair day (cumulus) up to the storm's
// tower and its anvil (cumulonimbus), the low sheets (stratocumulus's rolls, stratus's grey lid,
// nimbostratus's dark rain deck), the middle layers (altocumulus's cells, altostratus's veil the
// sun shows through as through frosted glass), the high ice (cirrus's hairs, cirrostratus's
// milky veil, cirrocumulus's fine ripples), the lens a mountain wave stands still (lenticular)
// and a sea of cloud filling the valleys under a clear sky (inversion). A game names a stack of
// them (`cloud_stack_named`, a cvar's word: "cumulus+cirrus") and deals each kind's bands off its
// own seed.
//
// WHAT A ROW SAYS: the bands [least, most] of its base over the ground's floor, its depth, its
// cover and its cell's span across the wind (m); how far a cell is drawn out along the wind;
// 1 a heap or 0 a sheet; the optical depth a full column shows the eye and takes off the sun; a
// cell's life (s) and the least speed the kind is seen to move at aloft (m/s); and its FORM
// (`CloudLayer`'s): the anvil a tower's top spreads into, how deep the detail eats its edge (a
// heap's boil against a lens's smooth skin), how far the detail is combed along the wind (ice's
// hairs), and how far its cells gather into patches with clear sky between (a mackerel sky).
//
// ge:summary The cloud genera a sky is built from, named and stacked, each a row of bands
// ge:keywords render clouds cloud-kinds cloud-genera cumulus congestus cumulonimbus anvil
// ge:keywords stratocumulus stratus nimbostratus altocumulus altostratus cirrus cirrostratus
// ge:keywords cirrocumulus lenticular inversion sea-of-cloud mackerel-sky cloud_stack_named
// ge:keywords cloud_kind_named cloud-form erode fibre patch
// ge:skill render
// ge:test tests/render_cloud_kinds_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>

#include "render/view.hpp"

namespace ge::render {

struct CloudKind {
  const char* name;
  const char* caption;
  double base[2], depth[2], cover[2], cell[2];
  double stretch, dome, opacity, shadow_tau, life, speed;
  // Its form (CloudLayer's): the anvil, the detail's bite, its comb and the patches.
  double anvil = 0, erode = 0.6, fibre = 0, patch = 0;
};

// Every kind, lowest first.
std::span<const CloudKind> cloud_kinds();
// The kind `name` names (case-insensitive), or null.
const CloudKind* cloud_kind_named(std::string_view name);
// Every kind's name, comma-separated (a diagnostic's list).
const std::string& cloud_kind_list();

// A STACK of kinds, the names joined by '+' ("cumulus+cirrus"), at most kCloudLayers: `out` its
// kinds in order and `count` how many. A name no kind has or one too many is what the returned
// problem says (empty when none); the kinds named before it are kept.
std::string cloud_stack_named(std::string_view names, const CloudKind* out[kCloudLayers],
                              int& count);

}  // namespace ge::render
