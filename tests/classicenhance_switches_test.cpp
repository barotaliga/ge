// Every classic-enhance effect a switch (plugins/classicenhance/switches.hpp): the catalog's ids
// are unique and each names a field of its own; every group has switches and every switch a
// group; the cvars are ce_<id>, archived, at the catalog's defaults, and writing one writes the
// value; the bake switches reach the pipeline's options.
//
// ge:summary Tests the classic-enhance switches' catalog, their ce_ cvars and their bake options
// ge:keywords test classicenhance switches catalog cvar ce_ toggle bake options defaults
// ge:skill classicenhance testing
#include <set>
#include <string>

#include "check.hpp"
#include "classicenhance/switches.hpp"
#include "console/cvar.hpp"

namespace ce = ge::classicenhance;

TEST(every_switch_has_its_own_id_field_and_group) {
  std::set<std::string_view> ids;
  std::set<const bool*> fields;
  const ce::Switches probe;
  for (const ce::SwitchSpec& s : ce::switches()) {
    CHECK_MSG(ids.insert(s.id).second, std::string(s.id) + " is in the catalog twice");
    CHECK_MSG(fields.insert(&(probe.*s.field)).second, std::string(s.id) + " shares another's field");
    CHECK(!s.text.empty() && s.hint.find("Off:") != std::string_view::npos);
    bool grouped = false;
    for (size_t g = 0; g < ce::kSwitchGroupCount; g++) grouped |= ce::kSwitchGroups[g] == s.group;
    CHECK_MSG(grouped, std::string(s.id) + "'s group is not in kSwitchGroups");
    CHECK(ce::find_switch(s.id) == &s);
  }
  // Every bool of Switches is in the catalog.
  CHECK(ce::kSwitchCount * sizeof(bool) == sizeof(ce::Switches));
  for (size_t g = 0; g < ce::kSwitchGroupCount; g++) {
    int n = 0;
    for (const ce::SwitchSpec& s : ce::switches()) n += s.group == ce::kSwitchGroups[g];
    CHECK_MSG(n > 0, std::string(ce::kSwitchGroups[g]) + " has no switch");
  }
  // What the player asked for by name.
  for (const char* id : {"decals", "flames", "skies", "water", "lava", "muzzles", "teleports",
                         "rubble"}) {
    CHECK_MSG(ce::find_switch(id) != nullptr, std::string(id) + " is not a switch");
  }
  CHECK(ce::find_switch("nothing") == nullptr);
}

TEST(each_switch_is_an_archived_cvar_that_writes_its_value) {
  ge::console::Cvars cvars;
  ce::Switches values;
  values.decals = false;  // binding writes the fallback
  ce::bind_switches(cvars, &values);
  CHECK(values.decals && !values.rounded);
  for (const ce::SwitchSpec& s : ce::switches()) {
    const ge::console::Cvar* c = cvars.find("ce_" + std::string(s.id));
    CHECK_MSG(c != nullptr, "ce_" + std::string(s.id) + " was not bound");
    if (c == nullptr) continue;
    CHECK((c->flags & ge::console::kArchive) != 0);
    CHECK(c->kind == ge::console::CvarKind::boolean);
    CHECK(c->on() == ce::Switches{}.*s.field);
  }
  CHECK(cvars.set("ce_decals", "0", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(!values.decals);
  CHECK(cvars.set("ce_rounded", "1", ge::console::Source::local) ==
        ge::console::SetResult::changed);
  CHECK(values.rounded);
}

TEST(the_bake_switches_reach_the_pipeline_options) {
  ce::Switches values;
  ce::RunOptions opts;
  ce::apply_bakes(values, &opts);
  CHECK(opts.models.round == 0);
  values.rounded = true;
  values.upscaler = false;
  ce::apply_bakes(values, &opts);
  CHECK(opts.models.round == 4);
  CHECK(opts.textures.network == nullptr);
  int bakes = 0;
  for (const ce::SwitchSpec& s : ce::switches()) bakes += s.bake;
  CHECK(bakes == 2);
}
