#pragma once

#include <cstddef>
#include <cstdint>

#include <fQSM/erased/descriptor.h>

namespace fqsm::erased {

    // Dense byte arena of values described by one Ops. Slots [0, size()) are always live.
    // Erase is swap-with-last: owners that keep a slot index must re-point the moved value.
    class Slots {
    public:
        using Index = std::uint32_t;
        using Builder = void (*)(void* dst, void* context);   // constructs one value into raw storage

        explicit Slots(const Ops& ops);
        Slots(const Slots& other);
        Slots(Slots&& other) noexcept;
        Slots& operator=(const Slots& other);
        Slots& operator=(Slots&& other) noexcept;
        ~Slots();

        const Ops& ops() const { return *valueOps; }
        std::size_t size() const { return count; }
        bool empty() const { return count == 0; }
        std::size_t stride() const { return step; }

        void* at(Index slot) { return storage + static_cast<std::size_t>(slot) * step; }
        const void* at(Index slot) const { return storage + static_cast<std::size_t>(slot) * step; }

        // Construct first, publish the index after: a throwing constructor leaves the arena unchanged.
        Index push_default();
        Index push_copy(const void* src);
        Index push_move(void* src);
        Index push_built(Builder build, void* context);

        // Destroys the value at slot; the last value (if any other) moves into slot.
        // Returns true when a value moved, from index size() (after the call) into slot.
        bool release(Index slot);

        void clear();
        void reserve(std::size_t capacity);

    private:
        enum class Emplace : std::uint8_t { construct, copy, move, build };
        Index emplace_back(Emplace how, const void* src, Builder builder = nullptr);
        void build(Emplace how, void* dst, const void* src, Builder builder) const;
        void grow(std::size_t next);
        void deallocate();

        const Ops* valueOps;
        std::size_t step;
        std::byte* storage = nullptr;
        std::size_t count = 0;
        std::size_t capacity = 0;
    };
}
