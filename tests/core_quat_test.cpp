// engine/core/quat.hpp against the oracle's runtime (tests/golden/core_quat.txt, written by
// tools/parity/golden-core-libm.mjs under game5's pinned Node: make golden-libm). Row:
//   in:  heading pitch roll  ax ay az angle  wx wy wz dt  vx vy vz  s
//   out: fromEuler[4] fromAxisAngle[4] multiply(qe, qa)[4] rotate(qe, v)[3] unrotate(qe, v)[3]
//        integrate(qe, w, dt)[4] toEuler(qe)[3] normalize(qe * s)[4]
//
// ge:summary Parity test: core/quat against the oracle
// ge:keywords test parity golden quat
// ge:skill testing
#include "core/quat.hpp"

#include "golden.hpp"

using ge::core::Quat;
using ge::core::Vec3;
using ge::test::as_double;

namespace {

void check_quat(const Quat& q, const std::vector<uint64_t>& row, size_t at) {
  CHECK_BITS(q.x, as_double(row[at]));
  CHECK_BITS(q.y, as_double(row[at + 1]));
  CHECK_BITS(q.z, as_double(row[at + 2]));
  CHECK_BITS(q.w, as_double(row[at + 3]));
}

void check_vec(const Vec3& v, const std::vector<uint64_t>& row, size_t at) {
  CHECK_BITS(v.x, as_double(row[at]));
  CHECK_BITS(v.y, as_double(row[at + 1]));
  CHECK_BITS(v.z, as_double(row[at + 2]));
}

}  // namespace

TEST(quat_matches_oracle_bit_for_bit) {
  const auto rows = ge::test::golden_rows("tests/golden/core_quat.txt");
  CHECK(rows.size() == 800);
  for (const auto& row : rows) {
    double in[15];
    for (size_t i = 0; i < 15; i++) in[i] = as_double(row[i]);
    const Quat qe = ge::core::from_euler(in[0], in[1], in[2]);
    const Quat qa = ge::core::from_axis_angle(in[3], in[4], in[5], in[6]);
    const Vec3 v{in[11], in[12], in[13]};
    check_quat(qe, row, 15);
    check_quat(qa, row, 19);
    check_quat(ge::core::multiply(qe, qa), row, 23);
    check_vec(ge::core::rotate(qe, v), row, 27);
    check_vec(ge::core::unrotate(qe, v), row, 30);
    check_quat(ge::core::integrate(qe, in[7], in[8], in[9], in[10]), row, 33);
    const ge::core::Euler e = ge::core::to_euler(qe);
    CHECK_BITS(e.heading, as_double(row[37]));
    CHECK_BITS(e.pitch, as_double(row[38]));
    CHECK_BITS(e.roll, as_double(row[39]));
    Quat scaled{qe.x * in[14], qe.y * in[14], qe.z * in[14], qe.w * in[14]};
    check_quat(ge::core::normalize(scaled), row, 40);
  }
}

TEST(identity_and_tiny_rate) {
  const Quat q = ge::core::identity();
  CHECK(q.x == 0 && q.y == 0 && q.z == 0 && q.w == 1);
  // A rate under 1e-9 rad/s leaves the orientation as it was, unnormalised.
  const Quat skew{0.0, 0.0, 0.0, 2.0};
  const Quat same = ge::core::integrate(skew, 1e-10, 0.0, 0.0, 1.0);
  CHECK(same.w == 2.0);
}
