// `fill_host` (host.hpp): the table's header and the engine's libm by address.
//
// ge:summary fill_host: the host table's size, ABI and the engine's libm entries
// ge:keywords mod host fill_host libm sin cos atan2 pow
// ge:skill mod
// ge:test tests/mod_loader_test.cpp
#include "mod/host.hpp"

#include "core/libm/libm.hpp"

namespace ge::mod {

void fill_host(ge_mod_host_v1& h) {
  namespace lm = core::libm;
  h = ge_mod_host_v1{};
  h.size = sizeof(ge_mod_host_v1);
  h.abi = GE_MOD_ABI;
  h.sin = lm::sin;
  h.cos = lm::cos;
  h.tan = lm::tan;
  h.asin = lm::asin;
  h.acos = lm::acos;
  h.atan = lm::atan;
  h.atan2 = lm::atan2;
  h.exp = lm::exp;
  h.expm1 = lm::expm1;
  h.log = lm::log;
  h.log2 = lm::log2;
  h.pow = lm::pow;
  h.cbrt = lm::cbrt;
  h.tanh = lm::tanh;
}

}  // namespace ge::mod
