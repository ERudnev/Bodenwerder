#include <fQSM/erased/patch_line.h>

#include <algorithm>

namespace fqsm::erased {

    PatchLine::PatchLine(const Ops& quantum, const Ops& global)
        : entries(quantum, global, Line::GlobalStart::absent)
    {}

    PatchLine::PatchLine(const Descriptor& descriptor)
        : PatchLine(*descriptor.quantum, *descriptor.global)
    {}

    Mention PatchLine::at(std::size_t position) const {
        return Mention{true, (flags[position] & tombstoneFlag) != 0, entries.value_at(position)};
    }

    Mention PatchLine::mention(RawId id) const {
        if (not maybe(id)) return {};
        const std::size_t position = entries.position_of(id);
        if (position == Line::npos) return {};
        return at(position);
    }

    void* PatchLine::find_mutable(RawId id) {
        return entries.find_mutable(id);
    }

    void* PatchLine::touch_global(const void* baseGlobal) {
        if (not entries.global()) {
            if (not baseGlobal) return nullptr;
            entries.set_global(baseGlobal);
        }
        return entries.global_mutable();
    }

    void* PatchLine::soft_insert(RawId id, const void* value, bool moveValue, std::uint8_t incoming) {
        const std::size_t position = entries.position_of(id);
        if (position != Line::npos) {
            void* stored = moveValue ? entries.emplace_move(id, const_cast<void*>(value)) : entries.insert(id, value);
            flags[position] = static_cast<std::uint8_t>((flags[position] & tombstoneFlag) | incoming);
            return stored;
        }
        if (flags.size() == flags.capacity())
            flags.reserve(flags.capacity() < 8 ? 8 : flags.capacity() * 2);
        void* stored = moveValue ? entries.emplace_move(id, const_cast<void*>(value)) : entries.insert(id, value);
        flags.push_back(incoming);
        remember(id);
        return stored;
    }

    void PatchLine::remember(RawId id) {
        // about eight bits per id: below that the filter is rebuilt four times wider from the ids present
        const std::size_t wanted = entries.size() * 8;
        if (filter.empty() or wanted > (std::size_t{1} << filterBits)) {
            int bits = filterBits == 0 ? 10 : filterBits + 2;
            while ((std::size_t{1} << bits) < wanted) bits += 2;
            filterBits = bits;
            filter.assign(std::size_t{1} << (bits - 6), 0);
            for (std::size_t i = 0; i < entries.size(); ++i) {
                const std::uint64_t bit = mix(entries.id_at(i)) >> (64 - filterBits);
                filter[bit >> 6] |= std::uint64_t{1} << (bit & 63);
            }
            return;
        }
        const std::uint64_t bit = mix(id) >> (64 - filterBits);
        filter[bit >> 6] |= std::uint64_t{1} << (bit & 63);
    }

    void* PatchLine::modify(RawId id, const void* value) {
        return soft_insert(id, value, false, verifiedFlag);
    }

    void* PatchLine::modify_move(RawId id, void* value) {
        return soft_insert(id, value, true, verifiedFlag);
    }

    void PatchLine::del(RawId id, const void* lastValue) {
        const std::size_t position = entries.position_of(id);
        if (position != Line::npos and entries.value_at(position) == lastValue) {
            flags[position] = tombstoneFlag | verifiedFlag;
            return;
        }
        soft_insert(id, lastValue, false, tombstoneFlag | verifiedFlag);
    }

    void* PatchLine::touch(RawId id, const void* baseValue) {
        const std::size_t position = entries.position_of(id);
        if (position != Line::npos) return entries.value_at(position);
        return soft_insert(id, baseValue, false, 0);
    }

    bool PatchLine::discard(RawId id) {
        const std::size_t position = entries.position_of(id);
        if (position == Line::npos) return false;
        entries.erase(id);
        flags[position] = flags.back();
        flags.pop_back();
        return true;
    }

    void PatchLine::reserve(std::size_t total) {
        if (total <= flags.capacity()) return;
        entries.reserve(total);
        flags.reserve(total);
    }

    void PatchLine::absorb(const PatchLine& other) {
        if (&other == this) return;
        reserve(count() + other.count());
        for (std::size_t i = 0; i < other.count(); ++i)
            soft_insert(other.id_at(i), other.entries.value_at(i), false, other.flags[i]);
        if (const void* global = other.global())
            entries.set_global(global);
    }

    void PatchLine::absorb_move(PatchLine& other) {
        if (&other == this) return;
        reserve(count() + other.count());
        for (std::size_t i = 0; i < other.count(); ++i)
            soft_insert(other.id_at(i), other.entries.value_at(i), true, other.flags[i]);
        if (const void* global = other.global())
            entries.set_global(global);
        other.clear();
    }

    void PatchLine::clear() {
        std::fill(filter.begin(), filter.end(), std::uint64_t{0});
        entries.clear();
        entries.reset_global();
        flags.clear();
    }

    const PatchLine& empty_patch_line() {
        static const PatchLine empty(ops_of<char>(), ops_of<char>());
        return empty;
    }
}
