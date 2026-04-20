#pragma once

#include <cstdint>

namespace base {

template<typename Meta, typename Storage = std::uint32_t>
class Identifier {
public:
    Identifier() = delete;

    explicit Identifier(Storage v) : value(v) {}

    // do not use for any kind of logic! Only for std::map and other containers!
    bool operator<(const Identifier& rhs) const { return value < rhs.value; }

    bool operator==(const Identifier& rhs) const { return value == rhs.value; }

    Storage raw() const { return value; }

private:
    Storage value;
};

} // namespace base
