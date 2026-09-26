#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <fQSM/identifier.h>
#include <fQSM/erased/descriptor.h>
#include <fQSM/erased/line.h>

namespace fqsm::erased {

    // Changes for one aspect line: one patchlet per id plus an optional global value.
    // A patchlet holds a value and two flags. tombstone: the id is deleted, the value is its last value.
    // verified: written by an honest put; false after a touch (value copied from the base for in-place edits).
    // Every write is a soft insert: the tombstone is ORed in and never cleared; only discard removes it.
    class PatchLine {
    public:
        PatchLine(const Ops& quantum, const Ops& global);
        explicit PatchLine(const Descriptor& descriptor);

        const Ops& quantum_ops() const { return entries.quantum_ops(); }
        const Ops& global_ops() const { return entries.global_ops(); }

        std::size_t count() const { return entries.size(); }
        // Patchlets the line can hold without growing; a cleared line keeps it (the pool judges by it).
        std::size_t capacity() const { return flags.capacity(); }
        bool has_changes() const { return count() != 0 or global() != nullptr; }

        RawId id_at(std::size_t position) const { return entries.id_at(position); }
        Mention at(std::size_t position) const;
        void* mutable_at(std::size_t position) { return entries.value_at(position); }
        bool verified_at(std::size_t position) const { return (flags[position] & verifiedFlag) != 0; }
        Mention mention(RawId id) const;
        void* find_mutable(RawId id);

        const void* global() const { return entries.global(); }       // nullptr: global not changed
        void* global_mutable() { return entries.global_mutable(); }
        void set_global(const void* value) { entries.set_global(value); }
        void* touch_global(const void* baseGlobal);

        // Honest put (modification or addition): verified, value replaced, tombstone kept.
        void* modify(RawId id, const void* value);
        void* modify_move(RawId id, void* value);
        // Deletion keeping lastValue (which may be this line's own value for id).
        void del(RawId id, const void* lastValue);
        // Existing patchlet: its value, flags unchanged. Else a new unverified patchlet copied from baseValue.
        void* touch(RawId id, const void* baseValue);
        // Removes the patchlet entirely; the only way to lift a tombstone.
        bool discard(RawId id);

        // Soft insert of every patchlet of other, in its order; other's global wins when set.
        void absorb(const PatchLine& other);
        // The same, moving the values out of other; other is cleared.
        void absorb_move(PatchLine& other);
        // Room for count patchlets in total, before a batch of inserts.
        void reserve(std::size_t count);
        void clear();

    private:
        static constexpr std::uint8_t tombstoneFlag = 1;
        static constexpr std::uint8_t verifiedFlag = 2;

        void* soft_insert(RawId id, const void* value, bool moveValue, std::uint8_t incoming);

        // Bit filter over the ids this line mentions: an id whose bit is clear is not here, so the overlay
        // cursor answers "not mentioned" without a hash lookup. Set on insert, never cleared on discard
        // (a stale bit only costs the lookup), rebuilt wider when the line grows, zeroed by clear().
        bool maybe(RawId id) const {
            if (filter.empty()) return false;
            const std::uint64_t bit = mix(id) >> (64 - filterBits);
            return (filter[bit >> 6] >> (bit & 63)) & 1u;
        }
        void remember(RawId id);
        static std::uint64_t mix(RawId id) { return static_cast<std::uint64_t>(id) * 0x9e3779b97f4a7c15ull; }

        Line entries;
        std::vector<std::uint8_t> flags;
        std::vector<std::uint64_t> filter;
        int filterBits = 0;   // log2 of the filter size in bits
    };

    // Shared read-only patch with no changes, for deltas over slots that were never written.
    const PatchLine& empty_patch_line();
}
