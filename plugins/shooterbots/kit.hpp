// THE KIT'S NAMES, READ BY THE BOTS. A bot plays through the shooter kit (plugins/shooter: the
// move, the hulls, the server's world, the command), so every file of this plugin reads the kit's
// names unqualified: `ge::shooterbots` sees `ge::shooter` through one using-directive, stated here
// once. The kit's types a bot header names without its full header are declared here too.
//
// ge:summary The shooter kit's names seen from the bots' namespace, and its forward declarations
// ge:keywords shooterbots shooter-kit namespace using-directive forward-declaration server
// ge:skill shooterbots
#pragma once

namespace ge::shooter {
struct Server;
}  // namespace ge::shooter

namespace ge::shooterbots {
using namespace ge::shooter;  // NOLINT(google-build-using-namespace): the kit's names, read by
                              // every bot file
}  // namespace ge::shooterbots
