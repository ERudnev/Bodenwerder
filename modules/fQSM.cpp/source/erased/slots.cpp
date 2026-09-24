#include <fQSM/erased/slots.h>

#include <algorithm>
#include <cassert>
#include <new>
#include <stdexcept>
#include <utility>

namespace fqsm::erased {

    namespace {
        std::size_t stride_of(const Ops& ops) {
            const std::size_t size = std::max<std::size_t>(ops.size, 1);
            return (size + ops.align - 1) / ops.align * ops.align;
        }
    }

    Slots::Slots(const Ops& ops)
        : valueOps(&ops)
        , step(stride_of(ops))
    {}

    Slots::Slots(const Slots& other)
        : valueOps(other.valueOps)
        , step(other.step)
    {
        try {
            reserve(other.count);
            for (std::size_t i = 0; i < other.count; ++i)
                push_copy(other.at(static_cast<Index>(i)));
        } catch (...) {
            clear();
            deallocate();
            throw;
        }
    }

    Slots::Slots(Slots&& other) noexcept
        : valueOps(other.valueOps)
        , step(other.step)
        , storage(std::exchange(other.storage, nullptr))
        , count(std::exchange(other.count, 0))
        , capacity(std::exchange(other.capacity, 0))
    {}

    Slots& Slots::operator=(const Slots& other) {
        if (this == &other) return *this;
        Slots copy(other);
        *this = std::move(copy);
        return *this;
    }

    Slots& Slots::operator=(Slots&& other) noexcept {
        if (this == &other) return *this;
        clear();
        deallocate();
        valueOps = other.valueOps;
        step = other.step;
        storage = std::exchange(other.storage, nullptr);
        count = std::exchange(other.count, 0);
        capacity = std::exchange(other.capacity, 0);
        return *this;
    }

    Slots::~Slots() {
        clear();
        deallocate();
    }

    void Slots::build(Emplace how, void* dst, const void* src) const {
        switch (how) {
        case Emplace::construct: valueOps->construct(dst); break;
        case Emplace::copy: valueOps->copy(dst, src); break;
        case Emplace::move: valueOps->move(dst, const_cast<void*>(src)); break;
        }
    }

    // The new value is built before old values relocate, so src may point into this arena.
    auto Slots::emplace_back(Emplace how, const void* src) -> Index {
        if (count < capacity) {
            build(how, storage + count * step, src);
            return static_cast<Index>(count++);
        }
        const std::size_t next = std::max(count + 1, capacity < 4 ? std::size_t{4} : capacity * 2);
        auto* fresh = static_cast<std::byte*>(::operator new(next * step, std::align_val_t{valueOps->align}));
        try {
            build(how, fresh + count * step, src);
        } catch (...) {
            ::operator delete(fresh, std::align_val_t{valueOps->align});
            throw;
        }
        for (std::size_t i = 0; i < count; ++i) {
            void* from = storage + i * step;
            valueOps->move(fresh + i * step, from);
            valueOps->destroy(from);
        }
        deallocate();
        storage = fresh;
        capacity = next;
        return static_cast<Index>(count++);
    }

    auto Slots::push_default() -> Index {
        if (not valueOps->construct)
            throw std::logic_error("fqsm::erased::Slots: value type is not default-constructible");
        return emplace_back(Emplace::construct, nullptr);
    }

    auto Slots::push_copy(const void* src) -> Index {
        return emplace_back(Emplace::copy, src);
    }

    auto Slots::push_move(void* src) -> Index {
        return emplace_back(Emplace::move, src);
    }

    bool Slots::release(Index slot) {
        assert(slot < count);
        const std::size_t last = count - 1;
        valueOps->destroy(at(slot));
        if (slot == last) {
            --count;
            return false;
        }
        void* tail = at(static_cast<Index>(last));
        valueOps->move(at(slot), tail);
        valueOps->destroy(tail);
        --count;
        return true;
    }

    void Slots::clear() {
        for (std::size_t i = count; i-- > 0; )
            valueOps->destroy(at(static_cast<Index>(i)));
        count = 0;
    }

    void Slots::reserve(std::size_t wanted) {
        if (wanted > capacity)
            grow(wanted);
    }

    void Slots::grow(std::size_t next) {
        auto* fresh = static_cast<std::byte*>(::operator new(next * step, std::align_val_t{valueOps->align}));
        for (std::size_t i = 0; i < count; ++i) {
            void* from = storage + i * step;
            valueOps->move(fresh + i * step, from);
            valueOps->destroy(from);
        }
        deallocate();
        storage = fresh;
        capacity = next;
    }

    void Slots::deallocate() {
        if (storage)
            ::operator delete(storage, std::align_val_t{valueOps->align});
        storage = nullptr;
        capacity = 0;
    }
}
