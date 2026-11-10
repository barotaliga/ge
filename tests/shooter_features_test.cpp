// A BOT'S FEATURES (plugins/shooter/features.hpp): the presets, the words applied in order, the
// refusal of an unknown word naming every feature, and the text form read back.
//
// ge:summary Tests: a bot's feature switches, their presets and their text form
// ge:keywords test features apply_features features_text presets plain-bot
// ge:skill shooter testing
#include <string>

#include "check.hpp"
#include "shooter/features.hpp"

namespace sh = ge::shooter;

TEST(the_presets_are_every_feature_or_none_and_a_novice_does_not_glance) {
  for (size_t k = 0; k < sh::kFeatures; k++) {
    const auto x = static_cast<sh::Feature>(k);
    // A trial is never on by default, nor a feature whose head to head is not settled (`ahead`).
    CHECK(sh::features_all()[x] == (x != sh::Feature::trial && x != sh::Feature::ahead));
    CHECK(!sh::features_none()[x]);
  }
  sh::Features skilled = sh::features_all();
  skilled.set(sh::Feature::eye, false);  // no preset reads a turn a reaction late
  for (sh::AimSkill s : {sh::AimSkill::average, sh::AimSkill::expert, sh::AimSkill::perfect})
    CHECK(sh::features_of(s) == skilled);
  sh::Features novice = skilled;
  novice.set(sh::Feature::glance, false);
  CHECK(sh::features_of(sh::AimSkill::novice) == novice);
}

TEST(words_apply_in_order_and_an_unknown_one_names_every_feature) {
  std::string error;
  sh::Features f = sh::features_all();
  CHECK(sh::apply_features("-swimaim,+shortgoals", f, error));
  CHECK(!f[sh::Feature::swimaim]);
  CHECK(f[sh::Feature::shortgoals]);
  CHECK(sh::apply_features("plain, +dodge/rj", f, error));
  for (size_t k = 0; k < sh::kFeatures; k++) {
    const auto x = static_cast<sh::Feature>(k);
    CHECK(f[x] == (x == sh::Feature::dodge || x == sh::Feature::rj));
  }
  CHECK(!sh::apply_features("+dodge,-flying", f, error));
  CHECK(error.find("'flying'") != std::string::npos);
  CHECK(error.find("airsteer") != std::string::npos);
  CHECK(error.find("surface") != std::string::npos);
}

TEST(the_text_form_reads_back_to_the_same_set) {
  std::string error;
  CHECK(sh::features_text(sh::features_all()) == std::string("all"));
  CHECK(sh::features_text(sh::features_none()) == std::string("none"));
  sh::Features f = sh::features_all();
  CHECK(sh::apply_features("-calm,-packs", f, error));
  CHECK(sh::features_text(f) == std::string("all,-calm,-packs"));
  sh::Features g = sh::features_none();
  CHECK(sh::apply_features("+hop", g, error));
  CHECK(sh::features_text(g) == std::string("none,+hop"));
  sh::Features t = sh::features_all();
  CHECK(sh::apply_features("+trial", t, error));
  CHECK(sh::features_text(t) == std::string("all,+trial"));
  for (const sh::Features& x : {f, g, t}) {
    sh::Features back = sh::features_all();
    CHECK(sh::apply_features(sh::features_text(x), back, error));
    CHECK(back == x);
  }
}
