// engine/render/cloud_kinds.hpp: every kind is found by its name in any case, its bands are in
// order and its form in range, a stack is read from names joined by '+' and says what is wrong
// with one it cannot read, and the billows' noise spans its range.
//
// ge:summary Tests for the cloud kinds, their bands and a stack read from a name
// ge:keywords test render clouds cloud-kinds cloud_stack_named cloud_kind_named cloud_detail
// ge:skill render testing
#include <set>
#include <string>

#include "check.hpp"
#include "render/cloud_kinds.hpp"
#include "render/clouds.hpp"

namespace r = ge::render;

TEST(every_kind_is_found_by_its_name_and_its_bands_are_in_order) {
  std::set<std::string> names;
  for (const r::CloudKind& k : r::cloud_kinds()) {
    CHECK(names.insert(k.name).second);
    CHECK(r::cloud_kind_named(k.name) == &k);
    std::string upper = k.name;
    for (char& c : upper) c = static_cast<char>(c - 'a' + 'A');
    CHECK(r::cloud_kind_named(upper) == &k);
    CHECK(r::cloud_kind_list().find(k.name) != std::string::npos);
    for (const double* band : {k.base, k.depth, k.cover, k.cell}) CHECK(band[0] <= band[1]);
    CHECK(k.depth[0] > 0 && k.cell[0] > 0 && k.cover[0] > 0 && k.cover[1] <= 1);
    CHECK(k.stretch >= 1 && k.dome >= 0 && k.dome <= 1 && k.opacity > 0 && k.life > 0);
    for (double f : {k.anvil, k.erode, k.fibre, k.patch, k.halo, k.deck}) CHECK(f >= 0 && f <= 1);
    // Particles a corona or an aureole is diffracted round: droplets of a few um to crystals of
    // tens, and a spread the shader's five sizes keep positive.
    CHECK(k.drop >= 3 && k.drop <= 60 && k.spread > 0 && k.spread <= 0.6);
    // A halo only off ice (the large particles).
    if (k.halo > 0) CHECK(k.drop >= 25);
    // Its thinnest patches a share of its cores, a patch at least a cell across.
    CHECK(k.thin > 0 && k.thin <= 1 && k.thick_cells >= 1);
  }
  CHECK(names.size() >= 13);
  CHECK(r::cloud_kind_named("cumulonimbus")->anvil > 0);
  CHECK(r::cloud_kind_named("cirrus")->fibre > 0);
  CHECK(r::cloud_kind_named("cirrocumulus")->patch > 0);
  CHECK(r::cloud_kind_named("lenticular")->erode < 0.2);
  // The veil the sun and the moon shine through: whole-sky ice, thin, its halo the plainest.
  const r::CloudKind* veil = r::cloud_kind_named("veil");
  CHECK(veil != nullptr && veil->cover[0] > 0.9 && veil->opacity < 1 && veil->halo == 1);
  // A corona's rings off a thin altocumulus's even droplets; an altostratus's glare off mixed
  // ones, lit through its top as a deck.
  CHECK(r::cloud_kind_named("altocumulus")->spread < 0.2);
  CHECK(r::cloud_kind_named("altostratus")->spread > 0.3);
  CHECK(r::cloud_kind_named("altostratus")->deck == 1 && r::cloud_kind_named("cirrus")->deck == 0);
  CHECK(r::cloud_kind_named("mammoth") == nullptr);
}

TEST(the_sun_shows_through_the_thin_patches_of_the_kinds_it_does_in_the_sky) {
  const auto thinnest = [](const char* name) {
    const r::CloudKind* k = r::cloud_kind_named(name);
    return k->opacity * k->thin;
  };
  const auto thickest = [](const char* name) { return r::cloud_kind_named(name)->opacity; };
  // A grey middle veil runs from the sun as through frosted glass to hiding it; a low grey lid's
  // thin patches show a watery disc; a rain deck never shows it; a high veil never hides it.
  CHECK(thinnest("altostratus") < 2 && thickest("altostratus") > 20);
  CHECK(thinnest("stratus") < 5 && thickest("stratus") > 20);
  CHECK(thinnest("nimbostratus") > 30);
  CHECK(thickest("veil") < 1 && thickest("cirrostratus") < 2);
  // The kinds that vary most are the sheets; a storm's tower is thick all through.
  CHECK(r::cloud_kind_named("cumulonimbus")->thin >= 0.5);
}

TEST(a_stack_is_read_from_names_joined_by_plus) {
  const r::CloudKind* kinds[r::kCloudLayers];
  int count = -1;
  CHECK(r::cloud_stack_named("cumulus+Cirrus", kinds, count).empty());
  CHECK(count == 2 && kinds[0] == r::cloud_kind_named("cumulus") &&
        kinds[1] == r::cloud_kind_named("cirrus"));
  CHECK(r::cloud_stack_named(" stratus + altostratus ", kinds, count).empty() && count == 2);
  CHECK(r::cloud_stack_named("", kinds, count).empty() && count == 0);
  // A name no kind has: what went wrong, naming the kinds; those before it kept.
  const std::string bad = r::cloud_stack_named("cumulus+fluff", kinds, count);
  CHECK(bad.find("fluff") != std::string::npos && bad.find("cirrus") != std::string::npos);
  CHECK(count == 1);
  // One too many.
  const std::string many =
      r::cloud_stack_named("cumulus+cirrus+stratus+altocumulus+wisps", kinds, count);
  CHECK(!many.empty() && count == r::kCloudLayers);
}

TEST(the_billows_span_their_range_and_differ_from_the_column) {
  const auto& d = r::cloud_detail();
  const auto& n = r::cloud_noise();
  CHECK(d.size() == n.size());
  int lo = 255, hi = 0;
  size_t same = 0;
  for (size_t i = 0; i < d.size(); ++i) {
    lo = std::min<int>(lo, d[i]);
    hi = std::max<int>(hi, d[i]);
    same += d[i] == n[i];
  }
  CHECK(lo == 0 && hi == 255);
  CHECK(same < d.size() / 20);
}

TEST(patches_thin_a_column_and_never_fill_one) {
  r::CloudLayer l;
  l.cover = 0.5;
  l.cell_m = 800;
  double full = 0, patched = 0;
  for (int i = 0; i < 64; ++i)
    for (int j = 0; j < 64; ++j) {
      l.patch = 0;
      const double a = r::cloud_column(l, i * 97.0, j * 89.0);
      l.patch = 1;
      const double b = r::cloud_column(l, i * 97.0, j * 89.0);
      CHECK(b <= a + 1e-12);
      full += a;
      patched += b;
    }
  CHECK(patched < 0.8 * full && patched > 0.1 * full);
}
