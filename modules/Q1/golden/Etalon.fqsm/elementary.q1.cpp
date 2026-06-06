#include <Etalon.fqsm/elementary.q1.h>

#include <cmath>

namespace Q1_iQSM::Syntax::Typization {

auto StructWithMethods::length() const -> Scalar {
    return std::sqrt(x * x + y * y);
}

void StructWithMethods::normalize(Scalar target_length) {
    const Scalar len = length();
    if (len <= Scalar{0}) {
        return;
    }
    const Scalar scale = target_length / len;
    x *= scale;
    y *= scale;
}

auto StructWithMethods::fromScalar(Scalar argument) -> StructWithMethods {
    return StructWithMethods{argument, argument};
}

} // namespace Q1_iQSM::Syntax::Typization
