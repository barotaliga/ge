// Quake II's user info kept up to date (games/shooters_edge/net/q2_remote.hpp's
// q2_user_info_moved): the settings as joined send nothing; a new skin (with the model's folder),
// hand, fov (cl_fov rounded) and msg move the user info once. The rename reaching a server is held
// by tests/shooters_edge_q2_net_test.cpp.
//
// ge:summary Tests Quake II's user info moved by the player's settings while joined
// ge:keywords test userinfo user-info q2_user_info_moved setinfo rename
// ge:skill net shooter testing
#include <map>
#include <string>

#include "check.hpp"
#include "shooters_edge/net/q2_remote.hpp"

namespace cn = shooters_edge::net;

TEST(the_user_info_moves_once_when_a_setting_in_it_changes) {
  // The settings as the console holds them, read through `Remote::setting`.
  std::map<std::string, std::string> set{{"name", "eve"}, {"skin", "male/grunt"}};
  cn::Q2ClientConfig q;
  q.name = "eve";
  const auto setting = [&set](const std::string& k) {
    const auto at = set.find(k);
    return at == set.end() ? std::string() : at->second;
  };
  CHECK(!cn::q2_user_info_moved(q, setting));  // as joined: nothing to send
  set["skin"] = "base";
  set["model"] = "female";
  set["hand"] = "2";
  set["cl_fov"] = "105.6";
  set["msg"] = "3";
  CHECK(cn::q2_user_info_moved(q, setting));
  CHECK(q.skin == "female/base" && q.hand == 2 && q.fov == 106 && q.msg == 3);
  CHECK(!cn::q2_user_info_moved(q, setting));  // sent once
}
