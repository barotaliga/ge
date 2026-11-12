# Player accounts (engine/account, docs/accounts.md): the store a master keeps (accounts, names
# reserved by a folded key, Argon2id hashes, the throttle, one-time codes) and its log on disk, the
# tickets the master signs and the service that answers sealed asks over the network. Its slow hash and digests are engine/net's (net/crypto.hpp, over Monocypher). No
# clock and no randomness of its own: the caller passes the time and fresh bytes, so a lab runs a
# store off one seed.
file(GLOB GE_ACCOUNT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine/account/*.cpp)
add_library(ge_account STATIC ${GE_ACCOUNT_SOURCES})
target_link_libraries(ge_account PUBLIC ge_core ge_net)

ge_module(ge_account EXPORT account)
