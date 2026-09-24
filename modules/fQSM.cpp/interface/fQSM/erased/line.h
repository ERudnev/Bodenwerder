#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <fQSM/identifier.h>
#include <fQSM/erased/descriptor.h>
#include <fQSM/erased/slots.h>

namespace fqsm::erased {

    class Line;
    class PatchLine;

    // What one patch layer says about one id. A tombstone keeps the last value.
    struct Mention {
        bool found = false;
        bool tombstone = false;
        const void* value = nullptr;
    };

    // Forward read cursor over a root Line plus a stack of overlaid patch layers.
    // Plain mode (no layers) walks the root in storage order.
    // Overlay mode: for an id, the topmost layer that mentions it decides visibility (a tombstone hides it)
    // and supplies the value; with no mention, root membership decides. Root entries are emitted first;
    // a patch-only id is emitted once, by the lowest layer at which it first becomes visible.
    class Cursor {
    public:
        static constexpr std::size_t MaxLayers = 8;

        struct Entry {
            RawId id;
            const void* value;
        };

        Cursor() = default;

        static Cursor plain(const Line& line, std::size_t position);

        // Copies base's layer stack, pushes one more layer and resolves the position from scratch:
        // visibility under N+1 layers can differ from visibility under N.
        static Cursor overlay(const Cursor& base, const PatchLine& layer, bool atEnd);

        Entry operator*() const;
        Cursor& operator++();
        bool operator==(const Cursor& other) const;

    private:
        enum class Phase : std::uint8_t { root, layer, end };

        Mention mention(RawId id) const;
        Mention mention_below(std::size_t layer, RawId id) const;
        bool visible_after_all(RawId id) const;
        bool visible_below(std::size_t layer, RawId id) const;
        void skip_to_visible();

        const Line* root = nullptr;
        std::size_t rootIndex = 0;
        const PatchLine* layers[MaxLayers]{};
        std::size_t layerCount = 0;
        Phase phase = Phase::root;
        std::size_t currentLayer = 0;
        std::size_t layerIndex = 0;
    };

    // Read side of one aspect line: a reality, or a reality seen through patches.
    class ReadLine {
    public:
        virtual ~ReadLine() = default;

        virtual bool contains(RawId id) const = 0;
        virtual const void* find(RawId id) const = 0;
        virtual std::size_t size() const = 0;
        virtual const void* global() const = 0;
        virtual Cursor cursor_begin() const = 0;
        virtual Cursor cursor_end() const = 0;
    };

    // Reality of one aspect: ids and values in parallel dense arrays, one global value.
    class Line final : public ReadLine {
    public:
        enum class GlobalStart : std::uint8_t { constructed, absent };

        // By default the global value is default-constructed when its type allows it, else it stays absent until set_global.
        Line(const Ops& quantum, const Ops& global, GlobalStart start = GlobalStart::constructed);
        explicit Line(const Descriptor& descriptor);

        const Ops& quantum_ops() const { return slots.ops(); }
        const Ops& global_ops() const { return globalSlot.ops(); }

        bool contains(RawId id) const override;
        const void* find(RawId id) const override;
        std::size_t size() const override { return ids.size(); }
        const void* global() const override;
        Cursor cursor_begin() const override;
        Cursor cursor_end() const override;

        static constexpr std::size_t npos = static_cast<std::size_t>(-1);
        std::size_t position_of(RawId id) const;

        void* find_mutable(RawId id);
        void* global_mutable();
        void set_global(const void* value);
        void build_global(Slots::Builder build, void* context);
        void reset_global();

        // Copy (or move) value in; an existing value for id is replaced. Returns the stored value.
        void* insert(RawId id, const void* value);
        void* emplace_move(RawId id, void* value);
        bool erase(RawId id);
        void clear();
        void reserve(std::size_t capacity);

        // Replaces all items and the global value with what source shows.
        void clone(const ReadLine& source);

        RawId id_at(std::size_t position) const { return ids[position]; }
        const void* value_at(std::size_t position) const { return slots.at(static_cast<Slots::Index>(position)); }
        void* value_at(std::size_t position) { return slots.at(static_cast<Slots::Index>(position)); }

    private:
        void* place(RawId id, Slots::Index fresh);

        std::vector<RawId> ids;
        std::unordered_map<RawId, Slots::Index> positions;
        Slots slots;
        Slots globalSlot;
    };
}
