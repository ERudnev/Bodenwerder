#pragma once

#include <iterator>
#include <unordered_map>
#include <vector>

#include <fQSM/identifier.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::manipulation {

    namespace detail {

        // Target::Id → Watcher ids.
        //   .ids(target)   → cheap id range (empty if none)
        //   .items(target) → {id, const Quantum&} via get
        template<category::Any Target, category::Any Watchers>
        struct InboundIndex {
            using TargetId = typename Target::Id;
            using WatcherId = typename Watchers::Id;
            using Quantum = typename Watchers::Quantum;
            using Map = std::unordered_map<TargetId, std::vector<WatcherId>>;

            Reading context;
            Map by_target;

            struct Related {
                WatcherId id;
                const Quantum& item;
            };

            struct RelatedRange {
                Reading context;
                const std::vector<WatcherId>* ids = nullptr;

                struct iterator {
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = Related;
                    using difference_type = std::ptrdiff_t;
                    using pointer = void;
                    using reference = Related;

                    Reading context;
                    const WatcherId* current = nullptr;

                    auto operator*() const -> Related {
                        return Related{
                            *current,
                            Watchers::Actions::get(context, *current),
                        };
                    }

                    auto operator++() -> iterator& {
                        ++current;
                        return *this;
                    }

                    auto operator++(int) -> iterator {
                        auto copy = *this;
                        ++*this;
                        return copy;
                    }

                    friend auto operator==(const iterator& a, const iterator& b) -> bool {
                        return a.current == b.current;
                    }

                    friend auto operator!=(const iterator& a, const iterator& b) -> bool {
                        return not (a == b);
                    }
                };

                auto begin() const -> iterator {
                    if (ids == nullptr or ids->empty())
                        return iterator{context, nullptr};
                    return iterator{context, ids->data()};
                }

                auto end() const -> iterator {
                    if (ids == nullptr or ids->empty())
                        return iterator{context, nullptr};
                    return iterator{context, ids->data() + ids->size()};
                }
            };

            auto ids(TargetId id) const -> const std::vector<WatcherId>& {
                const auto* bucket = bucket_for(id);
                if (bucket == nullptr) {
                    static const std::vector<WatcherId> empty{};
                    return empty;
                }
                return *bucket;
            }

            auto items(TargetId id) const -> RelatedRange {
                return RelatedRange{context, bucket_for(id)};
            }

        private:
            auto bucket_for(TargetId id) const -> const std::vector<WatcherId>* {
                const auto found = by_target.find(id);
                if (found == by_target.end())
                    return nullptr;
                return &found->second;
            }
        };

        // ask::relations<Target>(context).to<Watchers, &Watchers::Quantum::link>()
        template<category::Any Target>
        struct RelationsOf {
            Reading context;

            explicit RelationsOf(Reading context) : context(context) {}

            template<category::Any Watchers, ::fqsm::Affected<Target> Watchers::Quantum::* Link>
            auto to() const -> InboundIndex<Target, Watchers> {
                InboundIndex<Target, Watchers> index{.context = context};
                for (const auto entry : context->template aspect<Watchers>().items()) {
                    index.by_target[entry.value.*Link].push_back(entry.id);
                }
                return index;
            }
        };

    } // namespace detail

    template<category::Any Target>
    auto relations(Reading context) -> detail::RelationsOf<Target> {
        return detail::RelationsOf<Target>{context};
    }

}
