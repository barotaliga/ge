// A TEST FIXTURE, built as a mod package by tests/CMakeLists.txt (cmake/Mods.cmake): a mod whose
// table says it was built against mod ABI 2.0, a major this engine does not speak. Its manifest
// (written by the build) says 1.0, so the loader gets past the manifest and must refuse the table
// itself, naming both versions (tests/mod_loader_test.cpp).
//
// ge:summary Test fixture: a mod library whose table claims mod ABI 2.0, for the loader's refusal
// ge:keywords mod fixture abi-version wrong-abi refuse abi-two
// ge:skill mod testing
// ge:test tests/mod_loader_test.cpp
#include "shooters_edge/sim/mod_api.hpp"

namespace {

const se_mod_hooks_v1 kHooks = {sizeof(se_mod_hooks_v1),
                                SE_MOD_API,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr,
                                nullptr};

const ge_mod_v1 kMod = {sizeof(ge_mod_v1), 2u << 16, "abi-two", "1.0.0", SE_MOD_GAME,
                        SE_MOD_API,        0,        nullptr,   &kHooks};

}  // namespace

// Answers whatever the host speaks: the refusal is the loader's to make.
GE_MOD_EXPORT const ge_mod_v1* ge_mod_entry(uint32_t /*host_abi*/) { return &kMod; }
