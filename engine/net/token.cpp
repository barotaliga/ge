// Connect tokens (net/token.hpp): made under a host's derived key, opened by the host, and the
// client's copy as bytes.
//
// ge:summary Connect tokens made, opened, written and read; the OS's random bytes
// ge:keywords net token connect-token seal open host-key os-random
// ge:skill net
// ge:test tests/net_token_test.cpp
#include "net/token.hpp"

#include <cstring>

#include "net/udp_socket.hpp"
#include "net/wire.hpp"

namespace ge::net {

namespace {

// The sealed part's fields, in order.
constexpr size_t kAtHost = 0;
constexpr size_t kAtExpire = 8;
constexpr size_t kAtNonce = 16;
constexpr size_t kAtPrivate = kAtNonce + kNonceBytes;
constexpr size_t kAtTag = kAtPrivate + kSealedTokenPrivate;
static_assert(kAtTag + kTagBytes == kSealedTokenBytes);

// The associated data: the game's protocol id, the host id and the expiry.
void token_ad(uint8_t* ad, uint32_t protocol, const uint8_t* sealed) {
  for (int i = 0; i < 4; i++) ad[i] = static_cast<uint8_t>(protocol >> (8 * i));
  std::memcpy(ad + 4, sealed + kAtHost, 16);
}

void put_u64(uint8_t* p, uint64_t v) {
  for (int i = 0; i < 8; i++) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint64_t get_u64(const uint8_t* p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) v |= static_cast<uint64_t>(p[i]) << (8 * i);
  return v;
}

}  // namespace

Key host_key(const Key& master, uint64_t host_id) { return derive_key(master, "hostkey", host_id); }

ConnectToken token_make(const Key& master, uint32_t protocol, const TokenGrant& grant,
                        const uint8_t* random) {
  ConnectToken t;
  t.client_id = grant.client_id;
  t.host_id = grant.host_id;
  t.expire_us = grant.expire_us;
  std::memcpy(t.to_host.data(), random + kNonceBytes, 32);
  std::memcpy(t.to_client.data(), random + kNonceBytes + 32, 32);

  uint8_t* s = t.sealed.data();
  put_u64(s + kAtHost, grant.host_id);
  put_u64(s + kAtExpire, static_cast<uint64_t>(grant.expire_us));
  std::memcpy(s + kAtNonce, random, kNonceBytes);
  uint8_t* p = s + kAtPrivate;
  put_u64(p, grant.client_id);
  std::memcpy(p + 8, t.to_host.data(), 32);
  std::memcpy(p + 40, t.to_client.data(), 32);
  std::memcpy(p + 72, grant.user.data(), kTokenUserBytes);

  uint8_t ad[20];
  token_ad(ad, protocol, s);
  Key key = host_key(master, grant.host_id);
  seal(p, kSealedTokenPrivate, s + kAtTag, key, s + kAtNonce, ad, sizeof ad);
  wipe(key.data(), key.size());
  return t;
}

bool token_open(const Key& key, uint32_t protocol, const uint8_t* sealed, OpenedToken& out) {
  uint8_t copy[kSealedTokenBytes];
  std::memcpy(copy, sealed, sizeof copy);
  uint8_t ad[20];
  token_ad(ad, protocol, copy);
  uint8_t* p = copy + kAtPrivate;
  if (!open(p, kSealedTokenPrivate, copy + kAtTag, key, copy + kAtNonce, ad, sizeof ad)) {
    return false;
  }
  out.host_id = get_u64(copy + kAtHost);
  out.expire_us = static_cast<int64_t>(get_u64(copy + kAtExpire));
  out.client_id = get_u64(p);
  std::memcpy(out.to_host.data(), p + 8, 32);
  std::memcpy(out.to_client.data(), p + 40, 32);
  std::memcpy(out.user.data(), p + 72, kTokenUserBytes);
  std::memcpy(out.tag.data(), copy + kAtTag, kTagBytes);
  wipe(copy, sizeof copy);
  return true;
}

void token_write(const ConnectToken& t, uint8_t* out) {
  Writer w = make_writer(out, kTokenBytes);
  write_u64(w, t.client_id);
  write_u64(w, t.host_id);
  write_u64(w, static_cast<uint64_t>(t.expire_us));
  write_bytes(w, t.to_host.data(), 32);
  write_bytes(w, t.to_client.data(), 32);
  write_bytes(w, t.sealed.data(), kSealedTokenBytes);
}

void token_read(const uint8_t* in, ConnectToken& t) {
  Reader r = make_reader(in, kTokenBytes);
  t.client_id = read_u64(r);
  t.host_id = read_u64(r);
  t.expire_us = static_cast<int64_t>(read_u64(r));
  read_bytes(r, t.to_host.data(), 32);
  read_bytes(r, t.to_client.data(), 32);
  read_bytes(r, t.sealed.data(), kSealedTokenBytes);
}

void os_random_bytes(uint8_t* out, size_t size) {
  for (size_t i = 0; i < size; i += 8) {
    const uint64_t v = os_entropy();
    for (size_t k = 0; k < 8 && i + k < size; k++) out[i + k] = static_cast<uint8_t>(v >> (8 * k));
  }
}

}  // namespace ge::net
