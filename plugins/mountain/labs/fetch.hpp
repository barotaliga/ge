// THE LABS' DOWNLOADS: one URL fetched to one file with `curl`, the HTTP status read back, the
// file written whole or not at all (it is downloaded beside its name and renamed into place). What
// the bakes that read public data (`real-faces`' elevation tiles, `real-hints`' map data) share.
// Nothing here needs credentials; nothing else in the tree touches the network.
//
// ge:summary Lab tooling: a URL fetched to a file with curl, its HTTP status read back
// ge:keywords fetch curl download http status bake
// ge:skill mountain labs
// ge:test tests/lab_runner_test.cpp
#pragma once

#include <string>

namespace ge::mountain::labs {

// Fetch `url` into `file`: the HTTP status ("200", "404"; empty when curl could not be run or
// reached nothing). The file is written only on a 200; on anything else it is left as it was.
std::string fetch_to(const std::string& url, const std::string& file);

}  // namespace ge::mountain::labs
