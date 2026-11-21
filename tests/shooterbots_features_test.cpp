// A BOT'S FEATURES (plugins/shooterbots/features.hpp): the presets, the words applied in order, the
// refusal of an unknown word naming every feature, and the text form read back.
//
// ge:summary Tests: a bot's feature switches, their presets and their text form
// ge:keywords test features apply_features features_text presets plain-bot
// ge:skill shooter testing
#include <string>

#include "check.hpp"
#include "shooterbots/features.hpp"

namespace sh = ge::shooter;

namespace sb = ge::shooterbots;

TEST(the_presets_are_every_feature_or_none_and_a_novice_does_not_glance) {
  for (size_t k = 0; k < sb::kFeatures; k++) {
    const auto x = static_cast<sb::Feature>(k);
    // A trial is never on by default, nor a feature not judged or dropped.
    bool off = false;
    for (sb::Feature o :
         {sb::Feature::trial,      sb::Feature::lookway,     sb::Feature::counter,
          sb::Feature::chains,     sb::Feature::retarget,    sb::Feature::runupkeys,
          sb::Feature::underway,   sb::Feature::gappast,     sb::Feature::airgap,
          sb::Feature::pilotcast,  sb::Feature::landwall,    sb::Feature::circlekeys,
          sb::Feature::circleaim,  sb::Feature::leadmean,    sb::Feature::leadarc,
          sb::Feature::airarc,     sb::Feature::hopopen,     sb::Feature::fightway,
          sb::Feature::letgo,      sb::Feature::retreatrun,  sb::Feature::nogoal,
          sb::Feature::tripkeep,   sb::Feature::routetime,   sb::Feature::fetch,
          sb::Feature::liftbrake,  sb::Feature::scriptentry, sb::Feature::calmvel,
          sb::Feature::lookturn,   sb::Feature::viewbook,    sb::Feature::gapturn,
          sb::Feature::brinkline,  sb::Feature::sidebrink,   sb::Feature::fallseen,
          sb::Feature::liplook,    sb::Feature::sidehop,     sb::Feature::lostdry,
          sb::Feature::jumpcuts,   sb::Feature::hopland,     sb::Feature::hoppitch,
          sb::Feature::aimhop,     sb::Feature::noroute,     sb::Feature::rlfetch,
          sb::Feature::holdpoint,  sb::Feature::errandreach, sb::Feature::fields,
          sb::Feature::fieldway,   sb::Feature::earlob,      sb::Feature::lobtake,
          sb::Feature::aimclear,   sb::Feature::ledgerhits,  sb::Feature::ledgerheal,
          sb::Feature::holdra,     sb::Feature::rushdetour,  sb::Feature::rlhunt,
          sb::Feature::rlgang,     sb::Feature::shareclock,  sb::Feature::circuit,
          sb::Feature::swapground})
      off = off || x == o;
    CHECK(sb::features_all()[x] == !off);
    CHECK(!sb::features_none()[x]);
  }
  sb::Features skilled = sb::features_all();
  skilled.set(sb::Feature::eye, false);  // no preset reads a turn a reaction late
  for (sb::AimSkill s : {sb::AimSkill::average, sb::AimSkill::expert, sb::AimSkill::perfect})
    CHECK(sb::features_of(s) == skilled);
  sb::Features novice = skilled;
  novice.set(sb::Feature::glance, false);
  CHECK(sb::features_of(sb::AimSkill::novice) == novice);
}

TEST(words_apply_in_order_and_an_unknown_one_names_every_feature) {
  std::string error;
  sb::Features f = sb::features_all();
  CHECK(sb::apply_features("-swimaim,+shortgoals", f, error));
  CHECK(!f[sb::Feature::swimaim]);
  CHECK(f[sb::Feature::shortgoals]);
  CHECK(sb::apply_features("plain, +dodge/rj", f, error));
  for (size_t k = 0; k < sb::kFeatures; k++) {
    const auto x = static_cast<sb::Feature>(k);
    CHECK(f[x] == (x == sb::Feature::dodge || x == sb::Feature::rj));
  }
  CHECK(!sb::apply_features("+dodge,-flying", f, error));
  CHECK(error.find("'flying'") != std::string::npos);
  CHECK(error.find("airsteer") != std::string::npos);
  CHECK(error.find("surface") != std::string::npos);
}

TEST(the_text_form_reads_back_to_the_same_set) {
  std::string error;
  CHECK(sb::features_text(sb::features_all()) == std::string("all"));
  CHECK(sb::features_text(sb::features_none()) == std::string("none"));
  sb::Features f = sb::features_all();
  CHECK(sb::apply_features("-calm,-packs", f, error));
  CHECK(sb::features_text(f) == std::string("all,-calm,-packs"));
  sb::Features g = sb::features_none();
  CHECK(sb::apply_features("+hop", g, error));
  CHECK(sb::features_text(g) == std::string("none,+hop"));
  sb::Features t = sb::features_all();
  CHECK(sb::apply_features("+trial", t, error));
  CHECK(sb::features_text(t) == std::string("all,+trial"));
  sb::Features k = sb::features_all();
  CHECK(sb::apply_features("+lookway", k, error));
  CHECK(sb::features_text(k) == std::string("all,+lookway"));
  for (const sb::Features& x : {f, g, t}) {
    sb::Features back = sb::features_all();
    CHECK(sb::apply_features(sb::features_text(x), back, error));
    CHECK(back == x);
  }
}
