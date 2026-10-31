// The labs' side of a staged pack (client/stage.hpp): the runner's `--stage-pak=<name>` staged
// over a lab's search path, a note naming what it filled, a failure naming the packs there are.
//
// ge:summary The SE labs' --stage-pak: the named pack staged over a lab's search path, noted
// ge:keywords stage-pak fixture stand-in lab search-path note
// ge:skill shooter labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <string>
#include <vector>

#include "lab/lab.hpp"
#include "shooters_edge/client/stage.hpp"

namespace shooters_edge::labs {

// False (the lab failed) when the name is no pack's; true with nothing done when none was asked.
inline bool stage_asked(ge::lab::Lab& lab, client::SearchPath& files) {
  const std::string& name = lab.stage_pak();
  if (name.empty()) return true;
  std::vector<std::string> staged;
  std::string error;
  if (!client::stage_pak(files, name, &staged, error)) {
    lab.fail("--stage-pak: " + error, "Skill: shooter.");
    return false;
  }
  std::string list;
  for (const std::string& f : staged) list += (list.empty() ? "" : ", ") + f;
  lab.note("--stage-pak=" + name + ": staged " +
           (list.empty() ? std::string("nothing (the files hold every one)") : list));
  return true;
}

}  // namespace shooters_edge::labs
