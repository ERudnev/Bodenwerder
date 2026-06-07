#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace base {

    template<typename AId, typename BId>
    class Relations {
    public:
        struct Link final {
            AId a;
            BId b;

            bool operator==(const Link&) const = default;
        };

        using SizeType = std::size_t;

    private:
        struct LinkHash final {
            SizeType operator()(const Link& link) const noexcept {
                const auto hashA = std::hash<AId>{}(link.a);
                const auto hashB = std::hash<BId>{}(link.b);
                return hashA ^ (hashB + 0x9e3779b97f4a7c15ull + (hashA << 6) + (hashA >> 2));
            }
        };

        using SlotSet = std::unordered_set<SizeType>;
        using PairIndex = std::unordered_map<Link, SizeType, LinkHash>;
        using AIndex = std::unordered_map<AId, SlotSet>;
        using BIndex = std::unordered_map<BId, SlotSet>;

    public:
        class ConstIterator {
        public:
            using iterator_category = std::forward_iterator_tag;
            using difference_type = std::ptrdiff_t;
            using value_type = Link;
            using pointer = const Link*;
            using reference = const Link&;

            reference operator*() const {
                return (*links)[*slotIterator];
            }

            pointer operator->() const {
                return std::addressof((*links)[*slotIterator]);
            }

            ConstIterator& operator++() {
                ++slotIterator;
                return *this;
            }

            ConstIterator operator++(int) {
                ConstIterator copy = *this;
                ++*this;
                return copy;
            }

            bool operator==(const ConstIterator& other) const {
                return links == other.links && slotIterator == other.slotIterator;
            }

            bool operator!=(const ConstIterator& other) const {
                return !(*this == other);
            }

        private:
            friend class Relations;

            ConstIterator(const std::vector<Link>& links, typename SlotSet::const_iterator slotIterator)
                : links(std::addressof(links))
                , slotIterator(std::move(slotIterator))
            {}

            const std::vector<Link>* links;
            typename SlotSet::const_iterator slotIterator;
        };

        class Range final {
        public:
            ConstIterator begin() const {
                return ConstIterator(*links, slots->begin());
            }

            ConstIterator end() const {
                return ConstIterator(*links, slots->end());
            }

            bool empty() const {
                return slots->empty();
            }

            SizeType size() const {
                return slots->size();
            }

        private:
            friend class Relations;

            Range(const std::vector<Link>& links, const SlotSet& slots)
                : links(std::addressof(links))
                , slots(std::addressof(slots))
            {}

            const std::vector<Link>* links;
            const SlotSet* slots;
        };

        bool insert(AId a, BId b) {
            const Link link{ std::move(a), std::move(b) };
            if (pairToIndex.contains(link)) return false;

            const SizeType slot = links.size();
            links.push_back(link);
            pairToIndex.emplace(link, slot);
            byA[link.a].insert(slot);
            byB[link.b].insert(slot);
            return true;
        }

        bool erase(AId a, BId b) {
            const auto lookup = pairToIndex.find(Link{ std::move(a), std::move(b) });
            if (lookup == pairToIndex.end()) return false;

            erase_slot(lookup->second);
            return true;
        }

        void erase_a(AId a) {
            while (true) {
                const auto lookup = byA.find(a);
                if (lookup == byA.end() || lookup->second.empty()) return;
                erase_slot(*lookup->second.begin());
            }
        }

        void erase_b(BId b) {
            while (true) {
                const auto lookup = byB.find(b);
                if (lookup == byB.end() || lookup->second.empty()) return;
                erase_slot(*lookup->second.begin());
            }
        }

        std::vector<BId> find_a(AId a) const {
            const auto& slots = find_slots(byA, a);
            std::vector<BId> result;
            result.reserve(slots.size());

            for (const SizeType slot : slots) {
                result.push_back(links[slot].b);
            }

            return result;
        }

        std::vector<AId> find_b(BId b) const {
            const auto& slots = find_slots(byB, b);
            std::vector<AId> result;
            result.reserve(slots.size());

            for (const SizeType slot : slots) {
                result.push_back(links[slot].a);
            }

            return result;
        }

        bool contains(AId a, BId b) const {
            return pairToIndex.contains(Link{ std::move(a), std::move(b) });
        }

        SizeType size() const {
            return links.size();
        }

        bool empty() const {
            return links.empty();
        }

    private:
        static const SlotSet& empty_slots() {
            static const SlotSet empty{};
            return empty;
        }

        template<typename Index, typename Key>
        static const SlotSet& find_slots(const Index& index, const Key& key) {
            const auto lookup = index.find(key);
            if (lookup == index.end()) return empty_slots();
            return lookup->second;
        }

        template<typename Index, typename Key>
        static void remove_slot_from_index(Index& index, const Key& key, SizeType slot) {
            const auto lookup = index.find(key);
            if (lookup == index.end()) return;

            lookup->second.erase(slot);
            if (lookup->second.empty()) index.erase(lookup);
        }

        void move_slot_in_indices(const Link& link, SizeType from, SizeType to) {
            auto aLookup = byA.find(link.a);
            aLookup->second.erase(from);
            aLookup->second.insert(to);

            auto bLookup = byB.find(link.b);
            bLookup->second.erase(from);
            bLookup->second.insert(to);
        }

        void erase_slot(SizeType removedSlot) {
            const Link removed = links[removedSlot];
            pairToIndex.erase(removed);
            remove_slot_from_index(byA, removed.a, removedSlot);
            remove_slot_from_index(byB, removed.b, removedSlot);

            const SizeType lastSlot = links.size() - 1;
            if (removedSlot != lastSlot) {
                const Link moved = links[lastSlot];
                links[removedSlot] = moved;
                move_slot_in_indices(moved, lastSlot, removedSlot);
                pairToIndex.find(moved)->second = removedSlot;
            }

            links.pop_back();
        }

        std::vector<Link> links;
        PairIndex pairToIndex;
        AIndex byA;
        BIndex byB;
    };
}