// WHICH VARIANT a shared rule file is compiled for: the shared rules (common_*.cpp) are built once
// for the arena and once for the campaign (games/shooters_edge/CMakeLists.txt defines
// SE_RULES_CAMPAIGN for the second), each against its own declaration and in its own namespace
// (`rules::SE_RULES_NS`), as the QC variants compiled one common source against their own
// definitions. Internal to games/shooters_edge/rules.
//
// ge:summary Picks the arena's or the campaign's declaration for a shared rule file (internal)
// ge:keywords rules-common variant se_rules_campaign se_rules_ns native-arena native-campaign
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#pragma once

#if defined(SE_RULES_CAMPAIGN)
#include "shooters_edge/rules/campaign_decl.hpp"
#define SE_RULES_NS campaign
#else
#include "shooters_edge/rules/arena_decl.hpp"
#define SE_RULES_NS arena
#endif
