// THE PIECES EVERY HUB PAGE IS BUILT OF (pages.cpp's, shared with radar.cpp): the head with its
// title and stylesheet, the nav bar, the foot, and an HTML answer.
//
// ge:summary The hub pages' shared pieces: head, nav, foot, the HTML answer
// ge:keywords hub pages head nav foot html answer
// ge:skill net
// ge:test tests/hub_pages_test.cpp
#pragma once

#include <string>

#include "hub/api.hpp"
#include "hub/spec.hpp"

namespace ge::hub {

// The page's head (`refresh`: the meta refresh every kPageRefreshS) and the body's start.
std::string head(const HubSpec& spec, const std::string& title, bool refresh);
// The nav bar, `on` the word of the page shown.
std::string nav(const HubSpec& spec, const char* on);
// The foot and the body's end.
std::string foot();
// An HTML answer, cacheable for `max_age_s`.
Answer html(int code, std::string body, int max_age_s);

}  // namespace ge::hub
