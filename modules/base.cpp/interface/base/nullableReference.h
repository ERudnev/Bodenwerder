#pragma once

#include <stdexcept>

namespace base {

    template<typename T>
    struct NullableReference {
        NullableReference() = default;
        NullableReference(T& value) : ptr(&value) {}

        operator T&() const { return get(); }

        auto operator*() const -> T& { return get(); }
        auto operator->() const -> T* { return &get(); }

        void kill() { ptr = nullptr; }
        bool good() const { return ptr != nullptr; }

    private:
        auto get() const -> T& {
            if (not ptr) throw std::logic_error("base::NullableReference: empty");
            return *ptr;
        }
        T* ptr = nullptr;
    };

} // namespace base
