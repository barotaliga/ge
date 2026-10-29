// engine/net/token.hpp: a token made by the matchmaker opens with its host's key and gives back
// what was granted; another host's key, another game or any changed byte does not open; the
// client's copy survives its bytes.
//
// ge:summary Test: connect tokens made, opened, refused when changed, written and read
// ge:keywords test net token connect-token host-key matchmaker forged
// ge:skill net testing
#include "net/token.hpp"

#include <algorithm>
#include <cstring>

#include "check.hpp"
#include "net/packet.hpp"

using namespace ge::net;

namespace {

Key master_key() {
  Key k{};
  for (size_t i = 0; i < k.size(); i++) k[i] = static_cast<uint8_t>(3 * i + 1);
  return k;
}

ConnectToken make(uint64_t host, uint32_t protocol) {
  TokenGrant g;
  g.client_id = 4242;
  g.host_id = host;
  g.expire_us = 30000000;
  for (size_t i = 0; i < g.user.size(); i++) g.user[i] = static_cast<uint8_t>(i + 100);
  uint8_t random[kTokenRandomBytes];
  for (size_t i = 0; i < sizeof random; i++) random[i] = static_cast<uint8_t>(i * 7 + 5);
  return token_make(master_key(), protocol, g, random);
}

}  // namespace

TEST(token_opens_with_its_hosts_key) {
  const uint32_t protocol = protocol_id("token-test", 1);
  const ConnectToken t = make(9, protocol);
  OpenedToken o;
  CHECK(token_open(host_key(master_key(), 9), protocol, t.sealed.data(), o));
  CHECK(o.client_id == 4242 && o.host_id == 9 && o.expire_us == 30000000);
  CHECK(o.to_host == t.to_host && o.to_client == t.to_client && o.to_host != o.to_client);
  CHECK(o.user[0] == 100 && o.user[31] == 131);
}

TEST(token_refuses_another_host_another_game_and_any_change) {
  const uint32_t protocol = protocol_id("token-test", 1);
  const ConnectToken t = make(9, protocol);
  const Key key = host_key(master_key(), 9);
  OpenedToken o;
  CHECK(!token_open(host_key(master_key(), 10), protocol, t.sealed.data(), o));
  CHECK(!token_open(key, protocol_id("token-test", 2), t.sealed.data(), o));
  // Every byte of the sealed part matters: the host id, the expiry, the nonce, the private part
  // and the tag.
  int refused = 0;
  for (size_t i = 0; i < kSealedTokenBytes; i++) {
    std::array<uint8_t, kSealedTokenBytes> s = t.sealed;
    s[i] ^= 0x10;
    refused += !token_open(key, protocol, s.data(), o);
  }
  CHECK(refused == static_cast<int>(kSealedTokenBytes));
  // The keys are not in the sealed part as they are in the client's copy.
  CHECK(std::search(t.sealed.begin(), t.sealed.end(), t.to_host.begin(), t.to_host.end()) ==
        t.sealed.end());
}

TEST(token_survives_the_clients_bytes) {
  const ConnectToken t = make(3, 77);
  uint8_t bytes[kTokenBytes];
  token_write(t, bytes);
  ConnectToken back;
  token_read(bytes, back);
  CHECK(back.client_id == t.client_id && back.host_id == 3 && back.expire_us == t.expire_us);
  CHECK(back.to_host == t.to_host && back.to_client == t.to_client && back.sealed == t.sealed);
  // The sealed part fits in a request's padding with room to spare.
  CHECK(kRequestTokenAt + kSealedTokenBytes < kMaxPacket);
}
