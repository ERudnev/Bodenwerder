#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <vector>

#include <fQSM/erased/line.h>

namespace tests::erased {

    // Minimal patch layer over int values: soft insert ORs the tombstone, like the runtime patch.
    struct PatchStub final : fqsm::erased::ReadPatch {
        struct Item {
            fqsm::RawId id;
            bool tombstone;
            int value;
        };

        std::vector<Item> items;
        std::optional<int> changedGlobal;

        void put(fqsm::RawId id, int value, bool tombstone = false) {
            for (auto& item : items) {
                if (item.id != id) continue;
                item.tombstone = item.tombstone or tombstone;
                item.value = value;
                return;
            }
            items.push_back(Item{id, tombstone, value});
        }

        std::size_t count() const override { return items.size(); }
        fqsm::RawId id_at(std::size_t index) const override { return items[index].id; }
        fqsm::erased::Mention at(std::size_t index) const override {
            return fqsm::erased::Mention{true, items[index].tombstone, &items[index].value};
        }
        fqsm::erased::Mention mention(fqsm::RawId id) const override {
            for (const auto& item : items)
                if (item.id == id) return fqsm::erased::Mention{true, item.tombstone, &item.value};
            return {};
        }
        const void* global() const override { return changedGlobal ? &*changedGlobal : nullptr; }

        void apply_to(std::map<fqsm::RawId, int>& model) const {
            for (const auto& item : items) {
                if (item.tombstone) model.erase(item.id);
                else model[item.id] = item.value;
            }
        }
    };

    inline std::map<fqsm::RawId, int> collect(const fqsm::erased::ReadLine& line) {
        std::map<fqsm::RawId, int> out;
        for (auto it = line.cursor_begin(), end = line.cursor_end(); not (it == end); ++it) {
            const auto entry = *it;
            out.emplace(entry.id, *static_cast<const int*>(entry.value));
        }
        return out;
    }

    inline std::size_t steps(const fqsm::erased::ReadLine& line) {
        std::size_t out = 0;
        for (auto it = line.cursor_begin(), end = line.cursor_end(); not (it == end); ++it)
            ++out;
        return out;
    }
}
