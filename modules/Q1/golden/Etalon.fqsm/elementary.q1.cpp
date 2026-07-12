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

void StructWithMethods::add_to(StructWithMethods& target) const {
    target.x += x;
    target.y += y;
}

void StructWithMethods::add_from(const StructWithMethods& source) {
    x += source.x;
    y += source.y;
}

auto StructWithMethods::build_from(const StructWithMethods& source) -> StructWithMethods {
    return StructWithMethods{source.x, source.y};
}

} // namespace Q1_iQSM::Syntax::Typization
