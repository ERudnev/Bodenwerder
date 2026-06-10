#pragma once

#include <cstddef>
#include <functional>
#include <memory>

#include <fQSM/meta/rtid.h>
#include <fQSM/state/slice/data.h>

namespace fqsm::processing {

    template<aspect::Any Meta>
    struct Immediate {
        using Slice = state::slice::Data<Meta, meta::axis::order::state>;
        using SliceRef = ref<Slice>;
        using Quantum = fqsm::Quantum<Meta>;
        using Upstream = std::function<void(aspect::Rtid)>;

        struct ItemRef {
            Quantum* value = nullptr;

            auto operator->() const -> Quantum* { return value; }
            auto operator=(const ItemRef& rhs) -> ItemRef& {
                *value = *rhs.value;
                return *this;
            }
            auto operator=(const Quantum& rhs) -> ItemRef& {
                *value = rhs;
                return *this;
            }
        };

        struct Items final {
            using Table = typename Slice::ItemsData;

            struct Iterator {
                using Raw = typename std::vector<typename Table::Entry>::iterator;

                Raw current;
                mutable ItemRef ref{};

                auto operator*() const -> ItemRef& {
                    ref.value = std::addressof(current->value);
                    return ref;
                }

                auto operator++() -> Iterator& {
                    ++current;
                    return *this;
                }

                auto operator++(int) -> Iterator {
                    Iterator copy = *this;
                    ++*this;
                    return copy;
                }

                auto operator==(const Iterator& rhs) const -> bool { return current == rhs.current; }
                auto operator!=(const Iterator& rhs) const -> bool { return !(*this == rhs); }
            };

            explicit Items(Table& table) : table(std::addressof(table)) {}

            auto begin() -> Iterator { return Iterator{table->entries.begin()}; }
            auto end() -> Iterator { return Iterator{table->entries.end()}; }

            auto size() const -> std::size_t { return table->size(); }

            auto operator[](std::size_t index) -> ItemRef {
                return ItemRef{std::addressof(table->entries.at(index).value)};
            }

        private:
            Table* table = nullptr;
        };

        struct Commit final {
            Upstream upstream;

            ~Commit() {
                if (upstream) {
                    upstream(aspect::Rtid::of<Meta>());
                }
            }
        };

        Immediate(SliceRef slice, Upstream upstream)
            : slice(std::move(slice))
            , items(this->slice->items())
            , commit(std::make_shared<Commit>(Commit{std::move(upstream)}))
        {}

        Items items;

    private:
        SliceRef slice;
        std::shared_ptr<Commit> commit;
    };
}
