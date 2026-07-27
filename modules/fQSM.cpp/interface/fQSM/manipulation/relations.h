#pragma once

#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fQSM/identifier.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/manipulation/_experimental.h>
#include <fQSM/processing/contexts/review.h>

namespace fqsm::manipulation {

    namespace detail {

        // Target::Id → Watcher ids (scoped to a delta layer of Target).
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
                            call_action<Watchers>::get(context, *current),
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

        // ask::relations<Target>(reacting).removed|updated|added|addedOrUpdated<Watchers, link>()
        // Link: Affected<Target> or Id<Target> (Anchor/Custody aliases of Identifier).
        // Index covers only watchers linked to Target ids present in that delta layer;
        // empty layer → empty index (no Watchers scan).
        template<category::Any Target>
        struct RelationsOf {
            Reacting context;

            explicit RelationsOf(Reacting context) : context(std::move(context)) {}

            template<category::Any Watchers, ::fqsm::Affected<Target> Watchers::Quantum::* Link>
            auto removed() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().removed());
            }

            template<category::Any Watchers, ::fqsm::Id<Target> Watchers::Quantum::* Link>
            auto removed() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().removed());
            }

            template<category::Any Watchers, ::fqsm::Affected<Target> Watchers::Quantum::* Link>
            auto updated() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().updated());
            }

            template<category::Any Watchers, ::fqsm::Id<Target> Watchers::Quantum::* Link>
            auto updated() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().updated());
            }

            template<category::Any Watchers, ::fqsm::Affected<Target> Watchers::Quantum::* Link>
            auto added() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().added());
            }

            template<category::Any Watchers, ::fqsm::Id<Target> Watchers::Quantum::* Link>
            auto added() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().added());
            }

            template<category::Any Watchers, ::fqsm::Affected<Target> Watchers::Quantum::* Link>
            auto addedOrUpdated() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().addedOrUpdated());
            }

            template<category::Any Watchers, ::fqsm::Id<Target> Watchers::Quantum::* Link>
            auto addedOrUpdated() const -> InboundIndex<Target, Watchers> {
                return index_for_layer<Watchers, Link>(context.changes<Target>().addedOrUpdated());
            }

        private:
            template<category::Any Watchers, auto Link, typename LayerView>
            auto index_for_layer(LayerView layer) const -> InboundIndex<Target, Watchers> {
                InboundIndex<Target, Watchers> index{.context = context};
                if (layer.empty())
                    return index;

                std::unordered_set<typename Target::Id> interesting;
                for (const auto& change : layer)
                    interesting.insert(change.id);

                const Reading reading = context;
                for (const auto entry : reading->template aspect<Watchers>().items()) {
                    const auto& target = entry.value.*Link;
                    if (interesting.contains(target))
                        index.by_target[target].push_back(entry.id);
                }
                return index;
            }
        };

    } // namespace detail

    template<category::Any Target>
    auto relations(Reacting context) -> detail::RelationsOf<Target> {
        return detail::RelationsOf<Target>{std::move(context)};
    }

}
